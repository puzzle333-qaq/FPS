#include "Ui/ScoreboardScreen.h"

#include "Game/ShooterPlayerState.h"
#include "Game/ShooterGameState.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UScoreboardRowWidget::SetRow(const FScoreboardRow& InRow)
{
	Row = InRow;
	if (Text_Name) Text_Name->SetText(FText::FromString(Row.PlayerName));
	if (Text_Kills) Text_Kills->SetText(FText::AsNumber(Row.Kills));
	if (Text_Deaths) Text_Deaths->SetText(FText::AsNumber(Row.Deaths));
	if (Text_Assists) Text_Assists->SetText(FText::AsNumber(Row.Assists));
	OnRowUpdated(Row);
}

void UScoreboardScreen::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshScoreboard();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::RefreshScoreboard, FMath::Max(0.05f, RefreshInterval), true);
	}
}

void UScoreboardScreen::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
}

TArray<FScoreboardRow> UScoreboardScreen::CollectRows(bool bFilterTeam, uint8 TeamId) const
{
	TArray<FScoreboardRow> Rows;
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS) return Rows;
	const APlayerController* LocalPC = GetOwningPlayer();
	const APlayerState* LocalPS = LocalPC ? LocalPC->PlayerState.Get() : nullptr;
	for (APlayerState* Base : GS->PlayerArray)
	{
		const AShooterPlayerState* PS = Cast<AShooterPlayerState>(Base);
		if (!PS) continue;
		if (bFilterTeam && PS->GetTeamId() != TeamId) continue;
		FScoreboardRow R;
		R.PlayerName = PS->GetPlayerName();
		R.Kills = PS->GetKills();
		R.Deaths = PS->GetDeaths();
		R.Assists = PS->GetAssists();
		R.TeamId = PS->GetTeamId();
		R.bIsLocalPlayer = PS == LocalPS;
		Rows.Add(R);
	}
	Rows.Sort([](const FScoreboardRow& A, const FScoreboardRow& B)
	{
		if (A.Kills != B.Kills) return A.Kills > B.Kills;
		return A.Deaths < B.Deaths;
	});
	return Rows;
}

TArray<FScoreboardRow> UScoreboardScreen::GetTeamRows(uint8 TeamId) const
{
	return CollectRows(true, TeamId);
}

TArray<FScoreboardRow> UScoreboardScreen::GetAllRows() const
{
	return CollectRows(false, 0);
}

int32 UScoreboardScreen::GetTeamScore(uint8 TeamId) const
{
	const UWorld* World = GetWorld();
	const AShooterGameState* GS = World ? World->GetGameState<AShooterGameState>() : nullptr;
	return GS ? GS->GetTeamScore(TeamId) : 0;
}

void UScoreboardScreen::FillList(UPanelWidget* List, const TArray<FScoreboardRow>& Rows)
{
	if (!List || !RowWidgetClass) return;
	// 复用已有行，多了删、少了补
	while (List->GetChildrenCount() > Rows.Num())
	{
		List->RemoveChildAt(List->GetChildrenCount() - 1);
	}
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		UScoreboardRowWidget* RowWidget = i < List->GetChildrenCount() ? Cast<UScoreboardRowWidget>(List->GetChildAt(i)) : nullptr;
		if (!RowWidget)
		{
			RowWidget = CreateWidget<UScoreboardRowWidget>(GetOwningPlayer(), RowWidgetClass);
			if (!RowWidget) continue;
			List->AddChild(RowWidget);
		}
		RowWidget->SetRow(Rows[i]);
	}
}

void UScoreboardScreen::RefreshScoreboard()
{
	const UWorld* World = GetWorld();
	const AShooterGameState* SGS = World ? World->GetGameState<AShooterGameState>() : nullptr;
	const bool bTeams = SGS && SGS->GetNumTeams() >= 2;
	if (bTeams)
	{
		FillList(TeamAList, GetTeamRows(0));
		FillList(TeamBList, GetTeamRows(1));
	}
	else
	{
		FillList(TeamAList, GetAllRows());
		FillList(TeamBList, TArray<FScoreboardRow>());
	}
	if (Text_TeamAScore) Text_TeamAScore->SetText(FText::AsNumber(GetTeamScore(0)));
	if (Text_TeamBScore) Text_TeamBScore->SetText(FText::AsNumber(GetTeamScore(1)));
	OnScoreboardRefreshed();
}
