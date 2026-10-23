#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingsScreen.generated.h"

class UComboBoxString;
class UButton;
class UTextBlock;
class UPanelWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSettingsBackRequested);

// 画面/显示设置：WBP_Settings，父类 SettingsScreen。所有选项都是 ComboBoxString，按名字自动绑定。
UCLASS()
class FPS_API USettingsScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	// 从当前设置重新填充所有下拉框
	UFUNCTION(BlueprintCallable, Category = "FPS|Settings")
	void RefreshFromCurrent();

	// 应用并保存（GameUserSettings.ini）
	UFUNCTION(BlueprintCallable, Category = "FPS|Settings")
	void ApplySettings();

	// 恢复默认（bBenchmark = 先跑一次硬件测试自动选画质）
	UFUNCTION(BlueprintCallable, Category = "FPS|Settings")
	void ResetToDefaults(bool bBenchmark = false);

	// 返回：放弃未应用的修改，关闭/切回上一页
	UFUNCTION(BlueprintCallable, Category = "FPS|Settings")
	void Back();

	UFUNCTION(BlueprintPure, Category = "FPS|Settings")
	static bool IsDLSSAvailable();

	UPROPERTY(BlueprintAssignable, Category = "FPS|Settings")
	FSettingsBackRequested OnBackRequested;

	// 局内打开时锁住画面/显示设置（只能改操作设置）。主菜单 = false
	UFUNCTION(BlueprintCallable, Category = "FPS|Settings")
	void SetGraphicsLocked(bool bLocked);

	UFUNCTION(BlueprintPure, Category = "FPS|Settings")
	bool IsGraphicsLocked() const { return bGraphicsLocked; }

	// 启动时把保存的光追/DLSS 选项套到控制台变量上（GameInstance Init 调一次即可，可选）
	UFUNCTION(BlueprintCallable, Category = "FPS|Settings")
	static void ApplySavedExtraSettings();

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// ---- 操作设置 ----
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_Sensitivity;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_ADSSensitivity;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_InvertY;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_FOV;
	// 把所有画面/显示选项放进这个面板，局内会整块折叠
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> Panel_Graphics;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_GraphicsLockedHint;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_Resolution;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_WindowMode;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_FrameRate;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_VSync;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_OverallQuality;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_Shadows;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_AntiAliasing;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_PostProcess;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_Textures;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_Effects;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_ViewDistance;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_Foliage;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_Shading;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_GlobalIllumination;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_Reflections;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_RayTracing;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_ResolutionScale;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_DLSS;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_RestartHint;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Apply;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Reset;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Back;

private:
	UFUNCTION() void HandleApply();
	UFUNCTION() void HandleReset();
	UFUNCTION() void HandleBack();
	UFUNCTION() void HandleOverallChanged(FString Selected, ESelectInfo::Type Type);
	UFUNCTION() void HandleIndividualChanged(FString Selected, ESelectInfo::Type Type);
	UFUNCTION() void HandleRestartOptionChanged(FString Selected, ESelectInfo::Type Type);

	void BuildOptions();
	void Fill(UComboBoxString* Combo, const TArray<FString>& Options);
	void Select(UComboBoxString* Combo, const FString& Option);
	void SelectQuality(UComboBoxString* Combo, int32 Level);
	int32 ReadQuality(const UComboBoxString* Combo, int32 Fallback) const;
	void UpdateOverallFromIndividual();
	TArray<UComboBoxString*> GetQualityCombos() const;

	TArray<UWidget*> GetGraphicsWidgets() const;
	void ApplyGraphicsLockVisibility();
	void ApplyControls();
	bool bGraphicsLocked = false;
	bool bUpdating = false;
	bool bBuilt = false;
	TArray<FIntPoint> Resolutions;
};
