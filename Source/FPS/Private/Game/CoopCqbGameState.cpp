#include "Game/CoopCqbGameState.h"
#include "Net/UnrealNetwork.h"

void ACoopCqbGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACoopCqbGameState, Report);
 DOREPLIFETIME(ACoopCqbGameState, ObjectiveDescriptions);
 DOREPLIFETIME(ACoopCqbGameState, bRangeHub);
 DOREPLIFETIME(ACoopCqbGameState, ExpectedOperators);
 DOREPLIFETIME(ACoopCqbGameState, ReadyOperators);
 DOREPLIFETIME(ACoopCqbGameState, MissionStatus);
 DOREPLIFETIME(ACoopCqbGameState, ReturnEndServerTime);
 DOREPLIFETIME(ACoopCqbGameState, CoopPhase);
	DOREPLIFETIME(ACoopCqbGameState, PrimaryObjective);
	DOREPLIFETIME(ACoopCqbGameState, WinReason);
	DOREPLIFETIME(ACoopCqbGameState, bOperatorsWon);
	DOREPLIFETIME(ACoopCqbGameState, HostileAliveCount);
	DOREPLIFETIME(ACoopCqbGameState, HostileTotalCount);
	DOREPLIFETIME(ACoopCqbGameState, EvacEndServerTime);
	DOREPLIFETIME(ACoopCqbGameState, bDebriefOpen);
}

void ACoopCqbGameState::SetCoopPhase(ECoopCqbPhase NewPhase)
{
	if (!HasAuthority() || CoopPhase == NewPhase) return;
	CoopPhase = NewPhase;
	OnCoopPhaseChanged.Broadcast(CoopPhase);
 OnRep_Mission();
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

void ACoopCqbGameState::BeginEvac(float DelaySeconds)
{
	if (!HasAuthority()) return;
	EvacEndServerTime = GetServerWorldTimeSeconds() + FMath::Max(0.f, DelaySeconds);
	bDebriefOpen = false;
	ForceNetUpdate();
}

void ACoopCqbGameState::OpenDebrief()
{
	if (!HasAuthority()) return;
	bDebriefOpen = true;
 OnRep_Mission();
	ForceNetUpdate();
}

float ACoopCqbGameState::GetEvacSecondsRemaining() const
{
	if (bDebriefOpen || EvacEndServerTime < 0.f) return -1.f;
	return FMath::Max(0.f, EvacEndServerTime - GetServerWorldTimeSeconds());
}

void ACoopCqbGameState::OnRep_CoopPhase()
{
	OnCoopPhaseChanged.Broadcast(CoopPhase);
 OnRep_Mission();
}

void ACoopCqbGameState::OnRep_HostileCounts()
{
	OnObjectiveProgress.Broadcast(HostileAliveCount, HostileTotalCount);
}

void ACoopCqbGameState::OnRep_EvacState()
{
 OnRep_Mission();
}

void ACoopCqbGameState::OnRep_Mission()
{
 OnMissionUpdated.Broadcast();
}
