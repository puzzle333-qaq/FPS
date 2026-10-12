#include "CombatComponent.h"
#include "ShooterCharacter.h"
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
#include "ShooterType/ShooterType.h"
#include "Interface/RecoilModifierInterface.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	void AppendAttachmentRecoil(UObject* Object, FRecoilScale& Scale)
	{
		if (!IsValid(Object) || !Object->GetClass()->ImplementsInterface(URecoilModifier::StaticClass()))
		{
			return;
		}

		const FRecoilScale Part = IRecoilModifier::Execute_GetAttachmentRecoilScale(Object);
		Scale.Pitch *= FMath::Max(0.f, Part.Pitch);
		Scale.Yaw *= FMath::Max(0.f, Part.Yaw);
		Scale.Recovery *= FMath::Max(0.f, Part.Recovery);
	}

	void GatherAttachmentRecoil(const AActor* Actor, FRecoilScale& Scale)
	{
		if (!IsValid(Actor))
		{
			return;
		}

		AppendAttachmentRecoil(const_cast<AActor*>(Actor), Scale);
		TInlineComponentArray<UActorComponent*> Components(Actor);
		for (UActorComponent* Component : Components)
		{
			AppendAttachmentRecoil(Component, Scale);
		}
	}

	// Positive pitch looks up. The applied amount is what actually fit inside the view limits.
	void AddViewRecoil(APlayerController* PC, float PitchDegrees, float YawDegrees, float& OutAppliedPitch, float& OutAppliedYaw)
	{
		OutAppliedPitch = 0.f;
		OutAppliedYaw = 0.f;
		if (!IsValid(PC))
		{
			return;
		}

		const FRotator Control = PC->GetControlRotation();
		const float OldPitch = FRotator::NormalizeAxis(Control.Pitch);
		const float OldYaw = FRotator::NormalizeAxis(Control.Yaw);
		float PitchMin = -89.f;
		float PitchMax = 89.f;
		if (const APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
		{
			PitchMin = CameraManager->ViewPitchMin;
			PitchMax = CameraManager->ViewPitchMax;
		}
		const float NewPitch = FMath::Clamp(OldPitch + PitchDegrees, PitchMin, PitchMax);
		const float NewYaw = FRotator::NormalizeAxis(OldYaw + YawDegrees);

		FRotator Updated = Control;
		Updated.Pitch = NewPitch;
		Updated.Yaw = NewYaw;
		PC->SetControlRotation(Updated);

		OutAppliedPitch = NewPitch - OldPitch;
		OutAppliedYaw = FRotator::NormalizeAxis(NewYaw - OldYaw);
	}
}


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
	ResetRecoilPattern();
	OnCurrentWeaponChanged.Broadcast(CurrentWeapon); // 通知 UI：换枪了
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

	int32 LongGunCount = 0;
	int32 PistolCount = 0;
	int32 MeleeCount = 0;
	for (const AWeapon* Weapon : Inventory)
	{
		if (!IsValid(Weapon)) continue;
		switch (Weapon->WeaponKind)
		{
		case EWeaponKind::LongGun: ++LongGunCount; break;
		case EWeaponKind::Pistol: ++PistolCount; break;
		case EWeaponKind::Melee: ++MeleeCount; break;
		default: break;
		}
	}
	if (LongGunCount > 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has %d long guns. Main uses the first, secondary uses the second."), *GetNameSafe(GetOwner()), LongGunCount);
	}
	if (PistolCount > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has %d pistols. The pistol slot uses the first one."), *GetNameSafe(GetOwner()), PistolCount);
	}
	if (MeleeCount > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has %d knives. The melee slot uses the first one."), *GetNameSafe(GetOwner()), MeleeCount);
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
	ResetRecoilPattern();
	IPlayerInterface::Execute_WeaponReplicated(GetOwner());
	InitializeWeaponWidget();
	OnCurrentWeaponChanged.Broadcast(CurrentWeapon); // 客户端也通知 UI
}




void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickRecoil(DeltaTime);
}

void UCombatComponent::ResetRecoilPattern()
{
	RecoilShotIndex = 0;
}

FRecoilScale UCombatComponent::ResolveRecoilScale() const
{
	FRecoilScale Scale;
	if (!IsValid(CurrentWeapon))
	{
		return Scale;
	}

	const float AimScale = bAiming ? CurrentWeapon->AimRecoilMultiplier : 1.f;
	Scale.Pitch = FMath::Max(0.f, CurrentWeapon->RecoilPitchScale) * FMath::Max(0.f, AimScale);
	Scale.Yaw = FMath::Max(0.f, CurrentWeapon->RecoilYawScale) * FMath::Max(0.f, AimScale);
	Scale.Recovery = 1.f;
	GatherAttachmentRecoil(CurrentWeapon, Scale);
	GatherAttachmentRecoil(GetOwner(), Scale);
	return Scale;
}

void UCombatComponent::ApplyFireRecoil()
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn) || !OwningPawn->IsLocallyControlled() || !IsValid(CurrentWeapon))
	{
		return;
	}
	if (CurrentWeapon->WeaponKind == EWeaponKind::Melee || CurrentWeapon->RecoilPattern.Num() == 0)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwningPawn->GetController());
	if (!IsValid(PC))
	{
		return;
	}

	const int32 StepIndex = FMath::Clamp(RecoilShotIndex, 0, CurrentWeapon->RecoilPattern.Num() - 1);
	const FRecoilStep& Step = CurrentWeapon->RecoilPattern[StepIndex];
	const FRecoilScale Scale = ResolveRecoilScale();

	float AppliedPitch = 0.f;
	float AppliedYaw = 0.f;
	AddViewRecoil(PC, Step.Pitch * Scale.Pitch, Step.Yaw * Scale.Yaw, AppliedPitch, AppliedYaw);

	UnrecoveredRecoil.X += AppliedYaw;
	UnrecoveredRecoil.Y += AppliedPitch;
	SecondsSinceRecoilShot = 0.f;
	++RecoilShotIndex;
}

void UCombatComponent::TickRecoil(float DeltaTime)
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn) || !OwningPawn->IsLocallyControlled())
	{
		return;
	}
	if (RecoilShotIndex == 0 && UnrecoveredRecoil.IsNearlyZero())
	{
		return;
	}

	SecondsSinceRecoilShot += DeltaTime;

	const float ResetTime = IsValid(CurrentWeapon) ? CurrentWeapon->RecoilResetTime : 0.4f;
	const float RecoveryDelay = IsValid(CurrentWeapon) ? CurrentWeapon->RecoilRecoveryDelay : 0.12f;
	const float RecoverySpeed = IsValid(CurrentWeapon) ? CurrentWeapon->RecoilRecoverySpeed : 18.f;
	const bool bSprayHeld = bTriggerPressed
		&& IsValid(CurrentWeapon)
		&& CurrentWeapon->WeaponStatus != EWeaponStatus::Reloading
		&& CurrentWeapon->WeaponStatus != EWeaponStatus::Cycling;

	if (!bSprayHeld && SecondsSinceRecoilShot >= ResetTime)
	{
		RecoilShotIndex = 0;
	}

	if (bSprayHeld || SecondsSinceRecoilShot < RecoveryDelay || UnrecoveredRecoil.IsNearlyZero())
	{
		return;
	}

	const float Length = UnrecoveredRecoil.Size();
	const float Speed = RecoverySpeed * ResolveRecoilScale().Recovery;
	const float Step = FMath::Min(Length, FMath::Max(0.f, Speed) * DeltaTime);
	if (Step <= KINDA_SMALL_NUMBER)
	{
		UnrecoveredRecoil = FVector2D::ZeroVector;
		return;
	}

	const FVector2D GiveBack = UnrecoveredRecoil * (Step / Length);
	APlayerController* PC = Cast<APlayerController>(OwningPawn->GetController());
	float AppliedPitch = 0.f;
	float AppliedYaw = 0.f;
	AddViewRecoil(PC, -GiveBack.Y, -GiveBack.X, AppliedPitch, AppliedYaw);
	if (FMath::IsNearlyZero(AppliedPitch) && FMath::IsNearlyZero(AppliedYaw))
	{
		UnrecoveredRecoil = FVector2D::ZeroVector;
		return;
	}

	UnrecoveredRecoil.X += AppliedYaw;
	UnrecoveredRecoil.Y += AppliedPitch;
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
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;
	if (Inventory.Num() < 2) return;

	StopAimForWeaponSwitch();   // 切枪前先关镜

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

	bool bWalkGait = false;
	if (const AShooterCharacter* Shooter = Cast<AShooterCharacter>(Owner))
	{
		bWalkGait = Shooter->IsUsingWalkGait();
	}
	RefreshLocomotionBlendSpaces(bWalkGait);
}

bool UCombatComponent::HasStrafeWalkingAnim() const
{
	if (!IsValid(CurrentWeapon) || !IsValid(WeaponData)) return false;

	auto HasWalk = [this](const TMap<FGameplayTag, FPlayerAnim>& Anims) -> bool
	{
		if (const FPlayerAnim* Anim = Anims.Find(CurrentWeapon->WeaponType))
		{
			return IsValid(Anim->Strafe_Walking);
		}
		return false;
	};

	// Either view having Walk BS is enough — 3P is what you see for foot sliding
	return HasWalk(WeaponData->ThirdPersonAnims) || HasWalk(WeaponData->FirstPersonAnims);
}
void UCombatComponent::RefreshLocomotionBlendSpaces(bool bUseWalkGait)
{
	if (!IsValid(CurrentWeapon) || !IsValid(WeaponData)) return;

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UPlayerInterface>()) return;

	auto ApplyToMesh = [this, bUseWalkGait](USkeletalMeshComponent* Mesh, bool bFirstPerson)
	{
		if (!IsValid(Mesh)) return;
		UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
		if (!IsValid(AnimInstance)) return;

		const TMap<FGameplayTag, FPlayerAnim>& Anims = bFirstPerson
			? WeaponData->FirstPersonAnims
			: WeaponData->ThirdPersonAnims;
		const FPlayerAnim* WeaponAnim = Anims.Find(CurrentWeapon->WeaponType);
		if (!WeaponAnim) return;

		FPlayerAnim AnimCopy = *WeaponAnim;
		// 站立 Locomotion 绑的是 Strafe_Standing；慢走时换成 Walk BS，避免继续采 Jog 造成小碎步
		if (bUseWalkGait && IsValid(AnimCopy.Strafe_Walking))
		{
			AnimCopy.Strafe_Standing = AnimCopy.Strafe_Walking;
		}

		// ABP_FirstPerson spells this CurrenrAnim. ABP_ThirdPerson uses CurrentAnim.
		const FName PropertyName = bFirstPerson ? TEXT("CurrenrAnim") : TEXT("CurrentAnim");
		FStructProperty* AnimProperty = FindFProperty<FStructProperty>(AnimInstance->GetClass(), PropertyName);
		if (!AnimProperty || AnimProperty->Struct != FPlayerAnim::StaticStruct()) return;

		AnimProperty->CopyCompleteValue(AnimProperty->ContainerPtrToValuePtr<void>(AnimInstance), &AnimCopy);
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
	if (IsValid(CurrentWeapon) && CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;

	// 服务器这边也关镜，bAiming 会同步给其他玩家
	Local_Aim(false);

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
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;
	// The burst continues from FireTimerFinished while this timer is still the active callback.
	// Gating Local_FireWeapon on IsTimerActive stops that next shot.
	if (GetWorld()->GetTimerManager().IsTimerActive(FireTimer)) return;

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
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;

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
	ApplyFireRecoil();
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

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	// Empty mag starts the same reload as the reload key. ApplyReload here fills the
	// magazine immediately and never plays the montage.
	if (CurrentWeapon->Ammo == 0 && CurrentReserveAmmo > 0 && IsValid(OwningPawn) && OwningPawn->IsLocallyControlled())
	{
		Initiate_ReloadWeapon();
		return;
	}


	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;

	if (bTriggerPressed && CurrentWeapon->FireType == EFireType::Automatic&&CurrentWeapon->Ammo > 0)
	{
		Local_FireWeapon();
	}
}


void UCombatComponent::Server_FireWeapon_Implementation(const FHitResult& Hit)
{
	if (!IsValid(CurrentWeapon)) return;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;

	const float Now = GetWorld()->GetTimeSeconds();
	const float MinInterval = FMath::Max(0.f, CurrentWeapon->FireTime - 0.02f);
	if (MinInterval > 0.f && Now < ServerNextFireTime) return;
	ServerNextFireTime = Now + MinInterval;

	if (IsValid(Hit.GetActor()) && Hit.GetActor()->Implements<UPlayerInterface>())
	{
		const FName Bone = ResolveDamageBone(Hit);
		const float Damage = ScaleDamageForBone(CurrentWeapon->Damage, Bone, CurrentWeapon->HeadshotMultiplier, CurrentWeapon->LimbMultiplier);
		UE_LOG(LogTemp, Log, TEXT("Hit %s bone %s zone %s damage %.1f"),
			*GetNameSafe(Hit.GetActor()),
			*Bone.ToString(),
			*UEnum::GetValueAsString(ResolveHitZone(Bone)),
			Damage);
		IPlayerInterface::Execute_DoDamage(Hit.GetActor(), Damage, GetOwner());
	}

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
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Cycling) return;
	if (CurrentWeapon->Ammo >= CurrentWeapon->MagCapacity) return;
	if (CurrentReserveAmmo <= 0) return;

	UAnimMontage* ReloadMontage = nullptr;
	UAnimInstance* AnimInstance = nullptr;
	if (!GetReloadMontage(true, ReloadMontage, AnimInstance)) return;

	CurrentWeapon->WeaponStatus = EWeaponStatus::Reloading;
	ResetRecoilPattern();
	Server_ReloadWeapon();
	if (!PlayReloadMontage(true))
	{
		CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
		Server_EndReload();
	}
}

bool UCombatComponent::ApplyReload()
{
	if (!IsValid(CurrentWeapon)) return false;

	// Simulated proxies also play the 3P montage. Ignore their notify so only the
	// owning view and the server move rounds.
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	const bool bLocal = IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	if (!bLocal && GetOwnerRole() != ROLE_Authority) return false;

	const int32 Needed = CurrentWeapon->MagCapacity - CurrentWeapon->Ammo;
	const int32 Taken = FMath::Min(Needed, CurrentReserveAmmo);
	if (Taken <= 0) return false;

	CurrentWeapon->Ammo += Taken;
	CurrentReserveAmmo -= Taken;
	ReserveAmmo.Add(CurrentWeapon->WeaponType, CurrentReserveAmmo);
	NotifyAmmoChanged();
	return true;
}

bool UCombatComponent::GetReloadMontage(bool bFirstPerson, UAnimMontage*& OutMontage, UAnimInstance*& OutAnim) const
{
	OutMontage = nullptr;
	OutAnim = nullptr;
	if (!IsValid(CurrentWeapon) || !IsValid(WeaponData)) return false;

	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return false;

	const TMap<FGameplayTag, FMontageData>& ViewMontages = bFirstPerson
		? WeaponData->FirstPersonMontages
		: WeaponData->ThirdPersonMontages;
	const FMontageData* MontageData = ViewMontages.Find(CurrentWeapon->WeaponType);
	if (!MontageData || !IsValid(MontageData->ReloadMontage)) return false;

	USkeletalMeshComponent* Mesh = bFirstPerson
		? IPlayerInterface::Execute_GetMesh1P(Owner)
		: IPlayerInterface::Execute_GetMesh3P(Owner);
	UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
	if (!IsValid(AnimInstance)) return false;

	OutMontage = MontageData->ReloadMontage;
	OutAnim = AnimInstance;
	return true;
}

bool UCombatComponent::PlayReloadMontage(bool bFirstPerson)
{
	UAnimMontage* ReloadMontage = nullptr;
	UAnimInstance* AnimInstance = nullptr;
	if (!GetReloadMontage(bFirstPerson, ReloadMontage, AnimInstance)) return false;

	const float PlayedLength = AnimInstance->Montage_Play(ReloadMontage);
	if (PlayedLength <= 0.f) return false;

	// The gun reload moves the magazine on the weapon mesh. Play it only on the
	// owning first-person mesh; the third-person gun leaves the magazine floating.
	if (bFirstPerson)
	{
		if (const FMontageData* WeaponMontageData = WeaponData->WeaponMontages.Find(CurrentWeapon->WeaponType))
		{
			UAnimMontage* WeaponReloadMontage = WeaponMontageData->ReloadMontage;
			USkeletalMeshComponent* WeaponMesh = CurrentWeapon->GetMesh1P();
			UAnimInstance* WeaponAnim = IsValid(WeaponMesh) ? WeaponMesh->GetAnimInstance() : nullptr;
			if (IsValid(WeaponReloadMontage) && IsValid(WeaponAnim))
			{
				WeaponAnim->Montage_Play(WeaponReloadMontage);
			}
		}
	}

	// The owner's 1P montage owns the Reloading lock. The 3P montage is only what other people see.
	FOnMontageEnded EndDelegate;
	if (bFirstPerson)
	{
		EndDelegate.BindUObject(this, &UCombatComponent::OnReloadMontageEnded);
	}
	else
	{
		EndDelegate.BindUObject(this, &UCombatComponent::OnObservedReloadMontageEnded);
	}
	AnimInstance->Montage_SetEndDelegate(EndDelegate, ReloadMontage);
	return true;
}

void UCombatComponent::OnObservedReloadMontageEnded(UAnimMontage* /*Montage*/, bool /*bInterrupted*/)
{
	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->AttachToOwingPawn();
	}
}

void UCombatComponent::OnReloadMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->AttachToOwingPawn();
	}
	if (!IsValid(Montage) || !IsValid(CurrentWeapon)) return;
	if (CurrentWeapon->WeaponStatus != EWeaponStatus::Reloading) return;

	CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn) || !OwningPawn->IsLocallyControlled()) return;
	if (GetOwnerRole() != ROLE_Authority)
	{
		Server_EndReload();
	}
}


void UCombatComponent::Client_ReloadWeapon_Implementation(int32 NewWeaponAmmo, int32 NewCarryAmmo)
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(CurrentWeapon) || !IsValid(OwningPawn) || !OwningPawn->IsLocallyControlled()) return;

	CurrentWeapon->Ammo = NewWeaponAmmo;
	CurrentReserveAmmo = NewCarryAmmo;
	ReserveAmmo.Add(CurrentWeapon->WeaponType, NewCarryAmmo);
	CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
	NotifyAmmoChanged();

	if (bTriggerPressed && CurrentWeapon->Ammo > 0)
	{
		Local_FireWeapon();
	}
}

void UCombatComponent::Server_ReloadWeapon_Implementation()
{
	if (!IsValid(CurrentWeapon)) return;

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	const bool bListenServerOwner = GetNetMode() == NM_ListenServer && IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	if (!bListenServerOwner)
	{
		if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;
		if (CurrentWeapon->WeaponStatus == EWeaponStatus::Cycling) return;
		if (CurrentWeapon->Ammo >= CurrentWeapon->MagCapacity) return;
		if (CurrentReserveAmmo <= 0) return;
		CurrentWeapon->WeaponStatus = EWeaponStatus::Reloading;
	}

	Multicast_ReloadWeapon();
}

void UCombatComponent::Server_EndReload_Implementation()
{
	if (!IsValid(CurrentWeapon)) return;
	if (CurrentWeapon->WeaponStatus != EWeaponStatus::Reloading) return;
	CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
}

void UCombatComponent::Multicast_ReloadWeapon_Implementation()
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn) || OwningPawn->IsLocallyControlled())
	{
		// Owner already plays the 1P montage. Playing it again from here restarts the clip.
		return;
	}

	PlayReloadMontage(false);
}

void UCombatComponent::Initiate_Aim_Pressed()
{
	const bool bNextAim = !bAiming;
	Local_Aim(bNextAim);
	Server_Aim(bNextAim);
}

AWeapon* UCombatComponent::FindWeaponForSlot(EWeaponSlot Slot) const
{
	int32 LongGunIndex = 0;
	for (AWeapon* Weapon : Inventory)
	{
		if (!IsValid(Weapon)) continue;

		if (Slot == EWeaponSlot::Pistol)
		{
			if (Weapon->WeaponKind == EWeaponKind::Pistol)
			{
				return Weapon;
			}
			continue;
		}

		if (Slot == EWeaponSlot::Melee)
		{
			if (Weapon->WeaponKind == EWeaponKind::Melee)
			{
				return Weapon;
			}
			continue;
		}

		if (Weapon->WeaponKind != EWeaponKind::LongGun) continue;

		const int32 WantedLongGun = (Slot == EWeaponSlot::Secondary) ? 1 : 0;
		if (LongGunIndex == WantedLongGun)
		{
			return Weapon;
		}
		++LongGunIndex;
	}

	return nullptr;
}

void UCombatComponent::Initiate_EquipSlot(EWeaponSlot Slot)
{
	AWeapon* Weapon = FindWeaponForSlot(Slot);
	if (!IsValid(Weapon) || Weapon == CurrentWeapon) return;

	if (IsValid(CurrentWeapon))
	{
		if (CurrentWeapon->WeaponStatus == EWeaponStatus::Cycling) return;
		if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;
		// 以前开镜时直接 return（按数字键没反应），现在改成切枪前自动关镜
	}

	const int32 WeaponIndex = Inventory.IndexOfByKey(Weapon);
	if (!Inventory.IsValidIndex(WeaponIndex)) return;

	StopAimForWeaponSwitch();   // 切枪前先关镜
	LocalWeaponIndex = WeaponIndex;
	Local_CycleWeapon(WeaponIndex, true);
	Server_EquipSlot(Slot);
}

void UCombatComponent::Server_EquipSlot_Implementation(EWeaponSlot Slot)
{
	AWeapon* Weapon = FindWeaponForSlot(Slot);
	if (!IsValid(Weapon)) return;

	const int32 WeaponIndex = Inventory.IndexOfByKey(Weapon);
	if (!Inventory.IsValidIndex(WeaponIndex)) return;

	// Server resolves the slot again, so a client cannot put a pistol in the main slot.
	Server_CycleWeapon_Implementation(WeaponIndex);
}

void UCombatComponent::Initiate_EquipMainWeapon()
{
	Initiate_EquipSlot(EWeaponSlot::Main);
}

void UCombatComponent::Initiate_EquipSeconedaryWeapon()
{
	Initiate_EquipSlot(EWeaponSlot::Secondary);
}

void UCombatComponent::Initiate_EquipMeleeWeapon()
{
	Initiate_EquipSlot(EWeaponSlot::Melee);
}

void UCombatComponent::Initiate_EquipPistolWeapon()
{
	Initiate_EquipSlot(EWeaponSlot::Pistol);
}

void UCombatComponent::Notify_ReloadWeapon()
{
	if (!IsValid(CurrentWeapon)) return;
	// Client RPCs only run when invoked with authority. A 1P notify on a remote client
	// must not call Client_ReloadWeapon; the server's 3P notify sends the result.
	if (GetOwnerRole() != ROLE_Authority) return;

	const int32 EmptySpace = CurrentWeapon->MagCapacity - CurrentWeapon->Ammo;
	const int32 AmountToRefill = FMath::Min(EmptySpace, CurrentReserveAmmo);
	if (AmountToRefill > 0)
	{
		CurrentWeapon->Ammo += AmountToRefill;
		CurrentReserveAmmo -= AmountToRefill;
		ReserveAmmo.Add(CurrentWeapon->WeaponType, CurrentReserveAmmo);
	}

	CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
	Client_ReloadWeapon(CurrentWeapon->Ammo, CurrentReserveAmmo);
}

void UCombatComponent::Server_Aim_Implementation(bool bPressed)
{
	Local_Aim(bPressed);
}



void UCombatComponent::Local_Aim(bool bPressed)
{
	if (bAiming == bPressed) return;   // 状态没变就不广播，避免 listen server 上重复触发
	bAiming = bPressed;
	OnAimStateChanged.Broadcast(bAiming);
}

void UCombatComponent::StopAimForWeaponSwitch()
{
	if (!bAiming) return;
	Local_Aim(false);    // 本地立刻关镜，手感不延迟
	Server_Aim(false);   // 通知服务器（Reliable RPC 按顺序到达）
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


// ================= 配件 =================

const TArray<AWeapon*>& UCombatComponent::GetInventory() const
{
	return Inventory;
}

void UCombatComponent::Initiate_EquipAttachment(UAttachmentData* Attachment)
{
	Initiate_EquipAttachmentOnWeapon(CurrentWeapon, Attachment);
}

void UCombatComponent::Initiate_RemoveAttachment(FGameplayTag SlotTag)
{
	Initiate_RemoveAttachmentOnWeapon(CurrentWeapon, SlotTag);
}

void UCombatComponent::Initiate_EquipAttachmentOnWeapon(AWeapon* Weapon, UAttachmentData* Attachment)
{
	if (!IsValid(Attachment)) return;
	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return;

	AWeapon* Target = IsValid(Weapon) ? Weapon : CurrentWeapon.Get();
	if (!IsValid(Target) || !Inventory.Contains(Target)) return;

	// 服务器（或单机）直接装；客户端发 RPC 让服务器装，结果再同步回来
	if (Owner->HasAuthority())
	{
		Server_EquipAttachment_Implementation(Target, Attachment);
	}
	else
	{
		Server_EquipAttachment(Target, Attachment);
	}
}

void UCombatComponent::Initiate_RemoveAttachmentOnWeapon(AWeapon* Weapon, FGameplayTag SlotTag)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return;

	AWeapon* Target = IsValid(Weapon) ? Weapon : CurrentWeapon.Get();
	if (!IsValid(Target) || !Inventory.Contains(Target)) return;

	if (Owner->HasAuthority())
	{
		Server_RemoveAttachment_Implementation(Target, SlotTag);
	}
	else
	{
		Server_RemoveAttachment(Target, SlotTag);
	}
}

void UCombatComponent::Initiate_ToggleFlashlight()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return;
	if (Owner->HasAuthority())
	{
		Server_ToggleFlashlight_Implementation();
	}
	else
	{
		Server_ToggleFlashlight();
	}
}

void UCombatComponent::Server_EquipAttachment_Implementation(AWeapon* Weapon, UAttachmentData* Attachment)
{
	if (IsValid(Weapon) && Inventory.Contains(Weapon))
	{
		Weapon->EquipAttachment(Attachment);
	}
}

void UCombatComponent::Server_RemoveAttachment_Implementation(AWeapon* Weapon, FGameplayTag SlotTag)
{
	if (IsValid(Weapon) && Inventory.Contains(Weapon))
	{
		Weapon->RemoveAttachment(SlotTag);
	}
}

void UCombatComponent::Server_ToggleFlashlight_Implementation()
{
	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->ToggleFlashlight();
	}
}

float UCombatComponent::GetCurrentAimFOV(float Fallback) const
{
	return IsValid(CurrentWeapon) ? CurrentWeapon->GetEffectiveAimFOV() : Fallback;
}
