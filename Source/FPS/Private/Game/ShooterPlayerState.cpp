#include "Game/ShooterPlayerState.h"

#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"

void AShooterPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShooterPlayerState, TeamId);
	DOREPLIFETIME(AShooterPlayerState, Kills);
	DOREPLIFETIME(AShooterPlayerState, Deaths);
	DOREPLIFETIME(AShooterPlayerState, Assists);
	DOREPLIFETIME(AShooterPlayerState, RespawnServerTime);
	DOREPLIFETIME(AShooterPlayerState, bHasBomb);
	DOREPLIFETIME(AShooterPlayerState, Money);
}

float AShooterPlayerState::GetRespawnRemaining() const
{
	if (RespawnServerTime <= 0.f) return 0.f;
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const float Now = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	return FMath::Max(0.f, RespawnServerTime - Now);
}

void AShooterPlayerState::SetTeamId(uint8 NewTeam)
{
	TeamId = NewTeam;
	OnRep_TeamId();
}

void AShooterPlayerState::AddKill()
{
	++Kills;
	OnRep_Stats();
}

void AShooterPlayerState::AddDeath()
{
	++Deaths;
	OnRep_Stats();
}

void AShooterPlayerState::AddAssist()
{
	++Assists;
	OnRep_Stats();
	ForceNetUpdate();
}

void AShooterPlayerState::SetRespawnServerTime(float ServerTime)
{
	RespawnServerTime = ServerTime;
	ForceNetUpdate();
}

void AShooterPlayerState::OnRep_TeamId()
{
	OnTeamChanged.Broadcast(TeamId);
}

void AShooterPlayerState::OnRep_Stats()
{
	OnStatsChanged.Broadcast(Kills, Deaths);
}

void AShooterPlayerState::SetHasBomb(bool bInHasBomb)
{
	bHasBomb = bInHasBomb;
	OnRep_HasBomb();
	ForceNetUpdate();
}

void AShooterPlayerState::OnRep_HasBomb()
{
	OnBombCarryChanged.Broadcast(bHasBomb);
}

void AShooterPlayerState::SetMoney(int32 NewMoney)
{
	Money = FMath::Max(0, NewMoney);
	OnRep_Money();
	ForceNetUpdate();
}

void AShooterPlayerState::AddMoney(int32 Delta)
{
	SetMoney(Money + Delta);
}

void AShooterPlayerState::OnRep_Money()
{
	OnMoneyChanged.Broadcast(Money);
}