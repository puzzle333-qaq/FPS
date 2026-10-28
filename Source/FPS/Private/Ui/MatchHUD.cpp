#include "Ui/MatchHUD.h"
#include "ShooterCharacter.h"
#include "Styling/CoreStyle.h"

#include "Ui/MatchHeaderWidget.h"
#include "Ui/KillFeedWidget.h"
#include "Ui/RadarWidget.h"
#include "Game/ShooterPlayerState.h"
#include "Game/SearchDestroyGameState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/NamedSlot.h"
#include "Components/ContentWidget.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Engine/World.h"

FText UMatchHUD::FormatMatchTime(float Seconds)
{
	return UMatchHeaderWidget::FormatMatchTime(Seconds);
}

void UMatchHUD::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (!HeaderHost) HeaderHost = GetWidgetFromName(TEXT("HeaderHost"));
	if (!TDMHeader) TDMHeader = Cast<UMatchHeaderWidget>(GetWidgetFromName(TEXT("TDMHeader")));
	if (!KillFeed) KillFeed = Cast<UKillFeedWidget>(GetWidgetFromName(TEXT("KillFeed")));
	if (!Radar) Radar = Cast<URadarWidget>(GetWidgetFromName(TEXT("Radar")));
	if (!ResultPanel) ResultPanel = GetWidgetFromName(TEXT("ResultPanel"));
	if (!Text_MatchResult) Text_MatchResult = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_MatchResult")));
	if (!Text_ResultDetail) Text_ResultDetail = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_ResultDetail")));

	EnsureResultWidgets();
	// TacticalBatch1: radar is an equippable later — hide for now (keep code).
	EnsureRadarWidget();
	if (Radar)
	{
		Radar->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (KillFeed)
	{
		KillFeed->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetResultVisible(false);
	if (Text_Respawn)
	{
		Text_Respawn->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (UWorld* World = GetWorld())
	{
		if (AShooterGameState* GS = World->GetGameState<AShooterGameState>())
		{
			BindGameState(GS);
		}
		World->GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSet);
		World->GetTimerManager().SetTimer(RespawnTimer, this, &ThisClass::RefreshRespawn, 0.1f, true);
	}
}

void UMatchHUD::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.RemoveAll(this);
		World->GetTimerManager().ClearTimer(RespawnTimer);
		World->GetTimerManager().ClearTimer(RoundBannerTimer);
	}
	UnbindGameState();
	Super::NativeDestruct();
}

void UMatchHUD::HandleGameStateSet(AGameStateBase* NewGameState)
{
	BindGameState(Cast<AShooterGameState>(NewGameState));
}

void UMatchHUD::BindGameState(AShooterGameState* GS)
{
	if (BoundGameState.Get() == GS) return;
	UnbindGameState();
	BoundGameState = GS;
	if (!IsValid(GS)) return;

	GS->OnMatchModeChanged.AddUniqueDynamic(this, &ThisClass::HandleMatchModeChanged);
	GS->OnMatchEnded.AddUniqueDynamic(this, &ThisClass::HandleMatchEnded);

	if (ASearchDestroyGameState* SDGS = Cast<ASearchDestroyGameState>(GS))
	{
		SDGS->OnRoundEnded.AddUniqueDynamic(this, &ThisClass::HandleRoundEnded);
		SDGS->OnRoundPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandleRoundPhaseChanged);
		if (SDGS->GetRoundPhase() == EBombRoundPhase::PostRound
			&& GS->GetMatchState() != EShooterMatchState::PostMatch)
		{
			ShowRoundResult(SDGS->GetRoundWinningTeam(), SDGS->GetLastWinReason());
		}
	}

	RebuildHeader();
	if (GS->GetMatchState() == EShooterMatchState::PostMatch)
	{
		ShowMatchResult(GS->GetWinningTeam());
	}
}

void UMatchHUD::UnbindGameState()
{
	if (BoundGameState.IsValid())
	{
		BoundGameState->OnMatchModeChanged.RemoveDynamic(this, &ThisClass::HandleMatchModeChanged);
		BoundGameState->OnMatchEnded.RemoveDynamic(this, &ThisClass::HandleMatchEnded);
		if (ASearchDestroyGameState* SDGS = Cast<ASearchDestroyGameState>(BoundGameState.Get()))
		{
			SDGS->OnRoundEnded.RemoveDynamic(this, &ThisClass::HandleRoundEnded);
			SDGS->OnRoundPhaseChanged.RemoveDynamic(this, &ThisClass::HandleRoundPhaseChanged);
		}
	}
	if (IsValid(ActiveHeader))
	{
		ActiveHeader->UnbindMatch();
		if (bSpawnedHeader)
		{
			ActiveHeader->RemoveFromParent();
		}
		ActiveHeader = nullptr;
		bSpawnedHeader = false;
	}
	BoundGameState.Reset();
}

void UMatchHUD::HandleMatchModeChanged(EShooterGameMode NewMode)
{
	(void)NewMode;
	RebuildHeader();
}

TSubclassOf<UMatchHeaderWidget> UMatchHUD::ResolveHeaderClass(EShooterGameMode Mode) const
{
	if (const TSubclassOf<UMatchHeaderWidget>* Found = HeaderClasses.Find(Mode))
	{
		if (*Found) return *Found;
	}
	switch (Mode)
	{
	case EShooterGameMode::TDM:
		if (TDMHeaderClass) return TDMHeaderClass;
		// C++ Valorant-style top bar (timer / scores / alive + portraits)
		return UTDMHeaderWidget::StaticClass();
	case EShooterGameMode::SearchDestroy:
		if (SDHeaderClass) return SDHeaderClass;
		return USDHeaderWidget::StaticClass();
	default:
		return nullptr;
	}
}

UMatchHeaderWidget* UMatchHUD::FindPlacedHeader() const
{
	if (IsValid(TDMHeader)) return TDMHeader;
	if (UMatchHeaderWidget* Named = Cast<UMatchHeaderWidget>(GetWidgetFromName(TEXT("TDMHeader")))) return Named;
	if (UMatchHeaderWidget* Named = Cast<UMatchHeaderWidget>(GetWidgetFromName(TEXT("SDHeader")))) return Named;
	if (UMatchHeaderWidget* Named = Cast<UMatchHeaderWidget>(GetWidgetFromName(TEXT("Header")))) return Named;
	if (WidgetTree)
	{
		UMatchHeaderWidget* Found = nullptr;
		WidgetTree->ForEachWidget([&Found](UWidget* Widget)
		{
			if (!Found)
			{
				Found = Cast<UMatchHeaderWidget>(Widget);
			}
		});
		if (Found) return Found;
	}
	return nullptr;
}

void UMatchHUD::RebuildHeader()
{
	if (IsValid(ActiveHeader))
	{
		ActiveHeader->UnbindMatch();
		if (bSpawnedHeader)
		{
			ActiveHeader->RemoveFromParent();
		}
		ActiveHeader = nullptr;
		bSpawnedHeader = false;
	}

	AShooterGameState* GS = BoundGameState.Get();
	if (!IsValid(GS)) return;
	if (!GS->HasMatchHeader() && GS->GetNumTeams() < 2) return;

	const EShooterGameMode Mode = GS->HasMatchHeader() ? GS->GetMatchMode() : EShooterGameMode::TDM;

	if (UMatchHeaderWidget* Placed = FindPlacedHeader())
	{
		ActiveHeader = Placed;
		ActiveHeader->SetVisibility(ESlateVisibility::HitTestInvisible);
		ActiveHeader->BindMatch(GS);
		OnHeaderSpawned(ActiveHeader, Mode);
		return;
	}

	const TSubclassOf<UMatchHeaderWidget> HeaderClass = ResolveHeaderClass(Mode);
	if (!HeaderClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MatchHUD] No header widget for mode %d on %s. Assign TDMHeaderClass or HeaderClasses."),
			static_cast<int32>(Mode), *GetName());
		return;
	}

	ActiveHeader = CreateWidget<UMatchHeaderWidget>(GetOwningPlayer(), HeaderClass);
	if (!IsValid(ActiveHeader)) return;
	PlaceHeader(ActiveHeader);
	bSpawnedHeader = true;
	ActiveHeader->BindMatch(GS);
	OnHeaderSpawned(ActiveHeader, Mode);
}

void UMatchHUD::PlaceHeader(UMatchHeaderWidget* Header)
{
	if (!IsValid(Header)) return;
	if (!HeaderHost) HeaderHost = GetWidgetFromName(TEXT("HeaderHost"));

	if (UOverlay* OverlayHost = Cast<UOverlay>(HeaderHost))
	{
		if (UOverlaySlot* OverlaySlot = OverlayHost->AddChildToOverlay(Header))
		{
			// Full width at top — header's equal Fill spacers keep the score block screen-centered
			OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			OverlaySlot->SetVerticalAlignment(VAlign_Top);
			OverlaySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 0.f));
		}
		return;
	}
	if (UCanvasPanel* CanvasHost = Cast<UCanvasPanel>(HeaderHost))
	{
		if (UCanvasPanelSlot* CanvasSlot = CanvasHost->AddChildToCanvas(Header))
		{
			CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.f));
			CanvasSlot->SetAlignment(FVector2D(0.f, 0.f));
			CanvasSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 120.f));
			CanvasSlot->SetAutoSize(false);
		}
		return;
	}
	if (UContentWidget* ContentHost = Cast<UContentWidget>(HeaderHost))
	{
		ContentHost->SetContent(Header);
		return;
	}
	if (UPanelWidget* Panel = Cast<UPanelWidget>(HeaderHost))
	{
		Panel->AddChild(Header);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MatchHUD] HeaderHost is missing on %s. Add an Overlay named 'HeaderHost'."), *GetName());
}

void UMatchHUD::HandleMatchEnded(uint8 WinningTeam)
{
	ShowMatchResult(WinningTeam);
}

void UMatchHUD::HandleRoundEnded(uint8 WinningTeam, ERoundWinReason Reason)
{
	if (bMatchResultVisible) return;
	if (BoundGameState.IsValid() && BoundGameState->GetMatchState() == EShooterMatchState::PostMatch)
	{
		return;
	}
	ShowRoundResult(WinningTeam, Reason);
}

void UMatchHUD::HandleRoundPhaseChanged(EBombRoundPhase NewPhase)
{
	if (NewPhase == EBombRoundPhase::PostRound)
	{
		if (bMatchResultVisible) return;
		if (ASearchDestroyGameState* SDGS = Cast<ASearchDestroyGameState>(BoundGameState.Get()))
		{
			ShowRoundResult(SDGS->GetRoundWinningTeam(), SDGS->GetLastWinReason());
		}
		return;
	}

	// 新回合开始：收起回合横幅（比赛结算保留）
	if (!bMatchResultVisible)
	{
		HideRoundBanner();
	}
}

void UMatchHUD::EnsureResultWidgets()
{
	if (ResultPanel && Text_MatchResult)
	{
		return;
	}
	if (bBuiltFallbackResult || !WidgetTree)
	{
		return;
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(GetRootWidget());
	if (!RootCanvas)
	{
		// 常见：根是 Overlay
		if (UOverlay* RootOverlay = Cast<UOverlay>(GetRootWidget()))
		{
			UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultPanel"));
			Panel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
			Panel->SetPadding(FMargin(48.f, 28.f));

			UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ResultBox"));
			UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_MatchResult"));
			UTextBlock* Detail = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_ResultDetail"));

			FSlateFontInfo TitleFont = Title->GetFont();
			TitleFont.Size = 48;
			Title->SetFont(TitleFont);
			Title->SetJustification(ETextJustify::Center);
			Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));

			FSlateFontInfo DetailFont = Detail->GetFont();
			DetailFont.Size = 22;
			Detail->SetFont(DetailFont);
			Detail->SetJustification(ETextJustify::Center);
			Detail->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.f)));

			if (UVerticalBoxSlot* TitleSlot = Box->AddChildToVerticalBox(Title))
			{
				TitleSlot->SetHorizontalAlignment(HAlign_Center);
				TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
			}
			if (UVerticalBoxSlot* DetailSlot = Box->AddChildToVerticalBox(Detail))
			{
				DetailSlot->SetHorizontalAlignment(HAlign_Center);
			}
			Panel->SetContent(Box);

			if (UOverlaySlot* OverlayResultSlot = RootOverlay->AddChildToOverlay(Panel))
			{
				OverlayResultSlot->SetHorizontalAlignment(HAlign_Center);
				OverlayResultSlot->SetVerticalAlignment(VAlign_Center);
			}

			ResultPanel = Panel;
			Text_MatchResult = Title;
			Text_ResultDetail = Detail;
			bBuiltFallbackResult = true;
			UE_LOG(LogTemp, Log, TEXT("[MatchHUD] Built fallback ResultPanel (Overlay root)."));
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("[MatchHUD] No ResultPanel and root is not Overlay/Canvas — round/match banner skipped until WBP adds ResultPanel."));
		return;
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultPanel"));
	Panel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	Panel->SetPadding(FMargin(48.f, 28.f));

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ResultBox"));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_MatchResult"));
	UTextBlock* Detail = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_ResultDetail"));

	FSlateFontInfo TitleFont = Title->GetFont();
	TitleFont.Size = 48;
	Title->SetFont(TitleFont);
	Title->SetJustification(ETextJustify::Center);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	FSlateFontInfo DetailFont = Detail->GetFont();
	DetailFont.Size = 22;
	Detail->SetFont(DetailFont);
	Detail->SetJustification(ETextJustify::Center);
	Detail->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.f)));

	if (UVerticalBoxSlot* TitleSlot = Box->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}
	if (UVerticalBoxSlot* DetailSlot = Box->AddChildToVerticalBox(Detail))
	{
		DetailSlot->SetHorizontalAlignment(HAlign_Center);
	}
	Panel->SetContent(Box);

	if (UCanvasPanelSlot* CanvasResultSlot = RootCanvas->AddChildToCanvas(Panel))
	{
		CanvasResultSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CanvasResultSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasResultSlot->SetAutoSize(true);
		CanvasResultSlot->SetZOrder(50);
	}

	ResultPanel = Panel;
	Text_MatchResult = Title;
	Text_ResultDetail = Detail;
	bBuiltFallbackResult = true;
	UE_LOG(LogTemp, Log, TEXT("[MatchHUD] Built fallback ResultPanel (Canvas root)."));
}

void UMatchHUD::SetResultVisible(bool bVisible)
{
	if (ResultPanel)
	{
		ResultPanel->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMatchHUD::ApplyResultColors(bool bDraw, bool bLocalWon)
{
	const FLinearColor Color = bDraw ? DrawColor : (bLocalWon ? LocalWinColor : LocalLossColor);
	if (Text_MatchResult)
	{
		Text_MatchResult->SetColorAndOpacity(FSlateColor(Color));
	}
	if (Text_ResultDetail)
	{
		Text_ResultDetail->SetColorAndOpacity(FSlateColor(FLinearColor(Color.R, Color.G, Color.B, 0.85f)));
	}
}

FText UMatchHUD::ReasonToText(ERoundWinReason Reason) const
{
	switch (Reason)
	{
	case ERoundWinReason::Elimination: return ReasonEliminationText;
	case ERoundWinReason::BombExploded: return ReasonBombExplodedText;
	case ERoundWinReason::BombDefused: return ReasonBombDefusedText;
	case ERoundWinReason::TimeExpired: return ReasonTimeExpiredText;
	default: return FText::GetEmpty();
	}
}

void UMatchHUD::ShowMatchResult(uint8 WinningTeam)
{
	EnsureResultWidgets();
	BindLocalPlayerState();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoundBannerTimer);
	}

	const uint8 LocalTeam = BoundPlayerState.IsValid() ? BoundPlayerState->GetTeamId() : AShooterPlayerState::NoTeam;
	const bool bDraw = WinningTeam == AShooterPlayerState::NoTeam;
	const bool bWon = !bDraw && LocalTeam != AShooterPlayerState::NoTeam && LocalTeam == WinningTeam;

	if (Text_MatchResult)
	{
		Text_MatchResult->SetText(bDraw ? DrawText : (bWon ? VictoryText : DefeatText));
	}

	FText Detail = FText::GetEmpty();
	if (BoundGameState.IsValid())
	{
		Detail = FText::Format(MatchScoreFormat,
			FText::AsNumber(BoundGameState->GetTeamScore(0)),
			FText::AsNumber(BoundGameState->GetTeamScore(1)));
	}
	// 爆破终局也带上最后一回合原因
	if (ASearchDestroyGameState* SDGS = Cast<ASearchDestroyGameState>(BoundGameState.Get()))
	{
		const FText Reason = ReasonToText(SDGS->GetLastWinReason());
		if (!Reason.IsEmpty())
		{
			Detail = FText::Format(NSLOCTEXT("MatchHUD", "MatchDetailCombo", "{0}  ·  {1}"), Detail, Reason);
		}
	}
	if (Text_ResultDetail)
	{
		Text_ResultDetail->SetText(Detail);
		Text_ResultDetail->SetVisibility(Detail.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	ApplyResultColors(bDraw, bWon);
	SetResultVisible(true);
	bMatchResultVisible = true;
	OnShowMatchResult(WinningTeam, bWon, bDraw);
}

void UMatchHUD::ShowRoundResult(uint8 WinningTeam, ERoundWinReason Reason)
{
	if (bMatchResultVisible) return;
	EnsureResultWidgets();
	BindLocalPlayerState();

	const uint8 LocalTeam = BoundPlayerState.IsValid() ? BoundPlayerState->GetTeamId() : AShooterPlayerState::NoTeam;
	const bool bDraw = WinningTeam == AShooterPlayerState::NoTeam;
	const bool bWon = !bDraw && LocalTeam != AShooterPlayerState::NoTeam && LocalTeam == WinningTeam;

	if (Text_MatchResult)
	{
		Text_MatchResult->SetText(bDraw ? RoundDrawText : (bWon ? RoundVictoryText : RoundDefeatText));
	}

	FText Detail = ReasonToText(Reason);
	if (BoundGameState.IsValid())
	{
		const FText Score = FText::Format(MatchScoreFormat,
			FText::AsNumber(BoundGameState->GetTeamScore(0)),
			FText::AsNumber(BoundGameState->GetTeamScore(1)));
		Detail = Detail.IsEmpty()
			? Score
			: FText::Format(NSLOCTEXT("MatchHUD", "RoundDetailCombo", "{0}  ·  {1}"), Detail, Score);
	}
	if (Text_ResultDetail)
	{
		Text_ResultDetail->SetText(Detail);
		Text_ResultDetail->SetVisibility(Detail.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	ApplyResultColors(bDraw, bWon);
	SetResultVisible(true);
	OnShowRoundResult(WinningTeam, Reason, bWon, bDraw);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoundBannerTimer);
		World->GetTimerManager().SetTimer(
			RoundBannerTimer,
			this,
			&ThisClass::HideRoundBanner,
			FMath::Max(RoundBannerDuration, 0.5f),
			false);
	}
}

void UMatchHUD::HideRoundBanner()
{
	if (bMatchResultVisible) return;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoundBannerTimer);
	}
	SetResultVisible(false);
	OnHideRoundResult();
}

void UMatchHUD::BindLocalPlayerState()
{
	APlayerController* PC = GetOwningPlayer();
	AShooterPlayerState* PS = IsValid(PC) ? PC->GetPlayerState<AShooterPlayerState>() : nullptr;
	BoundPlayerState = PS;
}

void UMatchHUD::RefreshRespawn()
{
	RefreshRadarEquip();

	if (!ActiveHeader && BoundGameState.IsValid())
	{
		RebuildHeader();
	}
	BindLocalPlayerState();
	if (!Text_Respawn) return;

	const float Remaining = BoundPlayerState.IsValid() ? BoundPlayerState->GetRespawnRemaining() : 0.f;
	if (Remaining <= 0.f)
	{
		if (LastRespawnSecond != 0)
		{
			LastRespawnSecond = 0;
			Text_Respawn->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const int32 Seconds = FMath::CeilToInt(Remaining);
	if (Seconds == LastRespawnSecond) return;
	LastRespawnSecond = Seconds;
	Text_Respawn->SetVisibility(ESlateVisibility::HitTestInvisible);
	Text_Respawn->SetText(FText::Format(RespawnFormat, FText::AsNumber(Seconds)));
}

void UMatchHUD::EnsureRadarWidget()
{
	auto ApplyRadarLayout = [this](URadarWidget* W)
	{
		if (!IsValid(W)) return;
		// 强制左上角大雷达（即使蓝图里曾经摆在左下）
		if (W->RadarPixelSize < 220.f) W->RadarPixelSize = 232.f;
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(W->Slot))
		{
			const float Margin = 24.f;
			const float Size = W->RadarPixelSize;
			CanvasSlot->SetAnchors(FAnchors(0.f, 0.f));
			CanvasSlot->SetAlignment(FVector2D(0.f, 0.f));
			CanvasSlot->SetPosition(FVector2D(Margin, Margin));
			CanvasSlot->SetSize(FVector2D(Size, Size));
			CanvasSlot->SetZOrder(20);
		}
		else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(W->Slot))
		{
			const float Margin = 24.f;
			OverlaySlot->SetHorizontalAlignment(HAlign_Left);
			OverlaySlot->SetVerticalAlignment(VAlign_Top);
			OverlaySlot->SetPadding(FMargin(Margin, Margin, 0.f, 0.f));
		}
		W->SetVisibility(ESlateVisibility::Collapsed); // tactical: radar off by default
	};

	if (Radar)
	{
		ApplyRadarLayout(Radar);
		return;
	}
	if (URadarWidget* Named = Cast<URadarWidget>(GetWidgetFromName(TEXT("Radar"))))
	{
		Radar = Named;
		ApplyRadarLayout(Radar);
		return;
	}
	if (bBuiltFallbackRadar) return;

	APlayerController* PC = GetOwningPlayer();
	Radar = CreateWidget<URadarWidget>(PC, URadarWidget::StaticClass());
	if (!IsValid(Radar)) return;
	PlaceRadar(Radar);
	bBuiltFallbackRadar = true;
	ApplyRadarLayout(Radar);
	UE_LOG(LogTemp, Log, TEXT("[MatchHUD] Built fallback Radar widget."));
}

void UMatchHUD::PlaceRadar(URadarWidget* InRadar)
{
	if (!IsValid(InRadar)) return;

	const float Margin = 24.f;
	if (InRadar->RadarPixelSize < 220.f) InRadar->RadarPixelSize = 232.f;
	const float Size = InRadar->RadarPixelSize;

	if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(GetRootWidget()))
	{
		if (UCanvasPanelSlot* RadarSlot = RootCanvas->AddChildToCanvas(InRadar))
		{
			// 左上角
			RadarSlot->SetAnchors(FAnchors(0.f, 0.f));
			RadarSlot->SetAlignment(FVector2D(0.f, 0.f));
			RadarSlot->SetPosition(FVector2D(Margin, Margin));
			RadarSlot->SetSize(FVector2D(Size, Size));
			RadarSlot->SetZOrder(20);
		}
		return;
	}

	if (UOverlay* RootOverlay = Cast<UOverlay>(GetRootWidget()))
	{
		if (UOverlaySlot* RadarSlot = RootOverlay->AddChildToOverlay(InRadar))
		{
			RadarSlot->SetHorizontalAlignment(HAlign_Left);
			RadarSlot->SetVerticalAlignment(VAlign_Top);
			RadarSlot->SetPadding(FMargin(Margin, Margin, 0.f, 0.f));
		}
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MatchHUD] Cannot place Radar — root is not Canvas/Overlay on %s"), *GetName());
}


void UMatchHUD::RefreshRadarEquip()
{
	EnsureRadarWidget();
	if (!IsValid(Radar)) return;

	bool bShow = false;
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (const AShooterCharacter* Char = Cast<AShooterCharacter>(PC->GetPawn()))
		{
			bShow = Char->HasRadar();
		}
	}
	Radar->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UMatchHUD::EnsureWatchText()
{
	if (IsValid(Text_WatchTime)) return;
	if (UTextBlock* Named = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_WatchTime"))))
	{
		Text_WatchTime = Named;
		return;
	}
	// Runtime fallback centered text
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	Text_WatchTime = NewObject<UTextBlock>(this, TEXT("Text_WatchTime_Runtime"));
	if (!IsValid(Text_WatchTime)) return;
	Text_WatchTime->SetVisibility(ESlateVisibility::Collapsed);
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 28);
	Text_WatchTime->SetFont(Font);
	Text_WatchTime->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 1.f, 0.85f, 1.f)));
	Text_WatchTime->SetJustification(ETextJustify::Center);

	if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(GetRootWidget()))
	{
		if (UCanvasPanelSlot* WatchCanvasSlot = RootCanvas->AddChildToCanvas(Text_WatchTime))
		{
			WatchCanvasSlot->SetAnchors(FAnchors(0.5f, 0.35f));
			WatchCanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			WatchCanvasSlot->SetAutoSize(true);
			WatchCanvasSlot->SetZOrder(50);
		}
	}
	else if (UOverlay* RootOverlay = Cast<UOverlay>(GetRootWidget()))
	{
		if (UOverlaySlot* WatchOverlaySlot = RootOverlay->AddChildToOverlay(Text_WatchTime))
		{
			WatchOverlaySlot->SetHorizontalAlignment(HAlign_Center);
			WatchOverlaySlot->SetVerticalAlignment(VAlign_Center);
			WatchOverlaySlot->SetPadding(FMargin(0.f, -120.f, 0.f, 0.f));
		}
	}
}

void UMatchHUD::ShowWatchTime(float RemainingSeconds, float DisplaySeconds)
{
	EnsureWatchText();
	if (!IsValid(Text_WatchTime)) return;

	FText Msg;
	if (RemainingSeconds <= 0.f)
	{
		Msg = NSLOCTEXT("MatchHUD", "WatchNone", "无倒计时");
	}
	else
	{
		Msg = FText::Format(NSLOCTEXT("MatchHUD", "WatchFmt", "{0}"), FormatMatchTime(RemainingSeconds));
	}
	Text_WatchTime->SetText(Msg);
	Text_WatchTime->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WatchHideTimer);
		World->GetTimerManager().SetTimer(WatchHideTimer, this, &ThisClass::HideWatchTime, FMath::Max(0.5f, DisplaySeconds), false);
	}
}

void UMatchHUD::HideWatchTime()
{
	if (IsValid(Text_WatchTime))
	{
		Text_WatchTime->SetVisibility(ESlateVisibility::Collapsed);
	}
}

