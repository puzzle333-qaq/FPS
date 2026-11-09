#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ShooterPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShooterStatChanged, int32, Kills, int32, Deaths);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShooterTeamChanged, uint8, TeamId);

// Team and match stats. Server writes, clients replicate.
UCLASS()
class FPS_API AShooterPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	static constexpr uint8 NoTeam = 255;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;

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

	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	float GetRespawnRemaining() const;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Match")
	FShooterStatChanged OnStatsChanged;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Match")
	FShooterTeamChanged OnTeamChanged;

	void SetTeamId(uint8 NewTeam);
	void SetPartyHost(bool bHost);
	void SetPlatformId(const FString& InId);
	bool IsPartyHost() const { return bPartyHost; }
	const FString& GetPlatformId() const { return PlatformId; }
	void AddKill();
	void AddDeath();
	void AddAssist();
	void SetRespawnServerTime(float ServerTime);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_TeamId)
	uint8 TeamId = NoTeam;

	UPROPERTY(Replicated)
	bool bPartyHost = false;

	UPROPERTY(Replicated)
	FString PlatformId;

	UPROPERTY(ReplicatedUsing = OnRep_Stats)
	int32 Kills = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Stats)
	int32 Deaths = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Stats)
	int32 Assists = 0;

	UPROPERTY(Replicated)
	float RespawnServerTime = 0.f;

	UFUNCTION()
	void OnRep_TeamId();

	UFUNCTION()
	void OnRep_Stats();
};
