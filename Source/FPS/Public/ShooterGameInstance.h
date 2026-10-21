#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Loadout/LoadoutTypes.h"
#include "ShooterGameInstance.generated.h"

class UShooterLoadoutSave;

// 房间（服务器侧，存在 GameInstance 里，跨 ServerTravel 保留）
USTRUCT(BlueprintType)
struct FShooterRoomState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Room")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Room")
	FString RoomCode;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Room")
	EShooterGameMode Mode = EShooterGameMode::TDM;

	// 房主的 RoomKey（UniqueId 或名字）
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Room")
	FString HostKey;

	// 点开始时房间里的人（对局里等这些人到齐）
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Room")
	TArray<FString> ExpectedPlayerKeys;
};

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

	// ---------- 房间大厅 ----------
	// 房间大厅地图（World Settings 的 GameMode 填 BP_LobbyGameMode，或者填下面 LobbyGameModeClass）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Room")
	TSoftObjectPtr<UWorld> RoomLobbyMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Room")
	TSoftClassPtr<AGameModeBase> LobbyGameModeClass;

	// 本地玩家名字（加入时作为 ?Name= 发给服务器；空 = 用引擎默认名）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Room")
	FString LocalPlayerName;

	// 创建房间：自己当 listen server 打开大厅地图（你就是房主）
	UFUNCTION(BlueprintCallable, Category = "FPS|Room")
	bool CreateRoom(EShooterGameMode Mode);

	// 创建房间（专用服务器）：连接服务器 IP，作为第一个进入者成为房主，并请求模式
	UFUNCTION(BlueprintCallable, Category = "FPS|Room")
	bool CreateRoomOnServer(const FString& Address, EShooterGameMode Mode);

	// ---- 服务器侧房间状态 ----
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Room")
	FShooterRoomState Room;

	// 新建房间（生成 6 位房间号）
	void ResetRoom(EShooterGameMode Mode);

	static FString MakeRoomCode();

	// 玩家在房间里的标识：有 UniqueId 用 UniqueId，否则用名字
	static FString GetRoomKey(const APlayerState* PS);

	// 拼 ?Name= 选项
	FString GetNameOption() const;

	// ---------- 配装 ----------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Loadout")
	FString SaveSlotName = TEXT("Loadout");

	UFUNCTION(BlueprintPure, Category = "FPS|Loadout")
	FLoadoutPreset GetCurrentLoadout() const { return CurrentLoadout; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Loadout")
	void SetCurrentLoadout(const FLoadoutPreset& NewLoadout) { CurrentLoadout = NewLoadout; }

	// 把一把枪写进当前配装（手枪/刀/投掷：同类只留一把；长枪：同 class 更新配件）
	UFUNCTION(BlueprintCallable, Category = "FPS|Loadout")
	void SetWeaponPreset(const FWeaponPreset& Preset);

	// 装备成主武器：长枪插到第一把（原第一把变成第二把，最多两把）。手枪/刀同 SetWeaponPreset。
	UFUNCTION(BlueprintCallable, Category = "FPS|Loadout")
	void EquipPrimaryWeapon(const FWeaponPreset& Preset);

	UFUNCTION(BlueprintPure, Category = "FPS|Loadout")
	TSubclassOf<AWeapon> GetPrimaryLongGun() const;

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

	bool TravelTo(const TSoftObjectPtr<UWorld>& Map, const TSoftClassPtr<AGameModeBase>& GameMode, bool bListen, const TArray<FString>& ExtraOptions = TArray<FString>());
};