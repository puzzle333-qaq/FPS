// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterGameModeBase.generated.h"

class AShooterPlayerState;

// 所有局内模式的父类：击杀记录、击杀播报、定时复活。靶场/死斗直接用它，团队竞技用 ATDMGameMode
UCLASS()
class FPS_API AShooterGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShooterGameModeBase();

	// 死亡后几秒复活（<=0 不自动复活）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Match")
	float RespawnDelay = 5.f;

	// 由角色在服务器上调用：伤害是否允许（队友伤害、比赛结束等）
	virtual bool CanDamage(AController* InstigatorController, AController* VictimController) const;

	// 由角色在服务器上调用：Victim 被 Killer 打死（Killer 可能为空或等于 Victim = 自杀）
	virtual void OnPlayerKilled(AController* KillerController, AController* VictimController);

	UFUNCTION(BlueprintCallable, Category = "FPS|Match")
	void RespawnPlayer(AController* Controller);

protected:
	// 子类加分用
	virtual void ScoreKill(AShooterPlayerState* KillerPS, AShooterPlayerState* VictimPS) {}
	virtual bool CanRespawn(AController* Controller) const { return true; }

	TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimers;
	void ClearRespawnTimers();
};