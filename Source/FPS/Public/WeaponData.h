// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "Animation/AnimMontage.h"

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
};

USTRUCT(BlueprintType)
struct FMontageData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly)
	TObjectPtr<UAnimMontage>EquipMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimMontage>ReloadMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimMontage>FireMontage = nullptr;

	// ---- 换弹分种类（都可以不填，不填就退回上面的 ReloadMontage） ----

	// 弹匣打空后的换弹（一般多一段拉机柄/按空仓挂机）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reload")
	TObjectPtr<UAnimMontage>EmptyReloadMontage = nullptr;

	// 弹匣里还有子弹时的战术换弹
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reload")
	TObjectPtr<UAnimMontage>TacticalReloadMontage = nullptr;

	// 这把枪第一次上弹（可选）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reload")
	TObjectPtr<UAnimMontage>FirstReloadMontage = nullptr;

	// 拉栓/泵动。填了它，这把枪每打一发都要播完它才能开下一枪（狙击、泵动霰弹用）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bolt")
	TObjectPtr<UAnimMontage>BoltMontage = nullptr;

	// 第 9 步：检视武器（只在自己的第一人称播放，不同步）。
	// 填在 FirstPersonMontages（手臂）里；想让枪身也动，在 WeaponMontages 里同样填一个
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inspect")
	TObjectPtr<UAnimMontage>InspectMontage1P = nullptr;

	// 按换弹种类挑蒙太奇：First -> (Empty 或 Tactical) -> ReloadMontage，哪个没填就往后退
	UAnimMontage* ResolveReloadMontage(EReloadType Type) const
	{
		if (Type == EReloadType::First && FirstReloadMontage)
		{
			return FirstReloadMontage;
		}
		if (Type == EReloadType::Empty && EmptyReloadMontage)
		{
			return EmptyReloadMontage;
		}
		if (Type == EReloadType::Tactical && TacticalReloadMontage)
		{
			return TacticalReloadMontage;
		}
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
