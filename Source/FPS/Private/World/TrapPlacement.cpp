#include "World/TrapPlacement.h"

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

	if (!Pawn->GetWorld())
	{
		return Fail(TEXT("无效世界"));
	}

	(void)bIgnoreSetup;
	return true;
}
