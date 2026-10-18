#include "Menu/MainMenuGameMode.h"
#include "Menu/MainMenuPlayerController.h"

AMainMenuGameMode::AMainMenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AMainMenuPlayerController::StaticClass();
}