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
	DOREPLIFETIME(AShooterPlayerState, bReady);
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
void AShooterPlayerState::SetReady(bool bInReady)
{
	bReady = bInReady;
	OnRep_Ready();
	ForceNetUpdate();
}

void AShooterPlayerState::OnRep_Ready()
{
	OnReadyChanged.Broadcast(bReady ? 1 : 0);
}

void AShooterPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	if (AShooterPlayerState* NewPS = Cast<AShooterPlayerState>(PlayerState))
	{
		NewPS->TeamId = TeamId;
		NewPS->JoinOrder = JoinOrder;
		// 准备状态、战绩、金钱不带（回大厅/进对局都重新开始）
	}
}
