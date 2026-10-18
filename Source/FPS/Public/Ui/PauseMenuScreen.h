#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuScreen.generated.h"

class UButton;

// 局内暂停菜单的 C++ 父类（可选）。WBP_PauseMenu 选它当父类，下面的按钮按名字自动绑定，都可以不放。
// 不会真的暂停游戏（联机）。菜单打开时是纯 UI 输入，Esc / P 由这里处理关闭。
UCLASS()
class FPS_API UPauseMenuScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	// 关掉菜单，回到游戏
	UFUNCTION(BlueprintCallable, Category = "FPS|PauseMenu")
	void Resume();

	// 回主菜单（客户端断开；房主回菜单时其他客户端也会断线）
	UFUNCTION(BlueprintCallable, Category = "FPS|PauseMenu")
	void ReturnToLobby();

	// 点了设置按钮。设置页你自己在蓝图里做（例如切 Widget Switcher）
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|PauseMenu")
	void OnSettingsRequested();

	// 菜单打开 / 关闭时通知蓝图（可用来播动画、重置页面）
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|PauseMenu")
	void OnMenuOpened();

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Resume;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Settings;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Lobby;

private:
	UFUNCTION()
	void HandleSettings();
};