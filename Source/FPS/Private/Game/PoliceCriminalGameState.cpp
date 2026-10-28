#include "Game/PoliceCriminalGameState.h"
#include "Net/UnrealNetwork.h"

void APoliceCriminalGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APoliceCriminalGameState, Phase);
	DOREPLIFETIME(APoliceCriminalGameState, MatchType);
	DOREPLIFETIME(APoliceCriminalGameState, SetupEndServerTime);
	DOREPLIFETIME(APoliceCriminalGameState, AssaultEndServerTime);
}

float APoliceCriminalGameState::GetRemainingTime() const
{
	if (Phase == EPoliceCriminalPhase::CriminalSetup)
	{
		return GetSetupRemaining();
	}
	if (Phase == EPoliceCriminalPhase::Assault && AssaultEndServerTime > 0.f)
	{
		return FMath::Max(0.f, AssaultEndServerTime - GetServerWorldTimeSeconds());
	}
	return Super::GetRemainingTime();
}

float APoliceCriminalGameState::GetSetupRemaining() const
{
	if (Phase != EPoliceCriminalPhase::CriminalSetup || SetupEndServerTime <= 0.f) return 0.f;
	return FMath::Max(0.f, SetupEndServerTime - GetServerWorldTimeSeconds());
}

bool APoliceCriminalGameState::IsMovementLocked() const
{
	if (Phase == EPoliceCriminalPhase::PostMatch || Phase == EPoliceCriminalPhase::Waiting) return true;
	return false;
}

bool APoliceCriminalGameState::IsActionLocked() const
{
	return Phase == EPoliceCriminalPhase::PostMatch;
}

bool APoliceCriminalGameState::IsFactionLocked(uint8 TeamId) const
{
	// Police cannot leave staging / cannot freely assault during criminal setup.
	if (Phase == EPoliceCriminalPhase::CriminalSetup && TeamId == PoliceCriminalTeams::Police)
	{
		return true;
	}
	if (Phase == EPoliceCriminalPhase::Waiting || Phase == EPoliceCriminalPhase::PostMatch)
	{
		return true;
	}
	return false;
}

void APoliceCriminalGameState::SetPhase(EPoliceCriminalPhase NewPhase)
{
	Phase = NewPhase;
	OnRep_Phase();
	ForceNetUpdate();
}

void APoliceCriminalGameState::SetMatchType(EPoliceCriminalMatchType Type)
{
	MatchType = Type;
	ForceNetUpdate();
}

void APoliceCriminalGameState::SetSetupEndServerTime(float ServerTime)
{
	SetupEndServerTime = ServerTime;
	ForceNetUpdate();
}

void APoliceCriminalGameState::SetAssaultEndServerTime(float ServerTime)
{
	AssaultEndServerTime = ServerTime;
	SetMatchEndServerTime(ServerTime);
	ForceNetUpdate();
}

void APoliceCriminalGameState::OnRep_Phase()
{
	OnPhaseChanged.Broadcast(Phase);
}
