#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ModeHudWidget.generated.h"

class AShooterGameState;

// 模式专属 HUD 层（叠在 WBP_ShooterOverlay 公共层之上）。
// 各模式 GameMode 的 ModeHudWidgetClass 填对应子类；团战用 TDMHudWidget，爆破可用 SD 头顶或自建。
// 击杀播报 / 结算仍走 MatchHUD。
UCLASS()
class FPS_API UModeHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FPS|HUD")
	virtual void BindMatch(AShooterGameState* InGameState);

	UFUNCTION(BlueprintCallable, Category = "FPS|HUD")
	virtual void UnbindMatch();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|HUD")
	TWeakObjectPtr<AShooterGameState> BoundGameState;
};