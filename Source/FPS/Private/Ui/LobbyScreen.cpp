#include "Ui/LobbyScreen.h"

#include "Lobby/LobbyGameState.h"
#include "Lobby/LobbyGameMode.h"
#include "Lobby/LobbyPlayerController.h"
#include "Game/ShooterPlayerState.h"
#include "ShooterGameInstance.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"

void ULobbyPlayerRowWidget::SetRow(const FLobbyPlayerRow& InRow)
{
	if (Text_Name)
	{
		FString Name = InRow.PlayerName;
		if (InRow.bIsHost) Name = TEXT("[房主] ") + Name;
		if (InRow.bIsLocalPlayer) Name += TEXT("（你）");
		Text_Name->SetText(FText::FromString(Name));
	}
	if (Text_Status)
	{
		Text_Status->SetText(InRow.bReady ? NSLOCTEXT("Lobby", "Ready", "已准备") : NSLOCTEXT("Lobby", "NotReadyShort", "未准备"));
	}
	OnRowUpdated(InRow);
}

void ULobbyScreen::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_JoinA) Button_JoinA->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleJoinA);
	if (Button_JoinB) Button_JoinB->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleJoinB);
	if (Button_Ready) Button_Ready->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReady);
	if (Button_Start) Button_Start->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStart);
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
void ULobbyScreen::HandleLeave() { LeaveRoom(); }
void ULobbyScreen::HandleModeTDM() { SetRoomMode(EShooterGameMode::TDM); }
void ULobbyScreen::HandleModeBDM() { SetRoomMode(EShooterGameMode::SearchDestroy); }

FString ULobbyScreen::GetRoomCode() const
{
	const ALobbyGameState* GS = GetWorld() ? GetWorld()->GetGameState<ALobbyGameState>() : nullptr;
	return GS ? GS->GetRoomCode() : FString();
}

EShooterGameMode ULobbyScreen::GetRoomMode() const
{
	const ALobbyGameState* GS = GetWorld() ? GetWorld()->GetGameState<ALobbyGameState>() : nullptr;
	return GS ? GS->GetRoomMode() : EShooterGameMode::TDM;
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

	if (Sig != LastSignature)
	{
		LastSignature = Sig;
		OnLobbyUpdated();
	}
}
