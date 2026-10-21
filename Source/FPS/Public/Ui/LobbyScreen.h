#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Loadout/LoadoutTypes.h"
#include "LobbyScreen.generated.h"

class UTextBlock;
class UButton;
class UPanelWidget;
class ALobbyPlayerController;

USTRUCT(BlueprintType)
struct FLobbyPlayerRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Room")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Room")
	bool bReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Room")
	bool bIsHost = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Room")
	bool bIsLocalPlayer = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Room")
	uint8 TeamId = 255;
};

// 一行玩家：WBP_LobbyPlayerRow，父类 LobbyPlayerRowWidget；放 Text_Name / Text_Status（可选）
UCLASS()
class FPS_API ULobbyPlayerRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FPS|Room")
	void SetRow(const FLobbyPlayerRow& InRow);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Room")
	void OnRowUpdated(const FLobbyPlayerRow& InRow);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Status;
};

// 房间大厅界面：WBP_RoomLobby，父类 LobbyScreen。
// 控件都是可选的，按名字自动绑定；也可以全部自己用下面的函数做。
UCLASS()
class FPS_API ULobbyScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FPS|Room")
	void JoinTeam(int32 TeamId);

	UFUNCTION(BlueprintCallable, Category = "FPS|Room")
	void ToggleReady();

	UFUNCTION(BlueprintCallable, Category = "FPS|Room")
	void StartGame();

	UFUNCTION(BlueprintCallable, Category = "FPS|Room")
	void SetRoomMode(EShooterGameMode Mode);

	UFUNCTION(BlueprintCallable, Category = "FPS|Room")
	void LeaveRoom();

	UFUNCTION(BlueprintPure, Category = "FPS|Room")
	FString GetRoomCode() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Room")
	EShooterGameMode GetRoomMode() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Room")
	FText GetRoomModeName() const;

	// 0 = 左（TeamA），1 = 右（TeamB），255 = 未选边
	UFUNCTION(BlueprintPure, Category = "FPS|Room")
	TArray<FLobbyPlayerRow> GetTeamPlayers(int32 TeamId) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Room")
	bool IsLocalHost() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Room")
	bool IsLocalReady() const;

	// 本地按服务器同样的规则预判（真正判断在服务器）
	UFUNCTION(BlueprintPure, Category = "FPS|Room")
	bool CanStart(FText& OutReason) const;

	// 房间信息/人员有变化时触发
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Room")
	void OnLobbyUpdated();

	// 服务器拒绝（满员/人数差/没准备…）
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Room")
	void OnJoinTeamRejected(const FText& Reason);

	void HandleRejected(const FText& Reason);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> TeamAList;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> TeamBList;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> UnassignedList;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_RoomCode;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Mode;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Message;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_JoinA;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_JoinB;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Ready;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ReadyLabel;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Start;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Leave;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_ModeTDM;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_ModeBDM;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Room")
	TSubclassOf<ULobbyPlayerRowWidget> RowWidgetClass;

private:
	UFUNCTION() void HandleJoinA();
	UFUNCTION() void HandleJoinB();
	UFUNCTION() void HandleReady();
	UFUNCTION() void HandleStart();
	UFUNCTION() void HandleLeave();
	UFUNCTION() void HandleModeTDM();
	UFUNCTION() void HandleModeBDM();

	ALobbyPlayerController* GetLobbyPC() const;
	void Refresh();
	void FillList(UPanelWidget* List, const TArray<FLobbyPlayerRow>& Rows);
	FString LastSignature;
	FTimerHandle RefreshTimer;
};
