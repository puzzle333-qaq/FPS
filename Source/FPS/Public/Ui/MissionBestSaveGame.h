#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "MissionBestSaveGame.generated.h"

UCLASS()
class FPS_API UMissionBestSaveGame : public USaveGame
{
 GENERATED_BODY()
public:
 UPROPERTY(SaveGame) TMap<FString,float> BestSeconds;
 UPROPERTY(SaveGame) TMap<FString,int32> BestScores;
};
