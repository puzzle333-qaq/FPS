#pragma once

#include "CoreMinimal.h"
#include "Game/ShooterGameState.h"
#include "Game/CoopCqbTypes.h"
#include "Game/MissionTypes.h"
#include "CoopCqbGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCoopCqbPhaseChanged, ECoopCqbPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCoopCqbObjectiveProgress, int32, RemainingHostiles, int32, TotalHostiles);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMissionUpdated);

UCLASS()
class FPS_API ACoopCqbGameState : public AShooterGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "FPS|CoopCQB")
	ECoopCqbPhase GetCoopPhase() const { return CoopPhase; }

	UFUNCTION(BlueprintPure, Category = "FPS|CoopCQB")
	ECoopCqbObjectiveType GetPrimaryObjective() const { return PrimaryObjective; }

	UFUNCTION(BlueprintPure, Category = "FPS|CoopCQB")
	ECoopCqbWinReason GetWinReason() const { return WinReason; }

	UFUNCTION(BlueprintPure, Category = "FPS|CoopCQB")
	bool DidOperatorsWin() const { return bOperatorsWon; }

	UFUNCTION(BlueprintPure, Category = "FPS|CoopCQB")
	int32 GetHostileAliveCount() const { return HostileAliveCount; }

	UFUNCTION(BlueprintPure, Category = "FPS|CoopCQB")
	int32 GetHostileTotalCount() const { return HostileTotalCount; }

	UFUNCTION(BlueprintPure, Category = "FPS|CoopCQB")
	bool IsMissionLive() const { return CoopPhase == ECoopCqbPhase::InProgress; }

	UFUNCTION(BlueprintPure, Category = "FPS|CoopCQB")
	float GetEvacSecondsRemaining() const;

	UFUNCTION(BlueprintPure, Category = "FPS|CoopCQB")
	bool IsDebriefOpen() const { return bDebriefOpen; }

	UPROPERTY(BlueprintAssignable, Category = "FPS|CoopCQB")
	FCoopCqbPhaseChanged OnCoopPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "FPS|CoopCQB")
	FCoopCqbObjectiveProgress OnObjectiveProgress;

	virtual bool IsMovementLocked() const override { return !bRangeHub && CoopPhase != ECoopCqbPhase::InProgress; }
 virtual bool IsActionLocked() const override { return IsMovementLocked(); }
 UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="Mission") FMissionReport Report;
 UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="Mission") TArray<FText> ObjectiveDescriptions;
 UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="Mission") bool bRangeHub = false;
 UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="Mission") int32 ExpectedOperators = 1;
 UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="Mission") int32 ReadyOperators = 0;
 UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="Mission") FString MissionStatus;
 UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="Mission") float ReturnEndServerTime = -1.f;
 UPROPERTY(BlueprintAssignable, Category="Mission") FMissionUpdated OnMissionUpdated;
 UFUNCTION() void OnRep_Mission();
 void PublishMission() { OnRep_Mission(); ForceNetUpdate(); }

 // Server
	void SetCoopPhase(ECoopCqbPhase NewPhase);
	void SetPrimaryObjective(ECoopCqbObjectiveType Type);
	void SetHostileCounts(int32 Alive, int32 Total);
	void SetMissionResult(bool bWon, ECoopCqbWinReason Reason);
	void BeginEvac(float DelaySeconds);
	void OpenDebrief();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CoopPhase)
	ECoopCqbPhase CoopPhase = ECoopCqbPhase::Waiting;

	UPROPERTY(Replicated)
	ECoopCqbObjectiveType PrimaryObjective = ECoopCqbObjectiveType::ClearEnemies;

	UPROPERTY(Replicated)
	ECoopCqbWinReason WinReason = ECoopCqbWinReason::None;

	UPROPERTY(Replicated)
	bool bOperatorsWon = false;

	UPROPERTY(ReplicatedUsing = OnRep_HostileCounts)
	int32 HostileAliveCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_HostileCounts)
	int32 HostileTotalCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_EvacState)
	float EvacEndServerTime = -1.f;

	UPROPERTY(ReplicatedUsing = OnRep_EvacState)
	bool bDebriefOpen = false;

	UFUNCTION()
	void OnRep_CoopPhase();

	UFUNCTION()
	void OnRep_HostileCounts();

	UFUNCTION()
	void OnRep_EvacState();
};
