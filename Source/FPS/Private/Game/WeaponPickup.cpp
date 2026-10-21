#include "Game/WeaponPickup.h"

#include "Weapon/Weapon.h"
#include "Weapon/AttachmentData.h"
#include "ShooterPlayerController.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AWeaponPickup::AWeaponPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
	SetRootComponent(InteractBox);
	InteractBox->InitBoxExtent(FVector(40.f, 12.f, 8.f));
	InteractBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	VisualMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(InteractBox);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 枪平躺在地上（侧放）
	VisualMesh->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));

	FallbackMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackMesh"));
	FallbackMesh->SetupAttachment(InteractBox);
	FallbackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FallbackMesh->SetRelativeScale3D(FVector(0.6f, 0.15f, 0.12f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded()) FallbackMesh->SetStaticMesh(CubeMesh.Object);
	FallbackMesh->SetVisibility(false);
}

void AWeaponPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWeaponPickup, WeaponClass);
	DOREPLIFETIME(AWeaponPickup, Attachments);
}

AWeaponPickup* AWeaponPickup::SpawnDropped(UWorld* World, TSubclassOf<AWeaponPickup> PickupClass, const FWeaponPreset& Preset, const FVector& Location, float Yaw, AActor* IgnoreActor)
{
	if (!World || !Preset.WeaponClass) return nullptr;
	UClass* ClassToSpawn = PickupClass ? PickupClass.Get() : AWeaponPickup::StaticClass();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWeaponPickup* Pickup = World->SpawnActor<AWeaponPickup>(ClassToSpawn, Location, FRotator(0.f, Yaw, 0.f), Params);
	if (!IsValid(Pickup)) return nullptr;
	Pickup->WeaponClass = Preset.WeaponClass;
	Pickup->Attachments = Preset.Attachments;
	Pickup->RefreshVisual();
	Pickup->SnapToGround(Location, IgnoreActor);
	Pickup->ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[Interact] Dropped %s at %s"), *GetNameSafe(Preset.WeaponClass), *Pickup->GetActorLocation().ToString());
	return Pickup;
}

void AWeaponPickup::SnapToGround(const FVector& Location, AActor* IgnoreActor)
{
	UWorld* World = GetWorld();
	if (!World) return;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WeaponPickupSnap), false, this);
	if (IgnoreActor) Q.AddIgnoredActor(IgnoreActor);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Location + FVector(0.f, 0.f, 50.f), Location - FVector(0.f, 0.f, 500.f), ECC_Visibility, Q))
	{
		const float HalfZ = InteractBox ? InteractBox->GetScaledBoxExtent().Z : 8.f;
		SetActorLocation(Hit.ImpactPoint + FVector(0.f, 0.f, HalfZ + 0.5f));
	}
}

FWeaponPreset AWeaponPickup::GetPreset() const
{
	FWeaponPreset P;
	P.WeaponClass = WeaponClass;
	P.Attachments = Attachments;
	return P;
}

void AWeaponPickup::OnRep_Weapon()
{
	RefreshVisual();
}

void AWeaponPickup::RefreshVisual()
{
	USkeletalMesh* SkelAsset = nullptr;
	if (WeaponClass)
	{
		if (const AWeapon* CDO = WeaponClass->GetDefaultObject<AWeapon>())
		{
			if (const USkeletalMeshComponent* Src = CDO->GetMesh3P())
			{
				SkelAsset = Src->GetSkeletalMeshAsset();
			}
			if (!SkelAsset)
			{
				if (const USkeletalMeshComponent* Src1P = CDO->GetMesh1P())
				{
					SkelAsset = Src1P->GetSkeletalMeshAsset();
				}
			}
		}
	}
	if (VisualMesh)
	{
		VisualMesh->SetSkeletalMeshAsset(SkelAsset);
		VisualMesh->SetVisibility(SkelAsset != nullptr);
	}
	if (FallbackMesh)
	{
		FallbackMesh->SetVisibility(SkelAsset == nullptr && WeaponClass != nullptr);
	}
}

bool AWeaponPickup::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return WeaponClass != nullptr && IsValid(InteractingPawn);
}

FText AWeaponPickup::GetInteractPrompt_Implementation(APawn* InteractingPawn) const
{
	const FText Name = WeaponClass ? WeaponClass->GetDisplayNameText() : FText::GetEmpty();
	return FText::Format(NSLOCTEXT("Interact", "PickupWeapon", "按 F 拾取 {0}"), Name);
}

void AWeaponPickup::Interact_Implementation(APawn* InteractingPawn)
{
	if (!HasAuthority() || !IsValid(InteractingPawn)) return;
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(InteractingPawn->GetController()))
	{
		if (PC->Auth_PickupWeapon(this))
		{
			Destroy();
		}
	}
}
