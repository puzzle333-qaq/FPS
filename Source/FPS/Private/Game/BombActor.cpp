#include "Game/BombActor.h"

#include "Game/SearchDestroyGameMode.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ABombActor::ABombActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	SetRootComponent(PickupSphere);
	PickupSphere->InitSphereRadius(80.f);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupSphere->SetGenerateOverlapEvents(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(PickupSphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetRelativeScale3D(FVector(0.28f, 0.18f, 0.12f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
}

void ABombActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABombActor, Carrier);
	DOREPLIFETIME(ABombActor, bPlanted);
}

void ABombActor::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	if (!HasAuthority() || bPlanted || IsValid(Carrier)) return;
	if (ASearchDestroyGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASearchDestroyGameMode>() : nullptr)
	{
		GM->NotifyBombOverlap(this, OtherActor);
	}
}

void ABombActor::GiveTo(APawn* NewCarrier)
{
	if (!HasAuthority() || !IsValid(NewCarrier)) return;
	bPlanted = false;
	Carrier = NewCarrier;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorHiddenInGame(true);
	SetPickupEnabled(false);
	ForceNetUpdate();
	Multicast_PickedUp(NewCarrier);
}

void ABombActor::DropAt(const FVector& WorldLocation)
{
	if (!HasAuthority()) return;
	bPlanted = false;
	Carrier = nullptr;
	ApplyWorldDrop();
	SetActorHiddenInGame(false);
	SetActorLocation(WorldLocation);
	SetPickupEnabled(true);
	ForceNetUpdate();
	Multicast_Dropped();
}

void ABombActor::PlantAt(const FVector& WorldLocation, EBombSiteId Site)
{
	if (!HasAuthority()) return;
	Carrier = nullptr;
	bPlanted = true;
	ApplyWorldDrop();
	SetActorHiddenInGame(false);
	SetActorLocation(WorldLocation);
	SetPickupEnabled(false);
	ForceNetUpdate();
	Multicast_Planted(Site);
}

void ABombActor::ApplyCarryAttach()
{
	if (!IsValid(Carrier)) return;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	AttachToComponent(Carrier->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	SetActorRelativeLocation(CarryOffset);
}

void ABombActor::ApplyWorldDrop()
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
}

void ABombActor::SetPickupEnabled(bool bEnabled)
{
	if (PickupSphere)
	{
		PickupSphere->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void ABombActor::OnRep_Carrier()
{
	if (IsValid(Carrier))
	{
		SetActorHiddenInGame(true);
		SetPickupEnabled(false);
	}
	else if (!bPlanted)
	{
		ApplyWorldDrop();
		SetActorHiddenInGame(false);
		SetPickupEnabled(true);
	}
}

void ABombActor::OnRep_Planted()
{
	if (bPlanted)
	{
		ApplyWorldDrop();
		SetPickupEnabled(false);
	}
}

void ABombActor::Multicast_PickedUp_Implementation(APawn* NewCarrier)
{
	OnBombPickedUp(NewCarrier);
}

void ABombActor::Multicast_Dropped_Implementation()
{
	OnBombDropped();
}

void ABombActor::Multicast_Planted_Implementation(EBombSiteId Site)
{
	OnBombPlanted(Site);
}

void ABombActor::Multicast_Exploded_Implementation()
{
	OnBombExploded();
}

void ABombActor::Multicast_Defused_Implementation()
{
	OnBombDefused();
}
