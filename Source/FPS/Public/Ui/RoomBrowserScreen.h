#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/ShooterSessionSubsystem.h"
#include "RoomBrowserScreen.generated.h"

class UTextBlock;
class UButton;
class UPanelWidget;
class UWidget;
class URoomBrowserScreen;

// 房间列表一行：WBP_RoomBrowserRow，父类 RoomBrowserRowWidget
UCLASS()
class FPS_API URoomBrowserRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FRoomBrowserEntry& InEntry, URoomBrowserScreen* InOwner);

	UFUNCTION(BlueprintPure, Category = "FPS|Session")
	FRoomBrowserEntry GetEntry() const { return Entry; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Session")
	void JoinThisRoom();

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Session")
	void OnEntrySet(const FRoomBrowserEntry& InEntry);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_RoomCode;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Mode;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Players;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Ping;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Host;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Join;

private:
	UFUNCTION() void HandleJoin();
	FRoomBrowserEntry Entry;
	TWeakObjectPtr<URoomBrowserScreen> Owner;
};

// 房间列表页：WBP_RoomBrowser，父类 RoomBrowserScreen。主菜单的模式按钮调用 ShowRoomBrowser(Mode)
UCLASS()
class FPS_API URoomBrowserScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FPS|Session")
	void SetMode(EShooterGameMode InMode);

	UFUNCTION(BlueprintPure, Category = "FPS|Session")
	EShooterGameMode GetMode() const { return Mode; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Session")
	void RefreshRooms();

	// 本机开房（listen server → 房间大厅，你是房主）
	UFUNCTION(BlueprintCallable, Category = "FPS|Session")
	void CreateRoom();

	UFUNCTION(BlueprintCallable, Category = "FPS|Session")
	void JoinRoomAt(int32 ResultIndex);

	// 输入框里是 IP（含 . 或 :）就直连，否则当房间号
	UFUNCTION(BlueprintCallable, Category = "FPS|Session")
	void JoinByCodeOrIP(const FString& CodeOrIP);

	UFUNCTION(BlueprintCallable, Category = "FPS|Session")
	void Back();

	UFUNCTION(BlueprintCallable, Category = "FPS|Session")
	void SetStatus(const FText& InText);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Session")
	void OnRoomsUpdated(const TArray<FRoomBrowserEntry>& Rooms);

	// 点返回后触发（默认已经把自己从屏幕移除，主菜单可在这里切回首页）
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Session")
	void OnBackRequested();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> RoomList;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Create;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Refresh;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_JoinCode;
	// EditableTextBox 或 EditableText 都行
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UWidget> EditText_RoomCode;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Back;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Status;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Mode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Session")
	TSubclassOf<URoomBrowserRowWidget> RowWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Session")
	EShooterGameMode Mode = EShooterGameMode::TDM;

private:
	UFUNCTION() void HandleCreate();
	UFUNCTION() void HandleRefresh();
	UFUNCTION() void HandleJoinCode();
	UFUNCTION() void HandleBack();
	UFUNCTION() void HandleRoomsFound(bool bSuccess, const TArray<FRoomBrowserEntry>& Rooms);
	UFUNCTION() void HandleJoinFailed(const FText& Reason);

	UShooterSessionSubsystem* GetSessions() const;
	FString GetCodeInput() const;
};
