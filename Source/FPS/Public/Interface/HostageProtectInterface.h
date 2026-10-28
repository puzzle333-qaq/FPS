#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HostageProtectInterface.generated.h"

// Marker: actors that should lower the shooter muzzle when aimed at close range.
// Future hostage Blueprints/C++ implement this. Teammates use team id instead.
UINTERFACE(MinimalAPI)
class UHostageProtectInterface : public UInterface
{
	GENERATED_BODY()
};

class FPS_API IHostageProtectInterface
{
	GENERATED_BODY()
};
