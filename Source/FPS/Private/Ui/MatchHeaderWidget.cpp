#include "Ui/MatchHeaderWidget.h"

#include "Game/ShooterPlayerState.h"
#include "Game/SearchDestroyGameState.h"
#include "Game/BombSite.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
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

void USDHeaderWidget::ResolveWidgets()
{
	if (!Text_TeamAScore) Text_TeamAScore = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_TeamAScore")));
	if (!Text_TeamBScore) Text_TeamBScore = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_TeamBScore")));
	if (!Text_Timer) Text_Timer = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Timer")));
	if (!Text_Round) Text_Round = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Round")));
	if (!Text_BombState) Text_BombState = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_BombState")));
	if (!Text_Prompt) Text_Prompt = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Prompt")));
	if (!Text_Role) Text_Role = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Role")));
	if (!Progress_Action) Progress_Action = Cast<UProgressBar>(GetWidgetFromName(TEXT("Progress_Action")));
}

void USDHeaderWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveWidgets();
	if (Text_TeamAScore) Text_TeamAScore->SetColorAndOpacity(FSlateColor(TeamAColor));
	if (Text_TeamBScore) Text_TeamBScore->SetColorAndOpacity(FSlateColor(TeamBColor));
}

void USDHeaderWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}
	Super::NativeDestruct();
}

ASearchDestroyGameState* USDHeaderWidget::GetSDState() const
{
	return Cast<ASearchDestroyGameState>(GameState.Get());
}

void USDHeaderWidget::BindMatch(AShooterGameState* InGameState)
{
	Super::BindMatch(InGameState);
	ResolveWidgets();
	ASearchDestroyGameState* SD = GetSDState();
	if (!IsValid(SD)) return;

	SD->OnTeamScoreChanged.AddUniqueDynamic(this, &ThisClass::HandleTeamScoreChanged);
	SD->OnBombStateChanged.AddUniqueDynamic(this, &ThisClass::HandleBombStateChanged);
	SD->OnRoundPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandleRoundPhaseChanged);

	BindLocalPlayerState();
	RefreshScores();
	RefreshBomb();
	RefreshRole();
	LastDisplayedSeconds = -1;
	LastPrompt = EBombPrompt::None;
	TickHeader();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimerHandle, this, &ThisClass::TickHeader, 0.1f, true);
	}
}

void USDHeaderWidget::UnbindMatch()
{
	if (ASearchDestroyGameState* SD = GetSDState())
	{
		SD->OnTeamScoreChanged.RemoveDynamic(this, &ThisClass::HandleTeamScoreChanged);
		SD->OnBombStateChanged.RemoveDynamic(this, &ThisClass::HandleBombStateChanged);
		SD->OnRoundPhaseChanged.RemoveDynamic(this, &ThisClass::HandleRoundPhaseChanged);
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

void USDHeaderWidget::BindLocalPlayerState()
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

void USDHeaderWidget::RefreshScores()
{
	ASearchDestroyGameState* SD = GetSDState();
	if (!IsValid(SD)) return;
	const int32 ScoreA = SD->GetTeamScore(0);
	const int32 ScoreB = SD->GetTeamScore(1);
	if (Text_TeamAScore) Text_TeamAScore->SetText(FText::AsNumber(ScoreA));
	if (Text_TeamBScore) Text_TeamBScore->SetText(FText::AsNumber(ScoreB));
	OnScoresUpdated(ScoreA, ScoreB);
}

void USDHeaderWidget::RefreshTimer()
{
	ASearchDestroyGameState* SD = GetSDState();
	if (!IsValid(SD)) return;
	BindLocalPlayerState();

	const float Remaining = SD->GetRemainingTime();
	const int32 Seconds = FMath::CeilToInt(Remaining);
	if (Seconds != LastDisplayedSeconds)
	{
		LastDisplayedSeconds = Seconds;
		if (Text_Timer)
		{
			Text_Timer->SetText(FormatMatchTime(Remaining));
			Text_Timer->SetColorAndOpacity(FSlateColor(SD->IsBombDown() ? BombTimerColor : RoundTimerColor));
		}
	}

	if (Text_Round)
	{
		const int32 Round = SD->GetRoundIndex();
		if (SD->GetRoundPhase() == EBombRoundPhase::WaitingForPlayers)
		{
			Text_Round->SetText(NSLOCTEXT("SearchDestroy", "WaitingPlayers", "等待玩家"));
		}
		else
		Text_Round->SetText(Round > 0
			? FText::Format(NSLOCTEXT("SearchDestroy", "RoundFmt", "第 {0} 回合"), FText::AsNumber(Round))
			: NSLOCTEXT("SearchDestroy", "WarmupRound", "准备中"));
	}
}

void USDHeaderWidget::RefreshBomb()
{
	ASearchDestroyGameState* SD = GetSDState();
	if (!IsValid(SD) || !Text_BombState) return;

	FText Status;
	switch (SD->GetBombState())
	{
	case EBombState::Planting:
		Status = FText::Format(NSLOCTEXT("SearchDestroy", "Planting", "正在安放 {0}"), ABombSite::GetSiteLetter(SD->GetPlantedSite()));
		break;
	case EBombState::Planted:
		Status = FText::Format(NSLOCTEXT("SearchDestroy", "Planted", "炸弹已安放 {0}"), ABombSite::GetSiteLetter(SD->GetPlantedSite()));
		break;
	case EBombState::Defusing:
		Status = NSLOCTEXT("SearchDestroy", "Defusing", "正在拆除");
		break;
	case EBombState::Exploded:
		Status = NSLOCTEXT("SearchDestroy", "Exploded", "炸弹爆炸");
		break;
	case EBombState::Defused:
		Status = NSLOCTEXT("SearchDestroy", "Defused", "炸弹已拆除");
		break;
	default:
		Status = FText::GetEmpty();
		break;
	}
	Text_BombState->SetText(Status);
}

void USDHeaderWidget::RefreshRole()
{
	ASearchDestroyGameState* SD = GetSDState();
	if (!IsValid(SD) || !Text_Role) return;
	BindLocalPlayerState();
	if (!BoundPlayerState.IsValid() || !BoundPlayerState->HasTeam())
	{
		Text_Role->SetText(FText::GetEmpty());
		return;
	}
	const bool bAttacker = SD->IsAttackingTeam(BoundPlayerState->GetTeamId());
	Text_Role->SetText(bAttacker
		? NSLOCTEXT("SearchDestroy", "RoleAtk", "进攻")
		: NSLOCTEXT("SearchDestroy", "RoleDef", "防守"));
}

void USDHeaderWidget::RefreshPrompt()
{
	ASearchDestroyGameState* SD = GetSDState();
	APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = IsValid(PC) ? PC->GetPawn() : nullptr;
	const EBombPrompt Prompt = IsValid(SD) ? SD->GetPromptForPawn(Pawn) : EBombPrompt::None;
	if (Prompt == LastPrompt) return;
	LastPrompt = Prompt;

	FText PromptText;
	switch (Prompt)
	{
	case EBombPrompt::Pickup: PromptText = NSLOCTEXT("SearchDestroy", "PromptPickup", "靠近拾取炸弹"); break;
	case EBombPrompt::Plant:  PromptText = NSLOCTEXT("SearchDestroy", "PromptPlant", "按住 F 安放炸弹"); break;
	case EBombPrompt::Defuse: PromptText = NSLOCTEXT("SearchDestroy", "PromptDefuse", "按住 F 拆除炸弹"); break;
	default: break;
	}
	if (Text_Prompt)
	{
		Text_Prompt->SetText(PromptText);
		Text_Prompt->SetVisibility(Prompt == EBombPrompt::None ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	OnHeaderPromptChanged(Prompt);
}

void USDHeaderWidget::HandleTeamScoreChanged(uint8 TeamId, int32 NewScore)
{
	(void)TeamId;
	(void)NewScore;
	RefreshScores();
}

void USDHeaderWidget::HandleBombStateChanged(EBombState NewState)
{
	RefreshBomb();
	LastDisplayedSeconds = -1;
	RefreshTimer();
	OnHeaderBombStateChanged(NewState);
}

void USDHeaderWidget::HandleRoundPhaseChanged(EBombRoundPhase NewPhase)
{
	RefreshRole();
	RefreshBomb();
	LastDisplayedSeconds = -1;
	RefreshTimer();
	OnHeaderRoundPhaseChanged(NewPhase);
}

void USDHeaderWidget::HandleLocalTeamChanged(uint8 TeamId)
{
	RefreshRole();
	OnLocalTeamChanged(TeamId);
}

void USDHeaderWidget::TickHeader()
{
	RefreshTimer();
	RefreshPrompt();
	ASearchDestroyGameState* State = GetSDState();
	if (!IsValid(State) || !Progress_Action) return;
	const float Progress = State->GetActionProgress();
	const bool bShow = Progress > 0.f;
	Progress_Action->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bShow) Progress_Action->SetPercent(Progress);
}
