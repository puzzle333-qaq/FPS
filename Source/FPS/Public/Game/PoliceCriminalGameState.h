#pragma once

#include "CoreMinimal.h"
#include "Game/ShooterGameState.h"
#include "Game/PoliceCriminalTypes.h"
#include "PoliceCriminalGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPoliceCriminalPhaseChanged, EPoliceCriminalPhase, NewPhase);

UCLASS()
class FPS_API APoliceCriminalGameState : public AShooterGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float GetRemainingTime() const override;
	virtual bool IsMovementLocked() const override;
	virtual bool IsActionLocked() const override;

	UFUNCTION(BlueprintPure, Category = "FPS|PoliceCriminal")
	EPoliceCriminalPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "FPS|PoliceCriminal")
	EPoliceCriminalMatchType GetMatchType() const { return MatchType; }

	UFUNCTION(BlueprintPure, Category = "FPS|PoliceCriminal")
	float GetSetupRemaining() const;

	UFUNCTION(BlueprintPure, Category = "FPS|PoliceCriminal")
	bool IsCriminalSetup() const { return Phase == EPoliceCriminalPhase::CriminalSetup; }

	UFUNCTION(BlueprintPure, Category = "FPS|PoliceCriminal")
	bool IsAssaultLive() const { return Phase == EPoliceCriminalPhase::Assault; }

	UFUNCTION(BlueprintPure, Category = "FPS|PoliceCriminal")
	EPoliceCriminalWinReason GetLastWinReason() const { return LastWinReason; }

	/** Police locked during criminal setup; both free in assault. */
	UFUNCTION(BlueprintPure, Category = "FPS|PoliceCriminal")
	bool IsFactionLocked(uint8 TeamId) const;

	UPROPERTY(BlueprintAssignable, Category = "FPS|PoliceCriminal")
	FPoliceCriminalPhaseChanged OnPhaseChanged;

	// Server
	void SetPhase(EPoliceCriminalPhase NewPhase);
	void SetMatchType(EPoliceCriminalMatchType Type);
	void SetSetupEndServerTime(float ServerTime);
	void SetAssaultEndServerTime(float ServerTime);
	void SetLastWinReason(EPoliceCriminalWinReason Reason);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	EPoliceCriminalPhase Phase = EPoliceCriminalPhase::Waiting;

	UPROPERTY(Replicated)
	EPoliceCriminalMatchType MatchType = EPoliceCriminalMatchType::Elimination;

	UPROPERTY(Replicated)
	float SetupEndServerTime = 0.f;

	UPROPERTY(Replicated)
	float AssaultEndServerTime = 0.f;

	UPROPERTY(Replicated)
	EPoliceCriminalWinReason LastWinReason = EPoliceCriminalWinReason::None;

	UFUNCTION()
	void OnRep_Phase();
};
