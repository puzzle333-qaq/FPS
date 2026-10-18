#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ShooterPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShooterStatChanged, int32, Kills, int32, Deaths);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShooterTeamChanged, uint8, TeamId);

// 每个玩家的队伍和战绩（服务器写，复制给所有人）
UCLASS()
class FPS_API AShooterPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	static constexpr uint8 NoTeam = 255;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	uint8 GetTeamId() const { return TeamId; }

	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	bool HasTeam() const { return TeamId != NoTeam; }

	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	int32 GetKills() const { return Kills; }

	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	int32 GetDeaths() const { return Deaths; }

	// 距离复活还有几秒（没在等复活返回 0）。本地 HUD 每帧读它显示倒计时
	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	float GetRespawnRemaining() const;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Match")
	FShooterStatChanged OnStatsChanged;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Match")
	FShooterTeamChanged OnTeamChanged;

	// ---- 只在服务器调用 ----
	void SetTeamId(uint8 NewTeam);
	void AddKill();
	void AddDeath();
	void SetRespawnServerTime(float ServerTime);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_TeamId)
	uint8 TeamId = NoTeam;

	UPROPERTY(ReplicatedUsing = OnRep_Stats)
	int32 Kills = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Stats)
	int32 Deaths = 0;

	// 服务器时间，0 = 不在等复活
	UPROPERTY(Replicated)
	float RespawnServerTime = 0.f;

	UFUNCTION()
	void OnRep_TeamId();

	UFUNCTION()
	void OnRep_Stats();
};