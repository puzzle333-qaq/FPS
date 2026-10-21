// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterGameModeBase.generated.h"

class UModeHudWidget;

class AShooterPlayerState;

// 所有局内模式的父类：击杀记录、击杀播报、定时复活。靶场/死斗直接用它，团队竞技用 ATDMGameMode，爆破用 ASearchDestroyGameMode
UCLASS()
class FPS_API AShooterGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShooterGameModeBase();

	// 死亡后几秒复活（<=0 不自动复活）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Match")
	float RespawnDelay = 5.f;

	// 模式专属 HUD（可选）。团战填 TDMHudWidget 子类；为空则只用 MatchHUD 头顶条。
	// WBP_ShooterOverlay 仍是公共层（血条/弹药/准星），不放这里。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|HUD")
	TSubclassOf<UModeHudWidget> ModeHudWidgetClass;



	// 由角色在服务器上调用：伤害是否允许（队友伤害、比赛结束等）
	virtual bool CanDamage(AController* InstigatorController, AController* VictimController) const;

	// 由角色在服务器上调用：Victim 被 Killer 打死（Killer 可能为空或等于 Victim = 自杀）
	virtual void OnPlayerKilled(AController* KillerController, AController* VictimController);

	// 由角色在服务器上调用：记录谁打过谁（算助攻）
	void NotifyDamage(AController* InstigatorController, AController* VictimController);

	// 死前多少秒内造成过伤害算助攻
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Match")
	float AssistWindow = 10.f;

	UFUNCTION(BlueprintCallable, Category = "FPS|Match")
	void RespawnPlayer(AController* Controller, bool bForce = false);

	virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
	// 子类加分用
	virtual void ScoreKill(AShooterPlayerState* KillerPS, AShooterPlayerState* VictimPS) {}
	virtual bool CanRespawn(AController* Controller) const { return true; }

	TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimers;

	// 受害者 -> (攻击者 -> 最近一次伤害时间)
	TMap<TWeakObjectPtr<AController>, TMap<TWeakObjectPtr<AController>, float>> RecentDamage;
	void ClearRespawnTimers();
	void AssignShortPlayerName(APlayerController* NewPlayer);
};