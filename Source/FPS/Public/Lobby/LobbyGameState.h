#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Loadout/LoadoutTypes.h"
#include "LobbyGameState.generated.h"

class APlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLobbyStateChanged);

// 房间大厅状态（复制给所有人）：房间号、模式、房主、规则
UCLASS()
class FPS_API ALobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "FPS|Room")
	FString GetRoomCode() const { return RoomCode; }

	UFUNCTION(BlueprintPure, Category = "FPS|Room")
	EShooterGameMode GetRoomMode() const { return RoomMode; }

	UFUNCTION(BlueprintPure, Category = "FPS|Room")
	APlayerState* GetHostPlayerState() const { return HostPlayerState; }

	UFUNCTION(BlueprintPure, Category = "FPS|Room")
	bool IsStarting() const { return bStarting; }

	UFUNCTION(BlueprintPure, Category = "FPS|Room")
	int32 GetMaxPlayersPerTeam() const { return MaxPlayersPerTeam; }

	UFUNCTION(BlueprintPure, Category = "FPS|Room")
	int32 GetMaxTeamDifference() const { return MaxTeamDifference; }

	UPROPERTY(BlueprintAssignable, Category = "FPS|Room")
	FLobbyStateChanged OnLobbyStateChanged;

	// ---- 服务器 ----
	void SetRoomCode(const FString& InCode) { RoomCode = InCode; NotifyChanged(); }
	void SetRoomMode(EShooterGameMode InMode) { RoomMode = InMode; NotifyChanged(); }
	void SetHostPlayerState(APlayerState* InHost) { HostPlayerState = InHost; NotifyChanged(); }
	void SetStarting(bool bIn) { bStarting = bIn; NotifyChanged(); }
	void SetRules(int32 InMaxPerTeam, int32 InMaxDiff) { MaxPlayersPerTeam = InMaxPerTeam; MaxTeamDifference = InMaxDiff; NotifyChanged(); }

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Lobby)
	FString RoomCode;

	UPROPERTY(ReplicatedUsing = OnRep_Lobby)
	EShooterGameMode RoomMode = EShooterGameMode::TDM;

	UPROPERTY(ReplicatedUsing = OnRep_Lobby)
	TObjectPtr<APlayerState> HostPlayerState;

	UPROPERTY(ReplicatedUsing = OnRep_Lobby)
	bool bStarting = false;

	UPROPERTY(ReplicatedUsing = OnRep_Lobby)
	int32 MaxPlayersPerTeam = 5;

	UPROPERTY(ReplicatedUsing = OnRep_Lobby)
	int32 MaxTeamDifference = 2;

	UFUNCTION()
	void OnRep_Lobby();

	void NotifyChanged();
};
