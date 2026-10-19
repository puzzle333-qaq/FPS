#include "Game/ShooterGameState.h"

#include "Net/UnrealNetwork.h"

void AShooterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShooterGameState, ShooterMatchState);
	DOREPLIFETIME(AShooterGameState, TeamScores);
	DOREPLIFETIME(AShooterGameState, ScoreToWin);
	DOREPLIFETIME(AShooterGameState, MatchEndServerTime);
	DOREPLIFETIME(AShooterGameState, MatchStartServerTime);
	DOREPLIFETIME(AShooterGameState, WinningTeam);
	DOREPLIFETIME(AShooterGameState, bHasMatchHeader);
	DOREPLIFETIME(AShooterGameState, MatchMode);
}

float AShooterGameState::GetRemainingTime() const
{
	if (MatchEndServerTime <= 0.f || ShooterMatchState != EShooterMatchState::InProgress) return 0.f;
	return FMath::Max(0.f, MatchEndServerTime - GetServerWorldTimeSeconds());
}

float AShooterGameState::GetWarmupRemaining() const
{
	if (MatchStartServerTime <= 0.f || ShooterMatchState != EShooterMatchState::WaitingToStart) return 0.f;
	return FMath::Max(0.f, MatchStartServerTime - GetServerWorldTimeSeconds());
}

void AShooterGameState::SetMatchRules(EShooterGameMode Mode)
{
	MatchMode = Mode;
	bHasMatchHeader = true;
	OnRep_MatchRules();
	ForceNetUpdate();
}

void AShooterGameState::OnRep_MatchRules()
{
	OnMatchModeChanged.Broadcast(MatchMode);
}

void AShooterGameState::InitTeams(int32 NumTeams, int32 InScoreToWin)
{
	TeamScores.Init(0, FMath::Max(NumTeams, 0));
	ScoreToWin = InScoreToWin;
	OnRep_TeamScores();
}

void AShooterGameState::AddTeamScore(uint8 TeamId, int32 Delta)
{
	if (!TeamScores.IsValidIndex(TeamId)) return;
	TeamScores[TeamId] += Delta;
	OnRep_TeamScores();
}

void AShooterGameState::SetMatchState(EShooterMatchState NewState)
{
	if (ShooterMatchState == NewState) return;
	ShooterMatchState = NewState;
	OnRep_MatchState();
	ForceNetUpdate();
}

void AShooterGameState::OnRep_MatchState()
{
	OnMatchStateChanged.Broadcast(ShooterMatchState);
	if (ShooterMatchState == EShooterMatchState::PostMatch)
	{
		OnMatchEnded.Broadcast(WinningTeam);
	}
}

void AShooterGameState::OnRep_TeamScores()
{
	for (int32 i = 0; i < TeamScores.Num(); ++i)
	{
		if (!LastBroadcastScores.IsValidIndex(i) || LastBroadcastScores[i] != TeamScores[i])
		{
			OnTeamScoreChanged.Broadcast(static_cast<uint8>(i), TeamScores[i]);
		}
	}
	LastBroadcastScores = TeamScores;
}

void AShooterGameState::Multicast_KillFeed_Implementation(const FString& KillerName, uint8 KillerTeam, const FString& VictimName, uint8 VictimTeam, UTexture2D* WeaponIcon)
{
	OnKillFeed.Broadcast(KillerName, KillerTeam, VictimName, VictimTeam, WeaponIcon);
}