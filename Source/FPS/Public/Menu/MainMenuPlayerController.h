#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Loadout/LoadoutTypes.h"
#include "MainMenuPlayerController.generated.h"

class UUserWidget;
class UGunsmithComponent;
class URoomBrowserScreen;
class UWidgetSwitcher;
class USettingsScreen;

// 主菜单控制器：开局创建 MainMenuWidgetClass，切成纯 UI 输入并显示鼠标。改枪台逻辑在 Gunsmith 组件上
UCLASS()
class FPS_API AMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMainMenuPlayerController();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Menu")
	TSubclassOf<UUserWidget> MainMenuWidgetClass;

	UFUNCTION(BlueprintPure, Category = "FPS|Menu")
	UUserWidget* GetMainMenuWidget() const { return MainMenuWidget; }

	// 填 WBP_RoomBrowser（父类 RoomBrowserScreen）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Menu")
	TSubclassOf<URoomBrowserScreen> RoomBrowserWidgetClass;

	// 主菜单右侧模式按钮调用：记下模式并打开房间列表
	UFUNCTION(BlueprintCallable, Category = "FPS|Menu")
	void ShowRoomBrowser(EShooterGameMode Mode);

	UFUNCTION(BlueprintCallable, Category = "FPS|Menu")
	void HideRoomBrowser();

	// 单人练习：直接进爆破/团战地图，不需要开房间
	UFUNCTION(BlueprintCallable, Category = "FPS|Menu")
	void StartPractice(EShooterGameMode Mode);

	/** Console (~): MenuStartPoliceCriminal — jump to range test map with PoliceCriminal GM (no room). */
	UFUNCTION(Exec, BlueprintCallable, Category = "FPS|Menu")
	void MenuStartPoliceCriminal();

	/** Console: MenuHostPoliceCriminal — listen-server on test map (second client: fps.LocalJoin). */
	UFUNCTION(Exec, BlueprintCallable, Category = "FPS|LocalMP")
	void MenuHostPoliceCriminal();

	// 本机双人测试（无需 EOS）：listen 开靶场/团战/大厅
	UFUNCTION(BlueprintCallable, Category = "FPS|LocalMP")
	void HostLocalListenTest(FString Target);

	UFUNCTION(BlueprintCallable, Category = "FPS|LocalMP")
	void JoinLocalhostTest(FString Address);

	// 房间列表是否嵌在主菜单 WidgetSwitcher 里（RoomBrowserPage）
	UFUNCTION(BlueprintPure, Category = "FPS|Menu")
	bool IsRoomBrowserEmbedded() const;

	// ---- 设置页 ----
	// 填 WBP_Settings（父类 SettingsScreen）。WBP_MainMenu 的 Switcher 里有名为 SettingsPage 的页时优先切页
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Menu")
	TSubclassOf<USettingsScreen> SettingsWidgetClass;

	UFUNCTION(BlueprintCallable, Category = "FPS|Menu")
	void ShowSettings();

	UFUNCTION(BlueprintCallable, Category = "FPS|Menu")
	void HideSettings();

	UFUNCTION(BlueprintPure, Category = "FPS|Menu")
	URoomBrowserScreen* GetRoomBrowser() const { return RoomBrowser; }

	UFUNCTION(BlueprintPure, Category = "FPS|Gunsmith")
	UGunsmithComponent* GetGunsmith() const { return Gunsmith; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith")
	TObjectPtr<UGunsmithComponent> Gunsmith;

private:
	void BindOptionalPoliceCriminalButtons();
	UFUNCTION() void HandleBtnPoliceCriminalPractice();
	UFUNCTION() void HandleBtnPoliceCriminalRoom();

	UPROPERTY()
	TObjectPtr<UUserWidget> MainMenuWidget;

	UPROPERTY()
	TObjectPtr<URoomBrowserScreen> RoomBrowser;

	TWeakObjectPtr<UWidgetSwitcher> EmbeddedSwitcher;
	int32 PreviousSwitcherIndex = 0;

	UPROPERTY()
	TObjectPtr<USettingsScreen> SettingsScreen;
	TWeakObjectPtr<UWidgetSwitcher> SettingsSwitcher;
	int32 SettingsPrevIndex = 0;
};