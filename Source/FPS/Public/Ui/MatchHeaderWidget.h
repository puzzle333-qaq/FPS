#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/ShooterGameState.h"
#include "Game/SearchDestroyTypes.h"
#include "MatchHeaderWidget.generated.h"

class UTextBlock;
class UProgressBar;
class AShooterPlayerState;
class ASearchDestroyGameState;

// 头顶计分条基类。每种模式做一个 WBP 子类（TDM = UTDMHeaderWidget）。
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

	TWeakObjectPtr<AShooterGameState> GameState;
};

// TDM 头顶：左队分 | 倒计时 | 右队分。WBP 里控件按下面的名字放即可，C++ 会写文字。
UCLASS()
class FPS_API UTDMHeaderWidget : public UMatchHeaderWidget
{
	GENERATED_BODY()

public:
	virtual void BindMatch(AShooterGameState* InGameState) override;
	virtual void UnbindMatch() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 0 队分数
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TeamAScore;

	// 1 队分数
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TeamBScore;

	// 暖身 / 比赛倒计时 MM:SS
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Timer;

	// 可选：胜利分，例如 "先到 50"
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ScoreToWin;

	// 可选：准备中 / 比赛中 / 已结束
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MatchState;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|TDM")
	FLinearColor TeamAColor = FLinearColor(0.2f, 0.55f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|TDM")
	FLinearColor TeamBColor = FLinearColor(1.f, 0.25f, 0.2f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|TDM")
	FText WarmupStateText = NSLOCTEXT("MatchHUD", "Warmup", "准备中");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|TDM")
	FText InProgressStateText = NSLOCTEXT("MatchHUD", "InProgress", "");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|TDM")
	FText PostMatchStateText = NSLOCTEXT("MatchHUD", "PostMatch", "结束");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|TDM")
	FText UnlimitedTimeText = NSLOCTEXT("MatchHUD", "Unlimited", "--:--");

	// 分数刷新后（方便播缩放动画）
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD|TDM")
	void OnScoresUpdated(int32 ScoreA, int32 ScoreB);

	// 本地玩家分到队之后（用来高亮自己那一侧）
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD|TDM")
	void OnLocalTeamChanged(uint8 LocalTeamId);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD|TDM")
	void OnHeaderMatchStateChanged(EShooterMatchState NewState);

private:
	void ResolveWidgets();
	void BindLocalPlayerState();
	void RefreshScores();
	void RefreshTimer();
	void RefreshMatchState();
	void ApplyScoreColors();

	UFUNCTION()
	void HandleTeamScoreChanged(uint8 TeamId, int32 NewScore);

	UFUNCTION()
	void HandleMatchStateChanged(EShooterMatchState NewState);

	UFUNCTION()
	void HandleLocalTeamChanged(uint8 TeamId);

	FTimerHandle TimerHandle;
	TWeakObjectPtr<AShooterPlayerState> BoundPlayerState;
	int32 LastDisplayedSeconds = -1;
};

// 爆破头顶：回合分 | 倒计时（回合/炸弹）| 炸弹状态。WBP 控件按名字绑定。
UCLASS()
class FPS_API USDHeaderWidget : public UMatchHeaderWidget
{
	GENERATED_BODY()

public:
	virtual void BindMatch(AShooterGameState* InGameState) override;
	virtual void UnbindMatch() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TeamAScore;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TeamBScore;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Timer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Round;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_BombState;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Prompt;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Role;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Progress_Action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|SD")
	FLinearColor TeamAColor = FLinearColor(0.2f, 0.55f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|SD")
	FLinearColor TeamBColor = FLinearColor(1.f, 0.25f, 0.2f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|SD")
	FLinearColor BombTimerColor = FLinearColor(1.f, 0.35f, 0.1f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|SD")
	FLinearColor RoundTimerColor = FLinearColor(1.f, 1.f, 1.f, 1.f);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD|SD")
	void OnScoresUpdated(int32 ScoreA, int32 ScoreB);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD|SD")
	void OnLocalTeamChanged(uint8 LocalTeamId);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD|SD")
	void OnHeaderBombStateChanged(EBombState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD|SD")
	void OnHeaderRoundPhaseChanged(EBombRoundPhase NewPhase);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD|SD")
	void OnHeaderPromptChanged(EBombPrompt Prompt);

private:
	void ResolveWidgets();
	void BindLocalPlayerState();
	void RefreshScores();
	void RefreshTimer();
	void RefreshBomb();
	void RefreshRole();
	void RefreshPrompt();
	void TickHeader();
	ASearchDestroyGameState* GetSDState() const;

	UFUNCTION()
	void HandleTeamScoreChanged(uint8 TeamId, int32 NewScore);

	UFUNCTION()
	void HandleBombStateChanged(EBombState NewState);

	UFUNCTION()
	void HandleRoundPhaseChanged(EBombRoundPhase NewPhase);

	UFUNCTION()
	void HandleLocalTeamChanged(uint8 TeamId);

	FTimerHandle TimerHandle;
	TWeakObjectPtr<AShooterPlayerState> BoundPlayerState;
	int32 LastDisplayedSeconds = -1;
	EBombPrompt LastPrompt = EBombPrompt::None;
};
