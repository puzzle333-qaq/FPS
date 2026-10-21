#include "CombatComponent.h"
#include "Animation/FirstPersonAnimInstance.h"
#include "ShooterCharacter.h"
#include "ShooterGamePlayTags.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/Weapon.h"
#include "WeaponData.h"
#include "Interface/PlayerInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UnrealType.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "ShooterPlayerController.h"
#include "Loadout/LoadoutTypes.h"
#include "Weapon/AttachmentData.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ShooterType/ShooterType.h"
#include "Interface/RecoilModifierInterface.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Game/ShooterGameState.h"
#include "Game/SearchDestroyGameState.h"
#include "Game/ShooterPlayerState.h"
#include "Game/BombWeapon.h"
#include "Weapon/ThrownProjectile.h"
#include "Loadout/LoadoutTypes.h"

namespace
{
	bool MagHasCharacterReload(const UAttachmentData* Mag)
	{
		return IsValid(Mag) && (
			Mag->MagCharacterReloadMontage
			|| Mag->MagCharacterEmptyReloadMontage
			|| Mag->MagCharacterTacticalReloadMontage
			|| Mag->MagCharacterEmptyReloadMontage_Grab
			|| Mag->MagCharacterTacticalReloadMontage_Grab);
	}

	UAnimMontage* ResolveMagazineCharacterReload(const UAttachmentData* Mag, EReloadType Type, EGripHoldStyle Style)
	{
		if (!IsValid(Mag)) return nullptr;

		if (Style == EGripHoldStyle::Grab)
		{
			if (Type == EReloadType::Empty && Mag->MagCharacterEmptyReloadMontage_Grab)
			{
				return Mag->MagCharacterEmptyReloadMontage_Grab;
			}
			if (Type == EReloadType::Tactical && Mag->MagCharacterTacticalReloadMontage_Grab)
			{
				return Mag->MagCharacterTacticalReloadMontage_Grab;
			}
		}
		if (Type == EReloadType::Empty && Mag->MagCharacterEmptyReloadMontage)
		{
			return Mag->MagCharacterEmptyReloadMontage;
		}
		if (Type == EReloadType::Tactical && Mag->MagCharacterTacticalReloadMontage)
		{
			return Mag->MagCharacterTacticalReloadMontage;
		}
		return Mag->MagCharacterReloadMontage;
	}

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
		// 换弹中被切枪：停掉换弹动画，Notify 不会再触发，所以不给子弹
		if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading)
		{
			StopReloadMontages(CurrentWeapon);
		}
		CurrentWeapon->DetachFromPawn();
		CurrentWeapon->SetActorHiddenInGame(true);
		CurrentWeapon->WeaponStatus = EWeaponStatus::UnEquipped;
	}

	CurrentWeapon = Weapon;
	CurrentWeapon->SetActorHiddenInGame(false);
	CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
	CurrentWeapon->AttachToOwingPawn();
	StopActiveThrowMontages();
	bThrowInProgress = false;
	bThrowCommitted = false;
	bThrowPinPulling = false;
	bThrowReleaseRequested = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThrowReleaseTimer);
	}
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
	BurstShotsRemaining = 0;
	CurrentWeapon->SanitizeFireType();
	OnFireModeChanged.Broadcast(CurrentWeapon, CurrentWeapon->FireType);
	OnCurrentWeaponChanged.Broadcast(CurrentWeapon); // 通知 UI：换枪了
}


// 爆破默认手枪：BuyCatalog 里第一个 Price=0 的手枪（填 Glock18），否则 DefaultWeaponClass 里第一把手枪
static TSubclassOf<AWeapon> FPS_FindBombDefaultPistol(const UWorld* World, const TArray<TSubclassOf<AWeapon>>* Defaults)
{
	if (const ASearchDestroyGameState* SDGS = World ? World->GetGameState<ASearchDestroyGameState>() : nullptr)
	{
		for (const FBuyShopItem& Item : SDGS->GetBuyCatalog())
		{
			if (Item.Price == 0 && Item.WeaponClass && Item.WeaponClass->GetDefaultObject<AWeapon>()->WeaponKind == EWeaponKind::Pistol)
				return Item.WeaponClass;
		}
	}
	if (Defaults)
	{
		for (const TSubclassOf<AWeapon>& C : *Defaults)
		{
			if (C && C->GetDefaultObject<AWeapon>()->WeaponKind == EWeaponKind::Pistol) return C;
		}
	}
	return nullptr;
}

void UCombatComponent::SpawnInventory()
{
	if (GetOwner()->GetLocalRole() < ROLE_Authority)return;

	// 局外配装：每把预设枪替换默认武器里同 WeaponKind 的第一把；没有同类就追加
	TArray<TSubclassOf<AWeapon>> SpawnClasses = DefaultWeaponClass;
	const AShooterGameState* ModeGS = GetWorld() ? GetWorld()->GetGameState<AShooterGameState>() : nullptr;
	if (ModeGS && ModeGS->GetMatchMode() == EShooterGameMode::SearchDestroy)
	{
		// 爆破：不用角色默认武器，只给默认手枪（Glock18）+ 购买的枪
		SpawnClasses.Reset();
		if (TSubclassOf<AWeapon> Pistol = FPS_FindBombDefaultPistol(GetWorld(), &DefaultWeaponClass)) SpawnClasses.Add(Pistol);
		else UE_LOG(LogTemp, Warning, TEXT("[Shop] No default pistol: add Glock18 to BuyCatalog with Price=0"));
		// 刀永远默认带
		for (const TSubclassOf<AWeapon>& C : DefaultWeaponClass)
		{
			if (C && C->GetDefaultObject<AWeapon>()->WeaponKind == EWeaponKind::Melee) { SpawnClasses.Add(C); break; }
		}
	}
	TArray<const FWeaponPreset*> SpawnPresets;
	SpawnPresets.Init(nullptr, SpawnClasses.Num());
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const AShooterPlayerController* LoadoutPC = IsValid(OwnerPawn) ? Cast<AShooterPlayerController>(OwnerPawn->GetController()) : nullptr;
	if (const FLoadoutPreset* Loadout = IsValid(LoadoutPC) ? LoadoutPC->GetSubmittedLoadout() : nullptr)
	{
		for (const FWeaponPreset& Preset : Loadout->Weapons)
		{
			if (!Preset.WeaponClass) continue;
			const EWeaponKind Kind = Preset.WeaponClass->GetDefaultObject<AWeapon>()->WeaponKind;
			int32 Slot = INDEX_NONE;
			for (int32 i = 0; i < SpawnClasses.Num(); ++i)
			{
				if (!SpawnPresets[i] && SpawnClasses[i] && SpawnClasses[i]->GetDefaultObject<AWeapon>()->WeaponKind == Kind)
				{
					Slot = i;
					break;
				}
			}
			if (Slot == INDEX_NONE)
			{
				Slot = SpawnClasses.Add(Preset.WeaponClass);
				SpawnPresets.Add(nullptr);
			}
			SpawnClasses[Slot] = Preset.WeaponClass;
			SpawnPresets[Slot] = &Preset;
		}
	}

	const AShooterGameState* GS = GetWorld() ? GetWorld()->GetGameState<AShooterGameState>() : nullptr;
	const bool bExtraction = !GS || !GS->HasMatchHeader() || GS->GetMatchMode() == EShooterGameMode::Extraction;
	const int32 MaxLong = bExtraction ? 2 : 1;
	const bool bAllowThrowable = !GS || !GS->HasMatchHeader() || GS->GetMatchMode() != EShooterGameMode::Extraction;
	bool bLoadoutHasLong = false;
	for (const FWeaponPreset* Preset : SpawnPresets)
	{
		if (Preset && Preset->WeaponClass && Preset->WeaponClass->GetDefaultObject<AWeapon>()->WeaponKind == EWeaponKind::LongGun)
		{
			bLoadoutHasLong = true;
			break;
		}
	}
	int32 LongGunCount = 0;
	int32 PistolCount = 0;
	int32 MeleeCount = 0;
	int32 ThrowableCount = 0;

	for (int32 SpawnIndex = 0; SpawnIndex < SpawnClasses.Num(); ++SpawnIndex)
	{
		const AWeapon* CDO = SpawnClasses[SpawnIndex] ? SpawnClasses[SpawnIndex]->GetDefaultObject<AWeapon>() : nullptr;
		if (!IsValid(CDO) || CDO->WeaponKind == EWeaponKind::Bomb) continue;
		if (CDO->WeaponKind == EWeaponKind::LongGun)
		{
			if (LongGunCount >= MaxLong) continue;
			if (bLoadoutHasLong && !SpawnPresets[SpawnIndex]) continue;
		}
		if (CDO->WeaponKind == EWeaponKind::Pistol && PistolCount >= 1) continue;
		if (CDO->WeaponKind == EWeaponKind::Melee && MeleeCount >= 1) continue;
		if (CDO->WeaponKind == EWeaponKind::Throwable && (!bAllowThrowable || ThrowableCount >= 1)) continue;

		AWeapon* Weapon = SpawnWeapon(SpawnClasses[SpawnIndex]);
		if (!IsValid(Weapon)) continue;
		if (const FWeaponPreset* Preset = SpawnPresets[SpawnIndex])
		{
			// 预设里没有的槽先卸掉（必需槽 RemoveAttachment 会拒绝），再装预设里的；EquipAttachment 自己做白名单校验
			for (const FWeaponAttachmentSlot& AttachSlot : Weapon->GetAttachmentSlots())
			{
				const bool bInPreset = Preset->Attachments.ContainsByPredicate([&AttachSlot](const UAttachmentData* A)
				{
					return IsValid(A) && A->SlotTag == AttachSlot.SlotTag;
				});
				if (!bInPreset) Weapon->RemoveAttachment(AttachSlot.SlotTag);
			}
			for (UAttachmentData* Data : Preset->Attachments)
			{
				if (IsValid(Data)) Weapon->EquipAttachment(Data);
			}
		}

		switch (Weapon->WeaponKind)
		{
		case EWeaponKind::LongGun: ++LongGunCount; break;
		case EWeaponKind::Pistol: ++PistolCount; break;
		case EWeaponKind::Melee: ++MeleeCount; break;
		case EWeaponKind::Throwable: ++ThrowableCount; break;
		default: break;
		}

		Inventory.AddUnique(Weapon);
		ReserveAmmo.Add(Weapon->WeaponType, Weapon->StartingCarriedAmmo);
	}

	LongGunCount = 0;
	PistolCount = 0;
	MeleeCount = 0;
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
		AWeapon* ToEquip = Inventory[0];
		if (ModeGS && ModeGS->GetMatchMode() == EShooterGameMode::SearchDestroy)
		{
			// 爆破：买了长枪就直接拿长枪
			for (AWeapon* W : Inventory) { if (IsValid(W) && W->WeaponKind == EWeaponKind::LongGun) { ToEquip = W; break; } }
		}
		Equip(ToEquip);
		InitializeWeaponWidget();
		IPlayerInterface::Execute_WeaponReplicated(GetOwner());
	}
}

void UCombatComponent::RebuildInventory()
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority()) return;
	if (IsValid(CurrentWeapon) && CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading)
	{
		StopReloadMontages(CurrentWeapon);
	}
	DestroyInventory();
	Inventory.Reset();
	CurrentWeapon = nullptr;
	ReserveAmmo.Reset();
	SpawnInventory();

	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (const AShooterPlayerState* PS = OwnerPawn->GetPlayerState<AShooterPlayerState>())
		{
			if (PS->IsCarryingBomb())
			{
				GrantBombWeapon();
			}
		}
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
	BurstShotsRemaining = 0;
	CurrentWeapon->SanitizeFireType();
	OnFireModeChanged.Broadcast(CurrentWeapon, CurrentWeapon->FireType);
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
	if (CurrentWeapon->WeaponKind == EWeaponKind::Melee
		|| CurrentWeapon->WeaponKind == EWeaponKind::Throwable
		|| CurrentWeapon->RecoilPattern.Num() == 0)
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

	// 枪身视觉后坐只推模型和手臂。开镜时装了瞄具时准心在模型上，不能再踢，否则线会跟着后坐往上爬。
	// 镜头后坐仍然在，子弹还是跟着镜头中心。
	if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(OwningPawn))
	{
		const bool bSightStaysPut = bAiming && IsValid(CurrentWeapon->GetOpticAttachment());
		const float AimVisual = bSightStaysPut ? 0.f : (bAiming ? FMath::Max(0.f, CurrentWeapon->VisualRecoilAimMultiplier) : 1.f);
		const FVector& L = CurrentWeapon->VisualRecoilLocation;
		const FRotator& R = CurrentWeapon->VisualRecoilRotation;
		const FVector KickLocation(
			-L.X * Scale.Pitch,
			FMath::FRandRange(-L.Y, L.Y) * Scale.Yaw,
			L.Z * Scale.Pitch);
		const FRotator KickRotation(
			Step.Pitch * R.Pitch * Scale.Pitch,
			Step.Yaw * R.Yaw * Scale.Yaw,
			FMath::FRandRange(-R.Roll, R.Roll));
		Shooter->AddVisualRecoil(KickLocation * AimVisual, KickRotation * AimVisual);
	}
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

void UCombatComponent::Local_CycleWeapon(int32 WeaponIndex, bool bFirstPerson)
{
	if (!Inventory.IsValidIndex(WeaponIndex)) return;

	AWeapon* NextWeapon = Inventory[WeaponIndex];
	if (!IsValid(NextWeapon)) return;

	// 打断上一次拔枪：先停掉旧 montage，再完整走一遍新枪的拔枪流程
	StopActiveEquipMontages();

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

		// Grip hold style: Cradle vs Grab — same source as ResolveReloadMontage(GetGripHoldStyle()).
		// Push IdleAnim_Grab into CurrenrAnim.IdleAnim. Do NOT InitAnim here: it destroys
		// DefaultSlot Grab-idle dynamic montages started by FirstPersonAnimInstance.
		const EGripHoldStyle GripStyle = CurrentWeapon->GetGripHoldStyle();
		if (GripStyle == EGripHoldStyle::Grab)
		{
			if (IsValid(AnimCopy.IdleAnim_Grab) && AnimCopy.IdleAnim_Grab != AnimCopy.LeftHandHoldPose_Grab)
			{
				AnimCopy.IdleAnim = AnimCopy.IdleAnim_Grab;
			}
			if (IsValid(AnimCopy.AimIdleAnim_Grab))
			{
				AnimCopy.AimIdleAnim = AnimCopy.AimIdleAnim_Grab;
			}
		}

		const FName PropertyName = bFirstPerson ? TEXT("CurrenrAnim") : TEXT("CurrentAnim");
		FStructProperty* AnimProperty = FindFProperty<FStructProperty>(AnimInstance->GetClass(), PropertyName);
		if (!AnimProperty || AnimProperty->Struct != FPlayerAnim::StaticStruct()) return;

		AnimProperty->CopyCompleteValue(AnimProperty->ContainerPtrToValuePtr<void>(AnimInstance), &AnimCopy);

		if (UFirstPersonAnimInstance* FPAnim = Cast<UFirstPersonAnimInstance>(AnimInstance))
		{
			FPAnim->GripHoldStyle = GripStyle;
			if (GripStyle == EGripHoldStyle::Grab)
			{
				FPAnim->LeftHandHoldPose = AnimCopy.LeftHandHoldPose_Grab.Get();
				FPAnim->ActiveIdleAnim = AnimCopy.IdleAnim.Get();
			}
			else
			{
				UAnimSequence* CradlePose = AnimCopy.LeftHandHoldPose_Cradle.Get();
				FPAnim->LeftHandHoldPose = (CradlePose && CradlePose != AnimCopy.IdleAnim.Get()) ? CradlePose : nullptr;
				FPAnim->ActiveIdleAnim = AnimCopy.IdleAnim.Get();
			}
			FPAnim->bUseLeftHandHoldPose = IsValid(FPAnim->LeftHandHoldPose);

			if (bFirstPerson)
			{
				UE_LOG(LogTemp, Log, TEXT("[GripHold] Refresh CurrenrAnim Idle=%s style=%d hasGrabIdle=%d"),
					*GetNameSafe(AnimCopy.IdleAnim.Get()),
					(int32)GripStyle,
					IsValid(AnimCopy.IdleAnim_Grab) ? 1 : 0);
			}
		}
	};

	ApplyToMesh(IPlayerInterface::Execute_GetMesh1P(Owner), true);
	ApplyToMesh(IPlayerInterface::Execute_GetMesh3P(Owner), false);
}

void UCombatComponent::PlayEquipMontage(AWeapon* Weapon, bool bFirstPerson)
{
	if (!IsValid(Weapon) || !IsValid(WeaponData)) return;

	// 投掷物的 EquipMontage 是拉保险，切到手里只进 idle，按下投掷后再播。
	if (Weapon->WeaponKind == EWeaponKind::Throwable)
	{
		Weapon->WeaponStatus = EWeaponStatus::Idle;
		return;
	}

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UPlayerInterface>()) return;

	const TMap<FGameplayTag, FMontageData>& ViewMontages = bFirstPerson
		? WeaponData->FirstPersonMontages
		: WeaponData->ThirdPersonMontages;

	const FMontageData* MontageData = ViewMontages.Find(Weapon->WeaponType);
	UAnimMontage* EquipMontage = MontageData ? MontageData->ResolveEquipMontage() : nullptr;
	if (!IsValid(EquipMontage))
	{
		if (const FMontageData* SharedData = WeaponData->WeaponMontages.Find(Weapon->WeaponType))
		{
			EquipMontage = SharedData->ResolveEquipMontage();
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

	// X24 枪身没有对得上的蒙太奇。拔枪时直接播套筒序列。
	if (Weapon->WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_X24))
	{
		if (USkeletalMeshComponent* GunMesh = bFirstPerson ? Weapon->GetMesh1P() : Weapon->GetMesh3P())
		{
			if (UAnimSequence* EquipSequence = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/FPS_Controller/Animations/WeaponsAnims/Pistol_X24/A_X24_Body_First_Equip.A_X24_Body_First_Equip")))
			{
				GunMesh->PlayAnimation(EquipSequence, false);
			}
		}
	}

	// 第一人称拔枪时，枪身蒙太奇跟手臂一起播。手臂已经有 Equip 时，WeaponMontages 不会当替补，所以这里单独播。
	if (bFirstPerson)
	{
		if (const FMontageData* WeaponMontageData = WeaponData->WeaponMontages.Find(Weapon->WeaponType))
		{
			if (UAnimMontage* WeaponEquip = WeaponMontageData->ResolveEquipMontage())
			{
				if (USkeletalMeshComponent* WeaponMesh = Weapon->GetMesh1P())
				{
					if (UAnimInstance* WeaponAnim = WeaponMesh->GetAnimInstance())
					{
						WeaponAnim->Montage_Play(WeaponEquip);
					}
				}
			}
		}
	}

	if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(EquipMontage))
	{
		MontageInstance->bEnableAutoBlendOut = true;
	}

	const int32 ViewIdx = bFirstPerson ? 0 : 1;
	ActiveEquipAnim[ViewIdx] = AnimInstance;
	ActiveEquipMontage[ViewIdx] = EquipMontage;

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UCombatComponent::OnEquipMontageEnded, TWeakObjectPtr<AWeapon>(Weapon));
	AnimInstance->Montage_SetEndDelegate(EndDelegate, EquipMontage);
	Weapon->WeaponStatus = EWeaponStatus::Cycling;
}

void UCombatComponent::OnEquipMontageEnded(UAnimMontage* Montage, bool bInterrupted, TWeakObjectPtr<AWeapon> EquippingWeapon)
{
	// 只解锁这次拔枪对应的那把枪；被打断的旧拔枪结束回调不能把新枪提前设成 Idle
	AWeapon* Weapon = EquippingWeapon.Get();
	if (IsValid(Weapon) && Weapon == CurrentWeapon && Weapon->WeaponStatus == EWeaponStatus::Cycling)
	{
		Weapon->WeaponStatus = EWeaponStatus::Idle;
	}
}

void UCombatComponent::StopActiveEquipMontages()
{
	for (int32 i = 0; i < 2; ++i)
	{
		UAnimInstance* Anim = ActiveEquipAnim[i].Get();
		UAnimMontage* Montage = ActiveEquipMontage[i].Get();
		if (IsValid(Anim) && IsValid(Montage) && Anim->Montage_IsPlaying(Montage))
		{
			// 先解绑结束回调，免得它晚到把新枪状态改掉
			if (FAnimMontageInstance* Inst = Anim->GetActiveInstanceForMontage(Montage))
			{
				Inst->OnMontageEnded.Unbind();
			}
			Anim->Montage_Stop(0.1f, Montage);
		}
		ActiveEquipAnim[i] = nullptr;
		ActiveEquipMontage[i] = nullptr;
	}
}

void UCombatComponent::StopReloadMontages(AWeapon* Weapon)
{
	if (!IsValid(Weapon) || !IsValid(WeaponData)) return;
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UPlayerInterface>()) return;

	TArray<UAnimMontage*> Montages;
	auto Collect = [&Montages, Weapon](const TMap<FGameplayTag, FMontageData>& Map)
	{
		if (const FMontageData* Data = Map.Find(Weapon->WeaponType))
		{
			for (EReloadType Type : { EReloadType::First, EReloadType::Empty, EReloadType::Tactical })
			{
				for (EGripHoldStyle Style : { EGripHoldStyle::Cradle, EGripHoldStyle::Grab })
				{
					if (UAnimMontage* M = Data->ResolveReloadMontage(Type, Style)) Montages.AddUnique(M);
				}
			}
		}
	};
	Collect(WeaponData->FirstPersonMontages);
	Collect(WeaponData->ThirdPersonMontages);
	Collect(WeaponData->WeaponMontages);

	if (const UAttachmentData* Mag = Weapon->GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Magazine))
	{
		for (EReloadType Type : { EReloadType::First, EReloadType::Empty, EReloadType::Tactical })
		{
			for (EGripHoldStyle Style : { EGripHoldStyle::Cradle, EGripHoldStyle::Grab })
			{
				if (UAnimMontage* M = ResolveMagazineCharacterReload(Mag, Type, Style)) Montages.AddUnique(M);
			}
		}
	}

	TArray<UAnimInstance*> Anims;
	if (USkeletalMeshComponent* M1 = IPlayerInterface::Execute_GetMesh1P(Owner)) Anims.Add(M1->GetAnimInstance());
	if (USkeletalMeshComponent* M3 = IPlayerInterface::Execute_GetMesh3P(Owner)) Anims.Add(M3->GetAnimInstance());
	if (USkeletalMeshComponent* MW = Weapon->GetMesh1P()) Anims.Add(MW->GetAnimInstance());

	for (UAnimInstance* Anim : Anims)
	{
		if (!IsValid(Anim)) continue;
		for (UAnimMontage* M : Montages)
		{
			if (Anim->Montage_IsPlaying(M))
			{
				if (FAnimMontageInstance* Inst = Anim->GetActiveInstanceForMontage(M))
				{
					Inst->OnMontageEnded.Unbind();
				}
				Anim->Montage_Stop(0.1f, M);
			}
		}
	}
	Weapon->WeaponStatus = EWeaponStatus::Idle;
	Weapon->AttachToOwingPawn();
}

void UCombatComponent::Server_CycleWeapon_Implementation(int32 WeaponIndex)
{
	if (!Inventory.IsValidIndex(WeaponIndex)) return;

	// 服务器这边也关镜，bAiming 会同步给其他玩家
	Local_Aim(false);

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	const bool bListenServerOwner = GetNetMode() == NM_ListenServer && IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	LocalWeaponIndex = WeaponIndex;

	// Listen-server host already equipped and played the 1P montage in Initiate_CycleWeapon.
	if (!bListenServerOwner)
	{
		AWeapon* NextWeapon = Inventory[WeaponIndex];
		// 同一把枪：忽略（不重播拔枪）
		if (!IsValid(NextWeapon) || NextWeapon == CurrentWeapon) return;
		Equip(NextWeapon);
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
	CancelInspect();
	if (const UWorld* World = GetWorld())
	{
		if (const AShooterGameState* GS = World->GetGameState<AShooterGameState>())
		{
			if (GS->IsActionLocked()) return;
		}
	}
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(OwnerPawn->GetController()))
		{
			if (PC->IsSprinting())
			{
				PC->StopSprinting();
				return;
			}
		}
	}

	if (!IsValid(CurrentWeapon))return;
	if (CurrentWeapon->WeaponKind == EWeaponKind::Bomb) return;
	if (CurrentWeapon->WeaponKind == EWeaponKind::Throwable)
	{
		Initiate_Throw(true);
		return;
	}
	bTriggerPressed = true;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;
	// The burst continues from FireTimerFinished while this timer is still the active callback.
	// Gating Local_FireWeapon on IsTimerActive stops that next shot.
	if (GetWorld()->GetTimerManager().IsTimerActive(FireTimer)) return;

	if (CurrentWeapon->FireType == EFireType::Burst)
	{
		if (BurstShotsRemaining > 0) return;
		BurstShotsRemaining = FMath::Max(1, CurrentWeapon->BurstCount);
	}

	if (!CurrentWeapon->UsesAmmo() || CurrentWeapon->Ammo > 0)
	{
		Local_FireWeapon();
	}
	else
	{
		BurstShotsRemaining = 0;
	}
}

void UCombatComponent::Initiate_FireWeapon_Released()
{
	bTriggerPressed = false;
	if (IsValid(CurrentWeapon) && CurrentWeapon->WeaponKind == EWeaponKind::Throwable)
	{
		ReleaseThrow();
	}
}

void UCombatComponent::Local_FireWeapon()
{
	if (!IsValid(WeaponData) || !IsValid(CurrentWeapon))
	{
		return;
	}

	if (CurrentWeapon->WeaponKind == EWeaponKind::Throwable) return;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;

	if (const FMontageData* MontageData = WeaponData->FirstPersonMontages.Find(CurrentWeapon->WeaponType))
	{
		UAnimMontage* Montage1P = MontageData->ResolveFireMontage();
		USkeletalMeshComponent* Mesh1P = IPlayerInterface::Execute_GetMesh1P(GetOwner());
		UAnimInstance* AnimInstance1P = IsValid(Mesh1P) ? Mesh1P->GetAnimInstance() : nullptr;
		if (IsValid(Montage1P) && IsValid(AnimInstance1P))
		{
			AnimInstance1P->Montage_Play(Montage1P);
		}
	}

	// 枪身开火（拉栓/抛壳）只播在自己的 Mesh1P。蒙太奇槽必须和枪的 AnimBP 一致。
	if (CurrentWeapon->WeaponKind != EWeaponKind::Melee)
	{
		if (const FMontageData* WeaponMontageData = WeaponData->WeaponMontages.Find(CurrentWeapon->WeaponType))
		{
			if (UAnimMontage* WeaponFire = WeaponMontageData->ResolveFireMontage())
			{
				if (USkeletalMeshComponent* WeaponMesh = CurrentWeapon->GetMesh1P())
				{
					if (UAnimInstance* WeaponAnim = WeaponMesh->GetAnimInstance())
					{
						WeaponAnim->Montage_Play(WeaponFire);
					}
				}
			}
		}
	}

	// 近战：第三人称全身攻击（蒙太奇需挂 FullBody/DefaultSlot，勿只用 UpperBody）
	if (CurrentWeapon->WeaponKind == EWeaponKind::Melee)
	{
		if (const FMontageData* Montage3PData = WeaponData->ThirdPersonMontages.Find(CurrentWeapon->WeaponType))
		{
			UAnimMontage* Montage3P = Montage3PData->ResolveFireMontage();
			USkeletalMeshComponent* Mesh3P = IPlayerInterface::Execute_GetMesh3P(GetOwner());
			UAnimInstance* AnimInstance3P = IsValid(Mesh3P) ? Mesh3P->GetAnimInstance() : nullptr;
			if (IsValid(Montage3P) && IsValid(AnimInstance3P))
			{
				AnimInstance3P->Montage_Play(Montage3P);
			}
		}
	}

	FHitResult Hit;
	if (CurrentWeapon->WeaponKind == EWeaponKind::Melee)
	{
		CurrentWeapon->WeaponMeleeHit(Hit);
	}
	else
	{
		CurrentWeapon->WeaponTrace(Hit, TraceLength);
	}

	EPhysicalSurface ImpactSurfaceType = Hit.PhysMaterial.IsValid(false) ? Hit.PhysMaterial->SurfaceType.GetValue() : SurfaceType2;
	CurrentWeapon->Local_Fire(Hit.ImpactPoint, Hit.ImpactNormal, ImpactSurfaceType, true);
	OnWeaponFired.Broadcast();
	ApplyFireRecoil();
	NotifyAmmoChanged();

	if (CurrentWeapon->FireType == EFireType::Burst)
	{
		BurstShotsRemaining = FMath::Max(0, BurstShotsRemaining - 1);
	}

	const float Interval = CurrentWeapon->FireType == EFireType::Burst
		? CurrentWeapon->GetBurstShotInterval()
		: CurrentWeapon->FireTime;
	GetWorld()->GetTimerManager().SetTimer(FireTimer, this, &ThisClass::FireTimerFinished, Interval);

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
	// magazine immediately and never plays the montage. Throwables have no reload.
	if (CurrentWeapon->WeaponKind != EWeaponKind::Throwable
		&& CurrentWeapon->UsesAmmo() && CurrentWeapon->Ammo == 0)
	{
		BurstShotsRemaining = 0;
		if (CurrentReserveAmmo > 0 && IsValid(OwningPawn) && OwningPawn->IsLocallyControlled())
		{
			Initiate_ReloadWeapon();
		}
		return;
	}

	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;

	const bool bBurstContinue = CurrentWeapon->FireType == EFireType::Burst && BurstShotsRemaining > 0;
	const bool bAutoContinue = CurrentWeapon->FireType == EFireType::Automatic && bTriggerPressed;
	if ((bBurstContinue || bAutoContinue) && (!CurrentWeapon->UsesAmmo() || CurrentWeapon->Ammo > 0))
	{
		Local_FireWeapon();
		return;
	}
	BurstShotsRemaining = 0;
}


void UCombatComponent::Server_FireWeapon_Implementation(const FHitResult& Hit)
{
	if (!IsValid(CurrentWeapon)) return;
	if (CurrentWeapon->WeaponKind == EWeaponKind::Bomb) return;
	if (CurrentWeapon->WeaponKind == EWeaponKind::Throwable) return;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;
	if (const AShooterGameState* GS = GetWorld() ? GetWorld()->GetGameState<AShooterGameState>() : nullptr)
	{
		if (GS->IsActionLocked()) return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float ShotInterval = CurrentWeapon->FireType == EFireType::Burst
		? CurrentWeapon->GetBurstShotInterval()
		: CurrentWeapon->FireTime;
	const float MinInterval = FMath::Max(0.f, ShotInterval - 0.02f);
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
	// 本机操控的角色已在 Local_Fire 里扣过弹（房主 / 单机都一样），服务器不再扣
	const bool bLocallyPredicted = IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	if (!bLocallyPredicted)
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

	EPhysicalSurface ImpactSurfaceType = Hit.PhysMaterial.IsValid(false) ? Hit.PhysMaterial->SurfaceType.GetValue() : SurfaceType2;
	CurrentWeapon->Local_Fire(Hit.ImpactPoint, Hit.ImpactNormal, ImpactSurfaceType, false);

	if (const FMontageData* MontageData = WeaponData->ThirdPersonMontages.Find(CurrentWeapon->WeaponType))
	{
		UAnimMontage* Montage3P = MontageData->ResolveFireMontage();
		USkeletalMeshComponent* Mesh3P = IPlayerInterface::Execute_GetMesh3P(OwningPawn);
		UAnimInstance* AnimInstance3P = IsValid(Mesh3P) ? Mesh3P->GetAnimInstance() : nullptr;
		if (IsValid(Montage3P) && IsValid(AnimInstance3P))
		{
			AnimInstance3P->Montage_Play(Montage3P);
		}
	}
}


bool UCombatComponent::IsReloading() const
{
	return IsValid(CurrentWeapon) && CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading;
}

void UCombatComponent::Initiate_ReloadWeapon()
{
	CancelInspect();
	BurstShotsRemaining = 0;
	if (!IsValid(CurrentWeapon)) return;
	if (!CurrentWeapon->UsesAmmo()) return;
	if (CurrentWeapon->WeaponKind == EWeaponKind::Throwable) return;
	if (bThrowInProgress) return;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Cycling) return;
	if (CurrentWeapon->Ammo >= CurrentWeapon->MagCapacity) return;
	if (CurrentReserveAmmo <= 0) return;

	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(OwnerPawn->GetController()))
		{
			PC->StopSprinting();
		}
	}
	// 先定好这次是哪种换弹，手臂、枪、第三人称都用同一个种类
	const EReloadType ReloadType = ChooseReloadType();

	UAnimMontage* ReloadMontage = nullptr;
	UAnimInstance* AnimInstance = nullptr;
	if (!GetReloadMontage(true, ReloadType, ReloadMontage, AnimInstance)) return;

	StopAimForWeaponSwitch();
	CurrentWeapon->WeaponStatus = EWeaponStatus::Reloading;
	CurrentWeapon->bHasReloadedOnce = true;
	ResetRecoilPattern();
	Server_ReloadWeapon(ReloadType);
	if (!PlayReloadMontage(true, ReloadType))
	{
		CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
		Server_EndReload(false);
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

EReloadType UCombatComponent::ChooseReloadType() const
{
	if (!IsValid(CurrentWeapon)) return EReloadType::Tactical;
	if (!CurrentWeapon->bHasReloadedOnce) return EReloadType::First;
	return CurrentWeapon->Ammo <= 0 ? EReloadType::Empty : EReloadType::Tactical;
}

bool UCombatComponent::GetReloadMontage(bool bFirstPerson, EReloadType ReloadType, UAnimMontage*& OutMontage, UAnimInstance*& OutAnim) const
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

	const UAttachmentData* Mag = CurrentWeapon->GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Magazine);
	const EGripHoldStyle GripStyle = CurrentWeapon->GetGripHoldStyle();

	// 第一次换弹没填 FirstReloadMontage 时，按弹匣空不空退回空匣/战术。
	// 弹匣自己填了手臂换弹时也这样退：拓展弹匣只有 empty/tactical，没有 First。
	EReloadType Resolved = ReloadType;
	if (Resolved == EReloadType::First)
	{
		const bool bHasWeaponFirst = MontageData && IsValid(MontageData->FirstReloadMontage);
		if (MagHasCharacterReload(Mag) || !bHasWeaponFirst)
		{
			Resolved = CurrentWeapon->Ammo <= 0 ? EReloadType::Empty : EReloadType::Tactical;
		}
	}

	UAnimMontage* Chosen = nullptr;
	if (bFirstPerson)
	{
		Chosen = ResolveMagazineCharacterReload(Mag, Resolved, GripStyle);
	}
	if (!IsValid(Chosen) && MontageData)
	{
		Chosen = MontageData->ResolveReloadMontage(Resolved, GripStyle);
	}
	if (!IsValid(Chosen)) return false;

	USkeletalMeshComponent* Mesh = bFirstPerson
		? IPlayerInterface::Execute_GetMesh1P(Owner)
		: IPlayerInterface::Execute_GetMesh3P(Owner);
	UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
	if (!IsValid(AnimInstance)) return false;

	OutMontage = Chosen;
	OutAnim = AnimInstance;
	return true;
}

bool UCombatComponent::PlayReloadMontage(bool bFirstPerson, EReloadType ReloadType)
{
	UAnimMontage* ReloadMontage = nullptr;
	UAnimInstance* AnimInstance = nullptr;
	if (!GetReloadMontage(bFirstPerson, ReloadType, ReloadMontage, AnimInstance)) return false;

	const float PlayedLength = AnimInstance->Montage_Play(ReloadMontage);
	if (PlayedLength <= 0.f) return false;

	// The gun reload moves the magazine on the weapon mesh. Play it only on the
	// owning first-person mesh; the third-person gun leaves the magazine floating.
	if (bFirstPerson)
	{
		if (const FMontageData* WeaponMontageData = WeaponData->WeaponMontages.Find(CurrentWeapon->WeaponType))
		{
			// 枪身也按同一个种类挑，保证手和弹匣是成对的那两条动画
			EReloadType WeaponReloadType = ReloadType;
			if (WeaponReloadType == EReloadType::First && !IsValid(WeaponMontageData->FirstReloadMontage))
			{
				WeaponReloadType = CurrentWeapon->Ammo <= 0 ? EReloadType::Empty : EReloadType::Tactical;
			}
			const EGripHoldStyle WeaponGripStyle = IsValid(CurrentWeapon) ? CurrentWeapon->GetGripHoldStyle() : EGripHoldStyle::Cradle;
			UAnimMontage* WeaponReloadMontage = WeaponMontageData->ResolveReloadMontage(WeaponReloadType, WeaponGripStyle);
			USkeletalMeshComponent* WeaponMesh = CurrentWeapon->GetMesh1P();
			UAnimInstance* WeaponAnim = IsValid(WeaponMesh) ? WeaponMesh->GetAnimInstance() : nullptr;
			if (IsValid(WeaponReloadMontage) && IsValid(WeaponAnim))
			{
				WeaponAnim->Montage_Play(WeaponReloadMontage);
			}
		}
		EReloadType MagReloadType = ReloadType;
		if (MagReloadType == EReloadType::First)
		{
			MagReloadType = CurrentWeapon->Ammo <= 0 ? EReloadType::Empty : EReloadType::Tactical;
		}
		CurrentWeapon->PlayMagazineReload(MagReloadType);
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

	// 手臂上的 Reload 通知经常打不中（时间在片段外面，或枪上的通知类不在工程里）。
	// 正常播完仍结算一次。通知已经加过子弹时状态已是 Idle，进不了这里，不会加两次。
	// 被打断不给子弹。
	if (!bInterrupted && GetOwnerRole() == ROLE_Authority)
	{
		Notify_ReloadWeapon();
		return;
	}

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (IsValid(OwningPawn) && OwningPawn->IsLocallyControlled() && GetOwnerRole() != ROLE_Authority)
	{
		// 客户端 1P 播完：通知在客户端没权限、3P 也不在服务器播，所以让服务器补子弹，
		// 本地保持 Reloading，等 Client_ReloadWeapon 回传后切 Idle。被打断则只清状态。
		if (!bInterrupted)
		{
			Server_EndReload(true);
			return;
		}
		CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
		Server_EndReload(false);
		return;
	}

	CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
}


void UCombatComponent::Client_ReloadWeapon_Implementation(AWeapon* ReloadedWeapon, int32 NewWeaponAmmo, int32 NewCarryAmmo)
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(CurrentWeapon) || !IsValid(OwningPawn) || !OwningPawn->IsLocallyControlled()) return;
	// 客户端已经切到别的枪：只把子弹记到那把换弹的枪上，不碰当前枪
	if (IsValid(ReloadedWeapon) && ReloadedWeapon != CurrentWeapon)
	{
		ReloadedWeapon->Ammo = NewWeaponAmmo;
		ReserveAmmo.Add(ReloadedWeapon->WeaponType, NewCarryAmmo);
		return;
	}

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

void UCombatComponent::Server_ReloadWeapon_Implementation(EReloadType ReloadType)
{
	if (!IsValid(CurrentWeapon)) return;

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	// 本机操控的角色（房主 / 单机）在 Initiate_ReloadWeapon 里已经做过检查并设成 Reloading
	const bool bLocallyPredicted = IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	if (!bLocallyPredicted)
	{
		if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;
		if (CurrentWeapon->WeaponStatus == EWeaponStatus::Cycling) return;
		if (CurrentWeapon->Ammo >= CurrentWeapon->MagCapacity) return;
		if (CurrentReserveAmmo <= 0) return;
		CurrentWeapon->WeaponStatus = EWeaponStatus::Reloading;
		CurrentWeapon->bHasReloadedOnce = true;
	}

	Multicast_ReloadWeapon(ReloadType);
}

void UCombatComponent::Server_ReloadNotify_Implementation()
{
	Notify_ReloadWeapon();
}

void UCombatComponent::Server_EndReload_Implementation(bool bCompleted)
{
	if (!IsValid(CurrentWeapon)) return;
	if (CurrentWeapon->WeaponStatus != EWeaponStatus::Reloading) return;
	// 客户端换弹完整播完、通知没加过子弹：服务器补一次（会切 Idle 并回传客户端）
	if (bCompleted)
	{
		Notify_ReloadWeapon();
		return;
	}
	CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
}

void UCombatComponent::Multicast_ReloadWeapon_Implementation(EReloadType ReloadType)
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn) || OwningPawn->IsLocallyControlled())
	{
		// Owner already plays the 1P montage. Playing it again from here restarts the clip.
		return;
	}

	PlayReloadMontage(false, ReloadType);
}

void UCombatComponent::Initiate_CycleZoom()
{
	if (!bAiming || !IsValid(CurrentWeapon)) return;
	const int32 Count = CurrentWeapon->GetZoomLevelCount();
	if (Count < 2) return;

	// 倍率只影响自己的镜头 FOV（UpdateAimFieldOfView 会平滑过去），命中仍走准星，所以不发服务器
	CurrentWeapon->CurrentZoomIndex = (FMath::Clamp(CurrentWeapon->CurrentZoomIndex, 0, Count - 1) + 1) % Count;
	OnZoomLevelChanged.Broadcast(CurrentWeapon->CurrentZoomIndex, CurrentWeapon->GetEffectiveAimFOV());
}

bool UCombatComponent::CanSwitchFireMode() const
{
	if (!IsValid(CurrentWeapon) || CurrentWeapon->GetAllowedFireModes().Num() < 2) return false;
	const EWeaponStatus Status = CurrentWeapon->WeaponStatus;
	return Status != EWeaponStatus::Reloading
		&& Status != EWeaponStatus::Cycling;
}

void UCombatComponent::Initiate_SwitchFireMode()
{
	if (!CanSwitchFireMode()) return;

	const EFireType NewFireType = CurrentWeapon->GetNextFireMode();
	BurstShotsRemaining = 0;

	// 本地立刻生效（FireTimerFinished 读的就是它），再让服务器同步给其他人
	CurrentWeapon->FireType = NewFireType;
	OnFireModeChanged.Broadcast(CurrentWeapon, NewFireType);

	if (!GetOwner()->HasAuthority())
	{
		Server_SetFireMode(NewFireType);
	}
}

void UCombatComponent::Server_SetFireMode_Implementation(EFireType NewFireType)
{
	if (!IsValid(CurrentWeapon) || !CurrentWeapon->AllowsFireType(NewFireType)) return;
	BurstShotsRemaining = 0;
	CurrentWeapon->FireType = NewFireType;
}

void UCombatComponent::Initiate_Throw(bool bHighThrow)
{
	CancelInspect();
	if (const UWorld* World = GetWorld())
	{
		if (const AShooterGameState* GS = World->GetGameState<AShooterGameState>())
		{
			if (GS->IsActionLocked()) return;
		}
	}
	if (!IsValid(CurrentWeapon) || CurrentWeapon->WeaponKind != EWeaponKind::Throwable) return;
	if (bThrowInProgress) return;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Cycling) return;
	if (CurrentWeapon->Ammo <= 0) return;
	if (!IsValid(CurrentWeapon->ProjectileClass))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Throw] %s has no ProjectileClass"), *GetNameSafe(CurrentWeapon));
		return;
	}

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(OwnerPawn->GetController()))
		{
			if (PC->IsSprinting())
			{
				PC->StopSprinting();
			}
		}
	}

	StopAimForWeaponSwitch();
	bThrowInProgress = true;
	bThrowHigh = bHighThrow;
	bThrowCommitted = false;
	bThrowReleaseRequested = false;
	bThrowPinPulling = PlayThrowPinPull(true);
	Server_BeginThrow(bHighThrow);
}

void UCombatComponent::ReleaseThrow()
{
	if (!bThrowInProgress || bThrowCommitted) return;
	if (!IsValid(CurrentWeapon) || CurrentWeapon->WeaponKind != EWeaponKind::Throwable) return;
	if (bThrowReleaseRequested) return;

	bThrowReleaseRequested = true;
	Server_ReleaseThrow();
	TryBeginThrowRelease();
}

void UCombatComponent::TryBeginThrowRelease()
{
	if (!bThrowInProgress || bThrowCommitted || !bThrowReleaseRequested) return;
	if (bThrowPinPulling) return;
	BeginThrowRelease();
}

UAnimMontage* UCombatComponent::ResolveThrowPinPullMontage(bool bFirstPerson) const
{
	if (!IsValid(CurrentWeapon) || !IsValid(WeaponData)) return nullptr;

	const TMap<FGameplayTag, FMontageData>& ViewMontages = bFirstPerson
		? WeaponData->FirstPersonMontages
		: WeaponData->ThirdPersonMontages;
	if (const FMontageData* MontageData = ViewMontages.Find(CurrentWeapon->WeaponType))
	{
		if (UAnimMontage* PinPull = MontageData->ResolveEquipMontage())
		{
			return PinPull;
		}
	}
	if (const FMontageData* SharedData = WeaponData->WeaponMontages.Find(CurrentWeapon->WeaponType))
	{
		return SharedData->ResolveEquipMontage();
	}
	return nullptr;
}

bool UCombatComponent::PlayThrowPinPull(bool bFirstPerson)
{
	if (!IsValid(CurrentWeapon) || !IsValid(WeaponData)) return false;
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UPlayerInterface>()) return false;

	UAnimMontage* PinPull = ResolveThrowPinPullMontage(bFirstPerson);
	if (!IsValid(PinPull)) return false;

	USkeletalMeshComponent* Mesh = bFirstPerson
		? IPlayerInterface::Execute_GetMesh1P(Owner)
		: IPlayerInterface::Execute_GetMesh3P(Owner);
	UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
	if (!IsValid(AnimInstance)) return false;

	const float Played = AnimInstance->Montage_Play(PinPull);
	if (Played <= 0.f) return false;
	bThrowPinPulling = true;

	if (bFirstPerson)
	{
		if (const FMontageData* WeaponMontageData = WeaponData->WeaponMontages.Find(CurrentWeapon->WeaponType))
		{
			if (UAnimMontage* WeaponPinPull = WeaponMontageData->ResolveEquipMontage())
			{
				if (USkeletalMeshComponent* WeaponMesh = CurrentWeapon->GetMesh1P())
				{
					if (UAnimInstance* WeaponAnim = WeaponMesh->GetAnimInstance())
					{
						WeaponAnim->Montage_Play(WeaponPinPull);
					}
				}
			}
		}

		ActiveThrowAnim = AnimInstance;
		ActiveThrowMontage = PinPull;
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &UCombatComponent::OnThrowMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, PinPull);
	}
	else
	{
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &UCombatComponent::OnRemoteThrowPinPullEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, PinPull);
	}
	return true;
}

void UCombatComponent::BeginThrowRelease()
{
	if (!IsValid(CurrentWeapon) || bThrowCommitted) return;
	if (bThrowPinPulling) return;
	if (GetWorld()->GetTimerManager().IsTimerActive(ThrowReleaseTimer)) return;

	PlayThrowMontage(true, bThrowHigh);
	const float ReleaseTime = FMath::Max(0.05f, CurrentWeapon->ThrowReleaseTime);
	GetWorld()->GetTimerManager().SetTimer(ThrowReleaseTimer, this, &UCombatComponent::CommitThrow, ReleaseTime, false);
}

void UCombatComponent::StopActiveThrowMontages()
{
	UAnimInstance* Anim = ActiveThrowAnim.Get();
	UAnimMontage* Montage = ActiveThrowMontage.Get();
	if (IsValid(Anim) && IsValid(Montage))
	{
		if (FAnimMontageInstance* Inst = Anim->GetActiveInstanceForMontage(Montage))
		{
			Inst->OnMontageEnded.Unbind();
		}
		if (Anim->Montage_IsPlaying(Montage))
		{
			Anim->Montage_Stop(0.1f, Montage);
		}
	}
	ActiveThrowAnim = nullptr;
	ActiveThrowMontage = nullptr;
}

void UCombatComponent::PlayThrowMontage(bool bFirstPerson, bool bHighThrow)
{
	if (!IsValid(CurrentWeapon) || !IsValid(WeaponData)) return;
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UPlayerInterface>()) return;

	const TMap<FGameplayTag, FMontageData>& ViewMontages = bFirstPerson
		? WeaponData->FirstPersonMontages
		: WeaponData->ThirdPersonMontages;
	const FMontageData* MontageData = ViewMontages.Find(CurrentWeapon->WeaponType);
	UAnimMontage* ThrowMontage = MontageData ? MontageData->ResolveThrowMontage(bHighThrow) : nullptr;
	USkeletalMeshComponent* Mesh = bFirstPerson
		? IPlayerInterface::Execute_GetMesh1P(Owner)
		: IPlayerInterface::Execute_GetMesh3P(Owner);
	UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
	if (!IsValid(AnimInstance) || !IsValid(ThrowMontage)) return;

	const float Played = AnimInstance->Montage_Play(ThrowMontage);
	if (Played <= 0.f) return;

	if (bFirstPerson)
	{
		ActiveThrowAnim = AnimInstance;
		ActiveThrowMontage = ThrowMontage;
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &UCombatComponent::OnThrowMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, ThrowMontage);
	}
}

void UCombatComponent::Notify_ThrowWeapon()
{
	UAnimMontage* PinPull = ResolveThrowPinPullMontage(true);
	if (IsValid(PinPull) && ActiveThrowMontage.Get() == PinPull)
	{
		return;
	}

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	const bool bLocal = IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	if (bLocal || GetOwnerRole() == ROLE_Authority)
	{
		CommitThrow();
	}
}

void UCombatComponent::CommitThrow()
{
	if (!bThrowInProgress || bThrowCommitted) return;
	if (!IsValid(CurrentWeapon) || CurrentWeapon->WeaponKind != EWeaponKind::Throwable) return;
	bThrowCommitted = true;
	GetWorld()->GetTimerManager().ClearTimer(ThrowReleaseTimer);

	CurrentWeapon->SetHiddenForHeal(true);

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	const bool bLocal = IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	if (bLocal && CurrentWeapon->UsesAmmo())
	{
		CurrentWeapon->Ammo = FMath::Clamp(CurrentWeapon->Ammo - 1, 0, CurrentWeapon->MagCapacity);
		if (IsValid(OwningPawn) && !OwningPawn->HasAuthority())
		{
			// Sequence is private on AWeapon; Auth/Rep_Fire handle remote correction.
		}
		NotifyAmmoChanged();
	}

	if (GetOwnerRole() == ROLE_Authority)
	{
		SpawnThrownProjectile();
		if (!bLocal)
		{
			CurrentWeapon->Auth_Fire();
		}
		Client_ThrowAmmo(CurrentWeapon->Ammo);
	}
	else
	{
		Server_CommitThrow();
	}
}

void UCombatComponent::SpawnThrownProjectile()
{
	if (GetOwnerRole() != ROLE_Authority) return;
	if (!IsValid(CurrentWeapon) || !CurrentWeapon->ProjectileClass) return;

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	FVector Loc;
	FVector Dir;
	float Speed = 0.f;
	CurrentWeapon->ComputeThrowLaunch(bThrowHigh, Loc, Dir, Speed);

	FActorSpawnParameters Params;
	Params.Owner = OwningPawn;
	Params.Instigator = OwningPawn;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AThrownProjectile* Projectile = GetWorld()->SpawnActor<AThrownProjectile>(
		CurrentWeapon->ProjectileClass,
		Loc,
		Dir.Rotation(),
		Params);
	if (IsValid(Projectile))
	{
		Projectile->Launch(Dir, Speed);
	}
}

void UCombatComponent::FinishThrow()
{
	GetWorld()->GetTimerManager().ClearTimer(ThrowReleaseTimer);
	bThrowInProgress = false;
	bThrowPinPulling = false;
	bThrowReleaseRequested = false;
	StopActiveThrowMontages();

	if (!IsValid(CurrentWeapon) || CurrentWeapon->WeaponKind != EWeaponKind::Throwable)
	{
		bThrowCommitted = false;
		return;
	}

	if (CurrentWeapon->WeaponStatus != EWeaponStatus::Cycling)
	{
		CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
	}

	if (CurrentWeapon->Ammo <= 0)
	{
		EquipFallbackAfterThrow();
	}
	else
	{
		CurrentWeapon->SetHiddenForHeal(false);
	}
	bThrowCommitted = false;
}

void UCombatComponent::OnThrowMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bThrowInProgress) return;

	UAnimMontage* PinPull = ResolveThrowPinPullMontage(true);
	UAnimMontage* ThrowMontage = nullptr;
	if (IsValid(CurrentWeapon) && IsValid(WeaponData))
	{
		if (const FMontageData* MontageData = WeaponData->FirstPersonMontages.Find(CurrentWeapon->WeaponType))
		{
			ThrowMontage = MontageData->ResolveThrowMontage(bThrowHigh);
		}
	}

	if (IsValid(Montage) && Montage == PinPull && PinPull != ThrowMontage)
	{
		bThrowPinPulling = false;
		if (bInterrupted || bThrowCommitted)
		{
			FinishThrow();
			return;
		}
		TryBeginThrowRelease();
		return;
	}

	if (!bThrowCommitted)
	{
		CommitThrow();
	}
	FinishThrow();
}

void UCombatComponent::OnRemoteThrowPinPullEnded(UAnimMontage* /*Montage*/, bool bInterrupted)
{
	bThrowPinPulling = false;
	if (bInterrupted || !bThrowInProgress) return;
	if (bThrowReleaseRequested)
	{
		PlayThrowMontage(false, bThrowHigh);
	}
}

AWeapon* UCombatComponent::FindFallbackWeaponAfterThrow() const
{
	if (AWeapon* Main = FindWeaponForSlot(EWeaponSlot::Main)) return Main;
	if (AWeapon* Pistol = FindWeaponForSlot(EWeaponSlot::Pistol)) return Pistol;
	if (AWeapon* Melee = FindWeaponForSlot(EWeaponSlot::Melee)) return Melee;
	if (AWeapon* Secondary = FindWeaponForSlot(EWeaponSlot::Secondary)) return Secondary;
	return nullptr;
}

void UCombatComponent::EquipFallbackAfterThrow()
{
	AWeapon* Fallback = FindFallbackWeaponAfterThrow();
	if (!IsValid(Fallback) || Fallback == CurrentWeapon) return;
	const int32 WeaponIndex = Inventory.IndexOfByKey(Fallback);
	if (!Inventory.IsValidIndex(WeaponIndex)) return;
	LocalWeaponIndex = WeaponIndex;
	Local_CycleWeapon(WeaponIndex, true);
	Server_CycleWeapon(WeaponIndex);
}

void UCombatComponent::Server_BeginThrow_Implementation(bool bHighThrow)
{
	if (!IsValid(CurrentWeapon) || CurrentWeapon->WeaponKind != EWeaponKind::Throwable) return;
	if (CurrentWeapon->Ammo <= 0) return;
	if (const AShooterGameState* GS = GetWorld() ? GetWorld()->GetGameState<AShooterGameState>() : nullptr)
	{
		if (GS->IsActionLocked()) return;
	}

	bThrowInProgress = true;
	bThrowHigh = bHighThrow;
	if (!bThrowCommitted)
	{
		bThrowCommitted = false;
	}
	Multicast_BeginThrow(bHighThrow);
}

void UCombatComponent::Multicast_BeginThrow_Implementation(bool bHighThrow)
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (IsValid(OwningPawn) && OwningPawn->IsLocallyControlled()) return;
	bThrowInProgress = true;
	bThrowHigh = bHighThrow;
	bThrowCommitted = false;
	bThrowReleaseRequested = false;
	bThrowPinPulling = PlayThrowPinPull(false);
}

void UCombatComponent::Server_ReleaseThrow_Implementation()
{
	if (!bThrowInProgress || bThrowCommitted) return;
	if (!IsValid(CurrentWeapon) || CurrentWeapon->WeaponKind != EWeaponKind::Throwable) return;
	bThrowReleaseRequested = true;
	Multicast_ReleaseThrow();
}

void UCombatComponent::Multicast_ReleaseThrow_Implementation()
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (IsValid(OwningPawn) && OwningPawn->IsLocallyControlled()) return;
	bThrowReleaseRequested = true;
	if (bThrowPinPulling) return;
	PlayThrowMontage(false, bThrowHigh);
}

void UCombatComponent::Server_CommitThrow_Implementation()
{
	if (!IsValid(CurrentWeapon) || CurrentWeapon->WeaponKind != EWeaponKind::Throwable) return;
	if (const AShooterGameState* GS = GetWorld() ? GetWorld()->GetGameState<AShooterGameState>() : nullptr)
	{
		if (GS->IsActionLocked()) return;
	}

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	const bool bLocal = IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	if (!bThrowCommitted)
	{
		bThrowCommitted = true;
		if (!bLocal)
		{
			CurrentWeapon->Auth_Fire();
		}
		SpawnThrownProjectile();
		Client_ThrowAmmo(CurrentWeapon->Ammo);
	}
	else if (bLocal)
	{
		// Listen-server host already spawned in CommitThrow.
	}
}

void UCombatComponent::Client_ThrowAmmo_Implementation(int32 AuthAmmo)
{
	if (!IsValid(CurrentWeapon)) return;
	CurrentWeapon->Rep_Fire(AuthAmmo);
	NotifyAmmoChanged();
}

void UCombatComponent::Initiate_Aim_Pressed()
{
	CancelInspect();
	if (IsValid(CurrentWeapon) && CurrentWeapon->WeaponKind == EWeaponKind::Throwable)
	{
		Initiate_Throw(false);
		return;
	}
	// 近战 / 炸弹不能开镜
	if (IsValid(CurrentWeapon) && !CurrentWeapon->CanAimWeapon())
	{
		if (bAiming)
		{
			Local_Aim(false);
			Server_Aim(false);
		}
		return;
	}
	const bool bNextAim = !bAiming;
	Local_Aim(bNextAim);
	Server_Aim(bNextAim);
}

void UCombatComponent::Initiate_Aim_Released()
{
	if (IsValid(CurrentWeapon) && CurrentWeapon->WeaponKind == EWeaponKind::Throwable)
	{
		ReleaseThrow();
	}
}

AWeapon* UCombatComponent::FindWeaponForSlot(EWeaponSlot Slot) const
{
	int32 LongGunIndex = 0;
	for (AWeapon* Weapon : Inventory)
	{
		if (!IsValid(Weapon)) continue;

		if (Slot == EWeaponSlot::Pistol)
		{
			if (Weapon->WeaponKind == EWeaponKind::Pistol) return Weapon;
			continue;
		}
		if (Slot == EWeaponSlot::Melee)
		{
			if (Weapon->WeaponKind == EWeaponKind::Melee) return Weapon;
			continue;
		}
		if (Slot == EWeaponSlot::Throwable)
		{
			if (Weapon->WeaponKind == EWeaponKind::Throwable) return Weapon;
			continue;
		}
		if (Slot == EWeaponSlot::Bomb)
		{
			if (Weapon->WeaponKind == EWeaponKind::Bomb) return Weapon;
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
	CancelInspect();
	if (bThrowInProgress) return;
	AWeapon* Weapon = FindWeaponForSlot(Slot);
	if (!IsValid(Weapon) || Weapon == CurrentWeapon) return;
	if (Weapon->WeaponKind == EWeaponKind::Throwable && Weapon->Ammo <= 0) return;

	if (IsValid(CurrentWeapon))
	{
		// 拔枪中按别的枪：打断并完整重走新枪拔枪；按同一把在上面已经 return（不重播）
		// 换弹中按别的枪：打断换弹，不给子弹（Equip 里处理）
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

void UCombatComponent::Initiate_EquipThrowableWeapon()
{
	Initiate_EquipSlot(EWeaponSlot::Throwable);
}

void UCombatComponent::Initiate_EquipBombWeapon()
{
	Initiate_EquipSlot(EWeaponSlot::Bomb);
}

bool UCombatComponent::AllowsHeal() const
{
	const AShooterGameState* GS = GetWorld() ? GetWorld()->GetGameState<AShooterGameState>() : nullptr;
	return !GS || !GS->HasMatchHeader() || GS->GetMatchMode() == EShooterGameMode::Extraction;
}

bool UCombatComponent::Initiate_Hotkey(int32 Hotkey)
{
	const AShooterGameState* GS = GetWorld() ? GetWorld()->GetGameState<AShooterGameState>() : nullptr;
	const bool bArena = GS && GS->HasMatchHeader() && GS->GetMatchMode() != EShooterGameMode::Extraction;
	const bool bSearchDestroy = GS && GS->GetMatchMode() == EShooterGameMode::SearchDestroy;

	switch (Hotkey)
	{
	case 1:
		Initiate_EquipSlot(EWeaponSlot::Main);
		return true;
	case 2:
		Initiate_EquipSlot(bArena ? EWeaponSlot::Pistol : EWeaponSlot::Secondary);
		return true;
	case 3:
		Initiate_EquipSlot(EWeaponSlot::Melee);
		return true;
	case 4:
		Initiate_EquipSlot(bArena ? EWeaponSlot::Throwable : EWeaponSlot::Pistol);
		return true;
	case 5:
		if (bSearchDestroy)
		{
			Initiate_EquipSlot(EWeaponSlot::Bomb);
			return true;
		}
		return false;
	default:
		return false;
	}
}

void UCombatComponent::GrantBombWeapon()
{
	if (GetOwnerRole() < ROLE_Authority) return;
	if (FindWeaponForSlot(EWeaponSlot::Bomb)) return;

	TSubclassOf<AWeapon> ClassToSpawn = BombWeaponClass ? BombWeaponClass : TSubclassOf<AWeapon>(ABombWeapon::StaticClass());
	AWeapon* Bomb = SpawnWeapon(ClassToSpawn);
	if (!IsValid(Bomb)) return;
	Bomb->WeaponKind = EWeaponKind::Bomb;
	Inventory.AddUnique(Bomb);
	ReserveAmmo.Add(Bomb->WeaponType, Bomb->StartingCarriedAmmo);
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UCombatComponent::RemoveBombWeapon()
{
	if (GetOwnerRole() < ROLE_Authority) return;
	AWeapon* Bomb = FindWeaponForSlot(EWeaponSlot::Bomb);
	if (!IsValid(Bomb)) return;

	const bool bWasCurrent = CurrentWeapon == Bomb;
	Inventory.Remove(Bomb);
	Bomb->Destroy();
	if (bWasCurrent)
	{
		CurrentWeapon = nullptr;
		if (AWeapon* Main = FindWeaponForSlot(EWeaponSlot::Main))
		{
			Equip(Main);
		}
		else if (Inventory.Num() > 0)
		{
			Equip(Inventory[0]);
		}
	}
}

void UCombatComponent::Initiate_VersionWeapon()
{
	if (!IsValid(CurrentWeapon) || !IsValid(WeaponData)) return;
	// 只有空闲时能检视：不在换弹、拉栓、切枪、开火、开镜
	if (CurrentWeapon->WeaponStatus != EWeaponStatus::Idle) return;
	if (bAiming || bTriggerPressed) return;
	if (IsInspecting()) return;   // 正在检视就不重新开始

	const FMontageData* MontageData = WeaponData->FirstPersonMontages.Find(CurrentWeapon->WeaponType);
	UAnimMontage* InspectMontage = MontageData ? MontageData->InspectMontage1P.Get() : nullptr;
	USkeletalMeshComponent* Mesh1P = IPlayerInterface::Execute_GetMesh1P(GetOwner());
	UAnimInstance* AnimInstance = IsValid(Mesh1P) ? Mesh1P->GetAnimInstance() : nullptr;
	if (!IsValid(InspectMontage) || !IsValid(AnimInstance)) return;
	if (AnimInstance->Montage_Play(InspectMontage) <= 0.f) return;

	ActiveInspectMontage = InspectMontage;
	InspectWeapon = CurrentWeapon;
	ActiveInspectWeaponMontage = nullptr;

	// 枪身检视（可选）：WeaponMontages 里也填了就同时播
	if (const FMontageData* WeaponMontageData = WeaponData->WeaponMontages.Find(CurrentWeapon->WeaponType))
	{
		USkeletalMeshComponent* WeaponMesh = CurrentWeapon->GetMesh1P();
		UAnimInstance* WeaponAnim = IsValid(WeaponMesh) ? WeaponMesh->GetAnimInstance() : nullptr;
		if (IsValid(WeaponMontageData->InspectMontage1P) && IsValid(WeaponAnim)
			&& WeaponAnim->Montage_Play(WeaponMontageData->InspectMontage1P) > 0.f)
		{
			ActiveInspectWeaponMontage = WeaponMontageData->InspectMontage1P;
		}
	}
}

bool UCombatComponent::IsInspecting() const
{
	UAnimMontage* Montage = ActiveInspectMontage.Get();
	if (!IsValid(Montage)) return false;
	USkeletalMeshComponent* Mesh1P = IPlayerInterface::Execute_GetMesh1P(GetOwner());
	UAnimInstance* AnimInstance = IsValid(Mesh1P) ? Mesh1P->GetAnimInstance() : nullptr;
	return IsValid(AnimInstance) && AnimInstance->Montage_IsPlaying(Montage);
}

void UCombatComponent::CancelInspect()
{
	constexpr float InspectBlendOut = 0.15f;   // 短暂淡出，避免手臂突然跳回

	if (UAnimMontage* Montage = ActiveInspectMontage.Get())
	{
		USkeletalMeshComponent* Mesh1P = IPlayerInterface::Execute_GetMesh1P(GetOwner());
		UAnimInstance* AnimInstance = IsValid(Mesh1P) ? Mesh1P->GetAnimInstance() : nullptr;
		if (IsValid(AnimInstance) && AnimInstance->Montage_IsPlaying(Montage))
		{
			AnimInstance->Montage_Stop(InspectBlendOut, Montage);
		}
	}
	if (UAnimMontage* WeaponMontage = ActiveInspectWeaponMontage.Get())
	{
		AWeapon* Weapon = InspectWeapon.Get();
		USkeletalMeshComponent* WeaponMesh = IsValid(Weapon) ? Weapon->GetMesh1P() : nullptr;
		UAnimInstance* WeaponAnim = IsValid(WeaponMesh) ? WeaponMesh->GetAnimInstance() : nullptr;
		if (IsValid(WeaponAnim) && WeaponAnim->Montage_IsPlaying(WeaponMontage))
		{
			WeaponAnim->Montage_Stop(InspectBlendOut, WeaponMontage);
		}
	}
	ActiveInspectMontage = nullptr;
	ActiveInspectWeaponMontage = nullptr;
	InspectWeapon = nullptr;
}

void UCombatComponent::Notify_ReloadWeapon()
{
	if (!IsValid(CurrentWeapon)) return;
	// Client RPCs only run when invoked with authority. A 1P notify on a remote client
	// must not call Client_ReloadWeapon; the server's 3P notify sends the result.
	if (GetOwnerRole() != ROLE_Authority)
	{
		// 自己客户端上的 1P 通知：按通知帧让服务器加子弹。其他客户端看到的 3P 通知不发。
		// 服务器加完会切 Idle，之后的播完兜底 Server_EndReload(true) 进不去，不会加两次。
		const APawn* OwningPawn = Cast<APawn>(GetOwner());
		if (IsValid(OwningPawn) && OwningPawn->IsLocallyControlled() && CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading)
		{
			Server_ReloadNotify();
		}
		return;
	}
	// 换弹已被切枪打断：不给子弹
	if (CurrentWeapon->WeaponStatus != EWeaponStatus::Reloading) return;

	const int32 EmptySpace = CurrentWeapon->MagCapacity - CurrentWeapon->Ammo;
	const int32 AmountToRefill = FMath::Min(EmptySpace, CurrentReserveAmmo);
	if (AmountToRefill > 0)
	{
		CurrentWeapon->Ammo += AmountToRefill;
		CurrentReserveAmmo -= AmountToRefill;
		ReserveAmmo.Add(CurrentWeapon->WeaponType, CurrentReserveAmmo);
	}

	CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
	Client_ReloadWeapon(CurrentWeapon, CurrentWeapon->Ammo, CurrentReserveAmmo);
}

void UCombatComponent::Server_Aim_Implementation(bool bPressed)
{
	Local_Aim(bPressed);
}



void UCombatComponent::Local_Aim(bool bPressed)
{
	// 近战强制关镜：本地 / 服务器 / 切枪同步都走这里
	if (bPressed && IsValid(CurrentWeapon) && !CurrentWeapon->CanAimWeapon())
	{
		bPressed = false;
	}
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

void UCombatComponent::Initiate_ToggleLaser()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return;
	if (Owner->HasAuthority())
	{
		Server_ToggleLaser_Implementation();
	}
	else
	{
		Server_ToggleLaser();
	}
}

void UCombatComponent::Initiate_SetLaserOn(bool bOn)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !IsValid(CurrentWeapon) || !CurrentWeapon->HasLaser()) return;
	if (CurrentWeapon->IsLaserOn() == bOn) return;
	if (Owner->HasAuthority())
	{
		Server_SetLaserOn_Implementation(bOn);
	}
	else
	{
		Server_SetLaserOn(bOn);
	}
}

void UCombatComponent::Server_SetLaserOn_Implementation(bool bOn)
{
	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->SetLaserOn(bOn);
	}
}

void UCombatComponent::Server_ToggleLaser_Implementation()
{
	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->ToggleLaser();
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
	const float AimFOV = IsValid(CurrentWeapon) ? CurrentWeapon->GetEffectiveAimFOV() : 0.f;
	return AimFOV > 0.f ? AimFOV : Fallback;
}