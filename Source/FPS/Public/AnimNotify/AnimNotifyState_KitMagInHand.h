#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_KitMagInHand.generated.h"

// Kit guns (BP_Weapon_MCX / BP_Weapon_Deagle): while this state is active the weapon's
// "KitMag_1P" / "KitMag_3P" static mesh rides on the hand bone, then snaps back to the gun.
// No-op for weapons without a KitMag component (AK, pistol, ...).
UCLASS(meta = (DisplayName = "Kit Mag In Hand"))
class FPS_API UAnimNotifyState_KitMagInHand : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	FName HandBone = TEXT("hand_l");

	// Component name prefix on the weapon; "_1P"/"_3P" is appended.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	FString MagComponentPrefix = TEXT("KitMag");

	// Mag pose relative to HandBone while held (scale ignored: the mag keeps its original world scale).
	// Default: Kit MCX mag in left palm, orientation mapped from the AK mag in hand (AK reload 0.8-1.0s). Tune per notify instance in the montage.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Weapon", meta = (EditCondition = "bUseHandRelativeOffset"))
	FTransform HandRelativeOffset = FTransform(FRotator(54.5f, 30.9f, 177.4f), FVector(6.83f, -0.90f, -3.20f));

	// false = old behaviour (keep the world pose the mag had when the notify began).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	bool bUseHandRelativeOffset = true;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Kit Mag In Hand"); }
};
