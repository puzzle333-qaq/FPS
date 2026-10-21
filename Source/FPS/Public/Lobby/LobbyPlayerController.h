#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Loadout/LoadoutTypes.h"
#include "LobbyPlayerController.generated.h"

class ULobbyScreen;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLobbyRejected, const FText&, Reason);

// 房间大厅控制器：显示 LobbyWidgetClass，转发按钮到服务器
UCLASS()
class FPS_API ALobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// 填 WBP_RoomLobby（父类 LobbyScreen）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Room")
	TSubclassOf<ULobbyScreen> LobbyWidgetClass;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Room")
	FLobbyRejected OnLobbyRejected;

	UFUNCTION(Server, Reliable)
	void Server_RequestTeam(int32 TeamId);

	UFUNCTION(Server, Reliable)
	void Server_ToggleReady();

	UFUNCTION(Server, Reliable)
	void Server_SetRoomMode(EShooterGameMode Mode);

	UFUNCTION(Server, Reliable)
	void Server_StartGame();

	// 服务器拒绝（选边/开始/切模式）时告诉客户端原因
	UFUNCTION(Client, Reliable)
	void Client_LobbyRejected(const FText& Reason);

	UFUNCTION(BlueprintPure, Category = "FPS|Room")
	ULobbyScreen* GetLobbyScreen() const { return LobbyScreen; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TObjectPtr<ULobbyScreen> LobbyScreen;

	FTimerHandle CreateWidgetRetry;
	void TryCreateLobbyWidget();
};
