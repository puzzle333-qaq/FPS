#include "Game/CoopCqbGameState.h"
#include "Net/UnrealNetwork.h"

void ACoopCqbGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACoopCqbGameState, CoopPhase);
	DOREPLIFETIME(ACoopCqbGameState, PrimaryObjective);
	DOREPLIFETIME(ACoopCqbGameState, WinReason);
	DOREPLIFETIME(ACoopCqbGameState, bOperatorsWon);
	DOREPLIFETIME(ACoopCqbGameState, HostileAliveCount);
	DOREPLIFETIME(ACoopCqbGameState, HostileTotalCount);
}

void ACoopCqbGameState::SetCoopPhase(ECoopCqbPhase NewPhase)
{
	if (!HasAuthority() || CoopPhase == NewPhase) return;
	CoopPhase = NewPhase;
	OnCoopPhaseChanged.Broadcast(CoopPhase);
	ForceNetUpdate();
}

void ACoopCqbGameState::SetPrimaryObjective(ECoopCqbObjectiveType Type)
{
	if (!HasAuthority()) return;
	PrimaryObjective = Type;
	ForceNetUpdate();
}

void ACoopCqbGameState::SetHostileCounts(int32 Alive, int32 Total)
{
	if (!HasAuthority()) return;
	HostileAliveCount = FMath::Max(0, Alive);
	HostileTotalCount = FMath::Max(0, Total);
	OnObjectiveProgress.Broadcast(HostileAliveCount, HostileTotalCount);
	ForceNetUpdate();
}

void ACoopCqbGameState::SetMissionResult(bool bWon, ECoopCqbWinReason Reason)
{
	if (!HasAuthority()) return;
	bOperatorsWon = bWon;
	WinReason = Reason;
	ForceNetUpdate();
}

void ACoopCqbGameState::OnRep_CoopPhase()
{
	OnCoopPhaseChanged.Broadcast(CoopPhase);
}

void ACoopCqbGameState::OnRep_HostileCounts()
{
	OnObjectiveProgress.Broadcast(HostileAliveCount, HostileTotalCount);
}
