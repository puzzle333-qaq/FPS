#include "Game/BDMGameMode.h"
#include "Game/BDMGameState.h"

ABDMGameMode::ABDMGameMode()
{
	GameStateClass = ABDMGameState::StaticClass();
	// CS 风格：冻结阶段 = 购买窗口
	BuyTime = 20.f;
	FreezeTime = 20.f;
	StartMoney = 800;
}