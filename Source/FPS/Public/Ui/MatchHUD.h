#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Loadout/LoadoutTypes.h"
#include "Game/ShooterGameState.h"
#include "MatchHUD.generated.h"

class UWidget;
class UTextBlock;
class UMatchHeaderWidget;
class UKillFeedWidget;
class AShooterPlayerState;

// 局内头顶 HUD 根控件。WBP_MatchHUD 选它当父类。
// 击杀播报是通用的；头顶计分条按模式从 HeaderClasses 里创建。
UCLASS()
class FPS_API UMatchHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FPS|HUD")
	static FText FormatMatchTime(float Seconds);

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

	// 可选：复活倒计时（屏幕中央）
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Respawn;

	// 可选：结算底板，默认隐藏，比赛结束时显示
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ResultPanel;

	// 可选：结算文字（胜利 / 失败 / 平局）
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MatchResult;

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

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD")
	void OnShowMatchResult(uint8 WinningTeam, bool bLocalWon, bool bDraw);

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
	TSubclassOf<UMatchHeaderWidget> ResolveHeaderClass(EShooterGameMode Mode) const;
	UMatchHeaderWidget* FindPlacedHeader() const;

	UFUNCTION()
	void HandleMatchModeChanged(EShooterGameMode NewMode);

	UFUNCTION()
	void HandleMatchEnded(uint8 WinningTeam);

	void HandleGameStateSet(AGameStateBase* NewGameState);

	UPROPERTY()
	TObjectPtr<UMatchHeaderWidget> ActiveHeader;

	bool bSpawnedHeader = false;

	TWeakObjectPtr<AShooterGameState> BoundGameState;
	TWeakObjectPtr<AShooterPlayerState> BoundPlayerState;
	FTimerHandle RespawnTimer;
	int32 LastRespawnSecond = -1;
};
