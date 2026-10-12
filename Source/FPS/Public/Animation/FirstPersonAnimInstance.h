#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "FirstPersonAnimInstance.generated.h"

// 第一人称手臂的扶枪 IK。动画只提供持枪底姿，左手每帧被拉到枪或握把的 FABRIK 插槽。
UCLASS()
class FPS_API UFirstPersonAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// 左手目标。空间是手臂网格 hand_r 的骨骼空间，和第三人称 FABRIK 节点一致。
	UPROPERTY(BlueprintReadOnly, Category = "FPS|FABRIK")
	FTransform FABRIK_Socket1P = FTransform::Identity;

	// 0 时左手留在动画里。没拿枪，或枪和握把上都没有可用 FABRIK 时为 0。
	UPROPERTY(BlueprintReadOnly, Category = "FPS|FABRIK")
	float FABRIK_Alpha1P = 0.f;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
