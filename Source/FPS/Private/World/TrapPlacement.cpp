#include "World/TrapPlacement.h"

#include "Game/PoliceCriminalGameState.h"
#include "Game/PoliceCriminalTypes.h"
#include "Game/ShooterPlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AController* FPSTrapPlacement::GetController(APawn* Pawn)
{
	return IsValid(Pawn) ? Pawn->GetController() : nullptr;
}

bool FPSTrapPlacement::CanPlace(APawn* Pawn, bool bIgnoreSetup, FString* OutError)
{
	auto Fail = [&](const TCHAR* Msg) -> bool
	{
		if (OutError) *OutError = Msg;
		return false;
	};

	if (!IsValid(Pawn))
	{
		return Fail(TEXT("没有角色"));
	}

	UWorld* World = Pawn->GetWorld();
	if (!World)
	{
		return Fail(TEXT("无效世界"));
	}

	APoliceCriminalGameState* GS = World->GetGameState<APoliceCriminalGameState>();
	if (!GS)
	{
		// Range / non-PC modes: allow free placement for PIE.
		return true;
	}

	AController* C = GetController(Pawn);
	AShooterPlayerState* PS = C ? C->GetPlayerState<AShooterPlayerState>() : nullptr;
	if (!PS || !PS->HasTeam())
	{
		return Fail(TEXT("先 PickFaction 1 选匪"));
	}
	if (PS->GetTeamId() != PoliceCriminalTeams::Criminal)
	{
		return Fail(TEXT("只有匪徒可以布置陷阱"));
	}

	if (bIgnoreSetup)
	{
		return true;
	}

	if (GS->GetPhase() != EPoliceCriminalPhase::CriminalSetup)
	{
		return Fail(TEXT("只能在匪徒布置阶段放置（或用 SpawnTripwire/SpawnGasCan 测试）"));
	}
	return true;
}
