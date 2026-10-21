#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Loadout/LoadoutTypes.h"
#include "LobbyGameMode.generated.h"

class ALobbyGameState;
class ALobbyPlayerController;
class AShooterPlayerState;

// 房间大厅（开局前）。所有规则在服务器判断；专用服务器 / listen 都能用。
// 地图 World Settings → GameMode Override 选 BP_LobbyGameMode（父类 LobbyGameMode）。
UCLASS()
class FPS_API ALobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALobbyGameMode();

	// 每队最多人数
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Room", meta = (ClampMin = "1"))
	int32 MaxPlayersPerTeam = 5;

	// 换边后两队人数差不能超过这个
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Room", meta = (ClampMin = "0"))
	int32 MaxTeamDifference = 2;

	// 可选的模式（房主只能在这里面切）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Room")
	TArray<EShooterGameMode> AllowedModes = { EShooterGameMode::TDM, EShooterGameMode::SearchDestroy };

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	// ---- 由 LobbyPlayerController 的 Server RPC 调用 ----
	void RequestTeam(ALobbyPlayerController* PC, int32 TeamId);
	void ToggleReady(ALobbyPlayerController* PC);
	void RequestSetMode(ALobbyPlayerController* PC, EShooterGameMode Mode);
	void RequestStart(ALobbyPlayerController* PC);

	// 共用规则（UI 也按这个算 CanStart）
	static bool CheckCanStart(const UWorld* World, FText& OutReason);

private:
	ALobbyGameState* GetLobbyState() const;
	void OnPlayerEnteredRoom(APlayerController* PC);
	void EnsureUniqueName(APlayerController* PC);
	void RefreshHost(APlayerState* Leaving = nullptr);
	void CountTeams(int32& OutA, int32& OutB, const APlayerState* Exclude) const;
	bool IsHost(const APlayerController* PC) const;
	void Reject(ALobbyPlayerController* PC, const FText& Reason);
	void ResetAllReady();

	int32 NextJoinOrder = 1;
	// InitNewPlayer 解析到的 ?Host=1 / ?Mode=
	TSet<TWeakObjectPtr<APlayerController>> HostClaimers;
	TMap<TWeakObjectPtr<APlayerController>, int32> RequestedModes;
};
