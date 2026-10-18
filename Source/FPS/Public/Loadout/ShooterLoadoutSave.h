#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Loadout/LoadoutTypes.h"
#include "ShooterLoadoutSave.generated.h"

// 存在硬盘上的配装
UCLASS()
class FPS_API UShooterLoadoutSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "FPS|Loadout")
	TArray<FLoadoutPreset> Presets;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "FPS|Loadout")
	int32 SelectedIndex = 0;
};