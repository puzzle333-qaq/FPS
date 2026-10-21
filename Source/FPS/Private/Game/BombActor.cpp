#include "Game/BombActor.h"

#include "Game/SearchDestroyGameMode.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

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


// 从目标点往下打射线，让模型底部（按 Mesh 包围盒，含 BP 里的缩放/轴心）贴地，并按地面法线对齐
static void FPS_SnapBombToGround(AActor* Bomb, UStaticMeshComponent* Mesh, const FVector& WorldLocation, const AActor* IgnoreA)
{
	UWorld* World = Bomb ? Bomb->GetWorld() : nullptr;
	if (!World) return;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BombGroundSnap), false, Bomb);
	if (IgnoreA) Params.AddIgnoredActor(IgnoreA);
	FHitResult Hit;
	const FVector Start = WorldLocation + FVector(0.f, 0.f, 50.f);
	const FVector End = WorldLocation - FVector(0.f, 0.f, 500.f);
	const FRotator Yaw(0.f, Bomb->GetActorRotation().Yaw, 0.f);
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		Bomb->SetActorLocationAndRotation(WorldLocation, Yaw);
		UE_LOG(LogTemp, Warning, TEXT("[SD] Bomb ground trace missed at %s"), *WorldLocation.ToString());
		return;
	}
	// 先按水平朝向放在命中点，再量 Mesh 底部和 Actor 原点的高度差
	FQuat Rot = Yaw.Quaternion();
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	if (!Normal.IsNearlyZero()) Rot = FQuat::FindBetweenNormals(FVector::UpVector, Normal) * Rot;
	Bomb->SetActorLocationAndRotation(Hit.ImpactPoint, Rot);
	float BottomOffset = 0.f;
	if (Mesh && Mesh->GetStaticMesh())
	{
		Mesh->UpdateBounds();
		const FBox Box = Mesh->Bounds.GetBox();
		BottomOffset = Hit.ImpactPoint.Z - Box.Min.Z;
	}
	Bomb->SetActorLocation(Hit.ImpactPoint + Normal * FMath::Max(0.f, BottomOffset) + Normal * 0.5f);
}

void ABombActor::DropAt(const FVector& WorldLocation)
{
	if (!HasAuthority()) return;
	bPlanted = false;
	Carrier = nullptr;
	ApplyWorldDrop();
	SetActorHiddenInGame(false);
	FPS_SnapBombToGround(this, Mesh, WorldLocation, nullptr);
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
	FPS_SnapBombToGround(this, Mesh, WorldLocation, nullptr);
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
