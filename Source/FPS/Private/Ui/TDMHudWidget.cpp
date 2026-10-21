#include "Ui/TDMHudWidget.h"

#include "Ui/MatchHeaderWidget.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UTDMHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimerHandle, this, &ThisClass::Refresh, 0.2f, true);
	}
	Refresh();
}

void UTDMHudWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}
	Super::NativeDestruct();
}

void UTDMHudWidget::BindMatch(AShooterGameState* InGameState)
{
	UnbindMatch();
	Super::BindMatch(InGameState);
	if (AShooterGameState* GS = BoundGameState.Get())
	{
		GS->OnTeamScoreChanged.AddUniqueDynamic(this, &ThisClass::HandleTeamScoreChanged);
		GS->OnMatchStateChanged.AddUniqueDynamic(this, &ThisClass::HandleMatchStateChanged);
	}
	Refresh();
}

void UTDMHudWidget::UnbindMatch()
{
	if (AShooterGameState* GS = BoundGameState.Get())
	{
		GS->OnTeamScoreChanged.RemoveDynamic(this, &ThisClass::HandleTeamScoreChanged);
		GS->OnMatchStateChanged.RemoveDynamic(this, &ThisClass::HandleMatchStateChanged);
	}
	Super::UnbindMatch();
}

void UTDMHudWidget::HandleTeamScoreChanged(uint8 TeamId, int32 NewScore)
{
	(void)TeamId;
	(void)NewScore;
	Refresh();
}

void UTDMHudWidget::HandleMatchStateChanged(EShooterMatchState NewState)
{
	(void)NewState;
	Refresh();
}

void UTDMHudWidget::Refresh()
{
	AShooterGameState* GS = BoundGameState.Get();
	if (!IsValid(GS)) return;

	const int32 ScoreA = GS->GetTeamScore(0);
	const int32 ScoreB = GS->GetTeamScore(1);
	if (Text_TeamAScore) Text_TeamAScore->SetText(FText::AsNumber(ScoreA));
	if (Text_TeamBScore) Text_TeamBScore->SetText(FText::AsNumber(ScoreB));
	if (Text_Timer)
	{
		const float Remaining = GS->GetRemainingTime();
		Text_Timer->SetText(Remaining > 0.f ? UMatchHeaderWidget::FormatMatchTime(Remaining) : FText::FromString(TEXT("--:--")));
	}
	OnScoresUpdated(ScoreA, ScoreB);
}