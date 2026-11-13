#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Game/AINoiseEvent.h"
#include "Game/HostileSquadTypes.h"
#include "Perception/AIPerceptionTypes.h"
#include "HostileAIController.generated.h"

class AHostileAICharacter;
class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;

UENUM(BlueprintType)
enum class EHostileBrainState : uint8
{
	Idle,
	Investigate,
	Combat,
	TakeCover,
	Retreat,
	Flee,
	Surrender,
	Search
};

UENUM(BlueprintType)
enum class EHostileAlertState : uint8
{
	Relaxed,
	Suspicious,
	Alert,
	Searching
};

/**
 * Bandit brain:
 * - Hear gunshots, doors, explosions and footsteps, then investigate or go alert
 * - The first hit rolls surrender, last stand, or flee
 * - A runner keeps going until they can no longer see the player
 */
UCLASS()
class FPS_API AHostileAIController : public AAIController
{
	GENERATED_BODY()

public:
	AHostileAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Called by AINoiseEvent::Report on authority. */
	void HearNoise(const FVector& Location, float Loudness, AActor* NoiseInstigator, AINoiseEvent::EKind Kind);

	/** Called when pawn takes damage. */
	void NotifyTookDamage(AActor* DamageInstigator, const FVector& HitFrom);

	/** Ally alert broadcast (same level, within AlertBroadcastRadius). */
	void ReceiveAllyAlert(const FVector& Location, AActor* InstigatorActor);

	UFUNCTION(BlueprintPure, Category = "FPS|Hostile")
	EHostileMorale GetMorale() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Hostile")
	EHostileAlertState GetAlertState() const { return AlertState; }

	/** Change job after spawn. A solo hostile is forced back to Closer. */
	void SetSquadRole(EHostileSquadRole NewRole);

protected:
	UPROPERTY(VisibleAnywhere, Category = "FPS|HostileAI|Perception")
	TObjectPtr<UAIPerceptionComponent> AIPerception;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	/** Phase 2 hook only — sight / light is not used for alert yet. */
	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

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
	float CoverHoldMin = 0.6f;

	/** Leave cover and push again even if the target is still visible. */
	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI", meta = (ClampMin = "0.5"))
	float CoverHoldMax = 2.4f;

	/** After leaving cover, ignore new cover for this long so they actually move. */
	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI", meta = (ClampMin = "0"))
	float CoverCooldown = 4.f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI")
	float PeekInterval = 0.9f;

	/** First hit. Weights are renormalized, so they do not need to sum to 1. */
	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Morale", meta = (ClampMin = "0"))
	float SurrenderChance = 0.30f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Morale", meta = (ClampMin = "0"))
	float LastStandChance = 0.25f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Morale", meta = (ClampMin = "0"))
	float FleeChance = 0.45f;

	/** Morale roll while FlashSurrenderWindow active on the pawn (renormalized). */
	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Morale", meta = (ClampMin = "0"))
	float FlashSurrenderChance = 1.f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Morale", meta = (ClampMin = "0"))
	float FlashLastStandChance = 0.f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Morale", meta = (ClampMin = "0"))
	float FlashFleeChance = 0.f;

	/** Below this health fraction the bot stops pushing and falls back. */
	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Squad", meta = (ClampMin = "0", ClampMax = "1"))
	float RetreatHealthFraction = 0.30f;

	/** Suppressor will not walk farther than this from the post while holding. */
	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Squad", meta = (ClampMin = "50"))
	float SuppressorHoldRadius = 200.f;

	/** Suppressor investigates only this far from the post. */
	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Squad", meta = (ClampMin = "100"))
	float SuppressorLeash = 800.f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Squad", meta = (ClampMin = "100"))
	float FlankRadius = 640.f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Squad", meta = (ClampMin = "0", ClampMax = "160"))
	float FlankApproachAngle = 75.f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Squad", meta = (ClampMin = "0"))
	float ReflankThreshold = 320.f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Squad", meta = (ClampMin = "0", ClampMax = "90"))
	float FacingConeHalfAngle = 50.f;

	/** Keep flanking briefly after the ally's engaged count flickers off. */
	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Squad", meta = (ClampMin = "0"))
	float FlankCommitDelay = 1.6f;

	/** Heights are added to the target actor origin. Capsule center is the waist. */
	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Fire")
	float AimLegHeight = -48.f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Fire")
	float AimChestHeight = 18.f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Fire")
	float AimHeadHeight = 72.f;

	/** Each shot rolls one zone. Defaults are leg 70, chest 25, head 5. */
	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Fire", meta = (ClampMin = "0"))
	float AimLegChance = 0.70f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Fire", meta = (ClampMin = "0"))
	float AimChestChance = 0.25f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Fire", meta = (ClampMin = "0"))
	float AimHeadChance = 0.05f;

	/** Small cone inside the chosen zone so shots do not stack on one point. */
	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Fire", meta = (ClampMin = "0", ClampMax = "8"))
	float AimSpreadDegrees = 1.2f;

	/** How fast the body yaw catches the target. */
	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Fire", meta = (ClampMin = "30"))
	float AimTurnRate = 220.f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Fire", meta = (ClampMin = "1"))
	int32 BurstShotsMin = 3;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Fire", meta = (ClampMin = "1"))
	int32 BurstShotsMax = 6;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Fire", meta = (ClampMin = "0"))
	float BurstPauseMin = 0.85f;

	UPROPERTY(EditAnywhere, Category = "FPS|HostileAI|Fire", meta = (ClampMin = "0"))
	float BurstPauseMax = 1.6f;

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

private:
	void Think(float DeltaSeconds);
	void TickAlertDebug() const;
	void TickSearchLook(float Now, float DeltaSeconds);
	void SetAlertState(EHostileAlertState NewState);
	void EnterSearch(const FVector& Where);
	void BroadcastAlert(const FVector& Location, AActor* InstigatorActor);
	void NotifyLevelAlarm();
	void NotifyPlayerDetected();
	bool IsLevelAlarmRaised() const;
	void TickFootstepSense(float Now);
	void TickSuppress(AHostileAICharacter* Me, APawn* Target, bool bCanSee, float Dist, bool bUnderFire, float Now, float DeltaSeconds);
	void TickFlank(AHostileAICharacter* Me, APawn* Target, bool bCanSee, float Dist, float Now, float DeltaSeconds);
	void TickRetreat(AHostileAICharacter* Me, APawn* Target, bool bCanSee, float Dist, float Now, float DeltaSeconds);
	void RefreshSquadAwareness(float Now, bool bCanSee);
	void PublishSquad(AActor* TargetActor) const;
	bool IsHealthLow() const;
	bool PickSidestep(const FVector& ThreatLoc, FVector& OutPoint) const;
	bool PickFlankPoint(const FVector& ThreatLoc, FVector& OutPoint, FVector& OutWaypoint);
	bool IsPlayerFacingMe(const APawn* Player) const;
	void FaceLocation(const FVector& Loc);
	FVector ProjectToNav(const FVector& Point) const;
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
	EHostileAlertState AlertState = EHostileAlertState::Relaxed;
	EHostileSquadRole Role = EHostileSquadRole::Closer;
	FVector HomeLocation = FVector::ZeroVector;

	bool bWantFlank = false;
	bool bHasFlank = false;
	bool bFlankWaypointReached = false;
	FVector FlankPoint = FVector::ZeroVector;
	FVector FlankWaypoint = FVector::ZeroVector;
	float NextFlankPickTime = 0.f;
	float FlankCommitUntil = 0.f;

	int32 BurstShotsLeft = 0;
	float BurstResumeTime = 0.f;

	bool bHasRetreatPoint = false;
	FVector RetreatPoint = FVector::ZeroVector;

	float NextFireTime = 0.f;
	float NextPathTime = 0.f;
	float NextPeekTime = 0.f;
	float NextFootSenseTime = 0.f;
	float LostSightAt = -1.f;
	float InvestigateUntil = 0.f;
	float InvestigateMoveAt = 0.f;
	float UnderFireUntil = 0.f;
	float EnteredCoverAt = 0.f;
	float RelocateUntil = 0.f;
	float CoverCooldownUntil = 0.f;
	float SearchUntil = 0.f;
	float NextAlertBroadcastTime = 0.f;
	float SearchLookTimer = 0.f;

	FVector CachedGoal = FVector::ZeroVector;
	FVector LastKnownEnemy = FVector::ZeroVector;
	FVector InvestigatePoint = FVector::ZeroVector;
	FVector CoverPoint = FVector::ZeroVector;
	FVector PeekPoint = FVector::ZeroVector;
	FVector RelocatePoint = FVector::ZeroVector;

	bool bHasLastKnown = false;
	bool bHasInvestigate = false;
	bool bHasCover = false;
	bool bPeeking = false;
	bool bHasRelocate = false;
	float FleeSideSign = 1.f;
};
