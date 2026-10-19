#pragma once

#include "CoreMinimal.h"
#include "ShooterGameModeBase.h"
#include "TDMGameMode.generated.h"

// 团队竞技：两队，按人数平衡分队，队伍出生点，击杀给队伍加分，先到 ScoreToWin 或时间到分高的赢，结束后所有人回主菜单
UCLASS()
class FPS_API ATDMGameMode : public AShooterGameModeBase
{
	GENERATED_BODY()

public:
	ATDMGameMode();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|TDM", meta = (ClampMin = "1"))
	int32 ScoreToWin = 50;

	// 比赛时长（秒），<=0 不限时
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|TDM")
	float MatchDuration = 600.f;

	// 开局前等待（秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|TDM")
	float WarmupTime = 3.f;

	// 结束后几秒回主菜单（显示结算界面的时间）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|TDM")
	float PostMatchDelay = 8.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|TDM")
	bool bFriendlyFire = false;

	// PlayerStart 的 Player Start Tag：0 队 / 1 队
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|TDM")
	FName TeamAStartTag = TEXT("TeamA");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|TDM")
	FName TeamBStartTag = TEXT("TeamB");

	virtual void InitGameState() override;
	virtual void StartPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual bool CanDamage(AController* InstigatorController, AController* VictimController) const override;

protected:
	virtual void ScoreKill(AShooterPlayerState* KillerPS, AShooterPlayerState* VictimPS) override;
	virtual bool CanRespawn(AController* Controller) const override;

private:
	void ApplyMatchRules();
	void StartWarmupOrMatch();
	void StartMatch();
	void TickMatchTimer();
	void EndMatch(uint8 WinningTeam);
	void ReturnAllToMainMenu();
	uint8 PickTeamForNewPlayer() const;

	bool bMatchClockStarted = false;
	FTimerHandle WarmupTimer;
	FTimerHandle MatchTimer;
	FTimerHandle PostMatchTimer;
};