#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/PlayerInterface.h"
#include "TargetDummy.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class UAnimSequenceBase;
class USoundBase;

// 靶场靶子：走 IPlayerInterface::DoDamage（和打人同一条伤害链），被打倒→倒下动画→等待→起立动画→回满血
UCLASS()
class FPS_API ATargetDummy : public AActor, public IPlayerInterface
{
	GENERATED_BODY()

public:
	ATargetDummy();

	virtual bool DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 服务器：命中点在头部区域就返回 "head"（靶子骨骼只有 root/body），否则返回原骨骼名
	FName ResolveHitBone(const FHitResult& Hit, FName DefaultBone) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Target")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Target")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Target", meta = (ClampMin = "1"))
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Target", meta = (ClampMin = "0"))
	float ResetDelay = 3.f;

	// 头部区域：命中点相对靶子原点的高度（cm）超过这个值算爆头
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Target")
	bool bEnableHeadshotZone = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Target")
	float HeadZoneMinHeight = 165.f;

	// SKM_TargetDummy 默认朝向相对 Actor 前向偏了 -90°，用 +90 Yaw 摆正
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Target")
	float MeshFacingYaw = 0.f;

	// 无动画时铰链倒下的 Pitch（度）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Target")
	float FallPitch = -85.f;

	// 铰链插值速度（无动画或动画失败时用）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Target", meta = (ClampMin = "0.1"))
	float HingeInterpSpeed = 10.f;

	// 优先播 A_TargetDummy_Down/Up；播不出来再铰链旋转
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Target")
	bool bPreferAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Target")
	TObjectPtr<UAnimSequenceBase> FallAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Target")
	TObjectPtr<UAnimSequenceBase> RiseAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Target")
	TObjectPtr<USoundBase> FallSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Target")
	TObjectPtr<USoundBase> RiseSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Target")
	bool bPostKillFeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Target")
	FString KillFeedName = TEXT("Target");

	UFUNCTION(BlueprintPure, Category = "FPS|Target")
	bool IsDown() const { return bDown; }

	UFUNCTION(BlueprintPure, Category = "FPS|Target")
	float GetHealth() const { return Health; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(ReplicatedUsing = OnRep_Down)
	bool bDown = false;

	UPROPERTY(Replicated)
	float Health = 100.f;

	UFUNCTION()
	void OnRep_Down();

	void ApplyDownVisual(bool bAnimate);
	bool TryPlayStateAnim(bool bAnimate);
	void ApplyHingeRotation(float Pitch);
	void Auth_Reset();

	FTimerHandle ResetTimer;

	// 0 = 立起，1 = 倒下（铰链平滑）
	float HingeAlpha = 0.f;
	float HingeAlphaTarget = 0.f;
	bool bUsingAnimVisual = false;
};
