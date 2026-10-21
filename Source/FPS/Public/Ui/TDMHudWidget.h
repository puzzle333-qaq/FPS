#pragma once

#include "CoreMinimal.h"
#include "Ui/ModeHudWidget.h"
#include "Game/ShooterGameState.h"
#include "TDMHudWidget.generated.h"

class UTextBlock;

// 团战模式 HUD 骨架：头顶两队比分 + 倒计时。WBP_TDMHud 选它当父类（也可继续用现有 WBP_TDMHeader + MatchHUD）。
UCLASS()
class FPS_API UTDMHudWidget : public UModeHudWidget
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

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD|TDM")
	void OnScoresUpdated(int32 ScoreA, int32 ScoreB);

private:
	void Refresh();

	UFUNCTION()
	void HandleTeamScoreChanged(uint8 TeamId, int32 NewScore);

	UFUNCTION()
	void HandleMatchStateChanged(EShooterMatchState NewState);

	FTimerHandle TimerHandle;
};