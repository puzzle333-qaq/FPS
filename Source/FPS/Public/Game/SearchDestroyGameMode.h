#pragma once

#include "CoreMinimal.h"
#include "ShooterGameModeBase.h"
#include "Game/SearchDestroyTypes.h"
#include "Ui/BuyShopTypes.h"
#include "SearchDestroyGameMode.generated.h"

class ABombActor;
class UTexture2D;
class ABombSite;
class ASearchDestroyGameState;

// 断线时记下的信息（重连还原）
struct FSDReconnectInfo
{
	uint8 TeamId = 255;
	int32 Money = 0;
};

// 爆破：两队回合制。进攻方安放，防守方拆除。回合内不复活，先赢 RoundsToWin 局。
UCLASS()
class FPS_API ASearchDestroyGameMode : public AShooterGameModeBase
{
	GENERATED_BODY()

public:
	ASearchDestroyGameMode();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD", meta = (ClampMin = "1"))
	int32 RoundsToWin = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	float WarmupTime = 3.f;

	// 开局等待：每队至少这么多人才开始热身/购买
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD", meta = (ClampMin = "1"))
	int32 MinPlayersPerTeam = 1;

	// 等人超过这么多秒仍不满足就关房，所有人回主菜单（<=0 不关）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	float WaitForPlayersTimeout = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	float FreezeTime = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	float RoundTime = 115.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	float PlantTime = 3.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	float DefuseTime = 7.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	float BombTimer = 40.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	float RoundEndDelay = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	float PostMatchDelay = 8.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	bool bFriendlyFire = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	float PlantCancelSpeed = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	float DefuseRange = 140.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	float ExplosionRadius = 2000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	float ExplosionDamage = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	FName AttackerStartTag = TEXT("Attacker");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	FName DefenderStartTag = TEXT("Defender");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	FName TeamAStartTag = TEXT("TeamA");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	FName TeamBStartTag = TEXT("TeamB");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	TSubclassOf<ABombActor> BombClass;

	// 炸弹爆炸致死时击杀播报的图标（C4）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	TObjectPtr<UTexture2D> BombKillIcon;

	virtual void InitGameState() override;
	virtual void StartPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual bool CanDamage(AController* InstigatorController, AController* VictimController) const override;
	virtual void OnPlayerKilled(AController* KillerController, AController* VictimController) override;
	virtual void Logout(AController* Exiting) override;

	// 死后多少秒切到队友观战（等死亡镜头播完）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD")
	float SpectateDelay = 3.f;

	// 中途加入分队时每队上限
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SD", meta = (ClampMin = "1"))
	int32 MaxPlayersPerTeam = 5;


	void BeginBombInteract(AController* Controller);
	void EndBombInteract(AController* Controller);
	void NotifyBombOverlap(ABombActor* OverlappedBomb, AActor* OtherActor);
	// ---------- 赏金购枪（CS 风格：冻结阶段 = 购买窗口） ----------
	// BuyTime > 0 时覆盖 FreezeTime 作为购买/冻结时长
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Buy")
	float BuyTime = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Buy", meta = (ClampMin = "0"))
	int32 StartMoney = 800;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Buy", meta = (ClampMin = "0"))
	int32 MaxMoney = 16000;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Buy", meta = (ClampMin = "0"))
	int32 KillReward = 300;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Buy", meta = (ClampMin = "0"))
	int32 RoundWinReward = 3250;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Buy", meta = (ClampMin = "0"))
	int32 RoundLossReward = 1400;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Buy")
	TArray<FBuyShopItem> BuyCatalog;

	UFUNCTION(BlueprintPure, Category = "FPS|Buy")
	const TArray<FBuyShopItem>& GetBuyCatalog() const { return BuyCatalog; }

	UFUNCTION(BlueprintPure, Category = "FPS|Buy")
	bool GetBuyCatalogItem(int32 Index, FBuyShopItem& OutItem) const;

	// 服务器：尝试购买。成功返回 true
	bool Auth_TryBuyItem(class AShooterPlayerController* Buyer, int32 CatalogIndex);


protected:


	virtual void ScoreKill(AShooterPlayerState* KillerPS, AShooterPlayerState* VictimPS) override;
	virtual bool CanRespawn(AController* Controller) const override;

private:
	void ApplyMatchRules();
	void StartWarmupOrMatch();
	void StartMatch();
	void StartRound();
	void GoLive();
	void TickObjective();
	void FinishRound(uint8 WinningTeam, ERoundWinReason Reason);
	void EndMatch(uint8 WinningTeam);
	void ReturnAllToMainMenu();
	virtual void HandleTeamForfeit(uint8 WinningTeam) override;
	void SwitchSides();
	void RespawnAllPlayers();
	void SpawnOrResetBomb();
	void GiveBombToRandomAttacker();
	void DropBombFrom(APawn* Pawn);
	void ClearBombCarry();
	void TryPickupBomb(APawn* Pawn);
	void StartPlant(AController* Controller, ABombSite* Site);
	void StartDefuse(AController* Controller);
	void CompletePlant();
	void CompleteDefuse();
	void ExplodeBomb();
	void CancelAction();
	bool CanPlayerAct(AController* Controller) const;
	bool IsPawnAlive(const APawn* Pawn) const;
	bool IsAttacker(const AController* Controller) const;
	int32 CountAliveOnTeam(uint8 TeamId) const;
	void CheckElimination();
	ABombSite* FindSiteContaining(const APawn* Pawn) const;
	AActor* FindStartWithTag(FName Tag, bool bRequireFree) const;
	uint8 PickTeamForNewPlayer() const;
	ASearchDestroyGameState* GetSDState() const;
	bool ShouldSpawnNewPlayer() const;

	// ---- 观战 / 重连 ----
	TMap<FString, FSDReconnectInfo> ReconnectCache;
	FTimerHandle SpectateValidateTimer;
	void ValidateSpectators();
	void ScheduleSpectate(AController* Controller, float Delay);
	bool IsMidRound() const;

	bool bMatchClockStarted = false;
	bool bRoundLocked = false;
	TWeakObjectPtr<AController> Interactor;
	TWeakObjectPtr<ABombSite> PlantingSite;

	FTimerHandle WarmupTimer;
	FTimerHandle WaitForPlayersTimer;
	float WaitForPlayersStartTime = 0.f;
	void BeginWaitingForPlayers();
	void TickWaitingForPlayers();
	int32 CountPlayersOnTeam(uint8 TeamId) const;
	void CloseRoomToMainMenu();
	FTimerHandle FreezeTimer;
	FTimerHandle ObjectiveTimer;
	FTimerHandle RoundEndTimer;
	FTimerHandle PostMatchTimer;
};
