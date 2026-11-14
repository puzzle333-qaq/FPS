#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Loadout/LoadoutTypes.h"
#include "WarehouseLoadoutSaveGame.generated.h"

// Asset paths survive a restart without serializing weapon actors or live components.
USTRUCT()
struct FWarehouseSavedWeapon
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FSoftClassPath WeaponClass;

	UPROPERTY(SaveGame)
	TArray<FSoftObjectPath> Attachments;

	UPROPERTY(SaveGame)
	int32 StackCount = 0;
};

UCLASS()
class FPS_API UWarehouseLoadoutSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
 UPROPERTY(SaveGame) FText PresetName;
 UPROPERTY(SaveGame) bool bIncludesProtection=false;
 UPROPERTY(SaveGame) EPoliceFaceProtection FaceProtection=EPoliceFaceProtection::FlashGoggles;
 UPROPERTY(SaveGame) EBodyArmorType BodyArmor=EBodyArmorType::Light;
	UPROPERTY(SaveGame)
	int32 Version = 1;

	UPROPERTY(SaveGame)
	TArray<FWarehouseSavedWeapon> Weapons;

	static bool SaveLoadout(const FLoadoutPreset& Loadout, const FString& Slot = TEXT("WarehouseLoadout"));
	static bool LoadLoadout(FLoadoutPreset& OutLoadout, const FString& Slot = TEXT("WarehouseLoadout"));
};
