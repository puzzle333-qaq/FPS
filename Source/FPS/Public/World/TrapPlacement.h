#pragma once

#include "CoreMinimal.h"

class APawn;
class AController;
class UWorld;

/** Shared placement rules for criminal traps (tripwire / gas can). */
namespace FPSTrapPlacement
{
	/** True if pawn may place traps now. OutError optional Chinese reason. */
	bool CanPlace(APawn* Pawn, bool bIgnoreSetup, FString* OutError);

	AController* GetController(APawn* Pawn);
}
