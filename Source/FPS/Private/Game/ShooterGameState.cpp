#include "Game/ShooterGameState.h"

#include "Game/ShooterPlayerState.h"
#include "Stats/StatsComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

void AShooterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShooterGameState, ShooterMatchState);
	DOREPLIFETIME(AShooterGameState, TeamScores);
	DOREPLIFETIME(AShooterGameState, TeamAliveCounts);
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
	const int32 N = FMath::Max(NumTeams, 0);
	TeamScores.Init(0, N);
	TeamAliveCounts.Init(0, N);
	ScoreToWin = InScoreToWin;
	OnRep_TeamScores();
	OnRep_TeamAliveCounts();
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

double AShooterGameState::GetServerWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (World && bHasSyncedServerTime && GetNetMode() == NM_Client)
	{
		return World->GetTimeSeconds() + SyncedServerTimeOffset;
	}
	return Super::GetServerWorldTimeSeconds();
}

int32 AShooterGameState::GetTeamPlayerCount(uint8 TeamId) const
{
	int32 Count = 0;
	for (APlayerState* PS : PlayerArray)
	{
		const AShooterPlayerState* SPS = Cast<AShooterPlayerState>(PS);
		if (SPS && !SPS->IsInactive() && SPS->GetTeamId() == TeamId)
		{
			++Count;
		}
	}
	return Count;
}

void AShooterGameState::RefreshTeamAliveCounts()
{
	if (!HasAuthority()) return;

	const int32 NumTeams = FMath::Max(TeamScores.Num(), 2);
	if (TeamAliveCounts.Num() != NumTeams)
	{
		TeamAliveCounts.SetNumZeroed(NumTeams);
	}

	TArray<int32> Next;
	Next.Init(0, NumTeams);
	for (APlayerState* PS : PlayerArray)
	{
		const AShooterPlayerState* SPS = Cast<AShooterPlayerState>(PS);
		if (!SPS || SPS->IsInactive() || !SPS->HasTeam()) continue;
		const uint8 TeamId = SPS->GetTeamId();
		if (!Next.IsValidIndex(TeamId)) continue;

		const APawn* Pawn = SPS->GetPawn();
		if (!IsValid(Pawn)) continue;
		const UStatsComponent* Stats = UStatsComponent::FindStatsComponent(Pawn);
		if (!Stats || Stats->DeathState == EDeathState::NotDead)
		{
			++Next[TeamId];
		}
	}

	const bool bChanged = Next != TeamAliveCounts;
	TeamAliveCounts = MoveTemp(Next);
	if (bChanged)
	{
		OnRep_TeamAliveCounts();
		ForceNetUpdate();
	}
}

void AShooterGameState::OnRep_TeamAliveCounts()
{
	for (int32 i = 0; i < TeamAliveCounts.Num(); ++i)
	{
		if (!LastBroadcastAlive.IsValidIndex(i) || LastBroadcastAlive[i] != TeamAliveCounts[i])
		{
			OnTeamAliveChanged.Broadcast(static_cast<uint8>(i), TeamAliveCounts[i]);
		}
	}
	LastBroadcastAlive = TeamAliveCounts;
}
