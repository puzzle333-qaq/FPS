#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "WeaponDefinition.generated.h"

UENUM(BlueprintType)
enum class ESkeletonFamily : uint8
{
    Manny,
    RAR,
    Other
};

UCLASS(BlueprintType)
class FPS_API UWeaponDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FPS|Weapon Definition")
    FGameplayTag WeaponType;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FPS|Weapon Definition")
    ESkeletonFamily SkeletonFamily;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FPS|Weapon Definition")
    TObjectPtr<USkeletalMesh> WeaponMesh;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FPS|Weapon Definition|Arms")
    TObjectPtr<UAnimSequence> ArmsIdle;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FPS|Weapon Definition|Arms")
    TObjectPtr<UAnimSequence> ArmsAimIdle;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FPS|Weapon Definition|Arms")
    TObjectPtr<UAnimSequence> ArmsEquip;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FPS|Weapon Definition|Arms")
    TObjectPtr<UAnimSequence> ArmsReloadEmpty;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FPS|Weapon Definition|Arms")
    TObjectPtr<UAnimSequence> ArmsReloadTactical;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FPS|Weapon Definition|Arms")
    TObjectPtr<UAnimSequence> ArmsFire;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FPS|Weapon Definition|Arms")
    TObjectPtr<UAnimSequence> ArmsFireAim;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FPS|Weapon Definition|Weapons")
    TObjectPtr<UAnimSequence> WeaponFire;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FPS|Weapon Definition|Weapons")
    TObjectPtr<UAnimSequence> WeaponReloadEmpty;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FPS|Weapon Definition|Weapons")
    TObjectPtr<UAnimSequence> WeaponReloadTactical;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FPS|Weapon Definition|Weapons")
    TObjectPtr<UAnimSequence> WeaponEquip; 
};