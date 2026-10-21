#pragma once

#include "CoreMinimal.h"
#include "Game/SearchDestroyGameState.h"
#include "BDMGameState.generated.h"

// 爆破（BDM）GameState：继承 SearchDestroy，方便蓝图按 BDM 名字选父类。
UCLASS()
class FPS_API ABDMGameState : public ASearchDestroyGameState
{
	GENERATED_BODY()
};