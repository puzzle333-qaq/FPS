#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Loadout/LoadoutTypes.h"
#include "Game/ShooterGameState.h"
#include "Game/SearchDestroyTypes.h"
#include "MatchHUD.generated.h"

class UWidget;
class UTextBlock;
class UMatchHeaderWidget;
class UKillFeedWidget;
class AShooterPlayerState;
class ASearchDestroyGameState;
class UBorder;
class UVerticalBox;
class URadarWidget;

// 局内头顶 HUD 根控件。WBP_MatchHUD 选它当父类。
// 击杀播报是通用的；头顶计分条按模式从 HeaderClasses 里创建。
UCLASS()
class FPS_API UMatchHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FPS|HUD")
	static FText FormatMatchTime(float Seconds);

	/** Briefly show remaining match time (watch check). */
	UFUNCTION(BlueprintCallable, Category = "FPS|HUD")
	void ShowWatchTime(float RemainingSeconds, float DisplaySeconds = 4.f);

	/** Refresh radar visibility from local pawn HasRadar flag. */
	void RefreshRadarEquip();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 顶栏容器：Overlay / Canvas / Named Slot，名字必须是 HeaderHost。C++ 往里面塞模式计分条。
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HeaderHost;

	// 如果把 WBP_TDMHeader 直接拖进根控件，起这个名字即可，不必再走 HeaderClasses
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMatchHeaderWidget> TDMHeader;

	// 击杀播报控件（把 WBP_KillFeed 拖进来，名字必须是 KillFeed）
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UKillFeedWidget> KillFeed;

	// 可选：左侧雷达。名字 Radar；没有时 C++ 自动创建一个
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URadarWidget> Radar;

	// 可选：复活倒计时（屏幕中央）
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Respawn;

	// Optional / runtime: watch time readout
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_WatchTime;

	// 可选：结算底板，默认隐藏；回合结束短暂显示，比赛结束一直显示
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ResultPanel;

	// 可选：结算主标题（胜利 / 失败 / 平局 / 回合胜利）
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MatchResult;

	// 可选：副标题（歼灭/爆炸/拆除/超时，或最终比分）
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ResultDetail;

	// 每种模式用哪套头顶计分条。空着时 TDM 会自动加载 WBP_TDMHeader。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	TMap<EShooterGameMode, TSubclassOf<UMatchHeaderWidget>> HeaderClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	TSubclassOf<UMatchHeaderWidget> TDMHeaderClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	TSubclassOf<UMatchHeaderWidget> SDHeaderClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	FText RespawnFormat = NSLOCTEXT("MatchHUD", "RespawnFmt", "复活 {0}");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	FText VictoryText = NSLOCTEXT("MatchHUD", "Victory", "胜利");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	FText DefeatText = NSLOCTEXT("MatchHUD", "Defeat", "失败");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	FText DrawText = NSLOCTEXT("MatchHUD", "Draw", "平局");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	FText RoundVictoryText = NSLOCTEXT("MatchHUD", "RoundVictory", "回合胜利");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	FText RoundDefeatText = NSLOCTEXT("MatchHUD", "RoundDefeat", "回合失败");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	FText RoundDrawText = NSLOCTEXT("MatchHUD", "RoundDraw", "回合结束");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	FText ReasonEliminationText = NSLOCTEXT("MatchHUD", "ReasonElim", "歼灭");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	FText ReasonBombExplodedText = NSLOCTEXT("MatchHUD", "ReasonBoom", "炸弹爆炸");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	FText ReasonBombDefusedText = NSLOCTEXT("MatchHUD", "ReasonDefuse", "拆除炸弹");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	FText ReasonTimeExpiredText = NSLOCTEXT("MatchHUD", "ReasonTime", "时间耗尽");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	FText MatchScoreFormat = NSLOCTEXT("MatchHUD", "MatchScoreFmt", "{0} : {1}");

	// 回合横幅显示时长（秒）。应略短于 GameMode.RoundEndDelay
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD", meta = (ClampMin = "0.5"))
	float RoundBannerDuration = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	FLinearColor LocalWinColor = FLinearColor(0.35f, 0.95f, 0.45f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	FLinearColor LocalLossColor = FLinearColor(1.f, 0.3f, 0.25f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD")
	FLinearColor DrawColor = FLinearColor(0.92f, 0.92f, 0.92f, 1.f);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD")
	void OnShowMatchResult(uint8 WinningTeam, bool bLocalWon, bool bDraw);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD")
	void OnShowRoundResult(uint8 WinningTeam, ERoundWinReason Reason, bool bLocalWon, bool bDraw);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD")
	void OnHideRoundResult();

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD")
	void OnHeaderSpawned(UMatchHeaderWidget* Header, EShooterGameMode Mode);

private:
	void BindGameState(AShooterGameState* GS);
	void UnbindGameState();
	void RebuildHeader();
	void PlaceHeader(UMatchHeaderWidget* Header);
	void BindLocalPlayerState();
	void RefreshRespawn();
	void ShowMatchResult(uint8 WinningTeam);
	void ShowRoundResult(uint8 WinningTeam, ERoundWinReason Reason);
	void HideRoundBanner();
	void EnsureResultWidgets();
	void EnsureRadarWidget();
	void PlaceRadar(URadarWidget* InRadar);
	void SetResultVisible(bool bVisible);
	void ApplyResultColors(bool bDraw, bool bLocalWon);
	FText ReasonToText(ERoundWinReason Reason) const;
	TSubclassOf<UMatchHeaderWidget> ResolveHeaderClass(EShooterGameMode Mode) const;
	UMatchHeaderWidget* FindPlacedHeader() const;

	UFUNCTION()
	void HandleMatchModeChanged(EShooterGameMode NewMode);

	UFUNCTION()
	void HandleMatchEnded(uint8 WinningTeam);

	UFUNCTION()
	void HandleRoundEnded(uint8 WinningTeam, ERoundWinReason Reason);

	UFUNCTION()
	void HandleRoundPhaseChanged(EBombRoundPhase NewPhase);

	void HandleGameStateSet(AGameStateBase* NewGameState);

	UPROPERTY()
	TObjectPtr<UMatchHeaderWidget> ActiveHeader;

	bool bSpawnedHeader = false;
	bool bMatchResultVisible = false;
	bool bBuiltFallbackResult = false;
	bool bBuiltFallbackRadar = false;

	TWeakObjectPtr<AShooterGameState> BoundGameState;
	TWeakObjectPtr<AShooterPlayerState> BoundPlayerState;
	FTimerHandle RespawnTimer;
	FTimerHandle RoundBannerTimer;
	FTimerHandle WatchHideTimer;
	void HideWatchTime();
	void EnsureWatchText();
	int32 LastRespawnSecond = -1;
};
