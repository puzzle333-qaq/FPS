#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ShooterPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShooterStatChanged, int32, Kills, int32, Deaths);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShooterTeamChanged, uint8, TeamId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShooterBombCarryChanged, bool, bHasBomb);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShooterMoneyChanged, int32, NewMoney);

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

	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	int32 GetAssists() const { return Assists; }

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	bool IsCarryingBomb() const { return bHasBomb; }

	UFUNCTION(BlueprintPure, Category = "FPS|Buy")
	int32 GetMoney() const { return Money; }

	// 距离复活还有几秒（没在等复活返回 0）。本地 HUD 每帧读它显示倒计时
	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	float GetRespawnRemaining() const;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Match")
	FShooterStatChanged OnStatsChanged;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Match")
	FShooterTeamChanged OnTeamChanged;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Bomb")
	FShooterBombCarryChanged OnBombCarryChanged;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Buy")
	FShooterMoneyChanged OnMoneyChanged;

	// ---- 只在服务器调用 ----
	void SetTeamId(uint8 NewTeam);
	void AddKill();
	void AddDeath();
	void AddAssist();
	void SetRespawnServerTime(float ServerTime);
	void SetHasBomb(bool bInHasBomb);
	void SetMoney(int32 NewMoney);
	void AddMoney(int32 Delta);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_TeamId)
	uint8 TeamId = NoTeam;

	UPROPERTY(ReplicatedUsing = OnRep_Stats)
	int32 Kills = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Stats)
	int32 Deaths = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Stats)
	int32 Assists = 0;

	// 服务器时间，0 = 不在等复活
	UPROPERTY(Replicated)
	float RespawnServerTime = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_HasBomb)
	bool bHasBomb = false;

	UPROPERTY(ReplicatedUsing = OnRep_Money)
	int32 Money = 0;

	UFUNCTION()
	void OnRep_TeamId();

	UFUNCTION()
	void OnRep_Stats();

	UFUNCTION()
	void OnRep_HasBomb();

	UFUNCTION()
	void OnRep_Money();
};