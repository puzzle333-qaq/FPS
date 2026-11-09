#include "Game/ShooterPlayerState.h"

#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"

void AShooterPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShooterPlayerState, TeamId);
	DOREPLIFETIME(AShooterPlayerState, bPartyHost);
	DOREPLIFETIME(AShooterPlayerState, PlatformId);
	DOREPLIFETIME(AShooterPlayerState, Kills);
	DOREPLIFETIME(AShooterPlayerState, Deaths);
	DOREPLIFETIME(AShooterPlayerState, Assists);
	DOREPLIFETIME(AShooterPlayerState, RespawnServerTime);
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

void AShooterPlayerState::SetPartyHost(bool bHost)
{
	bPartyHost = bHost;
	ForceNetUpdate();
}

void AShooterPlayerState::SetPlatformId(const FString& InId)
{
	PlatformId = InId.Left(24);
	ForceNetUpdate();
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

void AShooterPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	if (AShooterPlayerState* NewPS = Cast<AShooterPlayerState>(PlayerState))
	{
		NewPS->TeamId = TeamId;
		NewPS->bPartyHost = bPartyHost;
		NewPS->PlatformId = PlatformId;
	}
}
