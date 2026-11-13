#pragma once

#include "CoreMinimal.h"
#include "ShooterGameModeBase.h"
#include "Game/AINoiseEvent.h"
#include "Game/CoopCqbTypes.h"
#include "Game/MissionTypes.h"
#include "CoopCqbGameMode.generated.h"

class AWeapon;
class ACoopCqbGameState;
class AHostileSpawner;
class AHostileAICharacter;

/**
 * 4-player coop CQB PVE skeleton.
 * - Players = Operators (team 0), police Bandits look + MVP police kit for now
 * - Missing slots stay empty (AI teammates later)
 * - Default objective: ClearEnemies
 * - Hostiles come from AHostileSpawner actors placed in the level
 */
UCLASS()
class FPS_API ACoopCqbGameMode : public AShooterGameModeBase
{
	GENERATED_BODY()
 friend class FMissionObjectiveFlowTest;
 friend class FMissionLoadBarrierTest;

public:
	ACoopCqbGameMode();
 virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
 virtual void Logout(AController* Exiting) override;
 void Auth_OperatorReady(APlayerController* PC);
 void Auth_ReturnToRange();
 UPROPERTY(EditDefaultsOnly, Category="Mission") FMissionScoreRules ScoreRules;
 UPROPERTY(EditDefaultsOnly, Category="Mission", meta=(ClampMin="5")) float DebriefDuration = 20.f;
 UPROPERTY(EditDefaultsOnly, Category="Mission", meta=(ClampMin="10")) float LoadTimeout = 120.f;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|CoopCQB", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxOperators = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|CoopCQB")
	float BriefingDuration = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|CoopCQB")
	float PostMissionDelay = 8.f;

	/** No friendly fire between operators. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|CoopCQB")
	bool bFriendlyFire = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|CoopCQB")
	ECoopCqbObjectiveType PrimaryObjective = ECoopCqbObjectiveType::ClearEnemies;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|CoopCQB")
	FName OperatorStartTag = TEXT("TeamA");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|CoopCQB")
	FName OperatorStartTagAlt = TEXT("Police");

	/** Soft paths for the operator primary and sidearm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|CoopCQB|Loadout")
	TArray<TSoftClassPtr<AWeapon>> OperatorWeapons;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|CoopCQB|Loadout")
	TArray<TSoftClassPtr<AWeapon>> OperatorThrowables;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|CoopCQB|Loadout", meta = (ClampMin = "0", ClampMax = "2"))
	int32 OperatorArmorLevel = 1;

	/** If no AHostileSpawner in level, spawn this many near tagged starts (debug). 0 = none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|CoopCQB|AI", meta = (ClampMin = "0"))
	int32 FallbackHostileCount = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|CoopCQB|AI")
	TSubclassOf<AHostileAICharacter> HostileClass;

	virtual void InitGameState() override;
	virtual void StartPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual bool CanDamage(AController* InstigatorController, AController* VictimController) const override;

	/** Called by hostile when it dies (server). */
	void NotifyHostileKilled(AHostileAICharacter* Hostile);

	/** Refresh living hostile count + check ClearEnemies. */
	void Auth_RefreshObjectives();

	void NotifyAlarmRaised();
	void NotifyPlayersDetected();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|CoopCQB|AI|Stealth")
	FAINoiseTuning NoiseTuning;

	UFUNCTION(BlueprintCallable, Category = "FPS|CoopCQB")
	void Auth_AbortMission();
 void Auth_DeploymentViewReady(APlayerController* PC);

protected:
	virtual void ScoreKill(AShooterPlayerState* KillerPS, AShooterPlayerState* VictimPS) override;
	virtual bool CanRespawn(AController* Controller) const override;
	virtual void OnPlayerKilled(AController* KillerController, AController* VictimController) override;

private:
	// Soft references let the cooker discover defaults without loading cinematic
	// camera subobjects before the engine's typed-element registry exists.
	UPROPERTY() TSoftClassPtr<AHostileAICharacter> DefaultHostileBlueprint;
	UPROPERTY() TSoftClassPtr<APawn> DefaultPawnBlueprint;
	UPROPERTY() TSoftClassPtr<APlayerController> DefaultControllerBlueprint;
	void ResolveLoadout();
	void BeginMissionFlow();
	void StartBriefing();
	void StartMission();
	void EndMission(bool bOperatorsWon, ECoopCqbWinReason Reason);
	void OpenDebrief();
 void CheckOperatorReadiness();
 TSet<TWeakObjectPtr<APlayerController>> DeploymentViewReadyPlayers;
 void UpdateReport(bool bFinal = false, bool bWon = false);
 bool bRangeHub = false;
 int32 ExpectedOperators = 1;
 float LoadStartedAt = 0.f;
 TSet<TWeakObjectPtr<APlayerController>> ReadyPlayers;
 TArray<TWeakObjectPtr<AHostileAICharacter>> MissionSuspects;
 TArray<TWeakObjectPtr<class AHostageActor>> MissionHostages;
 TArray<TWeakObjectPtr<class AMissionObjective>> MissionObjectives;
 TSet<TWeakObjectPtr<AHostileAICharacter>> UnlawfulKills;
 TSet<TWeakObjectPtr<AHostileAICharacter>> NeutralizedSuspects;
 TSet<TWeakObjectPtr<AHostileAICharacter>> ArrestedSuspects;
 TSet<TWeakObjectPtr<class AMissionObjective>> OptionalObjectives;
 TSet<TWeakObjectPtr<class AMissionObjective>> CompletedObjectives;
 FTimerHandle ReadinessTimer;
 FTimerHandle ReturnTimer;
	void SpawnHostilesFromLevel();
	void ApplyOperatorLookAndKit(APlayerController* PC);
	AActor* FindTaggedStart(FName Tag) const;
	int32 CountLivingOperators() const;
	int32 CountLivingHostiles() const;

	UPROPERTY(Transient)
	TArray<TSubclassOf<AWeapon>> ResolvedWeapons;

	UPROPERTY(Transient)
	TArray<TSubclassOf<AWeapon>> ResolvedThrowables;

	FTimerHandle BriefingTimer;
	FTimerHandle PostMissionTimer;
	bool bFlowStarted = false;
	bool bMissionEnded = false;
};
