#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/ShooterGameState.h"
#include "MatchHeaderWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UHorizontalBox;
class UVerticalBox;
class UBorder;
class USizeBox;
class UImage;
class AShooterPlayerState;

// 局内头顶计分条基类。每种模式做一个子类（TDM = UTDMHeaderWidget）。
// 由 UMatchHUD 按 GameState.MatchMode 创建并塞进 HeaderHost。
UCLASS(Abstract)
class FPS_API UMatchHeaderWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void BindMatch(AShooterGameState* InGameState);
	virtual void UnbindMatch();

	UFUNCTION(BlueprintPure, Category = "FPS|HUD")
	AShooterGameState* GetMatchGameState() const { return GameState.Get(); }

	UFUNCTION(BlueprintPure, Category = "FPS|HUD")
	static FText FormatMatchTime(float Seconds);

protected:
	virtual void NativeDestruct() override;

	/** Builds center timer/score/alive + flanking portrait rows. */
	void EnsureValorantTopBar(bool bIncludeSDExtras);

	/** Rebuild square portrait slots from GameState PlayerArray. */
	void RefreshPortraitRows();

	UWidget* MakePersonSilhouette(const FLinearColor& Tint) const;

	TWeakObjectPtr<AShooterGameState> GameState;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> PortraitRowA;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> PortraitRowB;

	bool bBuiltValorantTopBar = false;

	// Shared score colors (Valorant ref: left white, right gold)
	FLinearColor ScoreColorA = FLinearColor(0.96f, 0.97f, 0.98f, 1.f);
	FLinearColor ScoreColorB = FLinearColor(1.f, 0.82f, 0.32f, 1.f);
	FLinearColor PortraitBorderA = FLinearColor(0.15f, 0.85f, 0.82f, 1.f);
	FLinearColor PortraitBorderB = FLinearColor(0.45f, 0.72f, 1.f, 1.f);
	FLinearColor PortraitFill = FLinearColor(0.08f, 0.09f, 0.11f, 0.95f);
	FLinearColor CenterPanelColor = FLinearColor(0.04f, 0.045f, 0.055f, 0.92f);
	FLinearColor TimerBarColor = FLinearColor(0.02f, 0.022f, 0.028f, 0.96f);
};

// TDM 头顶：左侧头像 | 中间计时+比分+存活 | 右侧头像
UCLASS()
class FPS_API UTDMHeaderWidget : public UMatchHeaderWidget
{
	GENERATED_BODY()

public:
	virtual void BindMatch(AShooterGameState* InGameState) override;
	virtual void UnbindMatch() override;

protected:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TeamAScore;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TeamBScore;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TeamAAlive;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TeamBAlive;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Timer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ScoreToWin;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MatchState;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|TDM")
	FLinearColor TeamAColor = FLinearColor(0.96f, 0.97f, 0.98f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|TDM")
	FLinearColor TeamBColor = FLinearColor(1.f, 0.82f, 0.32f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|TDM")
	FText WarmupStateText = NSLOCTEXT("MatchHUD", "Warmup", "准备中");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|TDM")
	FText InProgressStateText = NSLOCTEXT("MatchHUD", "InProgress", "");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|TDM")
	FText PostMatchStateText = NSLOCTEXT("MatchHUD", "PostMatch", "结束");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|TDM")
	FText UnlimitedTimeText = NSLOCTEXT("MatchHUD", "Unlimited", "--:--");

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD|TDM")
	void OnScoresUpdated(int32 ScoreA, int32 ScoreB);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD|TDM")
	void OnAliveCountsUpdated(int32 AliveA, int32 AliveB);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD|TDM")
	void OnLocalTeamChanged(uint8 LocalTeamId);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD|TDM")
	void OnHeaderMatchStateChanged(EShooterMatchState NewState);

private:
	void ResolveWidgets();
	void BindLocalPlayerState();
	void RefreshScores();
	void RefreshAlive();
	void RefreshTimer();
	void RefreshMatchState();
	void ApplyScoreColors();

	UFUNCTION()
	void HandleTeamScoreChanged(uint8 TeamId, int32 NewScore);

	UFUNCTION()
	void HandleTeamAliveChanged(uint8 TeamId, int32 AliveCount);

	UFUNCTION()
	void HandleMatchStateChanged(EShooterMatchState NewState);

	UFUNCTION()
	void HandleLocalTeamChanged(uint8 TeamId);

	FTimerHandle TimerHandle;
	TWeakObjectPtr<AShooterPlayerState> BoundPlayerState;
	int32 LastDisplayedSeconds = -1;
};
