#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Loadout/LoadoutTypes.h"
#include "ShooterGameInstance.generated.h"

class UShooterLoadoutSave;

// 局外数据：当前配装、选中的模式，以及开房/加入/去靶场/回主菜单。
// 做一个蓝图子类 BP_ShooterGameInstance 填表，然后在 Project Settings → Game Instance Class 里选它。
UCLASS()
class FPS_API UShooterGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UShooterGameInstance();
	virtual void Init() override;

	// ---------- 模式表 ----------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Mode")
	TMap<EShooterGameMode, FShooterModeInfo> Modes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Mode")
	TSoftObjectPtr<UWorld> RangeMap;

	// 靶场用的 GameMode（空 = 用地图自己的设置）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Mode")
	TSoftClassPtr<AGameModeBase> RangeGameMode;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Mode")
	TSoftObjectPtr<UWorld> MainMenuMap;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Mode")
	EShooterGameMode SelectedMode = EShooterGameMode::TDM;

	UFUNCTION(BlueprintCallable, Category = "FPS|Mode")
	void SelectMode(EShooterGameMode Mode) { SelectedMode = Mode; }

	UFUNCTION(BlueprintPure, Category = "FPS|Mode")
	bool GetModeInfo(EShooterGameMode Mode, FShooterModeInfo& OutInfo) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Mode")
	bool IsModeAvailable(EShooterGameMode Mode) const;

	// 自己开房：以 listen server 打开该模式的地图。成功返回 true
	UFUNCTION(BlueprintCallable, Category = "FPS|Mode")
	bool HostRoom(EShooterGameMode Mode);

	// 输入 IP（可带 :端口）加入别人的房间
	UFUNCTION(BlueprintCallable, Category = "FPS|Mode")
	bool JoinByIP(const FString& Address);

	UFUNCTION(BlueprintCallable, Category = "FPS|Mode")
	bool OpenRange();

	UFUNCTION(BlueprintCallable, Category = "FPS|Mode")
	bool GoToMainMenu();

	// ---------- 配装 ----------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Loadout")
	FString SaveSlotName = TEXT("Loadout");

	UFUNCTION(BlueprintPure, Category = "FPS|Loadout")
	FLoadoutPreset GetCurrentLoadout() const { return CurrentLoadout; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Loadout")
	void SetCurrentLoadout(const FLoadoutPreset& NewLoadout) { CurrentLoadout = NewLoadout; }

	// 把一把枪写进当前配装（同 WeaponKind 的那把会被替换）
	UFUNCTION(BlueprintCallable, Category = "FPS|Loadout")
	void SetWeaponPreset(const FWeaponPreset& Preset);

	// 当前配装里这把枪的预设（没有返回 false）
	UFUNCTION(BlueprintPure, Category = "FPS|Loadout")
	bool FindWeaponPreset(TSubclassOf<AWeapon> WeaponClass, FWeaponPreset& OutPreset) const;

	UFUNCTION(BlueprintCallable, Category = "FPS|Loadout")
	bool SaveLoadout();

	UFUNCTION(BlueprintCallable, Category = "FPS|Loadout")
	bool LoadLoadout();

private:
	UPROPERTY()
	FLoadoutPreset CurrentLoadout;

	bool TravelTo(const TSoftObjectPtr<UWorld>& Map, const TSoftClassPtr<AGameModeBase>& GameMode, bool bListen);
};