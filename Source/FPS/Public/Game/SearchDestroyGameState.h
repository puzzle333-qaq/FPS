#pragma once

#include "CoreMinimal.h"
#include "Game/ShooterGameState.h"
#include "Game/SearchDestroyTypes.h"
#include "SearchDestroyGameState.generated.h"

class ABombActor;
class ABombSite;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBombStateChanged, EBombState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBombRoundPhaseChanged, EBombRoundPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBombRoundEnded, uint8, WinningTeam, ERoundWinReason, Reason);

UCLASS()
class FPS_API ASearchDestroyGameState : public AShooterGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float GetRemainingTime() const override;
	virtual bool IsMovementLocked() const override;
	virtual bool IsActionLocked() const override;

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	int32 GetRoundIndex() const { return RoundIndex; }

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	int32 GetRoundsToWin() const { return GetScoreToWin(); }

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	uint8 GetAttackerTeam() const { return AttackerTeam; }

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	uint8 GetDefenderTeam() const { return AttackerTeam == 0 ? 1 : 0; }

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	bool IsAttackingTeam(uint8 TeamId) const { return TeamId == AttackerTeam; }

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	EBombState GetBombState() const { return BombState; }

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	EBombSiteId GetPlantedSite() const { return PlantedSite; }

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	EBombRoundPhase GetRoundPhase() const { return RoundPhase; }

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	ERoundWinReason GetLastWinReason() const { return LastWinReason; }

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	uint8 GetRoundWinningTeam() const { return RoundWinningTeam; }

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	ABombActor* GetBomb() const { return Bomb; }

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	float GetActionProgress() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	bool IsBombDown() const { return BombState == EBombState::Planted || BombState == EBombState::Defusing; }

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	EBombPrompt GetPromptForPawn(const APawn* Pawn) const;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Bomb")
	FBombStateChanged OnBombStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Bomb")
	FBombRoundPhaseChanged OnRoundPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Bomb")
	FBombRoundEnded OnRoundEnded;

	// ---- 只在服务器调用 ----
	void SetRoundIndex(int32 Index);
	void SetAttackerTeam(uint8 Team);
	void SetBombState(EBombState NewState);
	void SetPlantedSite(EBombSiteId Site);
	void SetRoundPhase(EBombRoundPhase NewPhase);
	void SetLastWinReason(ERoundWinReason Reason);
	void SetRoundWinningTeam(uint8 Team);
	void SetBomb(ABombActor* InBomb);
	void SetFreezeEndServerTime(float ServerTime);
	void SetBombExplodeServerTime(float ServerTime);
	void SetActionClock(float StartServerTime, float Duration);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_RoundIndex)
	int32 RoundIndex = 0;

	UPROPERTY(ReplicatedUsing = OnRep_AttackerTeam)
	uint8 AttackerTeam = 0;

	UPROPERTY(ReplicatedUsing = OnRep_BombState)
	EBombState BombState = EBombState::Idle;

	UPROPERTY(Replicated)
	EBombSiteId PlantedSite = EBombSiteId::None;

	UPROPERTY(ReplicatedUsing = OnRep_RoundPhase)
	EBombRoundPhase RoundPhase = EBombRoundPhase::Warmup;

	UPROPERTY(Replicated)
	ERoundWinReason LastWinReason = ERoundWinReason::None;

	UPROPERTY(Replicated)
	uint8 RoundWinningTeam = 255;

	UPROPERTY(Replicated)
	TObjectPtr<ABombActor> Bomb;

	UPROPERTY(Replicated)
	float FreezeEndServerTime = 0.f;

	UPROPERTY(Replicated)
	float BombExplodeServerTime = 0.f;

	UPROPERTY(Replicated)
	float ActionStartServerTime = 0.f;

	UPROPERTY(Replicated)
	float ActionDuration = 0.f;

	UFUNCTION()
	void OnRep_RoundIndex();

	UFUNCTION()
	void OnRep_AttackerTeam();

	UFUNCTION()
	void OnRep_BombState();

	UFUNCTION()
	void OnRep_RoundPhase();
};
