// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "Animation/AnimMontage.h"
#include "Weapon/AttachmentData.h"

#include "WeaponData.generated.h"

class UAnimSequence;
class UBlendSpace;
class UAnimMontage;

// 这次换弹属于哪一种。只决定播哪条蒙太奇，填弹仍在 Notify_ReloadWeapon 里结算。
UENUM(BlueprintType)
enum class EReloadType : uint8
{
	Tactical UMETA(DisplayName = "Tactical (mag not empty)"),
	Empty UMETA(DisplayName = "Empty"),
	First UMETA(DisplayName = "First Reload")
};

USTRUCT(BlueprintType)
struct FPlayerAnim
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly)
	TObjectPtr<UAnimSequence> IdleAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimSequence> AimIdleAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimSequence> CrouchIdleAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimSequence> SprintAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UBlendSpace> AimOffset_Hip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UBlendSpace> AimOffset_Aim ;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UBlendSpace> Strafe_Standing;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UBlendSpace> Strafe_Crouching;

	// 站立慢走/开镜：放 Walk 采样的 BlendSpace；AnimBP 在 bUseWalkGait 时用它替换 Strafe_Standing
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UBlendSpace> Strafe_Walking;
	// Vertical grip (Grab) overrides; empty = use IdleAnim (Cradle/default)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GripHold")
	TObjectPtr<UAnimSequence> IdleAnim_Grab = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GripHold")
	TObjectPtr<UAnimSequence> AimIdleAnim_Grab = nullptr;

	// Optional left-hand pose layer (template G2). AnimBP Layered blend; empty = IdleAnim_Grab only
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GripHold")
	TObjectPtr<UAnimSequence> LeftHandHoldPose_Cradle = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GripHold")
	TObjectPtr<UAnimSequence> LeftHandHoldPose_Grab = nullptr;
};

USTRUCT(BlueprintType)
struct FMontageData
{
	GENERATED_BODY()

	// 普通掏枪 / 切换装备
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equip")
	TObjectPtr<UAnimMontage> EquipMontage = nullptr;

	// 本局第一次掏出这把枪（空则退回 EquipMontage）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equip")
	TObjectPtr<UAnimMontage> FirstEquipMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	TObjectPtr<UAnimMontage> FireMontage = nullptr;

	// 短按 R：空仓换弹蒙太奇（弹匣队列仍保留半匣）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reload")
	TObjectPtr<UAnimMontage> EmptyReloadMontage = nullptr;

	// 长按 R：战术换弹蒙太奇（丢掉当前匣）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reload")
	TObjectPtr<UAnimMontage> TacticalReloadMontage = nullptr;

	// 检视（只在自己的第一人称播放，不同步）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspect")
	TObjectPtr<UAnimMontage> InspectMontage1P = nullptr;

	// ---- 以下字段保留序列化兼容，编辑器里隐藏，代码不再作为主路径 ----

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use Empty/TacticalReloadMontage. Kept as silent fallback."))
	TObjectPtr<UAnimMontage> ReloadMontage = nullptr;

	// 投掷物右键低抛仍可用；枪械 DA 不要填
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Throwable low-throw only; unused for guns."))
	TObjectPtr<UAnimMontage> SecondaryFireMontage = nullptr;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Removed. First equip is FirstEquipMontage."))
	TObjectPtr<UAnimMontage> FirstReloadMontage = nullptr;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Removed grab reload variants."))
	TObjectPtr<UAnimMontage> EmptyReloadMontage_Grab = nullptr;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Removed grab reload variants."))
	TObjectPtr<UAnimMontage> TacticalReloadMontage_Grab = nullptr;

	// 近战等：多条攻击/掏枪蒙太奇，随机挑一条；空则退回上面的单条 Fire/Equip
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Variants")
	TArray<TObjectPtr<UAnimMontage>> FireMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee|Variants")
	TArray<TObjectPtr<UAnimMontage>> EquipMontages;

	UAnimMontage* ResolveFireMontage() const
	{
		TArray<UAnimMontage*> Candidates;
		for (const TObjectPtr<UAnimMontage>& M : FireMontages)
		{
			if (M) { Candidates.Add(M.Get()); }
		}
		if (FireMontage) { Candidates.AddUnique(FireMontage.Get()); }
		if (Candidates.Num() == 0) { return nullptr; }
		return Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	}

	UAnimMontage* ResolveThrowMontage(bool bHighThrow) const
	{
		if (!bHighThrow && SecondaryFireMontage)
		{
			return SecondaryFireMontage;
		}
		return ResolveFireMontage();
	}

	UAnimMontage* ResolveEquipMontage(bool bFirstDraw = false) const
	{
		if (bFirstDraw && FirstEquipMontage)
		{
			return FirstEquipMontage;
		}
		TArray<UAnimMontage*> Candidates;
		for (const TObjectPtr<UAnimMontage>& M : EquipMontages)
		{
			if (M) { Candidates.Add(M.Get()); }
		}
		if (EquipMontage) { Candidates.AddUnique(EquipMontage.Get()); }
		if (Candidates.Num() == 0)
		{
			return FirstEquipMontage;
		}
		return Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	}

	// GripStyle kept so call sites stay stable; Grab variants are unused.
	UAnimMontage* ResolveReloadMontage(EReloadType Type, EGripHoldStyle GripStyle = EGripHoldStyle::Cradle) const
	{
		(void)GripStyle;
		if (Type == EReloadType::Empty)
		{
			if (EmptyReloadMontage) return EmptyReloadMontage;
			if (TacticalReloadMontage) return TacticalReloadMontage;
			return ReloadMontage;
		}
		if (Type == EReloadType::Tactical)
		{
			if (TacticalReloadMontage) return TacticalReloadMontage;
			if (EmptyReloadMontage) return EmptyReloadMontage;
			return ReloadMontage;
		}
		// First / unknown: prefer empty then tactical then legacy
		if (EmptyReloadMontage) return EmptyReloadMontage;
		if (TacticalReloadMontage) return TacticalReloadMontage;
		return ReloadMontage;
	}
};

UCLASS()
class FPS_API UWeaponData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category = "FPS|Weapon Data")
	TMap<FGameplayTag, FName > GripPoints;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon Data|Weapon")
	TMap<FGameplayTag, FMontageData>WeaponMontages;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly , Category = "FPS|Weapon Data|First Person")
	TMap<FGameplayTag, FPlayerAnim> FirstPersonAnims;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly , Category = "FPS|Weapon Data|Third Person")
	TMap<FGameplayTag, FPlayerAnim> ThirdPersonAnims;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon Data|First Person")
	TMap<FGameplayTag, FMontageData>FirstPersonMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon Data|Third Person")
	TMap<FGameplayTag, FMontageData>ThirdPersonMontages;


};

