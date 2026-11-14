#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Character/PoliceAppearanceTypes.h"
#include "PoliceAppearanceSaveGame.generated.h"

UCLASS()
class FPS_API UPoliceAppearanceSaveGame : public USaveGame
{
 GENERATED_BODY()
public:
 UPROPERTY(SaveGame) int32 Version=1;
 UPROPERTY(SaveGame) EPoliceFaceProtection Face=EPoliceFaceProtection::FlashGoggles;
 UPROPERTY(SaveGame) EBodyArmorType Armor=EBodyArmorType::Light;
};
