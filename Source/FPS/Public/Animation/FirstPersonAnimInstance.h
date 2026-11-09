#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Weapon/AttachmentData.h"
#include "FirstPersonAnimInstance.generated.h"

class UAnimSequence;

// First-person arms. Hold pose from grip style; FABRIK snaps the left hand to GripSocket when present.
UCLASS()
class FPS_API UFirstPersonAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "FPS|FABRIK")
	FTransform FABRIK_Socket1P = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|FABRIK")
	float FABRIK_Alpha1P = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|GripHold")
	EGripHoldStyle GripHoldStyle = EGripHoldStyle::None;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|GripHold")
	TObjectPtr<UAnimSequence> LeftHandHoldPose = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|GripHold")
	bool bUseLeftHandHoldPose = false;

	/** Mask_Grip curve: 1 = fully mask grip pose/IK (reload). Drives FABRIK fade. */
	UPROPERTY(BlueprintReadOnly, Category = "FPS|GripHold")
	float GripPoseWeight = 1.f;

	/** Resolved idle this frame (IdleAnim or IdleAnim_Grab). Same idea as ResolveReloadMontage. */
	UPROPERTY(BlueprintReadOnly, Category = "FPS|GripHold")
	TObjectPtr<UAnimSequence> ActiveIdleAnim = nullptr;

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	/** After AnimBP Event Graph: force shotgun 1P to SequencePlayer+Slot only (no left-hand BlendList). */
	void ForceShotgunRawPose1P();

	// Cached on the game thread alongside ActiveIdleAnim; no asset/owner lookup during evaluation.
	bool bRestoreFlashIdle1P = false;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> GripBodyPose = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> GripAimPose = nullptr;

	void ApplyResolvedGripPose();
};
