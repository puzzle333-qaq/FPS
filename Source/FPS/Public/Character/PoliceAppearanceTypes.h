#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShooterType/ShooterType.h"
#include "PoliceAppearanceTypes.generated.h"

class USkeletalMesh;

UENUM(BlueprintType)
enum class EPoliceFaceProtection : uint8
{
 None UMETA(DisplayName="无面部装备"),
 FlashGoggles UMETA(DisplayName="闪光防护眼镜"),
 GogglesAndMask UMETA(DisplayName="闪光防护眼镜＋防弹面罩"),
 GasMask UMETA(DisplayName="防毒面具")
};

/** Authored mesh choices; equipment state continues to use the character's existing armor. */
UCLASS(BlueprintType)
class FPS_API UPoliceAppearanceData : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<USkeletalMesh> BaseMesh;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<USkeletalMesh> Goggles;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<USkeletalMesh> BallisticMask;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<USkeletalMesh> GasMask;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<TSoftObjectPtr<USkeletalMesh>> LightArmor;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<TSoftObjectPtr<USkeletalMesh>> HeavyArmor;
 // Hide the preset's baked equipment before attaching the selected modules.
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FName> ReplacedMaterialSlots;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FName> UnarmoredHiddenSlots;
};
