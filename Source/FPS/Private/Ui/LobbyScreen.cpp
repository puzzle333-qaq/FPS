#include "Ui/LobbyScreen.h"

#include "Lobby/LobbyGameState.h"
#include "Lobby/LobbyGameMode.h"
#include "Lobby/LobbyPlayerController.h"
#include "Game/ShooterPlayerState.h"
#include "ShooterGameInstance.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Ui/TacticalUIStyle.h"

void ULobbyPlayerRowWidget::SetRow(const FLobbyPlayerRow& InRow)
{
	FTacticalUIStyle::ApplyToUserWidget(this);
	if (Text_Name)
	{
		FString Name = InRow.PlayerName;
		if (InRow.bIsHost) Name = TEXT("[房主] ") + Name;
		if (InRow.bIsLocalPlayer) Name += TEXT("（你）");
		Text_Name->SetText(FText::FromString(Name));
	}
	if (Text_Status)
	{
		Text_Status->SetText(InRow.bIsBot
			? NSLOCTEXT("Lobby", "BotStatus", "BOT")
			: (InRow.bReady ? NSLOCTEXT("Lobby", "Ready", "已准备") : NSLOCTEXT("Lobby", "NotReadyShort", "未准备")));
	}
	OnRowUpdated(InRow);
}

void ULobbyScreen::NativeConstruct()
{
	Super::NativeConstruct();
	FTacticalUIStyle::ApplyToUserWidget(this);
	if (Button_JoinA) Button_JoinA->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleJoinA);
	if (Button_JoinB) Button_JoinB->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleJoinB);
	if (Button_Ready) Button_Ready->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReady);
	if (Button_Start) Button_Start->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStart);
	EnsureBotButtons();
	if (Button_AddBotA) Button_AddBotA->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAddBotA);
	if (Button_AddBotB) Button_AddBotB->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAddBotB);
	if (Button_RemoveBot) Button_RemoveBot->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRemoveBot);
	if (Button_Leave) Button_Leave->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLeave);
	if (Button_ModeTDM) Button_ModeTDM->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleModeTDM);
	if (Button_ModeBDM) Button_ModeBDM->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleModeBDM);
	Refresh();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::Refresh, 0.2f, true);
	}
}

void ULobbyScreen::NativeDestruct()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(RefreshTimer);
	Super::NativeDestruct();
}

ALobbyPlayerController* ULobbyScreen::GetLobbyPC() const
{
	return Cast<ALobbyPlayerController>(GetOwningPlayer());
}

void ULobbyScreen::JoinTeam(int32 TeamId) { if (ALobbyPlayerController* PC = GetLobbyPC()) PC->Server_RequestTeam(TeamId); }
void ULobbyScreen::ToggleReady() { if (ALobbyPlayerController* PC = GetLobbyPC()) PC->Server_ToggleReady(); }
void ULobbyScreen::SetRoomMode(EShooterGameMode Mode) { if (ALobbyPlayerController* PC = GetLobbyPC()) PC->Server_SetRoomMode(Mode); }

void ULobbyScreen::AddBot(int32 TeamId)
{
	// BOTS DISABLED — set bBotsDisabled=false (or delete this if) to re-enable
	static volatile bool bBotsDisabled = true;
	if (bBotsDisabled) { return; }
	if (ALobbyPlayerController* PC = GetLobbyPC()) PC->Server_AddBot(TeamId);
}

void ULobbyScreen::RemoveBot()
{
	if (ALobbyPlayerController* PC = GetLobbyPC()) PC->Server_RemoveBot();
}

void ULobbyScreen::StartGame()
{
	FText Reason;
	if (!CanStart(Reason))
	{
		HandleRejected(Reason);
		return;
	}
	if (ALobbyPlayerController* PC = GetLobbyPC()) PC->Server_StartGame();
}

void ULobbyScreen::LeaveRoom()
{
	if (UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>())
	{
		GI->GoToMainMenu();
	}
}

void ULobbyScreen::HandleJoinA() { JoinTeam(0); }
void ULobbyScreen::HandleJoinB() { JoinTeam(1); }
void ULobbyScreen::HandleReady() { ToggleReady(); }
void ULobbyScreen::HandleStart() { StartGame(); }
void ULobbyScreen::HandleAddBotA() { AddBot(0); }
void ULobbyScreen::HandleAddBotB() { AddBot(1); }
void ULobbyScreen::HandleRemoveBot() { RemoveBot(); }
void ULobbyScreen::HandleLeave() { LeaveRoom(); }
void ULobbyScreen::HandleModeTDM() { SetRoomMode(EShooterGameMode::PoliceCriminal); }
void ULobbyScreen::HandleModeBDM() { SetRoomMode(EShooterGameMode::PoliceCriminal); }

FString ULobbyScreen::GetRoomCode() const
{
	const ALobbyGameState* GS = GetWorld() ? GetWorld()->GetGameState<ALobbyGameState>() : nullptr;
	return GS ? GS->GetRoomCode() : FString();
}

EShooterGameMode ULobbyScreen::GetRoomMode() const
{
	const ALobbyGameState* GS = GetWorld() ? GetWorld()->GetGameState<ALobbyGameState>() : nullptr;
	return GS ? GS->GetRoomMode() : EShooterGameMode::PoliceCriminal;
}

FText ULobbyScreen::GetRoomModeName() const
{
	const EShooterGameMode Mode = GetRoomMode();
	if (const UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>())
	{
		FShooterModeInfo Info;
		if (GI->GetModeInfo(Mode, Info) && !Info.DisplayName.IsEmpty()) return Info.DisplayName;
	}
	return StaticEnum<EShooterGameMode>()->GetDisplayNameTextByValue((int64)Mode);
}

TArray<FLobbyPlayerRow> ULobbyScreen::GetTeamPlayers(int32 TeamId) const
{
	TArray<FLobbyPlayerRow> Rows;
	const ALobbyGameState* GS = GetWorld() ? GetWorld()->GetGameState<ALobbyGameState>() : nullptr;
	if (!GS) return Rows;
	const APlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->PlayerState.Get() : nullptr;
	TArray<const AShooterPlayerState*> Sorted;
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const AShooterPlayerState* SPS = Cast<AShooterPlayerState>(PS);
		if (!SPS || SPS->IsInactive()) continue;
		const int32 PSTeam = SPS->HasTeam() ? SPS->GetTeamId() : 255;
		if (PSTeam != TeamId) continue;
		Sorted.Add(SPS);
	}
	Sorted.Sort([](const AShooterPlayerState& A, const AShooterPlayerState& B) { return A.GetJoinOrder() < B.GetJoinOrder(); });
	for (const AShooterPlayerState* SPS : Sorted)
	{
		FLobbyPlayerRow R;
		R.PlayerName = SPS->GetPlayerName();
		R.bReady = SPS->IsReady();
		R.bIsHost = GS->GetHostPlayerState() == SPS;
		R.bIsLocalPlayer = SPS == LocalPS;
		R.bIsBot = SPS->IsABot();
		R.TeamId = SPS->GetTeamId();
		Rows.Add(R);
	}
	return Rows;
}

bool ULobbyScreen::IsLocalHost() const
{
	const ALobbyGameState* GS = GetWorld() ? GetWorld()->GetGameState<ALobbyGameState>() : nullptr;
	return GS && GetOwningPlayer() && GS->GetHostPlayerState() == GetOwningPlayer()->PlayerState;
}

bool ULobbyScreen::IsLocalReady() const
{
	const AShooterPlayerState* PS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AShooterPlayerState>() : nullptr;
	return PS && PS->IsReady();
}

bool ULobbyScreen::CanStart(FText& OutReason) const
{
	if (!IsLocalHost())
	{
		OutReason = NSLOCTEXT("Lobby", "NotHost", "只有房主可以操作");
		return false;
	}
	return ALobbyGameMode::CheckCanStart(GetWorld(), OutReason);
}

void ULobbyScreen::HandleRejected(const FText& Reason)
{
	if (Text_Message) Text_Message->SetText(Reason);
	OnJoinTeamRejected(Reason);
}

void ULobbyScreen::FillList(UPanelWidget* List, const TArray<FLobbyPlayerRow>& Rows)
{
	if (!List || !RowWidgetClass) return;
	while (List->GetChildrenCount() > Rows.Num()) List->RemoveChildAt(List->GetChildrenCount() - 1);
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		ULobbyPlayerRowWidget* RowW = i < List->GetChildrenCount() ? Cast<ULobbyPlayerRowWidget>(List->GetChildAt(i)) : nullptr;
		if (!RowW)
		{
			RowW = CreateWidget<ULobbyPlayerRowWidget>(GetOwningPlayer(), RowWidgetClass);
			if (!RowW) continue;
			List->AddChild(RowW);
		}
		RowW->SetRow(Rows[i]);
	}
}

UButton* ULobbyScreen::MakeBotButton(const FText& Label)
{
	if (!WidgetTree) return nullptr;
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	if (!Button || !Text) return Button;
	Text->SetText(Label);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = 14;
	Text->SetFont(Font);
	Button->SetContent(Text);
	return Button;
}

void ULobbyScreen::EnsureBotButtons()
{
	// 蓝图里已经放了同名按钮就用蓝图的。没有的话，排在开始按钮旁边。
	if (Button_AddBotA && Button_AddBotB && Button_RemoveBot) return;
	if (!WidgetTree) return;

	BotButtonBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BotButtonBar"));
	if (!BotButtonBar) return;

	auto Place = [&](TObjectPtr<UButton>& SlotButton, const FText& Label)
	{
		if (SlotButton) return;
		SlotButton = MakeBotButton(Label);
		if (!SlotButton) return;
		if (UHorizontalBoxSlot* BoxSlot = BotButtonBar->AddChildToHorizontalBox(SlotButton))
		{
			BoxSlot->SetPadding(FMargin(6.f, 0.f));
			BoxSlot->SetVerticalAlignment(VAlign_Center);
		}
	};
	Place(Button_AddBotA, NSLOCTEXT("Lobby", "AddBotA", "左边加BOT"));
	Place(Button_AddBotB, NSLOCTEXT("Lobby", "AddBotB", "右边加BOT"));
	Place(Button_RemoveBot, NSLOCTEXT("Lobby", "RemoveBot", "移除BOT"));

	UPanelWidget* Parent = Button_Start ? Button_Start->GetParent() : nullptr;
	if (!Parent) Parent = Cast<UPanelWidget>(WidgetTree->RootWidget);
	if (UVerticalBox* VBox = Cast<UVerticalBox>(Parent))
	{
		if (UVerticalBoxSlot* VSlot = VBox->AddChildToVerticalBox(BotButtonBar))
		{
			VSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
			VSlot->SetHorizontalAlignment(HAlign_Center);
		}
		return;
	}
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent))
	{
		if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(BotButtonBar))
		{
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetZOrder(20);
			if (UCanvasPanelSlot* StartSlot = Button_Start ? Cast<UCanvasPanelSlot>(Button_Start->Slot) : nullptr)
			{
				CanvasSlot->SetAnchors(StartSlot->GetAnchors());
				CanvasSlot->SetAlignment(StartSlot->GetAlignment());
				FVector2D Pos = StartSlot->GetPosition();
				Pos.Y += FMath::Max(StartSlot->GetSize().Y, 36.f) + 8.f;
				CanvasSlot->SetPosition(Pos);
				CanvasSlot->SetZOrder(StartSlot->GetZOrder() + 1);
			}
			else
			{
				CanvasSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
				CanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));
				CanvasSlot->SetPosition(FVector2D(0.f, -16.f));
			}
		}
		return;
	}
	if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
	{
		if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(BotButtonBar))
		{
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));
			CanvasSlot->SetPosition(FVector2D(0.f, -16.f));
			CanvasSlot->SetZOrder(50);
		}
		return;
	}
	if (Parent)
	{
		Parent->AddChild(BotButtonBar);
	}
}

void ULobbyScreen::Refresh()
{
	const TArray<FLobbyPlayerRow> A = GetTeamPlayers(0);
	const TArray<FLobbyPlayerRow> B = GetTeamPlayers(1);
	const TArray<FLobbyPlayerRow> U = GetTeamPlayers(255);
	const bool bHost = IsLocalHost();

	FString Sig = GetRoomCode() + FString::FromInt((int32)GetRoomMode()) + (bHost ? TEXT("H") : TEXT("-"));
	for (const TArray<FLobbyPlayerRow>* L : { &A, &B, &U })
	{
		Sig += TEXT("|");
		for (const FLobbyPlayerRow& R : *L) Sig += R.PlayerName + (R.bReady ? TEXT("1") : TEXT("0")) + (R.bIsHost ? TEXT("H") : TEXT(""));
	}

	FillList(TeamAList, A);
	FillList(TeamBList, B);
	FillList(UnassignedList, U);
	if (Text_RoomCode) Text_RoomCode->SetText(FText::Format(NSLOCTEXT("Lobby", "Code", "房间号：{0}"), FText::FromString(GetRoomCode())));
	if (Text_Mode) Text_Mode->SetText(GetRoomModeName());
	if (Text_ReadyLabel) Text_ReadyLabel->SetText(IsLocalReady() ? NSLOCTEXT("Lobby", "Unready", "取消准备") : NSLOCTEXT("Lobby", "DoReady", "准备"));
	if (Button_Start)
	{
		Button_Start->SetVisibility(bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		FText Dummy;
		Button_Start->SetIsEnabled(bHost && CanStart(Dummy));
	}
	if (Button_ModeTDM) Button_ModeTDM->SetIsEnabled(bHost);
	if (Button_ModeBDM) Button_ModeBDM->SetIsEnabled(bHost);
	const ESlateVisibility BotVis = bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (BotButtonBar) BotButtonBar->SetVisibility(BotVis);
	if (Button_AddBotA) Button_AddBotA->SetVisibility(BotVis);
	if (Button_AddBotB) Button_AddBotB->SetVisibility(BotVis);
	if (Button_RemoveBot) Button_RemoveBot->SetVisibility(BotVis);

	if (Sig != LastSignature)
	{
		LastSignature = Sig;
		OnLobbyUpdated();
	}
}
