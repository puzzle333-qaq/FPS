#pragma once

#include "CoreMinimal.h"
#include "LoadoutTypes.generated.h"

class AWeapon;
class AGameModeBase;
class UAttachmentData;
class UWorld;

// 主菜单右侧的模式按钮
UENUM(BlueprintType)
enum class EShooterGameMode : uint8
{
	TDM            UMETA(DisplayName = "团队竞技"),
	FFA            UMETA(DisplayName = "死斗"),
	SearchDestroy  UMETA(DisplayName = "爆破"),
	Story          UMETA(DisplayName = "剧情"),
	Duel           UMETA(DisplayName = "单挑"),
	Extraction     UMETA(DisplayName = "搜打撤")
};

// 一种模式用哪个 GameMode、哪张地图。在 BP_ShooterGameInstance 里填
USTRUCT(BlueprintType)
struct FShooterModeInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Mode")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Mode")
	TSoftClassPtr<AGameModeBase> GameModeClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Mode")
	TSoftObjectPtr<UWorld> Map;

	// 没做完的模式保持 false，按钮可以显示“敬请期待”
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Mode")
	bool bImplemented = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Mode", meta = (ClampMin = "1"))
	int32 MaxPlayers = 8;
};

// 一把枪的预设：枪 + 装着的配件（配件自己带 SlotTag，所以不用存槽位）
USTRUCT(BlueprintType)
struct FWeaponPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Loadout")
	TSubclassOf<AWeapon> WeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Loadout")
	TArray<TObjectPtr<UAttachmentData>> Attachments;
};

// 一套配装。数组里第一把长枪是进局 1 号位主枪；第二把长枪是摸金 2 号位。
USTRUCT(BlueprintType)
struct FLoadoutPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Loadout")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Loadout")
	TArray<FWeaponPreset> Weapons;
};