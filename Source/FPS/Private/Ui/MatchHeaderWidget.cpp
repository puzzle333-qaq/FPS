#include "Ui/MatchHeaderWidget.h"

#include "Game/ShooterPlayerState.h"
#include "Components/TextBlock.h"
#include "Styling/SlateColor.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UMatchHeaderWidget::BindMatch(AShooterGameState* InGameState)
{
	UnbindMatch();
	GameState = InGameState;
}

void UMatchHeaderWidget::UnbindMatch()
{
	GameState.Reset();
}

void UMatchHeaderWidget::NativeDestruct()
{
	UnbindMatch();
	Super::NativeDestruct();
}

FText UMatchHeaderWidget::FormatMatchTime(float Seconds)
{
	const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
	const int32 Minutes = Total / 60;
	const int32 Sec = Total % 60;
	return FText::FromString(FString::Printf(TEXT("%d:%02d"), Minutes, Sec));
}

void UTDMHeaderWidget::ResolveWidgets()
{
	if (!Text_TeamAScore) Text_TeamAScore = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_TeamAScore")));
	if (!Text_TeamBScore) Text_TeamBScore = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_TeamBScore")));
	if (!Text_Timer) Text_Timer = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Timer")));
	if (!Text_ScoreToWin) Text_ScoreToWin = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_ScoreToWin")));
	if (!Text_MatchState) Text_MatchState = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_MatchState")));
}

void UTDMHeaderWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveWidgets();
	ApplyScoreColors();
}

void UTDMHeaderWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}
	Super::NativeDestruct();
}

void UTDMHeaderWidget::BindMatch(AShooterGameState* InGameState)
{
	Super::BindMatch(InGameState);
	ResolveWidgets();
	if (!GameState.IsValid()) return;

	GameState->OnTeamScoreChanged.AddUniqueDynamic(this, &ThisClass::HandleTeamScoreChanged);
	GameState->OnMatchStateChanged.AddUniqueDynamic(this, &ThisClass::HandleMatchStateChanged);

	if (Text_ScoreToWin)
	{
		const int32 Need = GameState->GetScoreToWin();
		Text_ScoreToWin->SetText(Need > 0
			? FText::Format(NSLOCTEXT("MatchHUD", "ScoreToWinFmt", "先到 {0}"), FText::AsNumber(Need))
			: FText::GetEmpty());
	}

	BindLocalPlayerState();
	RefreshScores();
	RefreshMatchState();
	LastDisplayedSeconds = -1;
	RefreshTimer();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimerHandle, this, &ThisClass::RefreshTimer, 0.2f, true);
	}
}

void UTDMHeaderWidget::UnbindMatch()
{
	if (GameState.IsValid())
	{
		GameState->OnTeamScoreChanged.RemoveDynamic(this, &ThisClass::HandleTeamScoreChanged);
		GameState->OnMatchStateChanged.RemoveDynamic(this, &ThisClass::HandleMatchStateChanged);
	}
	if (BoundPlayerState.IsValid())
	{
		BoundPlayerState->OnTeamChanged.RemoveDynamic(this, &ThisClass::HandleLocalTeamChanged);
	}
	BoundPlayerState.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}
	Super::UnbindMatch();
}

void UTDMHeaderWidget::BindLocalPlayerState()
{
	APlayerController* PC = GetOwningPlayer();
	AShooterPlayerState* PS = IsValid(PC) ? PC->GetPlayerState<AShooterPlayerState>() : nullptr;
	if (PS == BoundPlayerState.Get()) return;

	if (BoundPlayerState.IsValid())
	{
		BoundPlayerState->OnTeamChanged.RemoveDynamic(this, &ThisClass::HandleLocalTeamChanged);
	}
	BoundPlayerState = PS;
	if (IsValid(PS))
	{
		PS->OnTeamChanged.AddUniqueDynamic(this, &ThisClass::HandleLocalTeamChanged);
		HandleLocalTeamChanged(PS->GetTeamId());
	}
}

void UTDMHeaderWidget::RefreshScores()
{
	if (!GameState.IsValid()) return;
	const int32 ScoreA = GameState->GetTeamScore(0);
	const int32 ScoreB = GameState->GetTeamScore(1);
	if (Text_TeamAScore) Text_TeamAScore->SetText(FText::AsNumber(ScoreA));
	if (Text_TeamBScore) Text_TeamBScore->SetText(FText::AsNumber(ScoreB));
	OnScoresUpdated(ScoreA, ScoreB);
}

void UTDMHeaderWidget::RefreshTimer()
{
	if (!GameState.IsValid()) return;

	BindLocalPlayerState();

	float Remaining = 0.f;
	bool bShowClock = true;
	switch (GameState->GetMatchState())
	{
	case EShooterMatchState::WaitingToStart:
		Remaining = GameState->GetWarmupRemaining();
		break;
	case EShooterMatchState::InProgress:
		if (GameState->HasTimeLimit())
		{
			Remaining = GameState->GetRemainingTime();
		}
		else
		{
			bShowClock = false;
		}
		break;
	case EShooterMatchState::PostMatch:
		Remaining = 0.f;
		break;
	}

	const int32 Seconds = bShowClock ? FMath::CeilToInt(Remaining) : -2;
	if (Seconds == LastDisplayedSeconds) return;
	LastDisplayedSeconds = Seconds;

	if (Text_Timer)
	{
		Text_Timer->SetText(bShowClock ? FormatMatchTime(Remaining) : UnlimitedTimeText);
	}
}

void UTDMHeaderWidget::RefreshMatchState()
{
	if (!GameState.IsValid()) return;
	const EShooterMatchState State = GameState->GetMatchState();
	if (Text_MatchState)
	{
		switch (State)
		{
		case EShooterMatchState::WaitingToStart: Text_MatchState->SetText(WarmupStateText); break;
		case EShooterMatchState::PostMatch:      Text_MatchState->SetText(PostMatchStateText); break;
		default:                                 Text_MatchState->SetText(InProgressStateText); break;
		}
	}
	OnHeaderMatchStateChanged(State);
}

void UTDMHeaderWidget::ApplyScoreColors()
{
	if (Text_TeamAScore) Text_TeamAScore->SetColorAndOpacity(FSlateColor(TeamAColor));
	if (Text_TeamBScore) Text_TeamBScore->SetColorAndOpacity(FSlateColor(TeamBColor));
}

void UTDMHeaderWidget::HandleTeamScoreChanged(uint8 TeamId, int32 NewScore)
{
	(void)TeamId;
	(void)NewScore;
	RefreshScores();
}

void UTDMHeaderWidget::HandleMatchStateChanged(EShooterMatchState NewState)
{
	(void)NewState;
	LastDisplayedSeconds = -1;
	RefreshMatchState();
	RefreshTimer();
}

void UTDMHeaderWidget::HandleLocalTeamChanged(uint8 TeamId)
{
	OnLocalTeamChanged(TeamId);
}
