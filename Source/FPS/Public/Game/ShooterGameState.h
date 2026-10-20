#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Loadout/LoadoutTypes.h"
#include "Engine/Texture2D.h"
#include "ShooterGameState.generated.h"

UENUM(BlueprintType)
enum class EShooterMatchState : uint8
{
	WaitingToStart,
	InProgress,
	PostMatch
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTeamScoreChanged, uint8, TeamId, int32, NewScore);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMatchStateChanged, EShooterMatchState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMatchEnded, uint8, WinningTeam);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMatchModeChanged, EShooterGameMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FKillFeed, const FString&, KillerName, uint8, KillerTeam, const FString&, VictimName, uint8, VictimTeam, UTexture2D*, WeaponIcon);

// 比赛状态（所有人都能读）：比分、剩余时间、胜方、击杀播报
UCLASS()
class FPS_API AShooterGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	EShooterMatchState GetMatchState() const { return ShooterMatchState; }

	// 队伍分数（没有这个队返回 0）
	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	int32 GetTeamScore(uint8 TeamId) const { return TeamScores.IsValidIndex(TeamId) ? TeamScores[TeamId] : 0; }

	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	int32 GetNumTeams() const { return TeamScores.Num(); }

	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	int32 GetScoreToWin() const { return ScoreToWin; }

	// 比赛剩余秒数（没限时或没开始返回 0）。爆破模式在回合冻结/拆弹时也会走这里。
	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	virtual float GetRemainingTime() const;

	// 暖身剩余秒数（已开赛或没设暖身返回 0）
	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	float GetWarmupRemaining() const;

	// 冻结回合：不能移动、开火、安放/拆除
	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	virtual bool IsMovementLocked() const { return ShooterMatchState == EShooterMatchState::PostMatch; }

	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	virtual bool IsActionLocked() const { return ShooterMatchState == EShooterMatchState::PostMatch; }

	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	bool HasTimeLimit() const { return MatchEndServerTime > 0.f; }

	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	bool HasMatchHeader() const { return bHasMatchHeader; }

	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	EShooterGameMode GetMatchMode() const { return MatchMode; }

	// 255 = 平局 / 还没结束
	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	uint8 GetWinningTeam() const { return WinningTeam; }

	UPROPERTY(BlueprintAssignable, Category = "FPS|Match")
	FTeamScoreChanged OnTeamScoreChanged;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Match")
	FMatchStateChanged OnMatchStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Match")
	FMatchEnded OnMatchEnded;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Match")
	FMatchModeChanged OnMatchModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Match")
	FKillFeed OnKillFeed;

	// ---- 只在服务器调用 ----
	void InitTeams(int32 NumTeams, int32 InScoreToWin);
	void AddTeamScore(uint8 TeamId, int32 Delta);
	void SetMatchState(EShooterMatchState NewState);
	void SetMatchEndServerTime(float ServerTime) { MatchEndServerTime = ServerTime; }
	void SetMatchStartServerTime(float ServerTime) { MatchStartServerTime = ServerTime; }
	void SetWinningTeam(uint8 Team) { WinningTeam = Team; }
	void SetMatchRules(EShooterGameMode Mode);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_KillFeed(const FString& KillerName, uint8 KillerTeam, const FString& VictimName, uint8 VictimTeam, UTexture2D* WeaponIcon);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_MatchState)
	EShooterMatchState ShooterMatchState = EShooterMatchState::WaitingToStart;

	UPROPERTY(ReplicatedUsing = OnRep_TeamScores)
	TArray<int32> TeamScores;

	UPROPERTY(Replicated)
	int32 ScoreToWin = 0;

	UPROPERTY(Replicated)
	float MatchEndServerTime = 0.f;

	UPROPERTY(Replicated)
	float MatchStartServerTime = 0.f;

	UPROPERTY(Replicated)
	uint8 WinningTeam = 255;

	UPROPERTY(ReplicatedUsing = OnRep_MatchRules)
	bool bHasMatchHeader = false;

	UPROPERTY(ReplicatedUsing = OnRep_MatchRules)
	EShooterGameMode MatchMode = EShooterGameMode::TDM;

	UFUNCTION()
	void OnRep_MatchState();

	UFUNCTION()
	void OnRep_TeamScores();

	UFUNCTION()
	void OnRep_MatchRules();

private:
	TArray<int32> LastBroadcastScores;
};