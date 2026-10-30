#pragma once

#include "CoreMinimal.h"
#include "ShooterPlayerController.h"
#include "ShooterBotController.generated.h"

// 服务器侧的真人同链路 Bot：命中、寻路、开火。移动不依赖 AIController/PathFollowingComponent。
UCLASS()
class FPS_API AShooterBotController : public AShooterPlayerController
{
	GENERATED_BODY()

public:
	AShooterBotController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;
	virtual bool IsLocalController() const override;

private:
	void Think(float DeltaSeconds);
	void EnsureGun();
	void TryReload(float Now);
	void TryShoot(float Now, APawn* Enemy);
	void MoveToward(const FVector& Goal, float DeltaSeconds);
	APawn* FindEnemy() const;
	APawn* FindChasePawn() const;
	bool IsAlive() const;

	void ClearCachedPath();
	bool RebuildCachedPath(const FVector& Start, const FVector& Goal);
	bool HasClearCapsulePath(const FVector& From, const FVector& To) const;
	FVector GetCurrentSteeringTarget() const;

	float NextFireTime = 0.f;
	float ReloadCommitTime = 0.f;
	float NextPathTime = 0.f;
	float StuckTime = 0.f;
	float AvoidSideUntil = 0.f;
	FVector CachedGoal = FVector::ZeroVector;
	TArray<FVector> CachedPath;
	int32 PathIndex = 0;
	FVector AvoidSide = FVector::ZeroVector;
	FVector LastMoveAnchor = FVector::ZeroVector;
	FVector LastSteerDirection = FVector::ForwardVector;
	FVector LastLookDirection = FVector::ForwardVector;
};
