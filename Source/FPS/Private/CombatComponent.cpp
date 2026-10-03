#include "CombatComponent.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/Weapon.h"
#include "WeaponData.h"
#include "Interface/PlayerInterface.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"


UCombatComponent::UCombatComponent()
{

	PrimaryComponentTick.bCanEverTick = true;

	TraceLength = 20'000;

	bTriggerPressed = false;
	bAiming = false;
}


void UCombatComponent::Equip(AWeapon* Weapon)
{
	CurrentWeapon = Weapon;
	CurrentWeapon->AttachToOwingPawn();
}

void UCombatComponent::SpawnInventory()
{
	if (GetOwner()->GetLocalRole() < ROLE_Authority)return;

	for (TSubclassOf<AWeapon> WeaponClass : DefaultWeaponClass)
	{
		AWeapon* Weapon = SpawnWeapon(WeaponClass);
		Inventory.AddUnique(Weapon);
	}

	if (Inventory.Num() > 0)
	{
		Equip(Inventory[0]);
	}
}

void UCombatComponent::DestroyInventory()
{
	for (AWeapon* Weapon : Inventory)
	{
		if (IsValid(Weapon))
		{
			Weapon->Destroy();
		}
	}
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	
}

void UCombatComponent::OnRep_CurrentWeapon(AWeapon* LastWeapon)
{
	if (!IsValid(CurrentWeapon))return;
	CurrentWeapon->AttachToOwingPawn();
}




void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCombatComponent, Inventory);
	DOREPLIFETIME(UCombatComponent, CurrentWeapon);
	DOREPLIFETIME_CONDITION(UCombatComponent,bAiming,COND_SkipOwner)
}

void UCombatComponent::Initiate_CycleWeapon()
{
	GEngine->AddOnScreenDebugMessage(-1,5.f,FColor::Cyan,TEXT("Initiate CycleWeapon"), false);
}

void UCombatComponent::Initiate_FireWeapon_Pressed()
{
	bTriggerPressed = true;
	Local_FireWeapon();
}

void UCombatComponent::Initiate_FireWeapon_Released()
{
	bTriggerPressed = false;
}

void UCombatComponent::Local_FireWeapon()
{
	if (!IsValid(WeaponData) || !IsValid(CurrentWeapon))
	{
		return;
	}

	UAnimMontage* Montage1P = WeaponData->FirstPersonMontages.FindChecked(CurrentWeapon->WeaponType).FireMontage;
	USkeletalMeshComponent* Mesh1P = IPlayerInterface::Execute_GetMesh1P(GetOwner());
	UAnimInstance* AnimInstance1P = IsValid(Mesh1P) ? Mesh1P->GetAnimInstance() : nullptr;
	if (IsValid(Montage1P) && IsValid(AnimInstance1P))
	{
		AnimInstance1P->Montage_Play(Montage1P);
	}

	FHitResult Hit;
	CurrentWeapon->WeaponTrace(Hit,TraceLength);

	EPhysicalSurface ImpactSurfaceType = Hit.PhysMaterial.IsValid(false) ? Hit.PhysMaterial->SurfaceType.GetValue() : SurfaceType1;
	CurrentWeapon->Local_Fire(Hit.ImpactPoint, Hit.ImpactNormal, ImpactSurfaceType, true);

	GetWorld()->GetTimerManager().SetTimer(FireTimer, this, &ThisClass::FireTimerFinished, CurrentWeapon->FireTime);

	Server_FireWeapon(Hit);
}


void UCombatComponent::FireTimerFinished()
{
	if (!IsValid(CurrentWeapon))
	{
		return;
	}

	if (bTriggerPressed && CurrentWeapon->FireType == EFireType::Automatic)
	{
		Local_FireWeapon();
	}
}


void UCombatComponent::Server_FireWeapon_Implementation(const FHitResult& Hit)
{
	Multicast_FireWeapon(Hit);
}

void UCombatComponent::Multicast_FireWeapon_Implementation(const FHitResult& Hit)
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn) || OwningPawn->IsLocallyControlled())
	{
		return;
	}

	if (!IsValid(WeaponData) || !IsValid(CurrentWeapon))
	{
		ensure(IsValid(WeaponData));
		return;
	}

	EPhysicalSurface ImpactSurfaceType = Hit.PhysMaterial.IsValid(false) ? Hit.PhysMaterial->SurfaceType.GetValue() : SurfaceType1;
	CurrentWeapon->Local_Fire(Hit.ImpactPoint, Hit.ImpactNormal, ImpactSurfaceType, false);

	UAnimMontage* Montage3P = WeaponData->ThirdPersonMontages.FindChecked(CurrentWeapon->WeaponType).FireMontage;
	USkeletalMeshComponent* Mesh3P = IPlayerInterface::Execute_GetMesh3P(OwningPawn);
	UAnimInstance* AnimInstance3P = IsValid(Mesh3P) ? Mesh3P->GetAnimInstance() : nullptr;
	if (IsValid(Montage3P) && IsValid(AnimInstance3P))
	{
		AnimInstance3P->Montage_Play(Montage3P);
	}
}


void UCombatComponent::Initiate_ReloadWeapon()
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("Initiate ReloadWeapon"), false);
}

void UCombatComponent::Initiate_Aim_Pressed()
{
	Local_Aim(true);
	Server_Aim(true);
}

void UCombatComponent::Initiate_Aim_Released()
{
	Local_Aim(false);
	Server_Aim(false);
}

void UCombatComponent::Server_Aim_Implementation(bool bPressed)
{
	Local_Aim(bPressed);
}



void UCombatComponent::Local_Aim(bool bPressed)
{
	bAiming = bPressed;
}



AWeapon* UCombatComponent::SpawnWeapon(TSubclassOf<AWeapon> WeaponClass) const
{
	AActor* OwningActor = GetOwner();
	if (!IsValid(OwningActor))return nullptr;
	if (OwningActor->GetLocalRole() != ROLE_Authority)return nullptr;

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = Cast<APawn>(OwningActor);
	SpawnInfo.Owner = OwningActor;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	return GetWorld()->SpawnActor<AWeapon>(WeaponClass, SpawnInfo);
}

