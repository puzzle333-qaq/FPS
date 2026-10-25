#pragma once

#include "CoreMinimal.h"
#include "ShooterPlayerController.h"
#include "Game/SearchDestroyTypes.h"
#include "ShooterBotController.generated.h"

class ABombSite;

// 服务器侧的真人同链路 Bot：保留 PlayerController 的购买、命中、下包、观战逻辑，
// 但移动不依赖 AIController/PathFollowingComponent。
UCLASS()
class FPS_API AShooterBotController : public AShooterPlayerController
{
	GENERATED_BODY()

public:
	AShooterBotController();

	void SetPreferredSite(EBombSiteId Site) { PreferredSite = Site; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;
	virtual bool IsLocalController() const override;

private:
	void Think(float DeltaSeconds);
	void BuyOnce();
	void EnsureGun();
	void TryReload(float Now);
	void TryShoot(float Now, APawn* Enemy);
	void MoveToward(const FVector& Goal, float DeltaSeconds);
	void ReleasePlant();
	APawn* FindEnemy() const;
	APawn* FindChasePawn() const;
	bool WantsToPlantOrDefuse(FVector& OutGoal) const;
	ABombSite* FindPreferredSite() const;
	bool IsAlive() const;

	void ClearCachedPath();
	bool RebuildCachedPath(const FVector& Start, const FVector& Goal);
	bool HasClearCapsulePath(const FVector& From, const FVector& To) const;
	FVector GetCurrentSteeringTarget() const;

	EBombSiteId PreferredSite = EBombSiteId::A;
	int32 BoughtRound = -1;
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
	bool bHoldingBombInteract = false;
};
