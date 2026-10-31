#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Game/AINoiseEvent.h"
#include "HostileAIController.generated.h"

class AHostileAICharacter;

UENUM()
enum class EHostileBrainState : uint8
{
	Idle,
	Investigate,
	Combat,
	TakeCover
};

/**
 * CQB hostile brain MVP:
 * - Nav path toward last-known / investigate point
 * - Hear gunshots (and nearby footsteps) → investigate
 * - Seek nearby wall cover, crouch, peek/shoot
 */
UCLASS()
class FPS_API AHostileAIController : public AAIController
{
	GENERATED_BODY()

public:
	AHostileAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Called by AINoiseEvent::Report on authority. */
	void HearNoise(const FVector& Location, float Loudness, AActor* NoiseInstigator, AINoiseEvent::EKind Kind);

	/** Called when pawn takes damage. */
	void NotifyTookDamage(AActor* DamageInstigator, const FVector& HitFrom);

protected:
	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI")
	float HearGunshotRange = 4500.f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI")
	float HearFootstepRange = 900.f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI")
	float CoverSearchRadius = 900.f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI")
	float LoseSightGrace = 3.5f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI")
	float InvestigateTimeout = 8.f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI")
	float UnderFireDuration = 2.5f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI")
	float CoverHoldMin = 1.2f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI")
	float PeekInterval = 1.8f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI")
	float CombatPreferCoverChance = 0.55f;

private:
	void Think(float DeltaSeconds);
	void TickFootstepSense(float Now);
	APawn* FindBestPlayerTarget() const;
	bool HasLineOfSightTo(APawn* Target) const;
	bool HasLineOfSightFrom(const FVector& Eye, const FVector& TargetLoc) const;
	void MoveToward(const FVector& Goal, float DeltaSeconds);
	void TryShoot(APawn* Target, float Now);
	void SetState(EHostileBrainState NewState);
	void SetCrouch(bool bCrouch);
	bool FindCoverPoint(const FVector& ThreatLoc, FVector& OutCover) const;
	bool IsNear(const FVector& Loc, float DistCm) const;

	EHostileBrainState State = EHostileBrainState::Idle;

	float NextFireTime = 0.f;
	float NextPathTime = 0.f;
	float NextCoverSearchTime = 0.f;
	float NextPeekTime = 0.f;
	float NextFootSenseTime = 0.f;
	float LostSightAt = -1.f;
	float InvestigateUntil = 0.f;
	float UnderFireUntil = 0.f;
	float EnteredCoverAt = 0.f;

	FVector CachedGoal = FVector::ZeroVector;
	FVector LastKnownEnemy = FVector::ZeroVector;
	FVector InvestigatePoint = FVector::ZeroVector;
	FVector CoverPoint = FVector::ZeroVector;
	FVector PeekPoint = FVector::ZeroVector;

	bool bHasLastKnown = false;
	bool bHasInvestigate = false;
	bool bHasCover = false;
	bool bPeeking = false;
	bool bWantsCoverThisFight = false;
};
