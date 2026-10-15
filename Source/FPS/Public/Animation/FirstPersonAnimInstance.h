#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Weapon/AttachmentData.h"
#include "FirstPersonAnimInstance.generated.h"

class UAnimSequence;

// First-person arms. Hold pose from grip style; 1P FABRIK stays off.
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
	EGripHoldStyle GripHoldStyle = EGripHoldStyle::Cradle;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|GripHold")
	TObjectPtr<UAnimSequence> LeftHandHoldPose = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|GripHold")
	bool bUseLeftHandHoldPose = false;

	/** Resolved idle this frame (IdleAnim or IdleAnim_Grab). Same idea as ResolveReloadMontage. */
	UPROPERTY(BlueprintReadOnly, Category = "FPS|GripHold")
	TObjectPtr<UAnimSequence> ActiveIdleAnim = nullptr;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> GrabIdleSlotAsset = nullptr;
};