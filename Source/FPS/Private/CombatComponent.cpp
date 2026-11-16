#include "CombatComponent.h"
#include "Game/DoorGestureComponent.h"
#include "Game/DoorToolComponent.h"
#include "Game/RestraintActionComponent.h"



#include "Game/AINoiseEvent.h"
#include "FPSTraceChannels.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Animation/FirstPersonAnimInstance.h"
#include "Animation/PSRArmsAnimInstance.h"
#include "ShooterCharacter.h"
#include "Combat/HitZoneVitalsComponent.h"
#include "Game/HostileAICharacter.h"
#include "World/BloodPool.h"
#include "ShooterGameModeBase.h"
#include "ShooterGamePlayTags.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/Weapon.h"
#include "WeaponData.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Interface/PlayerInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Misc/PackageName.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UnrealType.h"
#include "UObject/GCObject.h"

#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "ShooterPlayerController.h"
#include "Loadout/LoadoutTypes.h"
#include "Loadout/PouchInventoryComponent.h"
#include "Weapon/WeaponFlash.h"
#include "Weapon/WeaponSting.h"
#include "Weapon/AttachmentData.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ShooterType/ShooterType.h"
#include "Interface/RecoilModifierInterface.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Game/ShooterGameState.h"
#include "Game/ShooterPlayerState.h"
#include "Range/TargetDummy.h"
#include "Ui/HudBodyMarks.h"
#include "Weapon/ThrownProjectile.h"
#include "Loadout/LoadoutTypes.h"

namespace MagCheckCache
{
	struct FCache : public FGCObject
	{
		TMap<FString, TObjectPtr<UAnimMontage>> Montages;
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			Collector.AddReferencedObjects(Montages);
		}
		virtual FString GetReferencerName() const override { return TEXT("FPSMagCheckMontageCache"); }
	};
	FCache GCache;

	UAnimMontage* GetOrCreate(const FString& Key, UAnimSequence* Seq, FName Slot, float InBlend, float OutBlend)
	{
		if (!IsValid(Seq)) return nullptr;
		if (const TObjectPtr<UAnimMontage>* Found = GCache.Montages.Find(Key))
		{
			if (IsValid(Found->Get()))
			{
				return Found->Get();
			}
		}
		if (UAnimMontage* Dyn = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Seq, Slot, InBlend, OutBlend))
		{
			GCache.Montages.Add(Key, Dyn);
			return Dyn;
		}
		return nullptr;
	}
}

namespace
{
	bool MagHasCharacterReload(const UAttachmentData* Mag)
	{
		return IsValid(Mag) && (
			Mag->MagCharacterReloadMontage
			|| Mag->MagCharacterEmptyReloadMontage
			|| Mag->MagCharacterTacticalReloadMontage);
	}

	UAnimMontage* ResolveMagazineCharacterReload(const UAttachmentData* Mag, EReloadType Type, EGripHoldStyle Style)
	{
		(void)Style;
		if (!IsValid(Mag)) return nullptr;

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
 if(GetOwner())
 {
  if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsBusy())return;
  if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>())Door->StopGesture();
 }
 if(GetOwner())if(auto* Action=GetOwner()->FindComponentByClass<URestraintActionComponent>())Action->CancelForCombatAction();
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
	// Arms override BEFORE attach so GripPoints/WeaponSocket resolve on the correct 1P skeleton.
	ApplyFirstPersonArmsForWeapon(CurrentWeapon);
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

	EnsureMagQueueForWeapon(CurrentWeapon);
	if (const TArray<int32>* Q = MagQueues.Find(CurrentWeapon->WeaponType))
	{
		CurrentReserveAmmo = Q->Num();
		ReserveAmmo.Add(CurrentWeapon->WeaponType, CurrentReserveAmmo);
	}
	else if (const int32* FoundReserve = ReserveAmmo.Find(CurrentWeapon->WeaponType))
	{
		CurrentReserveAmmo = *FoundReserve;
	}
	else
	{
		CurrentReserveAmmo = 0;
	}
	OnInspectionHudChanged.Broadcast();
	OnCurrentReserveAmmoChanged.Broadcast(CurrentReserveAmmo, CurrentWeapon->Ammo, CurrentWeapon->WeaponIcon);
	NotifyAmmoChanged();
	ResetRecoilPattern();
	BurstShotsRemaining = 0;
	CurrentWeapon->SanitizeFireType();
	OnInspectionHudChanged.Broadcast();
	OnFireModeChanged.Broadcast(CurrentWeapon, CurrentWeapon->FireType);
	OnCurrentWeaponChanged.Broadcast(CurrentWeapon); // 通知 UI：换枪了
}


void UCombatComponent::SpawnInventory()
{
	if (GetOwner()->GetLocalRole() < ROLE_Authority)return;

	// 局外配装：每把预设枪替换默认武器里同 WeaponKind 的第一把；没有同类就追加
	TArray<TSubclassOf<AWeapon>> SpawnClasses = DefaultWeaponClass;
	TArray<const FWeaponPreset*> SpawnPresets;
	SpawnPresets.Init(nullptr, SpawnClasses.Num());
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const AShooterPlayerController* LoadoutPC = IsValid(OwnerPawn) ? Cast<AShooterPlayerController>(OwnerPawn->GetController()) : nullptr;
	bool bReplaceThrowables = false;
	if (const FLoadoutPreset* Loadout = IsValid(LoadoutPC) ? LoadoutPC->GetSubmittedLoadout() : nullptr)
	{
		bReplaceThrowables = Loadout->bReplaceThrowables;
		for (const FWeaponPreset& Preset : Loadout->Weapons)
		{
			if (!Preset.WeaponClass) continue;
			const EWeaponKind Kind = Preset.WeaponClass->GetDefaultObject<AWeapon>()->WeaponKind;
			int32 Slot = INDEX_NONE;
			// Throwables: always append so flash+HE / molotov+HE both spawn.
			if (Kind != EWeaponKind::Throwable)
			{
				for (int32 i = 0; i < SpawnClasses.Num(); ++i)
				{
					if (!SpawnPresets[i] && SpawnClasses[i] && SpawnClasses[i]->GetDefaultObject<AWeapon>()->WeaponKind == Kind)
					{
						Slot = i;
						break;
					}
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

	const int32 MaxLong = 2;
	const bool bAllowThrowable = true;
	const int32 MaxThrowables = 4;
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
		if (CDO->WeaponKind == EWeaponKind::Throwable && bReplaceThrowables && !SpawnPresets[SpawnIndex]) continue;
		if (CDO->WeaponKind == EWeaponKind::Throwable && (!bAllowThrowable || ThrowableCount >= MaxThrowables)) continue;

		AWeapon* Weapon = SpawnWeapon(SpawnClasses[SpawnIndex]);
		if (!IsValid(Weapon)) continue;
		if (const FWeaponPreset* Preset = SpawnPresets[SpawnIndex])
		{
			// 预设里没有的槽先卸掉（必需槽 RemoveAttachment 会拒绝），再装预设里的；EquipAttachment 自己做白名单校验
			for (const FWeaponAttachmentSlot& AttachSlot : Weapon->GetAttachmentSlots())
			{
				const bool bInPreset = Preset->Attachments.ContainsByPredicate([Weapon, &AttachSlot](const UAttachmentData* A)
				{
					return IsValid(A) && IsValid(Weapon) && Weapon->GetEquipSlot(A) == AttachSlot.SlotTag;
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
		Equip(ToEquip);
		InitializeWeaponWidget();
		IPlayerInterface::Execute_WeaponReplicated(GetOwner());
	}
	SeedPouchFromInventory();
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

}

void UCombatComponent::ClearPouch()
{
	AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner());
	UPouchInventoryComponent* PouchInv = Shooter ? Shooter->GetPouch() : nullptr;
	if (!PouchInv) return;
	for (int32 i = 0; i < PouchInv->GetSlotCount(); ++i)
	{
		PouchInv->ClearSlot(i);
	}
}

void UCombatComponent::SeedPouchFromInventory()
{
	AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner());
	UPouchInventoryComponent* PouchInv = Shooter ? Shooter->GetPouch() : nullptr;
	if (!PouchInv) return;

	ClearPouch();

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const AShooterPlayerController* LoadoutPC = IsValid(OwnerPawn) ? Cast<AShooterPlayerController>(OwnerPawn->GetController()) : nullptr;
	const FLoadoutPreset* Loadout = IsValid(LoadoutPC) ? LoadoutPC->GetSubmittedLoadout() : nullptr;
	const bool bBenchCounts = Loadout && Loadout->bReplaceThrowables;

	AWeapon* Rifle = nullptr;
	AWeapon* Sidearm = nullptr;
	for (AWeapon* Weapon : Inventory)
	{
		if (!IsValid(Weapon) || !Weapon->UsesAmmo()) continue;
		const bool bShotgun = Weapon->WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_ShotGun)
			|| Weapon->WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_DoubleBarrel)
			|| Weapon->WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Winchester);
		if (!Rifle && Weapon->WeaponKind == EWeaponKind::LongGun && (bBenchCounts || !bShotgun))
		{
			Rifle = Weapon;
		}
		if (!Sidearm && Weapon->WeaponKind == EWeaponKind::Pistol)
		{
			Sidearm = Weapon;
		}
	}

	auto AddFullMags = [this, PouchInv](AWeapon* Weapon, FGameplayTag FallbackType, int32 Count, int32 FallbackCapacity)
	{
		const FGameplayTag Type = IsValid(Weapon) && Weapon->WeaponType.IsValid() ? Weapon->WeaponType : FallbackType;
		const int32 Capacity = IsValid(Weapon) && Weapon->MagCapacity > 0 ? Weapon->MagCapacity : FallbackCapacity;
		TArray<int32> Queue;
		Queue.Init(Capacity, Count);
		MagQueues.Add(Type, Queue);
		ReserveAmmo.Add(Type, Count);
		if (IsValid(CurrentWeapon) && CurrentWeapon->WeaponType.MatchesTagExact(Type))
		{
			CurrentReserveAmmo = Count;
			SpareMagRounds = Queue;
			OnInspectionHudChanged.Broadcast();
	OnCurrentReserveAmmoChanged.Broadcast(CurrentReserveAmmo, CurrentWeapon->Ammo, CurrentWeapon->WeaponIcon);
		}
		for (int32 i = 0; i < Count; ++i)
		{
			PouchInv->TryAddMagzine(Type, Capacity, Capacity);
		}
	};

	int32 RifleMags = 5;
	int32 PistolMags = 3;
	constexpr int32 FlashCount = 3;
	constexpr int32 StingCount = 2;
	if (bBenchCounts)
	{
		RifleMags = 0;
		PistolMags = 0;
		bool bSawLong = false;
		bool bSawPistol = false;
		for (const FWeaponPreset& Preset : Loadout->Weapons)
		{
			if (!Preset.WeaponClass) continue;
			const EWeaponKind Kind = Preset.WeaponClass->GetDefaultObject<AWeapon>()->WeaponKind;
			if (Kind == EWeaponKind::LongGun && !bSawLong)
			{
				RifleMags = FMath::Max(0, Preset.StackCount);
				bSawLong = true;
			}
			else if (Kind == EWeaponKind::Pistol && !bSawPistol)
			{
				PistolMags = FMath::Max(0, Preset.StackCount);
				bSawPistol = true;
			}
		}
	}

	AddFullMags(Rifle, ShooterTags::Tag_WeaponType_Rifle, RifleMags, 30);
	AddFullMags(Sidearm, ShooterTags::Tag_WeaponType_Pistol, PistolMags, 15);

	if (bBenchCounts)
	{
		for (AWeapon* Weapon : Inventory)
		{
			if (!IsValid(Weapon) || Weapon->WeaponKind != EWeaponKind::Throwable) continue;
			int32 Count = 0;
			for (const FWeaponPreset& Preset : Loadout->Weapons)
			{
				if (Preset.WeaponClass == Weapon->GetClass())
				{
					Count = FMath::Max(0, Preset.StackCount);
					break;
				}
			}
			Weapon->Ammo = Count;
			for (int32 i = 0; i < Count; ++i)
			{
				PouchInv->TryAddThrowable(Weapon->GetClass());
			}
		}
	}
	else
	{
		TSubclassOf<AWeapon> FlashClass = nullptr;
		for (AWeapon* Weapon : Inventory)
		{
			if (IsValid(Weapon) && Weapon->IsA(AWeaponFlash::StaticClass()))
			{
				Weapon->Ammo = FlashCount;
				FlashClass = Weapon->GetClass();
			}
		}
		if (!FlashClass)
		{
			FlashClass = AWeaponFlash::StaticClass();
		}
		for (int32 i = 0; i < FlashCount; ++i)
		{
			PouchInv->TryAddThrowable(FlashClass);
		}
		for (int32 i = 0; i < StingCount; ++i)
		{
			PouchInv->TryAddThrowable(AWeaponSting::StaticClass());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Pouch] %s %d rifle mags, %d pistol mags (%d / %d)"),
		*GetNameSafe(Shooter), RifleMags, PistolMags,
		PouchInv->CountUsed(), PouchInv->GetSlotCount());
}

void UCombatComponent::DestroyInventory()
{
	ClearPouch();
	for (AWeapon* Weapon : Inventory)
	{
		if (IsValid(Weapon))
		{
			Weapon->DetachFromPawn();
			Weapon->Destroy();
		}
	}
	Inventory.Reset();
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
	CurrentWeapon->UpdateMagazineAmmoVisuals();
	SyncSpareMagRoundsForHud();
	OnInspectionHudChanged.Broadcast();
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
	ApplyFirstPersonArmsForWeapon(CurrentWeapon);
	CurrentWeapon->AttachToOwingPawn();
	ApplyEquippedWeaponAnim(CurrentWeapon);
	ResetRecoilPattern();
	BurstShotsRemaining = 0;
	CurrentWeapon->SanitizeFireType();
	OnInspectionHudChanged.Broadcast();
	OnFireModeChanged.Broadcast(CurrentWeapon, CurrentWeapon->FireType);
	IPlayerInterface::Execute_WeaponReplicated(GetOwner());
	InitializeWeaponWidget();
	OnCurrentWeaponChanged.Broadcast(CurrentWeapon); // 客户端也通知 UI
}




void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickRecoil(DeltaTime);
	// MagCheck montage finished naturally — clear inspect state.
	if (ActiveInspectMontage.IsValid() && !IsInspecting())
	{
		CancelInspect();
	}
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

	// Mesh Loc kick kept OFF (no AddVisualRecoil). View recoil uses original COD pattern above.
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
	DOREPLIFETIME_CONDITION(UCombatComponent, SpareMagRounds, COND_OwnerOnly);
}

FGameplayTag UCombatComponent::ResolveAnimTag(const AWeapon* Weapon) const
{
	if (!IsValid(Weapon)) return FGameplayTag();
	if (Weapon->WeaponKind == EWeaponKind::Throwable
		&& Weapon->WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Flash))
	{
		return ShooterTags::Tag_WeaponType_Grenade;
	}
	return Weapon->WeaponType;
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
	// Multicast skips locally-controlled owners — also play 3P equip for ToggleThirdPerson.
	if (bFirstPerson)
	{
		PlayEquipMontage(NextWeapon, false);
	}
}


void UCombatComponent::CacheDefaultFirstPersonArms()
{
	if (bCachedDefaultArms) return;
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UPlayerInterface>()) return;
	USkeletalMeshComponent* Arms = IPlayerInterface::Execute_GetMesh1P(Owner);
	if (!IsValid(Arms)) return;
	DefaultArmsMesh1P = Arms->GetSkeletalMeshAsset();
	DefaultArmsAnimClass = Arms->GetAnimClass();
	bCachedDefaultArms = true;
}

void UCombatComponent::RecacheDefaultFirstPersonArms()
{
	bCachedDefaultArms = false;
	CacheDefaultFirstPersonArms();
}

void UCombatComponent::ApplyFirstPersonArmsForWeapon(AWeapon* Weapon)
{
 CacheDefaultFirstPersonArms();
 if(auto* Arms=IPlayerInterface::Execute_GetMesh1P(GetOwner()))
  Arms->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
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
		if (const FPlayerAnim* Anim = Anims.Find(ResolveAnimTag(CurrentWeapon)))
		{
			return IsValid(Anim->Strafe_Walking);
		}
		return false;
	};

	// Either view having Walk BS is enough — 3P is what you see for foot sliding
	return HasWalk(WeaponData->ThirdPersonAnims) || HasWalk(WeaponData->FirstPersonAnims);
}

void UCombatComponent::RefreshAnimsAfterMesh3PSwap(bool bReattachWeapon)
{
	if (IsValid(CurrentWeapon))
	{
		// Mesh3P skeletal asset just changed (Bandits) — rebind gun before anim push.
		if(bReattachWeapon)CurrentWeapon->AttachToOwingPawn();
		ApplyEquippedWeaponAnim(CurrentWeapon);
	}
}


namespace
{
	// Rifle / shotgun / PCC / PSR have no ThirdPersonAnims row. Reuse the AK hold
	// (idle + strafe BlendSpace + aim offset). Sidearms without a row reuse X24.
	// A row that exists is kept even when its BlendSpace slots are empty (grenade).
	FGameplayTag ResolveThirdPersonPoseTag(const UWeaponData* Data, const AWeapon* Weapon)
	{
		if (!IsValid(Data) || !IsValid(Weapon))
		{
			return FGameplayTag();
		}

		const FGameplayTag Requested = Weapon->WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Flash)
			? ShooterTags::Tag_WeaponType_Grenade
			: Weapon->WeaponType;
		if (Data->ThirdPersonAnims.Contains(Requested))
		{
			return Requested;
		}

		const bool bSidearm = Weapon->WeaponKind == EWeaponKind::Pistol
			|| Requested.MatchesTagExact(ShooterTags::Tag_WeaponType_Pistol)
			|| Requested.MatchesTagExact(ShooterTags::Tag_WeaponType_X24)
			|| Requested.MatchesTagExact(ShooterTags::Tag_WeaponType_Colt1911)
			|| Requested.MatchesTagExact(ShooterTags::Tag_WeaponType_Deagle);
		const FGameplayTag Preferred = bSidearm ? ShooterTags::Tag_WeaponType_X24 : ShooterTags::Tag_WeaponType_AK;
		if (Data->ThirdPersonAnims.Contains(Preferred))
		{
			return Preferred;
		}
		if (bSidearm && Data->ThirdPersonAnims.Contains(ShooterTags::Tag_WeaponType_Pistol))
		{
			return ShooterTags::Tag_WeaponType_Pistol;
		}
		return Requested;
	}
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
		const FGameplayTag AnimTag = bFirstPerson
			? ResolveHoldPoseTag(CurrentWeapon)
			: ResolveThirdPersonPoseTag(WeaponData, CurrentWeapon);
		const FPlayerAnim* WeaponAnim = Anims.Find(AnimTag);
		FPlayerAnim AnimCopy;
		if (WeaponAnim)
		{
			AnimCopy = *WeaponAnim;
		}
		// Arms*Sequence clips are first-person (SK_Arms). Writing them onto Mesh3P
		// replaces the body idle and leaves Strafe / AimOffset empty.
		if (bFirstPerson)
		{
			CurrentWeapon->ApplyAuthoredArmAnims(AnimCopy);
		}
		else if (AnimTag.MatchesTagExact(ShooterTags::Tag_WeaponType_AK)
			|| AnimTag.MatchesTagExact(ShooterTags::Tag_WeaponType_X24))
		{
			if (!CurrentWeapon->WeaponType.MatchesTagExact(AnimTag))
			{
				UE_LOG(LogTemp, Log, TEXT("[Anim] %s 3P pose %s (weapon tag %s)"),
					*GetNameSafe(CurrentWeapon), *AnimTag.ToString(), *CurrentWeapon->WeaponType.ToString());
			}
		}
		if (!IsValid(AnimCopy.IdleAnim))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Anim] %s WeaponType=%s has no %s Idle"),
				*GetNameSafe(CurrentWeapon), *CurrentWeapon->WeaponType.ToString(),
				bFirstPerson ? TEXT("FirstPerson") : TEXT("ThirdPerson"));
			return;
		}
		// 站立 Locomotion 绑的是 Strafe_Standing；慢走时换成 Walk BS，避免继续采 Jog 造成小碎步
		if (bUseWalkGait && IsValid(AnimCopy.Strafe_Walking))
		{
			AnimCopy.Strafe_Standing = AnimCopy.Strafe_Walking;
		}
		// SprintAnim 未填时用 Idle，避免空引用。冲刺动画填在 DA_WeaponData 的 FirstPersonAnims / ThirdPersonAnims。
		if (!IsValid(AnimCopy.SprintAnim) && IsValid(AnimCopy.IdleAnim))
		{
			AnimCopy.SprintAnim = AnimCopy.IdleAnim;
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


		// First person is parented to the camera, so look pitch is already in the view.
		// Aim offset stays on Mesh3P only.
		const bool bShotgunRaw1P = bFirstPerson
			&& CurrentWeapon->WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_ShotGun);
		if (bFirstPerson)
		{
			AnimCopy.AimOffset_Hip = nullptr;
			AnimCopy.AimOffset_Aim = nullptr;
		}
		if (bShotgunRaw1P)
		{
			AnimCopy.Strafe_Standing = nullptr;
			AnimCopy.Strafe_Crouching = nullptr;
			AnimCopy.Strafe_Walking = nullptr;
			AnimCopy.LeftHandHoldPose_Cradle = nullptr;
			AnimCopy.LeftHandHoldPose_Grab = nullptr;
			AnimCopy.IdleAnim_Grab = nullptr;
			AnimCopy.AimIdleAnim_Grab = nullptr;
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
			if (bShotgunRaw1P)
			{
				FPAnim->LeftHandHoldPose = nullptr;
				FPAnim->bUseLeftHandHoldPose = false;
			}

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

	const bool bFirstDraw = !Weapon->bHasEquippedOnce;
	const FGameplayTag MontageTag = bFirstPerson
		? ResolveHoldPoseTag(Weapon)
		: ResolveThirdPersonPoseTag(WeaponData, Weapon);
	const FMontageData* MontageData = ViewMontages.Find(MontageTag);
	UAnimMontage* EquipMontage = MontageData ? MontageData->ResolveEquipMontage(bFirstDraw) : nullptr;
	if (!IsValid(EquipMontage) && !bFirstPerson && MontageTag != ResolveAnimTag(Weapon))
	{
		if (const FMontageData* PoseData = WeaponData->ThirdPersonMontages.Find(ResolveAnimTag(Weapon)))
		{
			EquipMontage = PoseData->ResolveEquipMontage(bFirstDraw);
		}
	}
	if (!IsValid(EquipMontage))
	{
		if (const FMontageData* SharedData = WeaponData->WeaponMontages.Find(ResolveAnimTag(Weapon)))
		{
			EquipMontage = SharedData->ResolveEquipMontage(bFirstDraw);
		}
	}

	USkeletalMeshComponent* Mesh = bFirstPerson
		? IPlayerInterface::Execute_GetMesh1P(Owner)
		: IPlayerInterface::Execute_GetMesh3P(Owner);
	UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
	// Mesh1P is the arms skeleton. The assigned equip clips use the full-body mannequin
	// skeleton. Montage_Play still runs that pair; rejecting it is what silenced 1P.
	UAnimMontage* PlayedEquip = EquipMontage;
	bool bAlreadyPlaying = false;
	if (!IsValid(EquipMontage) && bFirstPerson && IsValid(Weapon->ArmsEquipSequence) && IsValid(AnimInstance))
	{
		PlayedEquip = AWeapon::PlaySequenceAsDynamicMontage(AnimInstance, Weapon->ArmsEquipSequence);
		bAlreadyPlaying = IsValid(PlayedEquip);
	}
	if (!IsValid(AnimInstance) || !IsValid(PlayedEquip))
	{
		// No equip anim: still allow Idle (sequence weapons may only have fire/reload).
		// Do not clobber an in-progress 1P equip when the optional 3P clip is missing.
		if (Weapon->WeaponStatus != EWeaponStatus::Cycling)
		{
			Weapon->WeaponStatus = EWeaponStatus::Idle;
		}
		if (bFirstPerson && IsValid(Weapon->WeaponEquipSequence))
		{
			AWeapon::PlayWeaponSequence(Weapon->GetMesh1P(), Weapon->WeaponEquipSequence);
		}
		return;
	}

	if (!bAlreadyPlaying)
	{
		float EquipRate = 1.f;
		if (const AShooterCharacter* ShooterOwner = Cast<AShooterCharacter>(Owner))
		{
			EquipRate = ShooterOwner->GetArmInjuryAnimPlayRate();
		}
		const float PlayedLength = AnimInstance->Montage_Play(PlayedEquip, EquipRate);
		if (PlayedLength <= 0.f)
		{
			if (Weapon->WeaponStatus != EWeaponStatus::Cycling)
			{
				Weapon->WeaponStatus = EWeaponStatus::Idle;
			}
			return;
		}
	}
	EquipMontage = PlayedEquip;

	// 枪身没有蒙太奇槽时，Weapon Equip Sequence 直接播在这一侧的枪上，停在最后一帧。
	bool bPlayedBodyEquip = false;
	if (IsValid(Weapon->WeaponEquipSequence))
	{
		USkeletalMeshComponent* GunMesh = bFirstPerson ? Weapon->GetMesh1P() : Weapon->GetMesh3P();
		AWeapon::PlayBodySequence(GunMesh, Weapon->WeaponEquipSequence);
		bPlayedBodyEquip = true;
	}

	// 第一人称拔枪时，枪身蒙太奇跟手臂一起播。手臂已经有 Equip 时，WeaponMontages 不会当替补，所以这里单独播。
	if (bFirstPerson)
	{
		bool bPlayedWeaponEquip = false;
		if (const FMontageData* WeaponMontageData = WeaponData->WeaponMontages.Find(ResolveAnimTag(Weapon)))
		{
			if (UAnimMontage* WeaponEquip = WeaponMontageData->ResolveEquipMontage(bFirstDraw))
			{
				if (USkeletalMeshComponent* WeaponMesh = Weapon->GetMesh1P())
				{
					if (UAnimInstance* WeaponAnim = WeaponMesh->GetAnimInstance())
					{
						WeaponAnim->Montage_Play(WeaponEquip);
						bPlayedWeaponEquip = true;
					}
					else
					{
						AWeapon::PlayWeaponMontageOrSequence(WeaponMesh, WeaponEquip);
						bPlayedWeaponEquip = true;
					}
				}
			}
		}
		if (!bPlayedWeaponEquip && !bPlayedBodyEquip && IsValid(Weapon->WeaponEquipSequence))
		{
			AWeapon::PlayWeaponSequence(Weapon->GetMesh1P(), Weapon->WeaponEquipSequence);
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
	Weapon->bHasEquippedOnce = true;
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
	// Grip notifies during equip can leave a KeepWorld-relative offset; snap back to authored attach.
	if (IsValid(Weapon) && Weapon == CurrentWeapon)
	{
		Weapon->AttachToOwingPawn();
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
	auto Collect = [this, &Montages, Weapon](const TMap<FGameplayTag, FMontageData>& Map)
	{
		if (const FMontageData* Data = Map.Find(ResolveAnimTag(Weapon)))
		{
			for (EReloadType Type : { EReloadType::Empty, EReloadType::Tactical })
			{
				if (UAnimMontage* M = Data->ResolveReloadMontage(Type)) Montages.AddUnique(M);
			}
			if (UAnimMontage* Legacy = Data->ReloadMontage) Montages.AddUnique(Legacy);
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
 if(GetOwner())
 {
  if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsBusy())return;
  if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>())Door->StopGesture();
 }
 if(GetOwner())if(auto* Action=GetOwner()->FindComponentByClass<URestraintActionComponent>())Action->CancelForCombatAction();
	CancelInspect();
	if (StopFireIfWeaponBlocked()) return;
	if (const AShooterCharacter* Self = Cast<AShooterCharacter>(GetOwner()))
	{
		if (Self->IsBandaging()) return;
	}
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
				// COD 式冲刺转开火：停冲刺，缓冲 SprintToFire 时间后如果扳机还按着就自动开第一枪
				const float Buffer = PC->GetSprintToFireDelay();
				PC->StopSprinting();
				if (!IsValid(CurrentWeapon)
					|| CurrentWeapon->WeaponKind == EWeaponKind::Bomb
					|| CurrentWeapon->WeaponKind == EWeaponKind::Throwable)
				{
					return;
				}
				bTriggerPressed = true;
				if (Buffer > KINDA_SMALL_NUMBER)
				{
					GetWorld()->GetTimerManager().SetTimer(SprintFireBufferTimer, this, &ThisClass::OnSprintFireBufferElapsed, Buffer, false);
					return;
				}
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
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading)
	{
		// 开火打断换弹：弹匣里还有子弹、弹匣还没插进去时才打断（空匣继续换）
		if (CurrentWeapon->Ammo <= 0 || !CancelReload()) return;
	}
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
		{
			APawn* LocalPawn = Cast<APawn>(GetOwner());
			if (IsValid(LocalPawn) && LocalPawn->IsLocallyControlled())
			{
				PlayDryFireClick();
			}
		}
	}
}

void UCombatComponent::OnSprintFireBufferElapsed()
{
	if (!bTriggerPressed) return;
	Initiate_FireWeapon_Pressed();
}

void UCombatComponent::Initiate_FireWeapon_Released()
{
	bTriggerPressed = false;
	if (IsValid(CurrentWeapon) && CurrentWeapon->WeaponKind == EWeaponKind::Throwable)
	{
		ReleaseThrow();
	}
}


void UCombatComponent::PlayOwnerThirdPersonFireMontage()
{
	if (!IsValid(WeaponData) || !IsValid(CurrentWeapon)) return;
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UPlayerInterface>()) return;

	UAnimMontage* Montage3P = nullptr;
	const FGameplayTag PoseTag = ResolveThirdPersonPoseTag(WeaponData, CurrentWeapon);
	if (const FMontageData* Montage3PData = WeaponData->ThirdPersonMontages.Find(PoseTag))
	{
		Montage3P = Montage3PData->ResolveFireMontage();
	}
	if (!IsValid(Montage3P) && PoseTag != ResolveAnimTag(CurrentWeapon))
	{
		if (const FMontageData* OwnData = WeaponData->ThirdPersonMontages.Find(ResolveAnimTag(CurrentWeapon)))
		{
			Montage3P = OwnData->ResolveFireMontage();
		}
	}
	USkeletalMeshComponent* Mesh3P = IPlayerInterface::Execute_GetMesh3P(Owner);
	UAnimInstance* AnimInstance3P = IsValid(Mesh3P) ? Mesh3P->GetAnimInstance() : nullptr;
	if (IsValid(Montage3P) && IsValid(AnimInstance3P))
	{
		AnimInstance3P->Montage_Play(Montage3P);
	}
 if(const auto* Shooter=Cast<AShooterCharacter>(Owner);Shooter&&Shooter->IsSingleBodyPilot())
  if(const auto* W=WeaponData->WeaponMontages.Find(ResolveAnimTag(CurrentWeapon)))
   if(auto* Fire=W->ResolveFireMontage())AWeapon::PlayWeaponMontageOrSequence(CurrentWeapon->GetMesh3P(),Fire);

}

void UCombatComponent::Local_FireWeapon()
{
	if (StopFireIfWeaponBlocked()) return;
	if (!IsValid(WeaponData) || !IsValid(CurrentWeapon))
	{
		return;
	}

	if (CurrentWeapon->WeaponKind == EWeaponKind::Throwable) return;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;

	bool bPlayedArmsFire = false;
	if (const FMontageData* MontageData = WeaponData->FirstPersonMontages.Find(ResolveHoldPoseTag(CurrentWeapon)))
	{
		UAnimMontage* Montage1P = MontageData->ResolveFireMontage();
		USkeletalMeshComponent* Mesh1P = IPlayerInterface::Execute_GetMesh1P(GetOwner());
		UAnimInstance* AnimInstance1P = IsValid(Mesh1P) ? Mesh1P->GetAnimInstance() : nullptr;
		if (IsValid(Montage1P) && IsValid(AnimInstance1P))
		{
			AnimInstance1P->Montage_Play(Montage1P);
			bPlayedArmsFire = true;
		}
	}
	// Sequence fallback (PSR bolt): dual-play arms + gun without mid-bolt reattach.
	if (!bPlayedArmsFire)
	{
		UAnimSequence* ArmsFire = (bAiming && IsValid(CurrentWeapon->ArmsFireAimSequence))
			? CurrentWeapon->ArmsFireAimSequence.Get()
			: CurrentWeapon->ArmsFireSequence.Get();
		if (IsValid(ArmsFire))
		{
			USkeletalMeshComponent* Mesh1P = IPlayerInterface::Execute_GetMesh1P(GetOwner());
			UAnimInstance* AnimInstance1P = IsValid(Mesh1P) ? Mesh1P->GetAnimInstance() : nullptr;
			if (AWeapon::PlaySequenceAsDynamicMontage(AnimInstance1P, ArmsFire))
			{
				bPlayedArmsFire = true;
			}
		}
	}

	// 枪身开火（拉栓/抛壳）只播在自己的 Mesh1P。蒙太奇槽必须和枪的 AnimBP 一致。
	bool bPlayedWeaponFire = false;
	if (CurrentWeapon->WeaponKind != EWeaponKind::Melee)
	{
		if (const FMontageData* WeaponMontageData = WeaponData->WeaponMontages.Find(ResolveAnimTag(CurrentWeapon)))
		{
			if (UAnimMontage* WeaponFire = WeaponMontageData->ResolveFireMontage())
			{
				if (USkeletalMeshComponent* WeaponMesh = CurrentWeapon->GetMesh1P())
				{
					if (UAnimInstance* WeaponAnim = WeaponMesh->GetAnimInstance())
					{
						WeaponAnim->Montage_Play(WeaponFire);
						bPlayedWeaponFire = true;
					}
					else
					{
						AWeapon::PlayWeaponMontageOrSequence(WeaponMesh, WeaponFire);
						bPlayedWeaponFire = true;
					}
				}
			}
		}
		if (!bPlayedWeaponFire && IsValid(CurrentWeapon->WeaponFireSequence))
		{
			AWeapon::PlayWeaponSequence(CurrentWeapon->GetMesh1P(), CurrentWeapon->WeaponFireSequence);
		}
	}

	// Owner Multicast skips 3P fire — always play 3P locally (melee + guns) so ToggleThirdPerson / body view sees it.
	PlayOwnerThirdPersonFireMontage();

	TArray<FHitResult> Hits;
	if (CurrentWeapon->WeaponKind == EWeaponKind::Melee)
	{
		FHitResult Hit;
		CurrentWeapon->WeaponMeleeHit(Hit);
		Hits.Add(Hit);
	}
	else
	{
		const int32 Pellets = FMath::Clamp(CurrentWeapon->PelletsPerShot, 1, 32);
		Hits.Reserve(Pellets);
		for (int32 i = 0; i < Pellets; ++i)
		{
			FHitResult Hit;
			CurrentWeapon->WeaponTrace(Hit, TraceLength);
			Hits.Add(Hit);
		}
	}

	if (Hits.Num() > 0)
	{
		// 第一颗走原来的 FireEffect（枪口火光/声音/弹孔）并扣 1 发；其余弹丸不重复播枪声。
		const FHitResult& Hit = Hits[0];
		EPhysicalSurface ImpactSurfaceType = Hit.PhysMaterial.IsValid(false) ? Hit.PhysMaterial->SurfaceType.GetValue() : SurfaceType2;
		CurrentWeapon->Local_Fire(Hit.ImpactPoint, Hit.ImpactNormal, ImpactSurfaceType, true, true);
		if (IsValid(Hit.GetActor()) && (Cast<AShooterCharacter>(Hit.GetActor()) || Cast<AHostileAICharacter>(Hit.GetActor()) || Cast<ATargetDummy>(Hit.GetActor())))
		{
			AShooterCharacter::SpawnBloodImpactFX(this, Hit.ImpactPoint, Hit.ImpactNormal, Hit.GetActor());
		}
	}
	if (Hits.Num() > 1)
	{
		// 每颗弹丸都留弹孔 + 命中特效
		CurrentWeapon->SpawnPelletImpacts(Hits);
		for (int32 PelletIdx = 1; PelletIdx < Hits.Num(); ++PelletIdx)
		{
			if (!IsValid(Hits[PelletIdx].GetActor())) continue;
			if (Cast<AShooterCharacter>(Hits[PelletIdx].GetActor()) || Cast<AHostileAICharacter>(Hits[PelletIdx].GetActor()) || Cast<ATargetDummy>(Hits[PelletIdx].GetActor()))
			{
				AShooterCharacter::SpawnBloodImpactFX(this, Hits[PelletIdx].ImpactPoint, Hits[PelletIdx].ImpactNormal, Hits[PelletIdx].GetActor());
			}
		}
	}
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

	Server_FireWeapon(Hits);
}

void UCombatComponent::OnRep_CurrentReserveAmmo()
{
	if (IsValid(CurrentWeapon))
	{
		OnCurrentReserveAmmoChanged.Broadcast(CurrentReserveAmmo, CurrentWeapon->Ammo, CurrentWeapon->WeaponIcon);
	}
}

void UCombatComponent::SyncSpareMagRoundsForHud()
{
	if (GetOwnerRole() != ROLE_Authority) return;
	SpareMagRounds.Reset();
	if (!IsValid(CurrentWeapon) || !CurrentWeapon->UsesAmmo()) return;
	EnsureMagQueueForWeapon(CurrentWeapon);
	if (const TArray<int32>* Q = MagQueues.Find(CurrentWeapon->WeaponType))
	{
		SpareMagRounds = *Q;
	}
}

void UCombatComponent::GetHudMagazineFills(TArray<float>& OutFills) const
{
	OutFills.Reset();
	if (!IsValid(CurrentWeapon) || !CurrentWeapon->UsesAmmo()) return;
	const float Cap = static_cast<float>(FMath::Max(1, CurrentWeapon->MagCapacity));
	OutFills.Add(FMath::Clamp(static_cast<float>(CurrentWeapon->Ammo) / Cap, 0.f, 1.f));

	if (SpareMagRounds.Num() > 0)
	{
		for (int32 Rounds : SpareMagRounds)
		{
			OutFills.Add(FMath::Clamp(static_cast<float>(Rounds) / Cap, 0.f, 1.f));
		}
		return;
	}

	if (GetOwnerRole() == ROLE_Authority)
	{
		if (const TArray<int32>* Q = MagQueues.Find(CurrentWeapon->WeaponType))
		{
			for (int32 Rounds : *Q)
			{
				OutFills.Add(FMath::Clamp(static_cast<float>(Rounds) / Cap, 0.f, 1.f));
			}
			return;
		}
	}

	for (int32 i = 0; i < FMath::Max(0, CurrentReserveAmmo); ++i)
	{
		OutFills.Add(1.f);
	}
}

void UCombatComponent::OnRep_SpareMagRounds()
{
 OnInspectionHudChanged.Broadcast();
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
	if (StopFireIfWeaponBlocked()) return;
	if (!IsValid(CurrentWeapon))
	{
		return;
	}

	// Tactical: no auto-reload. Empty mag just stops firing; player must press R.
	if (CurrentWeapon->WeaponKind != EWeaponKind::Throwable
		&& CurrentWeapon->UsesAmmo() && CurrentWeapon->Ammo == 0)
	{
		BurstShotsRemaining = 0;
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


// [RealisticRifle] Server-only wall penetration / ricochet. One extra segment per bullet, rifles+pistols only.
namespace FPSBallistics
{
	static void BuildSecondaryHits(UWorld* World, AActor* Owner, AWeapon* W, const FHitResult& In, TArray<FHitResult>& OutHits, TArray<float>& OutScale)
	{
		if (!World || !IsValid(W) || W->PelletsPerShot > 1) return;
		if (W->WeaponKind != EWeaponKind::LongGun && W->WeaponKind != EWeaponKind::Pistol) return;
		AActor* HitActor = In.GetActor();
		if (!In.bBlockingHit || !IsValid(HitActor) || HitActor->Implements<UPlayerInterface>()) return;
		if (Cast<ATargetDummy>(HitActor)) return;

		FVector Dir = (In.TraceEnd - In.TraceStart).GetSafeNormal();
		if (Dir.IsNearlyZero())
		{
			const APawn* P = Cast<APawn>(Owner);
			if (!P) return;
			Dir = (FVector(In.ImpactPoint) - P->GetPawnViewLocation()).GetSafeNormal();
			if (Dir.IsNearlyZero()) return;
		}
		const FVector N = In.ImpactNormal.GetSafeNormal();
		const FVector Impact = In.ImpactPoint;

		const FString Cls = W->GetClass()->GetName();
		const bool bAK = Cls.Contains(TEXT("_AK"));
		float MaxPen = W->WeaponKind == EWeaponKind::Pistol ? 8.f : (bAK ? 30.f : 22.f);   // cm through wood-equivalent
		const float Keep = bAK ? 0.65f : 0.55f;

		const EPhysicalSurface Surf = In.PhysMaterial.IsValid() ? In.PhysMaterial->SurfaceType.GetValue() : SurfaceType_Default;
		bool bCanRicochet = true;
		switch (Surf)
		{
		case SurfaceType2: MaxPen *= 0.35f; break;                       // Concrete: only thin partitions
		case SurfaceType3: MaxPen *= 2.0f; bCanRicochet = false; break;  // Glass
		case SurfaceType4: MaxPen *= 1.3f; bCanRicochet = false; break;  // Wood
		case SurfaceType5: MaxPen *= 0.4f; break;                        // Metal
		default:           MaxPen *= 0.6f; break;                        // unassigned = treat like drywall/brick
		}

		FCollisionQueryParams Q(SCENE_QUERY_STAT(FPSBallistics), false);
		Q.bReturnPhysicalMaterial = true;
		Q.AddIgnoredActor(Owner);
		Q.AddIgnoredActor(W);
		FCollisionResponseParams R;
		R.CollisionResponse.SetAllChannels(ECR_Ignore);
		R.CollisionResponse.SetResponse(ECC_Pawn, ECR_Block);
		R.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);
		R.CollisionResponse.SetResponse(ECC_WorldDynamic, ECR_Block);
		R.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Block);
		const FCollisionShape Shape = FCollisionShape::MakeSphere(FMath::Max(1.f, W->TraceRadius));

		auto Shoot = [&](const FVector& S, const FVector& D, float Len, float Scale)
		{
			FHitResult H;
			const FVector E = S + D * Len;
			if (!World->SweepSingleByChannel(H, S, E, FQuat::Identity, FPSTraceChannels::ECC_Weapon, Shape, Q, R))
			{
				return;
			}
			OutHits.Add(H);
			OutScale.Add(Scale);
		};

		// Ricochet: grazing hit (< ~15 deg from surface) glances off.
		const float CosInc = FMath::Abs(FVector::DotProduct(Dir, N));
		if (bCanRicochet && CosInc < 0.26f)
		{
			FVector Out = Dir.MirrorByVector(N);
			Out = FMath::VRandCone(Out, FMath::DegreesToRadians(4.f)).GetSafeNormal();
			if (FVector::DotProduct(Out, N) < 0.02f) Out = (Out + N * 0.05f).GetSafeNormal();
			Shoot(Impact + N * 2.f, Out, 4000.f, 0.35f);
			return;
		}

		// Penetration: look for the exit face by tracing back from MaxPen deep.
		const FVector Probe = Impact + Dir * MaxPen;
		FHitResult Back;
		FCollisionQueryParams BQ(SCENE_QUERY_STAT(FPSBallisticsBack), false);
		BQ.AddIgnoredActor(Owner);
		BQ.AddIgnoredActor(W);
		if (!World->LineTraceSingleByChannel(Back, Probe, Impact - Dir * 0.5f, FPSTraceChannels::ECC_Weapon, BQ, R)) return;
		if (Back.bStartPenetrating || Back.GetActor() != HitActor) return;   // still inside / thicker than MaxPen
		const float Thickness = FVector::Dist(Impact, FVector(Back.ImpactPoint));
		if (Thickness >= MaxPen) return;
		const float Scale = Keep * (1.f - 0.6f * Thickness / MaxPen);
		Q.AddIgnoredComponent(In.GetComponent());
		Shoot(FVector(Back.ImpactPoint) + Dir * 1.f, Dir, 8000.f, Scale);
	}
}

void UCombatComponent::Server_FireWeapon_Implementation(const TArray<FHitResult>& Hits)
{
 if(GetOwner())
 {
  if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsBusy())return;
  if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>())Door->StopGesture();
 }
 if(GetOwner())if(auto* Action=GetOwner()->FindComponentByClass<URestraintActionComponent>())Action->CancelForCombatAction();
	if (StopFireIfWeaponBlocked()) return;
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

	const int32 MaxPellets = FMath::Clamp(CurrentWeapon->PelletsPerShot, 1, 32);
	const int32 ApplyCount = FMath::Clamp(Hits.Num(), 0, MaxPellets);
	TArray<FHitResult> DmgHits;
	TArray<float> DmgScale;
	for (int32 i = 0; i < ApplyCount; ++i) { DmgHits.Add(Hits[i]); DmgScale.Add(1.f); }
	const int32 PrimaryCount = DmgHits.Num();
	for (int32 i = 0; i < PrimaryCount; ++i) FPSBallistics::BuildSecondaryHits(GetWorld(), GetOwner(), CurrentWeapon, Hits[i], DmgHits, DmgScale);
	for (int32 i = 0; i < DmgHits.Num(); ++i)
	{
		const FHitResult& Hit = DmgHits[i];
		if (IsValid(Hit.GetActor()) && Hit.GetActor()->Implements<UPlayerInterface>())
		{
			FName Bone = ResolveDamageBone(Hit);
			if (const ATargetDummy* Dummy = Cast<ATargetDummy>(Hit.GetActor())) Bone = Dummy->ResolveHitBone(Hit, Bone);
			float BaseDmg = CurrentWeapon->Damage;
			float HeadMul = CurrentWeapon->HeadshotMultiplier;
			float LimbMul = CurrentWeapon->LimbMultiplier;
			// Short-TTK migrate: C++ default used to be 15/2.0; BP copies often still carry that.
			if (CurrentWeapon->PelletsPerShot <= 1 && BaseDmg <= 20.f) BaseDmg = 40.f;
			if (HeadMul <= 2.01f) HeadMul = 2.5f;
			float Damage = ScaleDamageForBone(BaseDmg, Bone, HeadMul, LimbMul);
			AShooterGameModeBase* DamageGM = GetWorld() ? GetWorld()->GetAuthGameMode<AShooterGameModeBase>() : nullptr;
			if (DamageGM) DamageGM->SetPendingDamageIcon(CurrentWeapon->WeaponIcon);
			AShooterCharacter* ShooterSelf = Cast<AShooterCharacter>(GetOwner());
			const EHitZone Zone = ResolveHitZone(Bone);
			if (ShooterSelf) ShooterSelf->bPendingHeadshotHit = Zone == EHitZone::Head;
			const bool bPistolOrShotgun = CurrentWeapon->WeaponKind == EWeaponKind::Pistol
				|| CurrentWeapon->PelletsPerShot > 1
				|| CurrentWeapon->WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_ShotGun);
			const bool bMelee = CurrentWeapon->WeaponKind == EWeaponKind::Melee;
			if (AShooterCharacter* Victim = Cast<AShooterCharacter>(Hit.GetActor()))
			{
				HudBodyMarks::AddHit(Victim, Zone, HudBodyMarks::SideFromBone(Bone));
			}
			UE_LOG(LogTemp, Log, TEXT("Hit %s bone %s zone %s damage %.1f pellet %d"),
				*GetNameSafe(Hit.GetActor()),
				*Bone.ToString(),
				*UEnum::GetValueAsString(Zone),
				Damage,
				i);
			Damage *= DmgScale[i];   // [RealisticRifle] penetration / ricochet falloff
			if (HitZoneCombat::IsHitZoneVictim(Hit.GetActor()))
			{
				float Weight = HitZoneCombat::ResolveHitWeight(bMelee, bPistolOrShotgun, CurrentWeapon->PelletsPerShot, DmgScale[i]);
				if (UHitZoneVitalsComponent* VictimVitals = UHitZoneVitalsComponent::Find(Hit.GetActor()))
				{
					if (bMelee) Weight = VictimVitals->MeleeHitWeight * DmgScale[i];
					else if (CurrentWeapon->PelletsPerShot > 1) Weight = VictimVitals->ShotgunPelletWeight * DmgScale[i];
					else if (bPistolOrShotgun) Weight = VictimVitals->PistolHitWeight * DmgScale[i];
					else Weight = VictimVitals->RifleHitWeight * DmgScale[i];
				}
				HitZoneCombat::ApplyBallisticToActor(Hit.GetActor(), GetOwner(), Zone, bPistolOrShotgun, Weight, true);
			}
			else
			{
				IPlayerInterface::Execute_DoDamage(Hit.GetActor(), Damage, GetOwner());
			}
			if (ShooterSelf) ShooterSelf->bPendingHeadshotHit = false;
			if (DamageGM) DamageGM->ClearPendingDamageIcon();
		}
	}

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	// 本机操控的角色已在 Local_Fire 里扣过弹（房主 / 单机都一样），服务器不再扣
	const bool bLocallyPredicted = IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	if (!bLocallyPredicted)
	{
		CurrentWeapon->Auth_Fire();
	}

	TArray<FHitResult> FxHits;
	FxHits.Append(Hits.GetData(), FMath::Min(Hits.Num(), MaxPellets));
	for (int32 i = PrimaryCount; i < DmgHits.Num(); ++i) FxHits.Add(DmgHits[i]);   // exit holes / ricochet marks for remote viewers
	// CQB AI hear gunshot (authority).
	if (AActor* OwnerActor = GetOwner())
	{
		const AINoiseEvent::EKind ShotKind = AINoiseEvent::IsMuzzleSuppressed(CurrentWeapon)
			? AINoiseEvent::EKind::SuppressedGunshot
			: AINoiseEvent::EKind::Gunshot;
		AINoiseEvent::Report(GetWorld(), OwnerActor->GetActorLocation(), 1.f, OwnerActor, ShotKind);
		if (UHitZoneVitalsComponent* OwnerVitals = UHitZoneVitalsComponent::Find(OwnerActor))
		{
			OwnerVitals->NoteCombatEvent();
		}
	}

	Multicast_FireWeapon(FxHits, CurrentWeapon->Ammo);
}

void UCombatComponent::Multicast_FireWeapon_Implementation(const TArray<FHitResult>& Hits, int32 AuthAmmo)
{
	if (!IsValid(CurrentWeapon)) return;

	// BoneName is on the replicated hit, so the player who got shot can color that limb.
	for (const FHitResult& OneHit : Hits)
	{
		AShooterCharacter* Victim = Cast<AShooterCharacter>(OneHit.GetActor());
		if (!IsValid(Victim) || !Victim->IsLocallyControlled()) continue;
		const FName Bone = ResolveDamageBone(OneHit);
		HudBodyMarks::AddHit(Victim, ResolveHitZone(Bone), HudBodyMarks::SideFromBone(Bone));
	}

	const FHitResult Hit = Hits.Num() > 0 ? Hits[0] : FHitResult();

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
	if (IsValid(Hit.GetActor()) && (Cast<AShooterCharacter>(Hit.GetActor()) || Cast<ATargetDummy>(Hit.GetActor())))
	{
		AShooterCharacter::SpawnBloodImpactFX(this, Hit.ImpactPoint, Hit.ImpactNormal, Hit.GetActor());
	}
	if (Hits.Num() > 1)
	{
		CurrentWeapon->SpawnPelletImpacts(Hits);
		for (int32 PelletIdx = 1; PelletIdx < Hits.Num(); ++PelletIdx)
		{
			if (!IsValid(Hits[PelletIdx].GetActor())) continue;
			if (Cast<AShooterCharacter>(Hits[PelletIdx].GetActor()) || Cast<AHostileAICharacter>(Hits[PelletIdx].GetActor()) || Cast<ATargetDummy>(Hits[PelletIdx].GetActor()))
			{
				AShooterCharacter::SpawnBloodImpactFX(this, Hits[PelletIdx].ImpactPoint, Hits[PelletIdx].ImpactNormal, Hits[PelletIdx].GetActor());
			}
		}
	}

 if(const auto* Shooter=Cast<AShooterCharacter>(GetOwner());Shooter&&Shooter->IsSingleBodyPilot())PlayOwnerThirdPersonFireMontage();
 else
	if (const FMontageData* MontageData = WeaponData->ThirdPersonMontages.Find(ResolveAnimTag(CurrentWeapon)))
	{
		UAnimMontage* Montage3P = MontageData->ResolveFireMontage();
		USkeletalMeshComponent* Mesh3P = IPlayerInterface::Execute_GetMesh3P(OwningPawn);
		UAnimInstance* AnimInstance3P = IsValid(Mesh3P) ? Mesh3P->GetAnimInstance() : nullptr;
		if (IsValid(Montage3P) && IsValid(AnimInstance3P))
		{
			AnimInstance3P->Montage_Play(Montage3P);
		}
	}
	if (IsViewedFirstPersonLocally()) PlaySpectated1PFire();
}


bool UCombatComponent::IsReloading() const
{
	return IsValid(CurrentWeapon) && CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading;
}


int32 UCombatComponent::GetSpareMagCount() const
{
	if (!IsValid(CurrentWeapon)) return 0;
	if (const TArray<int32>* Q = MagQueues.Find(CurrentWeapon->WeaponType))
	{
		return Q->Num();
	}
	return FMath::Max(0, CurrentReserveAmmo);
}

void UCombatComponent::EnsureMagQueueForWeapon(AWeapon* Weapon)
{
	if (!IsValid(Weapon) || !Weapon->UsesAmmo()) return;
	if (MagQueues.Contains(Weapon->WeaponType)) return;

	TArray<int32> Queue;
	const int32 Cap = FMath::Max(1, Weapon->MagCapacity);
	int32 Left = FMath::Max(0, Weapon->StartingCarriedAmmo);
	// Convert legacy "spare rounds pool" into FIFO mags (full first, leftover last).
	while (Left > 0)
	{
		const int32 Take = FMath::Min(Cap, Left);
		Queue.Add(Take);
		Left -= Take;
	}
	MagQueues.Add(Weapon->WeaponType, Queue);
	ReserveAmmo.Add(Weapon->WeaponType, Queue.Num());
}

bool UCombatComponent::ConsumeMagFromQueue(AWeapon* Weapon, bool bFastDrop)
{
	if (!IsValid(Weapon)) return false;
	EnsureMagQueueForWeapon(Weapon);
	TArray<int32>* Queue = MagQueues.Find(Weapon->WeaponType);
	if (!Queue || Queue->Num() <= 0) return false;

	if (!bFastDrop && Weapon->Ammo > 0)
	{
		Queue->Add(Weapon->Ammo); // keep partial mag at the back
	}
	// empty mag or fast-drop: discard current mag

	Weapon->Ammo = (*Queue)[0];
	Queue->RemoveAt(0);
	CurrentReserveAmmo = Queue->Num();
	ReserveAmmo.Add(Weapon->WeaponType, CurrentReserveAmmo);
	NotifyAmmoChanged();
	return true;
}

void UCombatComponent::PlayDryFireClick()
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (Now - LastDryFireTime < 0.2) return;
	LastDryFireTime = Now;

	static TSoftObjectPtr<USoundBase> DryCue(
		FSoftObjectPath(TEXT("/Game/cgARMod/Weapons/Audio/SC_DryShot1_Cue.SC_DryShot1_Cue")));
	if (USoundBase* Sound = DryCue.LoadSynchronous())
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
}

void UCombatComponent::Initiate_ReloadWeapon(bool bFastDrop)
{
 if(GetOwner())
 {
  if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsBusy())return;
  if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>())Door->StopGesture();
 }
 if(GetOwner())if(auto* Action=GetOwner()->FindComponentByClass<URestraintActionComponent>())Action->CancelForCombatAction();
	CancelInspect();
	BurstShotsRemaining = 0;
	if (!IsValid(CurrentWeapon)) return;
	if (!CurrentWeapon->UsesAmmo()) return;
	if (CurrentWeapon->WeaponKind == EWeaponKind::Throwable) return;
	if (bThrowInProgress) return;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Cycling) return;
	if (CurrentWeapon->Ammo >= CurrentWeapon->MagCapacity) return;
	// CurrentReserveAmmo is replicated mag-count; MagQueues is server-authoritative.
	if (GetOwnerRole() == ROLE_Authority)
	{
		EnsureMagQueueForWeapon(CurrentWeapon);
		if (const TArray<int32>* Q = MagQueues.Find(CurrentWeapon->WeaponType))
		{
			CurrentReserveAmmo = Q->Num();
		}
	}
	if (CurrentReserveAmmo <= 0) return; // no spare mags
	bPendingFastReloadDrop = bFastDrop;

	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(OwnerPawn->GetController()))
		{
			PC->StopSprinting();
		}
	}
	// single R = Empty (keep mag); double R = Tactical (drop mag); hold R = MagCheck
	const EReloadType ReloadType = bFastDrop ? EReloadType::Tactical : EReloadType::Empty;

	UAnimMontage* ReloadMontage = nullptr;
	UAnimInstance* AnimInstance = nullptr;
	const bool bHasMontage = GetReloadMontage(true, ReloadType, ReloadMontage, AnimInstance);
	const bool bHasSeq = IsValid(CurrentWeapon->ArmsReloadEmptySequence) || IsValid(CurrentWeapon->ArmsReloadTacticalSequence);
	if (!bHasMontage && !bHasSeq) return;

	StopAimForWeaponSwitch();
	ClearAdsPendingAfterReload();   // 换弹开始：必须换弹中再按才排队换弹后开镜
	bLocalReloadNotified = false;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(SprintFireBufferTimer);
	CurrentWeapon->WeaponStatus = EWeaponStatus::Reloading;
	CurrentWeapon->bHasReloadedOnce = true;
	ResetRecoilPattern();
	Server_ReloadWeapon(ReloadType, bFastDrop);
	bool bPlayed = false;
	// KIN soft-wired ArmsReload* sequences must win over shared RAR WeaponData montages,
	// otherwise Mesh1P gets a RAR WEP montage (wrong skeleton) and Mag stays glued.
	if (bHasSeq)
	{
		bPlayed = PlayReloadSequences(true, ReloadType);
	}
	if (!bPlayed && bHasMontage)
	{
		bPlayed = PlayReloadMontage(true, ReloadType);
	}
	if (!bPlayed)
	{
		CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
		Server_EndReload(false);
	}
	else
	{
		// Multicast_Reload skips owner — drive Mesh3P reload for ToggleThirdPerson / body view.
		bool bPlayed3P = false;
		if (bHasSeq)
		{
			bPlayed3P = PlayReloadSequences(false, ReloadType);
		}
		if (!bPlayed3P)
		{
			PlayReloadMontage(false, ReloadType);
		}
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

	// Mag-queue model: swap whole magazines instead of topping up from a round pool.
	return ConsumeMagFromQueue(CurrentWeapon, bPendingFastReloadDrop);
}

EReloadType UCombatComponent::ChooseReloadType() const
{
	if (!IsValid(CurrentWeapon)) return EReloadType::Tactical;
	// Player short/long overrides this in Initiate_ReloadWeapon.
	// Bots / Auth still pick by chamber empty vs not. FirstReload retired.
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
	const FGameplayTag MontageTag = bFirstPerson
		? ResolveAnimTag(CurrentWeapon)
		: ResolveThirdPersonPoseTag(WeaponData, CurrentWeapon);
	const FMontageData* MontageData = ViewMontages.Find(MontageTag);

	const UAttachmentData* Mag = CurrentWeapon->GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Magazine);
	const EGripHoldStyle GripStyle = CurrentWeapon->GetGripHoldStyle();

	// FirstReload 已废弃：若仍传入 First，按弹匣是否空退回 Empty/Tactical。
	EReloadType Resolved = ReloadType;
	if (Resolved == EReloadType::First)
	{
		Resolved = CurrentWeapon->Ammo <= 0 ? EReloadType::Empty : EReloadType::Tactical;
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

bool UCombatComponent::PlayReloadSequences(bool bFirstPerson, EReloadType ReloadType)
{
	// ArmsReload* sequences are first-person arm clips. Playing them on Mesh3P
	// stomps the body idle with the wrong skeleton.
	if (!bFirstPerson) return false;
	if (!IsValid(CurrentWeapon)) return false;
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UPlayerInterface>()) return false;

	EReloadType Resolved = ReloadType;
	if (Resolved == EReloadType::First)
	{
		Resolved = CurrentWeapon->Ammo <= 0 ? EReloadType::Empty : EReloadType::Tactical;
	}

	UAnimSequence* ArmsSeq = (Resolved == EReloadType::Empty)
		? CurrentWeapon->ArmsReloadEmptySequence.Get()
		: CurrentWeapon->ArmsReloadTacticalSequence.Get();
	if (!IsValid(ArmsSeq))
	{
		ArmsSeq = IsValid(CurrentWeapon->ArmsReloadEmptySequence)
			? CurrentWeapon->ArmsReloadEmptySequence.Get()
			: CurrentWeapon->ArmsReloadTacticalSequence.Get();
	}
	if (!IsValid(ArmsSeq)) return false;

	USkeletalMeshComponent* Mesh = bFirstPerson
		? IPlayerInterface::Execute_GetMesh1P(Owner)
		: IPlayerInterface::Execute_GetMesh3P(Owner);
	UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
	UAnimMontage* Dyn = AWeapon::PlaySequenceAsDynamicMontage(AnimInstance, ArmsSeq);
	if (!IsValid(Dyn) || !IsValid(AnimInstance)) return false;

	if (bFirstPerson)
	{
		UAnimSequence* WeaponSeq = (Resolved == EReloadType::Empty)
			? CurrentWeapon->WeaponReloadEmptySequence.Get()
			: CurrentWeapon->WeaponReloadTacticalSequence.Get();
		if (!IsValid(WeaponSeq))
		{
			WeaponSeq = IsValid(CurrentWeapon->WeaponReloadEmptySequence)
				? CurrentWeapon->WeaponReloadEmptySequence.Get()
				: CurrentWeapon->WeaponReloadTacticalSequence.Get();
		}
		// Body Mag socket/bone motion is owned by PlayMagazineReload (Empty + Tactical).
		CurrentWeapon->PlayMagazineReload(Resolved);
	}

	FOnMontageEnded EndDelegate;
	if (bFirstPerson)
	{
		EndDelegate.BindUObject(this, &UCombatComponent::OnReloadMontageEnded);
	}
	else
	{
		EndDelegate.BindUObject(this, &UCombatComponent::OnObservedReloadMontageEnded);
	}
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Dyn);
	return true;
}

bool UCombatComponent::PlayReloadMontage(bool bFirstPerson, EReloadType ReloadType)
{
	UAnimMontage* ReloadMontage = nullptr;
	UAnimInstance* AnimInstance = nullptr;
	if (!GetReloadMontage(bFirstPerson, ReloadType, ReloadMontage, AnimInstance)) return false;

	float ReloadRate = 1.f;
	if (const AShooterCharacter* ShooterOwner = Cast<AShooterCharacter>(GetOwner()))
	{
		ReloadRate = ShooterOwner->GetArmInjuryAnimPlayRate();
	}
	const float PlayedLength = AnimInstance->Montage_Play(ReloadMontage, ReloadRate);
	if (PlayedLength <= 0.f) return false;

	// The gun reload moves the magazine on the weapon mesh. Play it only on the
	// owning first-person mesh; the third-person gun leaves the magazine floating.
	if (bFirstPerson || (Cast<AShooterCharacter>(GetOwner()) && Cast<AShooterCharacter>(GetOwner())->IsSingleBodyPilot()))
	{
		if (const FMontageData* WeaponMontageData = WeaponData->WeaponMontages.Find(ResolveAnimTag(CurrentWeapon)))
		{
			// 枪身也按同一个种类挑，保证手和弹匣是成对的那两条动画
			EReloadType WeaponReloadType = ReloadType;
			if (WeaponReloadType == EReloadType::First && !IsValid(WeaponMontageData->FirstReloadMontage))
			{
				WeaponReloadType = CurrentWeapon->Ammo <= 0 ? EReloadType::Empty : EReloadType::Tactical;
			}
			const EGripHoldStyle WeaponGripStyle = IsValid(CurrentWeapon) ? CurrentWeapon->GetGripHoldStyle() : EGripHoldStyle::Cradle;
			UAnimMontage* WeaponReloadMontage = WeaponMontageData->ResolveReloadMontage(WeaponReloadType, WeaponGripStyle);
			USkeletalMeshComponent* WeaponMesh = bFirstPerson?CurrentWeapon->GetMesh1P():CurrentWeapon->GetMesh3P();
			UAnimInstance* WeaponAnim = IsValid(WeaponMesh) ? WeaponMesh->GetAnimInstance() : nullptr;
			if (IsValid(WeaponReloadMontage))
			{
				AWeapon::PlayWeaponMontageOrSequence(WeaponMesh, WeaponReloadMontage);
			}
		}
		EReloadType MagReloadType = ReloadType;
		if (MagReloadType == EReloadType::First)
		{
			MagReloadType = CurrentWeapon->Ammo <= 0 ? EReloadType::Empty : EReloadType::Tactical;
		}
		if(bFirstPerson)CurrentWeapon->PlayMagazineReload(MagReloadType);
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
		CurrentWeapon->FinishMagazineReloadVisuals();
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
		ClearAdsPendingAfterReload();
		Server_EndReload(false);
		return;
	}

	CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
	ClearAdsPendingAfterReload();
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

	TryConsumeAdsAfterReload();

	if (bTriggerPressed && CurrentWeapon->Ammo > 0)
	{
		Local_FireWeapon();
	}
}

void UCombatComponent::Server_ReloadWeapon_Implementation(EReloadType ReloadType, bool bFastDrop)
{
 if(GetOwner())
 {
  if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsBusy())return;
  if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>())Door->StopGesture();
 }
 if(GetOwner())if(auto* Action=GetOwner()->FindComponentByClass<URestraintActionComponent>())Action->CancelForCombatAction();
	if (!IsValid(CurrentWeapon)) return;

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	// 本机操控的角色（房主 / 单机）在 Initiate_ReloadWeapon 里已经做过检查并设成 Reloading
	const bool bLocallyPredicted = IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	bPendingFastReloadDrop = bFastDrop;
	if (!bLocallyPredicted)
	{
		if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;
		if (CurrentWeapon->WeaponStatus == EWeaponStatus::Cycling) return;
		if (CurrentWeapon->Ammo >= CurrentWeapon->MagCapacity) return;
		EnsureMagQueueForWeapon(CurrentWeapon);
		{
			const TArray<int32>* Q = MagQueues.Find(CurrentWeapon->WeaponType);
			if (!Q || Q->Num() <= 0) return;
		}
		CurrentWeapon->WeaponStatus = EWeaponStatus::Reloading;
		CurrentWeapon->bHasReloadedOnce = true;
	}

	Multicast_ReloadWeapon(ReloadType);
}

void UCombatComponent::Server_ReloadNotify_Implementation()
{
	Notify_ReloadWeapon();
}

bool UCombatComponent::CancelReload()
{
	if (!IsValid(CurrentWeapon) || CurrentWeapon->WeaponStatus != EWeaponStatus::Reloading) return false;
	ClearAdsBuffer();
	ClearAdsPendingAfterReload();
	const APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn) || !OwningPawn->IsLocallyControlled()) return false;
	// 客户端：弹匣已经插进去（通知已发服务器），等服务器回传子弹即可，不再打断
	if (GetOwnerRole() != ROLE_Authority && bLocalReloadNotified) return false;

	StopReloadMontages(CurrentWeapon);   // 停 1P/3P/枪身换弹动画，状态回 Idle，不给子弹
	if (GetOwnerRole() == ROLE_Authority)
	{
		Multicast_CancelReload();
	}
	else
	{
		Server_CancelReload();
	}
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(OwningPawn->GetController()))
	{
		PC->RefreshMovementSpeed();
	}
	return true;
}

bool UCombatComponent::StopFireIfWeaponBlocked()
{
	const AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner());
	if (!Shooter || !Shooter->IsWeaponFireBlocked()) return false;
	// Cancel queued automatic/burst/buffered shots; the trigger must be pressed again
	// after the gun is lowered. Run before animation, effects, ammo or server damage.
	bTriggerPressed = false;
	BurstShotsRemaining = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimer);
		World->GetTimerManager().ClearTimer(SprintFireBufferTimer);
	}
	return true;
}

void UCombatComponent::Auth_FireWeapon(const TArray<FHitResult>& Hits)
{
	if (GetOwnerRole() != ROLE_Authority) return;
	Server_FireWeapon_Implementation(Hits);
}

void UCombatComponent::Auth_ReloadWeapon(EReloadType ReloadType)
{
	if (GetOwnerRole() != ROLE_Authority) return;
	Server_ReloadWeapon_Implementation(ReloadType, false);
}

void UCombatComponent::Auth_CancelReload()
{
	if (GetOwnerRole() != ROLE_Authority) return;
	if (!IsValid(CurrentWeapon) || CurrentWeapon->WeaponStatus != EWeaponStatus::Reloading) return;
	ClearAdsBuffer();
	ClearAdsPendingAfterReload();
	StopReloadMontages(CurrentWeapon);
	Multicast_CancelReload();
}

void UCombatComponent::Server_CancelReload_Implementation()
{
	Auth_CancelReload();
}

void UCombatComponent::Multicast_CancelReload_Implementation()
{
	// 服务器和本机操控者已经停过了；其他人停掉看到的 3P 换弹
	if (GetOwnerRole() == ROLE_Authority) return;
	const APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (IsValid(OwningPawn) && OwningPawn->IsLocallyControlled()) return;
	if (IsValid(CurrentWeapon)) StopReloadMontages(CurrentWeapon);
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

	if (!PlayReloadMontage(false, ReloadType))
	{
		PlayReloadSequences(false, ReloadType);
	}
	if (IsViewedFirstPersonLocally()) PlaySpectated1PReload(ReloadType);
}

void UCombatComponent::Initiate_CycleZoom()
{
 if(GetOwner())
 {
  if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsBusy())return;
  if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>())Door->StopGesture();
 }
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
 if(GetOwner())
 {
  if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsBusy())return;
  if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>())Door->StopGesture();
 }
	if (!CanSwitchFireMode()) return;

	const EFireType NewFireType = CurrentWeapon->GetNextFireMode();
	BurstShotsRemaining = 0;

	// 本地立刻生效（FireTimerFinished 读的就是它），再让服务器同步给其他人
	CurrentWeapon->FireType = NewFireType;
	OnInspectionHudChanged.Broadcast();
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
 if(GetOwner())
 {
  if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsBusy())return;
  if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>())Door->StopGesture();
 }
 if(GetOwner())if(auto* Action=GetOwner()->FindComponentByClass<URestraintActionComponent>())Action->CancelForCombatAction();
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
	if (const FMontageData* MontageData = ViewMontages.Find(ResolveAnimTag(CurrentWeapon)))
	{
		if (UAnimMontage* PinPull = MontageData->ResolveEquipMontage())
		{
			return PinPull;
		}
	}
	if (const FMontageData* SharedData = WeaponData->WeaponMontages.Find(ResolveAnimTag(CurrentWeapon)))
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
		if (const FMontageData* WeaponMontageData = WeaponData->WeaponMontages.Find(ResolveAnimTag(CurrentWeapon)))
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
	const FMontageData* MontageData = ViewMontages.Find(ResolveAnimTag(CurrentWeapon));
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

	// 没有投掷蒙太奇时结束回调不会来，状态会一直占着，数字键切不回枪。
	const bool bThrowAnimPlaying = IsValid(ActiveThrowAnim.Get())
		&& IsValid(ActiveThrowMontage.Get())
		&& ActiveThrowAnim->Montage_IsPlaying(ActiveThrowMontage.Get());
	if (!bThrowAnimPlaying)
	{
		FinishThrow();
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
		// 击杀播报：投掷物没单独填图标就用投掷武器的图标
		if (!Projectile->KillFeedIcon) Projectile->KillFeedIcon = CurrentWeapon->WeaponIcon;
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
		if (const FMontageData* MontageData = WeaponData->FirstPersonMontages.Find(ResolveAnimTag(CurrentWeapon)))
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
 if(GetOwner())
 {
  if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsBusy())return;
  if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>())Door->StopGesture();
 }
 if(GetOwner())if(auto* Action=GetOwner()->FindComponentByClass<URestraintActionComponent>())Action->CancelForCombatAction();
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

void UCombatComponent::ClearAdsBuffer()
{
	bAdsInputBuffered = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AdsBufferTimer);
	}
}

void UCombatComponent::ClearAdsPendingAfterReload()
{
	bAdsPendingAfterReload = false;
}

void UCombatComponent::TryConsumeAdsAfterReload()
{
	if (!bAdsPendingAfterReload) return;
	bAdsPendingAfterReload = false;
	if (bAiming) return;
	if (IsReloading()) return;
	if (!IsValid(CurrentWeapon) || !CurrentWeapon->CanAimWeapon()) return;

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(OwnerPawn->GetController()))
		{
			if (PC->IsSprinting())
			{
				PC->StopSprinting();
			}
			const float Remain = PC->GetAdsSettleRemaining();
			if (Remain > KINDA_SMALL_NUMBER)
			{
				bAdsInputBuffered = true;
				GetWorld()->GetTimerManager().SetTimer(AdsBufferTimer, this, &ThisClass::OnAdsBufferElapsed, Remain, false);
				return;
			}
		}
	}
	ClearAdsBuffer();
	Local_Aim(true);
	Server_Aim(true);
}

void UCombatComponent::OnAdsBufferElapsed()
{
	if (!bAdsInputBuffered) return;
	bAdsInputBuffered = false;
	if (bAiming) return;
	if (IsReloading())
	{
		// 换弹中绝不自动开镜：改记到换弹结束后再开
		bAdsPendingAfterReload = true;
		return;
	}
	if (IsValid(CurrentWeapon) && !CurrentWeapon->CanAimWeapon()) return;

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(OwnerPawn->GetController()))
		{
			if (PC->IsSprinting())
			{
				PC->StopSprinting();
			}
			const float Remain = PC->GetAdsSettleRemaining();
			if (Remain > KINDA_SMALL_NUMBER)
			{
				// 又进入冲刺/后摇：继续缓冲
				bAdsInputBuffered = true;
				GetWorld()->GetTimerManager().SetTimer(AdsBufferTimer, this, &ThisClass::OnAdsBufferElapsed, Remain, false);
				return;
			}
		}
	}
	Local_Aim(true);
	Server_Aim(true);
}

void UCombatComponent::Initiate_Aim_Pressed()
{
 if(GetOwner())
 {
  if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsBusy())return;
  if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>())Door->StopGesture();
 }
	CancelInspect();
	if (IsValid(CurrentWeapon) && CurrentWeapon->WeaponKind == EWeaponKind::Throwable)
	{
		Initiate_Throw(false);
		return;
	}
	// 近战 / 炸弹不能开镜
	if (IsValid(CurrentWeapon) && !CurrentWeapon->CanAimWeapon())
	{
		ClearAdsBuffer();
		ClearAdsPendingAfterReload();
		if (bAiming)
		{
			Local_Aim(false);
			Server_Aim(false);
		}
		return;
	}

	// 换弹期间不能开镜：清掉冲刺后摇预输入，按键记到换弹结束后再开；再按取消
	if (IsReloading())
	{
		ClearAdsBuffer();
		if (bAiming)
		{
			Local_Aim(false);
			Server_Aim(false);
		}
		if (bAdsPendingAfterReload)
		{
			ClearAdsPendingAfterReload();
			return;
		}
		bAdsPendingAfterReload = true;
		return;
	}

	// 预输入等待中再按一次 = 取消预输入（等同取消开镜意图）
	if (bAdsInputBuffered && !bAiming)
	{
		ClearAdsBuffer();
		ClearAdsPendingAfterReload();
		return;
	}

	const bool bNextAim = !bAiming;
	if (!bNextAim)
	{
		ClearAdsBuffer();
		ClearAdsPendingAfterReload();
		Local_Aim(false);
		Server_Aim(false);
		return;
	}

	// 想开镜：冲刺中先停冲刺；若还在后摇则预输入，时间一到自动开（本身还有 AdsTime）
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(OwnerPawn->GetController()))
		{
			if (PC->IsSprinting())
			{
				PC->StopSprinting();
			}
			const float Remain = PC->GetAdsSettleRemaining();
			if (Remain > KINDA_SMALL_NUMBER)
			{
				bAdsInputBuffered = true;
				GetWorld()->GetTimerManager().SetTimer(AdsBufferTimer, this, &ThisClass::OnAdsBufferElapsed, Remain, false);
				return;
			}
		}
	}
	ClearAdsBuffer();
	Local_Aim(true);
	Server_Aim(true);
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
 if(GetOwner())if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsProbeActive()){Tools->DeployReleased();return;}
 if(GetOwner())
 {
  if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsBusy())return;
  if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>())Door->StopGesture();
 }
	CancelInspect();
	if (bThrowInProgress)
	{
		const bool bMontagePlaying = IsValid(ActiveThrowAnim.Get())
			&& IsValid(ActiveThrowMontage.Get())
			&& ActiveThrowAnim->Montage_IsPlaying(ActiveThrowMontage.Get());
		if (bMontagePlaying || bThrowPinPulling)
		{
			return;
		}
		FinishThrow();
	}
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
 if(GetOwner())
 {
  if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsBusy())return;
  if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>())Door->StopGesture();
 }
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

void UCombatComponent::Initiate_EquipOrCycleThrowable()
{
 if(GetOwner())if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsProbeActive()){Tools->DeployReleased();return;}
 if(GetOwner())
 {
  if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsBusy())return;
  if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>())Door->StopGesture();
 }
	CancelInspect();
	if (bThrowInProgress) return;

	TArray<AWeapon*> Throws;
	for (AWeapon* Weapon : Inventory)
	{
		if (!IsValid(Weapon) || Weapon->WeaponKind != EWeaponKind::Throwable) continue;
		if (Weapon->Ammo <= 0) continue;
		Throws.Add(Weapon);
	}
	if (Throws.Num() == 0)
	{
		// Allow viewing empty throwable if any remain with 0 ammo? skip.
		return;
	}

	AWeapon* Target = Throws[0];
	if (IsValid(CurrentWeapon) && CurrentWeapon->WeaponKind == EWeaponKind::Throwable)
	{
		const int32 Idx = Throws.IndexOfByKey(CurrentWeapon);
		const int32 Next = (Idx == INDEX_NONE) ? 0 : (Idx + 1) % Throws.Num();
		Target = Throws[Next];
		if (Target == CurrentWeapon && Throws.Num() == 1)
		{
			return; // already holding the only throwable
		}
	}

	const int32 WeaponIndex = Inventory.IndexOfByKey(Target);
	if (!Inventory.IsValidIndex(WeaponIndex)) return;
	StopAimForWeaponSwitch();
	LocalWeaponIndex = WeaponIndex;
	Local_CycleWeapon(WeaponIndex, true);
	Server_CycleWeapon(WeaponIndex);
}


bool UCombatComponent::AllowsHeal() const
{
 if(GetOwner())
 {
  if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsBusy())return false;
  if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>())Door->StopGesture();
 }
 if(GetOwner())if(auto* Action=GetOwner()->FindComponentByClass<URestraintActionComponent>();Action&&Action->IsActive())return false;
	return true;
}

bool UCombatComponent::Initiate_Hotkey(int32 Hotkey)
{
	switch (Hotkey)
	{
	case 1:
		Initiate_EquipSlot(EWeaponSlot::Main);
		return true;
	case 2:
		Initiate_EquipSlot(EWeaponSlot::Pistol);
		return true;
	case 3:
		Initiate_EquipOrCycleThrowable();
		return true;
	case 4:
		Initiate_EquipSlot(EWeaponSlot::Throwable);
		return true;
	case 5:
		return false;
	default:
		return false;
	}
}



// Prefer MagCheck montage. No MagCheck asset => no-op (free inspect removed).
static UAnimMontage* ResolveKinMagCheckMontage(AWeapon* Weapon, bool bWeaponMesh)
{
	if (!IsValid(Weapon)) return nullptr;
	const FString Cls = Weapon->GetClass() ? Weapon->GetClass()->GetName() : FString();
	struct FKinMagCheck { const TCHAR* Match; const TCHAR* ArmsMontage; const TCHAR* ArmsSeq; const TCHAR* WpnMontage; const TCHAR* WpnSeq; };
	static const FKinMagCheck Table[] = {
		{ TEXT("Weapon_TR15"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/TR15/AM_RT_A_FP_TR15_Mag_Check.AM_RT_A_FP_TR15_Mag_Check"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/TR15/RT_A_FP_TR15_Mag_Check.RT_A_FP_TR15_Mag_Check"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/TR15/Weapon/AM_W_TR15_Mag_Check.AM_W_TR15_Mag_Check"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/TR15/Weapon/A_W_TR15_Mag_Check.A_W_TR15_Mag_Check") },
		{ TEXT("Weapon_AK105"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/AK105/AM_RT_A_FP_AK105_MagCheck.AM_RT_A_FP_AK105_MagCheck"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/AK105/RT_A_FP_AK105_MagCheck.RT_A_FP_AK105_MagCheck"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/AK105/Weapon/AM_W_AK105_MagCheck.AM_W_AK105_MagCheck"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/AK105/Weapon/A_W_AK105_MagCheck.A_W_AK105_MagCheck") },
		{ TEXT("Weapon_Mk14"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Mk14EBR/AM_RT_A_FP_Mk14EBR_MagCheck.AM_RT_A_FP_Mk14EBR_MagCheck"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Mk14EBR/RT_A_FP_Mk14EBR_MagCheck.RT_A_FP_Mk14EBR_MagCheck"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/Mk14EBR/Weapon/AM_W_Mk14EBR_MagCheck.AM_W_Mk14EBR_MagCheck"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/Mk14EBR/Weapon/A_W_Mk14EBR_MagCheck.A_W_Mk14EBR_MagCheck") },
		{ TEXT("Weapon_MKR9"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/MKR9/AM_RT_A_FP_MKR9_Mag_Check.AM_RT_A_FP_MKR9_Mag_Check"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/MKR9/RT_A_FP_MKR9_Mag_Check.RT_A_FP_MKR9_Mag_Check"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/MKR9/Weapon/AM_W_MKR9_Mag_Check.AM_W_MKR9_Mag_Check"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/MKR9/Weapon/A_W_MKR9_Mag_Check.A_W_MKR9_Mag_Check") },
		{ TEXT("Weapon_SRM12"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/SRM12/AM_RT_A_FP_SRM-12_FP_Mag_Check.AM_RT_A_FP_SRM-12_FP_Mag_Check"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/SRM12/RT_A_FP_SRM-12_FP_Mag_Check.RT_A_FP_SRM-12_FP_Mag_Check"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/SRM-12/Weapon/AM_W_SRM-12_Mag_Check.AM_W_SRM-12_Mag_Check"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/SRM-12/Weapon/A_W_SRM-12_Mag_Check.A_W_SRM-12_Mag_Check") },
		{ TEXT("Weapon_Viper"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Viper/AM_RT_A_FP_WK-11_Viper_Mag_Check.AM_RT_A_FP_WK-11_Viper_Mag_Check"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Viper/RT_A_FP_WK-11_Viper_Mag_Check.RT_A_FP_WK-11_Viper_Mag_Check"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/WK-11_Viper/Weapon/AM_W_WK-11_Viper_Mag_Check.AM_W_WK-11_Viper_Mag_Check"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/WK-11_Viper/Weapon/A_W_WK-11_Viper_Mag_Check.A_W_WK-11_Viper_Mag_Check") },
	};
	for (const FKinMagCheck& Row : Table)
	{
		if (!Cls.Contains(Row.Match)) continue;
		const TCHAR* MontPath = bWeaponMesh ? Row.WpnMontage : Row.ArmsMontage;
		const TCHAR* SeqPath = bWeaponMesh ? Row.WpnSeq : Row.ArmsSeq;
		if (MontPath)
		{
			if (UAnimMontage* M = LoadObject<UAnimMontage>(nullptr, MontPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return M;
			}
		}
		if (SeqPath)
		{
			if (UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, SeqPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				if (UAnimMontage* Dyn = MagCheckCache::GetOrCreate(FString(SeqPath), Seq, FName(TEXT("DefaultSlot")), 0.15f, 0.15f))
				{
					return Dyn;
				}
			}
		}
		break;
	}
	return nullptr;
}

static UAnimMontage* ResolveMagCheckMontage(UAnimMontage* InspectMontage, bool bWeaponMesh)
{
	TArray<FString> Candidates;
	if (IsValid(InspectMontage))
	{
		const FString Pkg = FPackageName::GetLongPackagePath(InspectMontage->GetOutermost()->GetName());
		const FString Name = InspectMontage->GetName();
		if (Name.Contains(TEXT("Inspect")))
		{
			const FString MagName = Name.Replace(TEXT("Inspect"), TEXT("MagCheck"));
			Candidates.AddUnique(Pkg / MagName + TEXT(".") + MagName);
		}
		if (Name.Contains(TEXT("RAR")) || Name.Contains(TEXT("AssaultRifle")))
		{
			Candidates.AddUnique(bWeaponMesh
				? FString(TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Weapon/Animations/AM_RAR_FP_WEP_AssaultRifle_MagCheck.AM_RAR_FP_WEP_AssaultRifle_MagCheck"))
				: FString(TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Actions/AM_RAR_FP_PCH_AssaultRifle_MagCheck.AM_RAR_FP_PCH_AssaultRifle_MagCheck")));
		}
	}
	else if (!bWeaponMesh)
	{
		Candidates.AddUnique(TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Actions/AM_RAR_FP_PCH_AssaultRifle_MagCheck.AM_RAR_FP_PCH_AssaultRifle_MagCheck"));
	}

	for (const FString& Path : Candidates)
	{
		if (UAnimMontage* M = LoadObject<UAnimMontage>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return M;
		}
	}

	// Character MagCheck may only exist as AnimSequence — wrap with inspect slot if possible.
	if (!bWeaponMesh)
	{
		const FString SeqPath = TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Actions/A_RAR_FP_PCH_AssaultRifle_MagCheck.A_RAR_FP_PCH_AssaultRifle_MagCheck");
		if (UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, *SeqPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			const FName Slot = (IsValid(InspectMontage) && InspectMontage->SlotAnimTracks.Num() > 0)
				? InspectMontage->SlotAnimTracks[0].SlotName
				: FName(TEXT("DefaultSlot"));
			const float InBlend = IsValid(InspectMontage) ? InspectMontage->BlendIn.GetBlendTime() : 0.15f;
			const float OutBlend = IsValid(InspectMontage) ? InspectMontage->BlendOut.GetBlendTime() : 0.15f;
			const FString CacheKey = SeqPath + TEXT("|") + Slot.ToString();
			if (UAnimMontage* Dyn = MagCheckCache::GetOrCreate(CacheKey, Seq, Slot, InBlend, OutBlend))
			{
				return Dyn;
			}
		}
	}
	return nullptr;
}

void UCombatComponent::Initiate_VersionWeapon(){StartInspection(true);}
void UCombatComponent::Initiate_InspectWeapon(){StartInspection(false);}
void UCombatComponent::StartInspection(bool bMagazineCheck)
{
 if(GetOwner())
 {
  if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsBusy())return;
  if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>())Door->StopGesture();
 }
	if (!IsValid(CurrentWeapon) || !IsValid(WeaponData)) return;
	// Mag check only while idle (hold-R). Free inspect key removed.
	if (CurrentWeapon->WeaponStatus != EWeaponStatus::Idle) return;
	if (IsReloading()) return;
	if (bAiming || bTriggerPressed) return;
	if (IsInspecting()) return;

	const auto* Shooter=Cast<AShooterCharacter>(GetOwner());
 // Keep the original 1P inspection as the source whenever it exists. The body
 // consumes that evaluated pose; only missing 1P actions use a body montage.
 const auto* FirstPerson=WeaponData->FirstPersonMontages.Find(ResolveAnimTag(CurrentWeapon));
 const auto* SourceArms=IPlayerInterface::Execute_GetMesh1P(GetOwner());
 const bool bHasFirstPerson=FirstPerson && FirstPerson->InspectMontage1P && SourceArms && SourceArms->GetAnimInstance();
 const bool bBody=Shooter&&Shooter->IsSingleBodyPilot()&&!bHasFirstPerson;
 const FMontageData* MontageData=bBody?WeaponData->ThirdPersonMontages.Find(ResolveThirdPersonPoseTag(WeaponData,CurrentWeapon)):WeaponData->FirstPersonMontages.Find(ResolveAnimTag(CurrentWeapon));
	UAnimMontage* InspectMontage = MontageData ? MontageData->InspectMontage1P.Get() : nullptr;
	USkeletalMeshComponent* Mesh1P = bBody?IPlayerInterface::Execute_GetMesh3P(GetOwner()):IPlayerInterface::Execute_GetMesh1P(GetOwner());
	UAnimInstance* AnimInstance = IsValid(Mesh1P) ? Mesh1P->GetAnimInstance() : nullptr;
	if (!IsValid(AnimInstance)) return;

	// KIN retargeted Mag_Check on shared arms DefaultSlot first; RAR Inspect/MagCheck fallback second.
	UAnimMontage* MagCheckMontage = !bMagazineCheck?InspectMontage:(bBody?nullptr:ResolveKinMagCheckMontage(CurrentWeapon, false));
	if (bMagazineCheck && !IsValid(MagCheckMontage))
	{
		MagCheckMontage = ResolveMagCheckMontage(InspectMontage, false);
	}
	if (!IsValid(MagCheckMontage))
	{
		UE_LOG(LogTemp, Log, TEXT("[MagCheck] no MagCheck montage for %s"), *GetNameSafe(CurrentWeapon));
		return;
	}
	const float Played = AnimInstance->Montage_Play(MagCheckMontage);
	if (Played <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MagCheck] Montage_Play failed for %s (slot/skeleton?). mont=%s"),
			*GetNameSafe(CurrentWeapon), *MagCheckMontage->GetName());
		return;
	}

	CurrentWeapon->UpdateMagazineAmmoVisuals();
	ActiveInspectMontage = MagCheckMontage;
 InspectAnimInstance=AnimInstance;bInspectionUsesBody=bBody;
	OnInspectionChanged.Broadcast(true);
	InspectWeapon = CurrentWeapon;
	ActiveInspectWeaponMontage = nullptr;

	{
		// RAR-like: drive Mesh1P Mag socket/bone; Mag DA stays attached (no hand reparent).
		// Use PlayWeaponMontageOrSequence so guns without AnimBP still PlayAnimation the sequence.
		USkeletalMeshComponent* WeaponMesh = bBody?CurrentWeapon->GetMesh3P():CurrentWeapon->GetMesh1P();
  InspectGunMesh=WeaponMesh;
		UAnimMontage* WeaponMagCheck = bMagazineCheck?ResolveKinMagCheckMontage(CurrentWeapon, true):nullptr;
		if (!IsValid(WeaponMagCheck))
		{
			if (const FMontageData* WeaponMontageData = WeaponData->WeaponMontages.Find(ResolveAnimTag(CurrentWeapon)))
			{
				WeaponMagCheck = bMagazineCheck?ResolveMagCheckMontage(WeaponMontageData->InspectMontage1P.Get(), true):WeaponMontageData->InspectMontage1P.Get();
			}
		}
		if (IsValid(WeaponMagCheck) && IsValid(WeaponMesh))
		{
			AWeapon::PlayWeaponMontageOrSequence(WeaponMesh, WeaponMagCheck);
			ActiveInspectWeaponMontage = WeaponMagCheck;
			UE_LOG(LogTemp, Log, TEXT("[MagCheck] weapon mesh playing %s"), *WeaponMagCheck->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[MagCheck] no weapon MagCheck anim for %s"), *GetNameSafe(CurrentWeapon));
		}
	}
	if(bBody)Server_BodyInspection(false,bMagazineCheck);
	UE_LOG(LogTemp, Log, TEXT("[MagCheck] playing %s (%.2fs)"), *MagCheckMontage->GetName(), Played);
}

bool UCombatComponent::IsInspecting() const
{
	UAnimMontage* Montage = ActiveInspectMontage.Get();
	if (!IsValid(Montage)) return false;
	UAnimInstance* AnimInstance = InspectAnimInstance.Get();
	return IsValid(AnimInstance) && AnimInstance->Montage_IsPlaying(Montage);
}

void UCombatComponent::CancelInspect()
{
	const bool bHadInspection = ActiveInspectMontage.IsValid();
	constexpr float InspectBlendOut = 0.15f;   // 短暂淡出，避免手臂突然跳回

	if (UAnimMontage* Montage = ActiveInspectMontage.Get())
	{
		UAnimInstance* AnimInstance = InspectAnimInstance.Get();
		if (IsValid(AnimInstance) && AnimInstance->Montage_IsPlaying(Montage))
		{
			AnimInstance->Montage_Stop(InspectBlendOut, Montage);
		}
	}
	if (UAnimMontage* WeaponMontage = ActiveInspectWeaponMontage.Get())
	{
		AWeapon* Weapon = InspectWeapon.Get();
		USkeletalMeshComponent* WeaponMesh = InspectGunMesh.Get();
		UAnimInstance* WeaponAnim = IsValid(WeaponMesh) ? WeaponMesh->GetAnimInstance() : nullptr;
		if (IsValid(WeaponAnim) && WeaponAnim->Montage_IsPlaying(WeaponMontage))
		{
			WeaponAnim->Montage_Stop(InspectBlendOut, WeaponMontage);
		}
	}
	if(bHadInspection&&bInspectionUsesBody)Server_BodyInspection(true,true);
 InspectAnimInstance.Reset();InspectGunMesh.Reset();bInspectionUsesBody=false;
	ActiveInspectMontage = nullptr;
	ActiveInspectWeaponMontage = nullptr;
	InspectWeapon = nullptr;
	if (bHadInspection) OnInspectionChanged.Broadcast(false);
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
			bLocalReloadNotified = true;
			Server_ReloadNotify();
		}
		return;
	}
	// 换弹已被切枪打断：不给子弹
	if (CurrentWeapon->WeaponStatus != EWeaponStatus::Reloading) return;

	ConsumeMagFromQueue(CurrentWeapon, bPendingFastReloadDrop);
	bPendingFastReloadDrop = false;

	CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
	Client_ReloadWeapon(CurrentWeapon, CurrentWeapon->Ammo, CurrentReserveAmmo);
}

void UCombatComponent::Server_Aim_Implementation(bool bPressed)
{
 if(GetOwner())
 {
  if(auto* Tools=GetOwner()->FindComponentByClass<UDoorToolComponent>();Tools&&Tools->IsBusy())return;
  if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>())Door->StopGesture();
 }
	Local_Aim(bPressed);
}



void UCombatComponent::Local_Aim(bool bPressed)
{
	// 枪管顶墙 / 包扎中禁止开镜
	if (bPressed)
	{
		if (const AShooterCharacter* Self = Cast<AShooterCharacter>(GetOwner()))
		{
			if (Self->IsWeaponBlocked() || Self->IsBandaging()) return;
		}
	}
	// 近战强制关镜：本地 / 服务器 / 切枪同步都走这里
	if (bPressed && IsValid(CurrentWeapon) && !CurrentWeapon->CanAimWeapon())
	{
		bPressed = false;
	}
	// 换弹中禁止开镜（关镜永远允许）；本地 + Server_Aim 都走这里
	if (bPressed && IsReloading())
	{
		return;
	}
	// 服务器校验：冲刺后摇未结束不允许进入开镜（关镜永远允许）
	if (bPressed)
	{
		if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(OwnerPawn->GetController()))
			{
				if (PC->IsSprinting())
				{
					PC->StopSprinting();
					return;
				}
				if (!PC->CanStartAds()) return;
			}
		}
	}
	if (bAiming == bPressed) return;   // 状态没变就不广播，避免 listen server 上重复触发
	bAiming = bPressed;
	OnAimStateChanged.Broadcast(bAiming);
}

void UCombatComponent::StopAimForWeaponSwitch()
{
	ClearAdsBuffer();
	ClearAdsPendingAfterReload();
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

// ================= 观战第一人称 =================

bool UCombatComponent::IsViewedFirstPersonLocally() const
{
	const AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner());
	return Shooter && !Shooter->IsLocallyControlled() && Shooter->IsFirstPersonViewedLocally();
}

void UCombatComponent::PlaySpectated1PFire()
{
	if (!IsValid(CurrentWeapon) || !IsValid(WeaponData)) return;
	if (const FMontageData* MontageData = WeaponData->FirstPersonMontages.Find(ResolveAnimTag(CurrentWeapon)))
	{
		UAnimMontage* Montage1P = MontageData->ResolveFireMontage();
		USkeletalMeshComponent* Mesh1P = IPlayerInterface::Execute_GetMesh1P(GetOwner());
		UAnimInstance* Anim1P = IsValid(Mesh1P) ? Mesh1P->GetAnimInstance() : nullptr;
		if (IsValid(Montage1P) && IsValid(Anim1P)) Anim1P->Montage_Play(Montage1P);
	}
	if (CurrentWeapon->WeaponKind != EWeaponKind::Melee)
	{
		if (const FMontageData* WeaponMontageData = WeaponData->WeaponMontages.Find(ResolveAnimTag(CurrentWeapon)))
		{
			UAnimMontage* WeaponFire = WeaponMontageData->ResolveFireMontage();
			USkeletalMeshComponent* WeaponMesh = CurrentWeapon->GetMesh1P();
			UAnimInstance* WeaponAnim = IsValid(WeaponMesh) ? WeaponMesh->GetAnimInstance() : nullptr;
			if (IsValid(WeaponFire) && IsValid(WeaponAnim)) WeaponAnim->Montage_Play(WeaponFire);
		}
	}
}

void UCombatComponent::PlaySpectated1PReload(EReloadType ReloadType)
{
	if (!IsValid(CurrentWeapon) || !IsValid(WeaponData)) return;
	UAnimMontage* ArmsMontage = nullptr;
	UAnimInstance* ArmsAnim = nullptr;
	if (GetReloadMontage(true, ReloadType, ArmsMontage, ArmsAnim))
	{
		ArmsAnim->Montage_Play(ArmsMontage);
	}
	if (const FMontageData* WeaponMontageData = WeaponData->WeaponMontages.Find(ResolveAnimTag(CurrentWeapon)))
	{
		EReloadType WeaponReloadType = ReloadType;
		if (WeaponReloadType == EReloadType::First && !IsValid(WeaponMontageData->FirstReloadMontage))
		{
			WeaponReloadType = CurrentWeapon->Ammo <= 0 ? EReloadType::Empty : EReloadType::Tactical;
		}
		UAnimMontage* WeaponReload = WeaponMontageData->ResolveReloadMontage(WeaponReloadType, CurrentWeapon->GetGripHoldStyle());
		USkeletalMeshComponent* WeaponMesh = CurrentWeapon->GetMesh1P();
		UAnimInstance* WeaponAnim = IsValid(WeaponMesh) ? WeaponMesh->GetAnimInstance() : nullptr;
		if (IsValid(WeaponReload)) AWeapon::PlayWeaponMontageOrSequence(WeaponMesh, WeaponReload);
	}
}



bool UCombatComponent::BeginDeploymentRaise()
{
 if(!CurrentWeapon||!WeaponData)return false;
 if(GetOwner()&&GetOwner()->Implements<UPlayerInterface>())
  if(auto* Arms=IPlayerInterface::Execute_GetMesh1P(GetOwner()))
   Arms->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 CancelInspect();StopActiveEquipMontages();PlayEquipMontage(CurrentWeapon,true);
 // Accepted is distinct from playing: equip animations are optional. The
 // camera state machine checks playback on the following tick and proceeds
 // immediately when no montage exists, rather than restarting forever.
 return true;
}
bool UCombatComponent::IsDeploymentRaisePlaying()const
{
 return ActiveEquipAnim[0].IsValid()&&ActiveEquipMontage[0].IsValid()
  &&ActiveEquipAnim[0]->Montage_IsActive(ActiveEquipMontage[0].Get());
}

FGameplayTag UCombatComponent::ResolveHoldPoseTag(const AWeapon* Weapon) const
{
 if(!Weapon)return FGameplayTag();
 if(Weapon->WeaponKind==EWeaponKind::LongGun)return ShooterTags::Tag_WeaponType_Rifle;
 if(Weapon->WeaponKind==EWeaponKind::Pistol)return ShooterTags::Tag_WeaponType_X24;
 if(Weapon->WeaponKind==EWeaponKind::Melee)return ShooterTags::Tag_WeaponType_Melee;
 return ResolveAnimTag(Weapon);
}

void UCombatComponent::Server_BodyInspection_Implementation(bool bStop,bool bMagazineCheck)
{
 if(bStop){Multicast_BodyInspection(nullptr,true);return;}
 if(!CurrentWeapon||!WeaponData||CurrentWeapon->WeaponStatus!=EWeaponStatus::Idle||bAiming)return;
 const auto* Row=WeaponData->ThirdPersonMontages.Find(ResolveThirdPersonPoseTag(WeaponData,CurrentWeapon));
 UAnimMontage* M=Row?Row->InspectMontage1P.Get():nullptr;
 if(bMagazineCheck)M=ResolveMagCheckMontage(M,false);
 if(M)Multicast_BodyInspection(M,false);
}
void UCombatComponent::Multicast_BodyInspection_Implementation(UAnimMontage* Montage,bool bStop)
{
 const auto* Pawn=Cast<APawn>(GetOwner());
 if(Pawn&&Pawn->IsLocallyControlled())return;
 auto* Mesh=IPlayerInterface::Execute_GetMesh3P(GetOwner());auto* Anim=Mesh?Mesh->GetAnimInstance():nullptr;
 if(!Anim)return;
 if(bStop)
 {
  if(ActiveInspectMontage.IsValid())Anim->Montage_Stop(.15f,ActiveInspectMontage.Get());
  if(auto* GunMesh=InspectGunMesh.Get())if(auto* GunAnim=GunMesh->GetAnimInstance())
   if(ActiveInspectWeaponMontage.IsValid())GunAnim->Montage_Stop(.15f,ActiveInspectWeaponMontage.Get());
  ActiveInspectMontage=nullptr;ActiveInspectWeaponMontage=nullptr;
  InspectAnimInstance.Reset();InspectGunMesh.Reset();bInspectionUsesBody=false;
  return;
 }
 if(Montage)
 {
  Anim->Montage_Play(Montage);ActiveInspectMontage=Montage;InspectAnimInstance=Anim;bInspectionUsesBody=true;
  if(CurrentWeapon&&WeaponData)if(const auto* W=WeaponData->WeaponMontages.Find(ResolveAnimTag(CurrentWeapon)))
  {
   UAnimMontage* GunMontage=Montage->GetName().Contains(TEXT("MagCheck"))?ResolveMagCheckMontage(W->InspectMontage1P.Get(),true):W->InspectMontage1P.Get();
   if(GunMontage){InspectGunMesh=CurrentWeapon->GetMesh3P();ActiveInspectWeaponMontage=GunMontage;AWeapon::PlayWeaponMontageOrSequence(InspectGunMesh.Get(),GunMontage);}
  }
 }
}
