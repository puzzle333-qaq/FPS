#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/PoliceAppearanceTypes.h"
#include "PoliceAppearanceComponent.generated.h"

class USkeletalMeshComponent;
class UAnimInstance;
class UMaterialInterface;

/** Cosmetic modules follow the existing animated body; no second animation graph or tick. */
UCLASS(ClassGroup=(Character),meta=(BlueprintSpawnableComponent))
class FPS_API UPoliceAppearanceComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UPoliceAppearanceComponent();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Police") TSoftObjectPtr<UPoliceAppearanceData> AppearanceData;
 UPROPERTY(ReplicatedUsing=OnRep_Protection,BlueprintReadOnly,Category="Police") EPoliceFaceProtection FaceProtection=EPoliceFaceProtection::FlashGoggles;
 UPROPERTY(Transient,BlueprintReadOnly,Category="Police") TArray<TObjectPtr<USkeletalMeshComponent>> Parts;
 UFUNCTION(BlueprintCallable,Category="Police") void SetProtection(EPoliceFaceProtection Face,EBodyArmorType Armor);
 UFUNCTION(Server,Reliable) void Server_SetProtection(EPoliceFaceProtection Face,EBodyArmorType Armor);
 UFUNCTION(BlueprintCallable,Category="Police") void RefreshAppearance();
 UFUNCTION(BlueprintCallable,Category="Police") void InitializePreview(USkeletalMeshComponent* Target,EPoliceFaceProtection Face,EBodyArmorType Armor);
 UFUNCTION(BlueprintCallable,Category="Police") void SetPreviewProtection(EPoliceFaceProtection Face,EBodyArmorType Armor);
 void SyncFlashGoggles(bool bEnabled);
 bool IsApplyingSelection() const { return bApplyingSelection; }
 static bool IsValidSelection(EPoliceFaceProtection Face,EBodyArmorType Armor);
private:
 UFUNCTION() void OnRep_Protection();
 UFUNCTION() void OnBodyAnimationInitialized();
 void RefreshEquippedPose();
 void InitializeLocalSelection();
 void ApplySelection(EPoliceFaceProtection Face,EBodyArmorType Armor);
 void SaveLocalSelection(EPoliceFaceProtection Face,EBodyArmorType Armor);
 void ClearParts();
 void RestorePreview();
 UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> PreviewTarget;
 UPROPERTY(Transient) TObjectPtr<USkeletalMesh> OriginalPreviewMesh;
 UPROPERTY(Transient) TSubclassOf<UAnimInstance> OriginalPreviewAnim;
 UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> OriginalPreviewMaterials;
 EBodyArmorType PreviewArmor=EBodyArmorType::Light;
 bool bApplyingSelection=false;
 bool bLocalInitialized=false;
 FTimerHandle LocalInitTimer;
 FTimerHandle PoseInitTimer;
};
