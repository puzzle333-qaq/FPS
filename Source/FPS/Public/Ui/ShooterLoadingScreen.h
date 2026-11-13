#pragma once

#include "CoreMinimal.h"

class UGameInstance;

/** Map-travel loading chrome (MoviePlayer + PIE viewport fallback). Hook from GameInstance Init/Shutdown. */
namespace ShooterLoadingScreen
{
	void Register(UGameInstance* GameInstance);
	void Unregister();
	void Hide();
 FString GetTaskName(const FString& MapPath);
 FString GetMapName(const FString& MapPath);
}
