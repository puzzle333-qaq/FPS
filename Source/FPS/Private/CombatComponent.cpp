#include "CombatComponent.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/Weapon.h"
#include "WeaponData.h"
#include "Interface/PlayerInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UnrealType.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"


UCombatComponent::UCombatComponent()
{

	PrimaryComponentTick.bCanEverTick = true;

	TraceLength = 20'000;

	bTriggerPressed = false;
	bAiming = false;

	LocalWeaponIndex = 0;
}


void UCombatComponent::Equip(AWeapon* Weapon)
{
	if (!IsValid(Weapon)) return;

	if (IsValid(CurrentWeapon) && CurrentWeapon != Weapon)
	{
		CurrentWeapon->DetachFromPawn();
		CurrentWeapon->SetActorHiddenInGame(true);
		CurrentWeapon->WeaponStatus = EWeaponStatus::UnEquipped;
	}

	CurrentWeapon = Weapon;
	CurrentWeapon->SetActorHiddenInGame(false);
	CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
	CurrentWeapon->AttachToOwingPawn();
	ApplyEquippedWeaponAnim(CurrentWeapon);

	if (const int32* FoundReserve = ReserveAmmo.Find(CurrentWeapon->WeaponType))
	{
		CurrentReserveAmmo = *FoundReserve;
	}
	else
	{
		CurrentReserveAmmo = CurrentWeapon->StartingCarriedAmmo;
		ReserveAmmo.Add(CurrentWeapon->WeaponType, CurrentReserveAmmo);
	}
	OnCurrentReserveAmmoChanged.Broadcast(CurrentReserveAmmo, CurrentWeapon->Ammo, CurrentWeapon->WeaponIcon);
	NotifyAmmoChanged();
}

void UCombatComponent::SpawnInventory()
{
	if (GetOwner()->GetLocalRole() < ROLE_Authority)return;

	for (TSubclassOf<AWeapon> WeaponClass : DefaultWeaponClass)
	{
		AWeapon* Weapon = SpawnWeapon(WeaponClass);
		if (!IsValid(Weapon)) continue;

		Inventory.AddUnique(Weapon);
		ReserveAmmo.Add(Weapon->WeaponType, Weapon->StartingCarriedAmmo);
	}

	if (Inventory.Num() > 0)
	{
		Equip(Inventory[0]);
		InitializeWeaponWidget();
		IPlayerInterface::Execute_WeaponReplicated(GetOwner());
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

void UCombatComponent::InitializeWeaponWidget()
{
	if (!IsValid(CurrentWeapon)) return;

	OnReticleChanged.Broadcast(CurrentWeapon->GetReticleDynamicMaterialInstance());
	NotifyAmmoChanged();
}

void UCombatComponent::NotifyAmmoChanged()
{
	if (!IsValid(CurrentWeapon)) return;
	OnCurrentReserveAmmoChanged.Broadcast(CurrentReserveAmmo, CurrentWeapon->Ammo, CurrentWeapon->WeaponIcon);
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	
}

void UCombatComponent::OnRep_CurrentWeapon(AWeapon* LastWeapon)
{
	if (IsValid(LastWeapon) && LastWeapon != CurrentWeapon)
	{
		LastWeapon->DetachFromPawn();
		LastWeapon->SetActorHiddenInGame(true);
		LastWeapon->WeaponStatus = EWeaponStatus::UnEquipped;
	}

	if (!IsValid(CurrentWeapon))return;

	CurrentWeapon->SetActorHiddenInGame(false);
	CurrentWeapon->AttachToOwingPawn();
	ApplyEquippedWeaponAnim(CurrentWeapon);
	IPlayerInterface::Execute_WeaponReplicated(GetOwner());
	InitializeWeaponWidget();
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
	DOREPLIFETIME_CONDITION(UCombatComponent, bAiming, COND_SkipOwner);

	DOREPLIFETIME_CONDITION(UCombatComponent, CurrentReserveAmmo, COND_OwnerOnly);
}

void UCombatComponent::Initiate_CycleWeapon()
{
	if (!IsValid(CurrentWeapon)) return;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Cycling) return;
	if (Inventory.Num() < 2) return;

	const int32 NextIndex = AdvanceWeaponIndex();
	Local_CycleWeapon(NextIndex, true);
	Server_CycleWeapon(NextIndex);
}

void UCombatComponent::Local_CycleWeapon(int32 WeaponIndex, bool bFirstPerson)
{
	if (!Inventory.IsValidIndex(WeaponIndex)) return;

	AWeapon* NextWeapon = Inventory[WeaponIndex];
	if (!IsValid(NextWeapon)) return;

	if (NextWeapon != CurrentWeapon)
	{
		Equip(NextWeapon);
	}

	PlayEquipMontage(NextWeapon, bFirstPerson);
}

void UCombatComponent::ApplyEquippedWeaponAnim(AWeapon* Weapon)
{
	if (!IsValid(Weapon) || !IsValid(WeaponData)) return;

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UPlayerInterface>()) return;

	auto ApplyToMesh = [Weapon, this](USkeletalMeshComponent* Mesh, bool bFirstPerson)
	{
		if (!IsValid(Mesh)) return;
		UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
		if (!IsValid(AnimInstance)) return;

		const TMap<FGameplayTag, FPlayerAnim>& Anims = bFirstPerson
			? WeaponData->FirstPersonAnims
			: WeaponData->ThirdPersonAnims;
		const FPlayerAnim* WeaponAnim = Anims.Find(Weapon->WeaponType);
		if (!WeaponAnim) return;

		// ABP_FirstPerson spells this CurrenrAnim. ABP_ThirdPerson uses CurrentAnim.
		const FName PropertyName = bFirstPerson ? TEXT("CurrenrAnim") : TEXT("CurrentAnim");
		FStructProperty* AnimProperty = FindFProperty<FStructProperty>(AnimInstance->GetClass(), PropertyName);
		if (!AnimProperty || AnimProperty->Struct != FPlayerAnim::StaticStruct()) return;

		AnimProperty->CopyCompleteValue(AnimProperty->ContainerPtrToValuePtr<void>(AnimInstance), WeaponAnim);
	};

	ApplyToMesh(IPlayerInterface::Execute_GetMesh1P(Owner), true);
	ApplyToMesh(IPlayerInterface::Execute_GetMesh3P(Owner), false);
}

void UCombatComponent::PlayEquipMontage(AWeapon* Weapon, bool bFirstPerson)
{
	if (!IsValid(Weapon) || !IsValid(WeaponData)) return;

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UPlayerInterface>()) return;

	const TMap<FGameplayTag, FMontageData>& ViewMontages = bFirstPerson
		? WeaponData->FirstPersonMontages
		: WeaponData->ThirdPersonMontages;

	const FMontageData* MontageData = ViewMontages.Find(Weapon->WeaponType);
	UAnimMontage* EquipMontage = (MontageData && IsValid(MontageData->EquipMontage)) ? MontageData->EquipMontage.Get() : nullptr;
	if (!IsValid(EquipMontage))
	{
		if (const FMontageData* SharedData = WeaponData->WeaponMontages.Find(Weapon->WeaponType))
		{
			EquipMontage = SharedData->EquipMontage;
		}
	}

	USkeletalMeshComponent* Mesh = bFirstPerson
		? IPlayerInterface::Execute_GetMesh1P(Owner)
		: IPlayerInterface::Execute_GetMesh3P(Owner);
	UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
	// Mesh1P is the arms skeleton. The assigned equip clips use the full-body mannequin
	// skeleton. Montage_Play still runs that pair; rejecting it is what silenced 1P.
	if (!IsValid(AnimInstance) || !IsValid(EquipMontage)) return;

	const float PlayedLength = AnimInstance->Montage_Play(EquipMontage);
	if (PlayedLength <= 0.f) return;

	if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(EquipMontage))
	{
		MontageInstance->bEnableAutoBlendOut = true;
	}

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UCombatComponent::OnEquipMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, EquipMontage);
	Weapon->WeaponStatus = EWeaponStatus::Cycling;
}

void UCombatComponent::OnEquipMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (IsValid(CurrentWeapon) && CurrentWeapon->WeaponStatus == EWeaponStatus::Cycling)
	{
		CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
	}
}

void UCombatComponent::Server_CycleWeapon_Implementation(int32 WeaponIndex)
{
	if (!Inventory.IsValidIndex(WeaponIndex)) return;

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	const bool bListenServerOwner = GetNetMode() == NM_ListenServer && IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	LocalWeaponIndex = WeaponIndex;

	// Listen-server host already equipped and played the 1P montage in Initiate_CycleWeapon.
	if (!bListenServerOwner)
	{
		AWeapon* NextWeapon = Inventory[WeaponIndex];
		if (IsValid(NextWeapon) && NextWeapon != CurrentWeapon)
		{
			Equip(NextWeapon);
		}
	}

	Multicast_CycleWeapon(WeaponIndex);
}

void UCombatComponent::Multicast_CycleWeapon_Implementation(int32 WeaponIndex)
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn) || OwningPawn->IsLocallyControlled())
	{
		// Owner already played the 1P montage. Re-entering Local_CycleWeapon from here
		// calls Server_CycleWeapon again and overflows the listen-server stack.
		return;
	}

	LocalWeaponIndex = WeaponIndex;
	Local_CycleWeapon(WeaponIndex, false);
}




void UCombatComponent::Initiate_FireWeapon_Pressed()
{
	if (!IsValid(CurrentWeapon))return;
	bTriggerPressed = true;

	if (CurrentWeapon->Ammo > 0)
	{
		Local_FireWeapon();
	}
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

	if (const FMontageData* MontageData = WeaponData->FirstPersonMontages.Find(CurrentWeapon->WeaponType))
	{
		UAnimMontage* Montage1P = MontageData->FireMontage;
		USkeletalMeshComponent* Mesh1P = IPlayerInterface::Execute_GetMesh1P(GetOwner());
		UAnimInstance* AnimInstance1P = IsValid(Mesh1P) ? Mesh1P->GetAnimInstance() : nullptr;
		if (IsValid(Montage1P) && IsValid(AnimInstance1P))
		{
			AnimInstance1P->Montage_Play(Montage1P);
		}
	}

	FHitResult Hit;
	CurrentWeapon->WeaponTrace(Hit,TraceLength);

	EPhysicalSurface ImpactSurfaceType = Hit.PhysMaterial.IsValid(false) ? Hit.PhysMaterial->SurfaceType.GetValue() : SurfaceType1;
	CurrentWeapon->Local_Fire(Hit.ImpactPoint, Hit.ImpactNormal, ImpactSurfaceType, true);
	NotifyAmmoChanged();

	GetWorld()->GetTimerManager().SetTimer(FireTimer, this, &ThisClass::FireTimerFinished, CurrentWeapon->FireTime);

	Server_FireWeapon(Hit);
}

void UCombatComponent::OnRep_CurrentReserveAmmo()
{
	if (IsValid(CurrentWeapon))
	{
		OnCurrentReserveAmmoChanged.Broadcast(CurrentReserveAmmo, CurrentWeapon->Ammo, CurrentWeapon->WeaponIcon);
	}
}

int32 UCombatComponent::AdvanceWeaponIndex()
{
	if (Inventory.Num() >= 2)
	{
		LocalWeaponIndex = (LocalWeaponIndex + 1) % Inventory.Num();
		
	}
	return LocalWeaponIndex;
}


void UCombatComponent::FireTimerFinished()
{
	if (!IsValid(CurrentWeapon))
	{
		return;
	}

	if (bTriggerPressed && CurrentWeapon->FireType == EFireType::Automatic&&CurrentWeapon->Ammo > 0)
	{
		Local_FireWeapon();
	}
}


void UCombatComponent::Server_FireWeapon_Implementation(const FHitResult& Hit)
{
	if (!IsValid(CurrentWeapon)) return;

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	const bool bListenServerOwner = GetNetMode() == NM_ListenServer && IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	if (!bListenServerOwner)
	{
		CurrentWeapon->Auth_Fire();
	}

	Multicast_FireWeapon(Hit, CurrentWeapon->Ammo);
}

void UCombatComponent::Multicast_FireWeapon_Implementation(const FHitResult& Hit, int32 AuthAmmo)
{
	if (!IsValid(CurrentWeapon)) return;

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	const bool bLocal = IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	if (bLocal)
	{
		CurrentWeapon->Rep_Fire(AuthAmmo);
		NotifyAmmoChanged();
		// Owning machine already played first-person FireEffect in Local_FireWeapon.
		return;
	}

	if (!IsValid(WeaponData))
	{
		ensure(IsValid(WeaponData));
		return;
	}

	EPhysicalSurface ImpactSurfaceType = Hit.PhysMaterial.IsValid(false) ? Hit.PhysMaterial->SurfaceType.GetValue() : SurfaceType1;
	CurrentWeapon->Local_Fire(Hit.ImpactPoint, Hit.ImpactNormal, ImpactSurfaceType, false);

	if (const FMontageData* MontageData = WeaponData->ThirdPersonMontages.Find(CurrentWeapon->WeaponType))
	{
		UAnimMontage* Montage3P = MontageData->FireMontage;
		USkeletalMeshComponent* Mesh3P = IPlayerInterface::Execute_GetMesh3P(OwningPawn);
		UAnimInstance* AnimInstance3P = IsValid(Mesh3P) ? Mesh3P->GetAnimInstance() : nullptr;
		if (IsValid(Montage3P) && IsValid(AnimInstance3P))
		{
			AnimInstance3P->Montage_Play(Montage3P);
		}
	}
}


void UCombatComponent::Initiate_ReloadWeapon()
{
	if (!IsValid(CurrentWeapon)) return;
	if (CurrentWeapon->Ammo >= CurrentWeapon->MagCapacity) return;
	if (CurrentReserveAmmo <= 0) return;

	ApplyReload();
	Server_ReloadWeapon();
}

void UCombatComponent::ApplyReload()
{
	if (!IsValid(CurrentWeapon)) return;

	const int32 Needed = CurrentWeapon->MagCapacity - CurrentWeapon->Ammo;
	const int32 Taken = FMath::Min(Needed, CurrentReserveAmmo);
	if (Taken <= 0) return;

	CurrentWeapon->Ammo += Taken;
	CurrentReserveAmmo -= Taken;
	ReserveAmmo.Add(CurrentWeapon->WeaponType, CurrentReserveAmmo);
	NotifyAmmoChanged();

	if (!IsValid(WeaponData)) return;
	if (const FMontageData* MontageData = WeaponData->FirstPersonMontages.Find(CurrentWeapon->WeaponType))
	{
		UAnimMontage* ReloadMontage = MontageData->ReloadMontage;
		USkeletalMeshComponent* Mesh1P = IPlayerInterface::Execute_GetMesh1P(GetOwner());
		UAnimInstance* AnimInstance1P = IsValid(Mesh1P) ? Mesh1P->GetAnimInstance() : nullptr;
		if (IsValid(ReloadMontage) && IsValid(AnimInstance1P))
		{
			AnimInstance1P->Montage_Play(ReloadMontage);
		}
	}
}

void UCombatComponent::Server_ReloadWeapon_Implementation()
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	const bool bListenServerOwner = GetNetMode() == NM_ListenServer && IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	if (!bListenServerOwner)
	{
		ApplyReload();
	}
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

	AWeapon* Weapon = GetWorld()->SpawnActor<AWeapon>(WeaponClass, SpawnInfo);
	if (IsValid(Weapon))
	{
		Weapon->SetActorHiddenInGame(true);
		Weapon->WeaponStatus = EWeaponStatus::UnEquipped;
	}
	return Weapon;
}

