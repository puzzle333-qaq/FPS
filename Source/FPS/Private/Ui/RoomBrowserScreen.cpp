#include "Ui/RoomBrowserScreen.h"

#include "ShooterGameInstance.h"
#include "Menu/MainMenuPlayerController.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/EditableText.h"

static FText FPS_ModeName(const UGameInstance* GIBase, EShooterGameMode Mode)
{
	if (const UShooterGameInstance* GI = Cast<UShooterGameInstance>(GIBase))
	{
		FShooterModeInfo Info;
		if (GI->GetModeInfo(Mode, Info) && !Info.DisplayName.IsEmpty()) return Info.DisplayName;
	}
	return StaticEnum<EShooterGameMode>()->GetDisplayNameTextByValue((int64)Mode);
}

// ---------------- Row ----------------

void URoomBrowserRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_Join) Button_Join->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleJoin);
}

void URoomBrowserRowWidget::Setup(const FRoomBrowserEntry& InEntry, URoomBrowserScreen* InOwner)
{
	Entry = InEntry;
	Owner = InOwner;
	if (Text_RoomCode) Text_RoomCode->SetText(FText::FromString(Entry.RoomCode));
	if (Text_Mode)
	{
		FText M = FPS_ModeName(GetGameInstance(), Entry.Mode);
		if (Entry.bInGame) M = FText::Format(NSLOCTEXT("Browser", "InGame", "{0}（对局中）"), M);
		Text_Mode->SetText(M);
	}
	if (Text_Players) Text_Players->SetText(FText::Format(NSLOCTEXT("Browser", "Players", "{0}/{1}"), FText::AsNumber(Entry.Players), FText::AsNumber(Entry.MaxPlayers)));
	if (Text_Ping) Text_Ping->SetText(FText::Format(NSLOCTEXT("Browser", "Ping", "{0} ms"), FText::AsNumber(Entry.PingMs)));
	if (Text_Host) Text_Host->SetText(FText::FromString(Entry.HostName));
	if (Button_Join) Button_Join->SetIsEnabled(!Entry.bInGame && (Entry.MaxPlayers <= 0 || Entry.Players < Entry.MaxPlayers));
	OnEntrySet(Entry);
}

void URoomBrowserRowWidget::HandleJoin() { JoinThisRoom(); }

void URoomBrowserRowWidget::JoinThisRoom()
{
	if (Owner.IsValid()) Owner->JoinRoomAt(Entry.ResultIndex);
}

// ---------------- Screen ----------------

UShooterSessionSubsystem* URoomBrowserScreen::GetSessions() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UShooterSessionSubsystem>() : nullptr;
}

void URoomBrowserScreen::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_Create) Button_Create->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCreate);
	if (Button_Refresh) Button_Refresh->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRefresh);
	if (Button_JoinCode) Button_JoinCode->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleJoinCode);
	if (Button_Back) Button_Back->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBack);
	if (UShooterSessionSubsystem* S = GetSessions())
	{
		S->OnRoomsFound.AddUniqueDynamic(this, &ThisClass::HandleRoomsFound);
		S->OnJoinFailed.AddUniqueDynamic(this, &ThisClass::HandleJoinFailed);
	}
	if (const UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>())
	{
		Mode = GI->SelectedMode;
	}
	SetMode(Mode);
}

void URoomBrowserScreen::NativeDestruct()
{
	if (UShooterSessionSubsystem* S = GetSessions())
	{
		S->OnRoomsFound.RemoveDynamic(this, &ThisClass::HandleRoomsFound);
		S->OnJoinFailed.RemoveDynamic(this, &ThisClass::HandleJoinFailed);
	}
	Super::NativeDestruct();
}

void URoomBrowserScreen::SetMode(EShooterGameMode InMode)
{
	Mode = InMode;
	if (UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>()) GI->SelectMode(Mode);
	if (Text_Mode) Text_Mode->SetText(FPS_ModeName(GetGameInstance(), Mode));
	RefreshRooms();
}

void URoomBrowserScreen::SetStatus(const FText& InText)
{
	if (Text_Status) Text_Status->SetText(InText);
}

void URoomBrowserScreen::RefreshRooms()
{
	UShooterSessionSubsystem* S = GetSessions();
	if (!S) return;
	SetStatus(NSLOCTEXT("Browser", "Searching", "正在搜索房间…"));
	S->FindRooms(Mode, false);
}

void URoomBrowserScreen::HandleRoomsFound(bool bSuccess, const TArray<FRoomBrowserEntry>& Rooms)
{
	if (RoomList && RowWidgetClass)
	{
		RoomList->ClearChildren();
		for (const FRoomBrowserEntry& E : Rooms)
		{
			if (URoomBrowserRowWidget* Row = CreateWidget<URoomBrowserRowWidget>(GetOwningPlayer(), RowWidgetClass))
			{
				RoomList->AddChild(Row);
				Row->Setup(E, this);
			}
		}
	}
	if (!bSuccess) SetStatus(NSLOCTEXT("Browser", "SearchFail", "搜索失败"));
	else if (Rooms.Num() == 0) SetStatus(NSLOCTEXT("Browser", "None", "没有找到房间，可以创建一个"));
	else SetStatus(FText::Format(NSLOCTEXT("Browser", "Count", "找到 {0} 个房间"), FText::AsNumber(Rooms.Num())));
	OnRoomsUpdated(Rooms);
}

void URoomBrowserScreen::HandleJoinFailed(const FText& Reason)
{
	SetStatus(Reason);
}

void URoomBrowserScreen::CreateRoom()
{
	UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>();
	if (!GI) return;
	SetStatus(NSLOCTEXT("Browser", "Creating", "正在创建房间…"));
	if (!GI->CreateRoom(Mode))
	{
		SetStatus(NSLOCTEXT("Browser", "CreateFail", "创建失败：GameInstance 没填 RoomLobbyMap"));
	}
}

void URoomBrowserScreen::JoinRoomAt(int32 ResultIndex)
{
	if (UShooterSessionSubsystem* S = GetSessions())
	{
		SetStatus(NSLOCTEXT("Browser", "Joining", "正在加入…"));
		S->JoinRoom(ResultIndex);
	}
}

void URoomBrowserScreen::JoinByCodeOrIP(const FString& CodeOrIP)
{
	const FString In = CodeOrIP.TrimStartAndEnd();
	if (In.Contains(TEXT(".")) || In.Contains(TEXT(":")) || In.Equals(TEXT("localhost"), ESearchCase::IgnoreCase))
	{
		if (UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>())
		{
			SetStatus(FText::Format(NSLOCTEXT("Browser", "ConnIP", "正在连接 {0}…"), FText::FromString(In)));
			GI->JoinByIP(In);
		}
		return;
	}
	if (UShooterSessionSubsystem* S = GetSessions())
	{
		SetStatus(FText::Format(NSLOCTEXT("Browser", "FindCode", "正在查找房间 {0}…"), FText::FromString(In.ToUpper())));
		S->JoinByCode(In);
	}
}

FString URoomBrowserScreen::GetCodeInput() const
{
	if (const UEditableTextBox* Box = Cast<UEditableTextBox>(EditText_RoomCode)) return Box->GetText().ToString();
	if (const UEditableText* Txt = Cast<UEditableText>(EditText_RoomCode)) return Txt->GetText().ToString();
	return FString();
}

void URoomBrowserScreen::Back()
{
	// 嵌在主菜单 Switcher 里就切回上一页，否则从屏幕移除
	if (AMainMenuPlayerController* PC = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
	{
		PC->HideRoomBrowser();
	}
	else
	{
		RemoveFromParent();
	}
	OnBackRequested();
}

void URoomBrowserScreen::HandleCreate() { CreateRoom(); }
void URoomBrowserScreen::HandleRefresh() { RefreshRooms(); }
void URoomBrowserScreen::HandleJoinCode() { JoinByCodeOrIP(GetCodeInput()); }
void URoomBrowserScreen::HandleBack() { Back(); }
