#pragma once

#include "CoreMinimal.h"
#include "Game/SearchDestroyGameMode.h"
#include "BDMGameMode.generated.h"

// 爆破模式（BDM）：SearchDestroy + 赏金购枪。BP_BDMGameMode 选它，填 BuyCatalog / ModeHudWidgetClass。
UCLASS()
class FPS_API ABDMGameMode : public ASearchDestroyGameMode
{
	GENERATED_BODY()

public:
	ABDMGameMode();
};