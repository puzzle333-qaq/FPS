#include "Ui/MatchHeaderWidget.h"

#include "Game/ShooterPlayerState.h"
#include "Stats/StatsComponent.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Spacer.h"
#include "Components/PanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Ui/TacticalUIStyle.h"

namespace MatchHeaderChrome
{
	static constexpr float PortraitSize = 48.f;
	static constexpr float PortraitGap = 5.f;
	static constexpr float CenterWidth = 148.f;

	static UTextBlock* MakeText(UWidgetTree* Tree, FName Name, int32 Size, bool bBold, const FLinearColor& Color, ETextJustify::Type Justify = ETextJustify::Center)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		T->SetFont(FTacticalUIStyle::MakeFont(Size, bBold));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetJustification(Justify);
		T->SetVisibility(ESlateVisibility::HitTestInvisible);
		return T;
	}
}

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

UWidget* UMatchHeaderWidget::MakePersonSilhouette(const FLinearColor& Tint) const
{
	if (!WidgetTree) return nullptr;

	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(12.f);
	Box->SetHeightOverride(14.f);

	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UBorder* Head = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Head->SetBrush(FTacticalUIStyle::MakePanelBrush(Tint, 6.f, 0.f));
	Head->SetBrushColor(FLinearColor::White);
	USizeBox* HeadBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	HeadBox->SetWidthOverride(7.f);
	HeadBox->SetHeightOverride(7.f);
	HeadBox->SetContent(Head);

	UBorder* Body = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Body->SetBrush(FTacticalUIStyle::MakePanelBrush(Tint, 3.f, 0.f));
	Body->SetBrushColor(FLinearColor::White);
	USizeBox* BodyBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BodyBox->SetWidthOverride(11.f);
	BodyBox->SetHeightOverride(6.f);
	BodyBox->SetContent(Body);

	if (UVerticalBoxSlot* HS = V->AddChildToVerticalBox(HeadBox))
	{
		HS->SetHorizontalAlignment(HAlign_Center);
		HS->SetPadding(FMargin(0.f, 0.f, 0.f, 1.f));
	}
	if (UVerticalBoxSlot* BS = V->AddChildToVerticalBox(BodyBox))
	{
		BS->SetHorizontalAlignment(HAlign_Center);
	}
	Box->SetContent(V);
	return Box;
}

void UMatchHeaderWidget::RefreshPortraitRows()
{
	if (!PortraitRowA || !PortraitRowB || !WidgetTree) return;

	PortraitRowA->ClearChildren();
	PortraitRowB->ClearChildren();

	AShooterGameState* GS = GameState.Get();
	if (!IsValid(GS)) return;

	APlayerController* PC = GetOwningPlayer();
	AShooterPlayerState* LocalPS = IsValid(PC) ? PC->GetPlayerState<AShooterPlayerState>() : nullptr;

	TArray<AShooterPlayerState*> TeamA;
	TArray<AShooterPlayerState*> TeamB;
	for (APlayerState* PS : GS->PlayerArray)
	{
		AShooterPlayerState* SPS = Cast<AShooterPlayerState>(PS);
		if (!SPS || SPS->IsInactive() || !SPS->HasTeam()) continue;
		if (SPS->GetTeamId() == 0) TeamA.Add(SPS);
		else if (SPS->GetTeamId() == 1) TeamB.Add(SPS);
	}

	auto AddSlot = [this](UHorizontalBox* Row, AShooterPlayerState* SPS, AShooterPlayerState* LocalPS)
	{
		const bool bLocal = (SPS == LocalPS);
		bool bAlive = false;
		if (APawn* Pawn = SPS->GetPawn())
		{
			const UStatsComponent* Stats = UStatsComponent::FindStatsComponent(Pawn);
			bAlive = !Stats || Stats->DeathState == EDeathState::NotDead;
		}
		FString Label = SPS->GetPlayerName();
		if (Label.IsEmpty()) Label = TEXT("P");

		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(MatchHeaderChrome::PortraitSize);
		Size->SetHeightOverride(MatchHeaderChrome::PortraitSize);

		const FLinearColor BorderCol = (SPS->GetTeamId() == 0) ? PortraitBorderA : PortraitBorderB;
		const float OutlineW = bLocal ? 2.6f : 1.4f;
		FLinearColor Line = bAlive ? BorderCol : FLinearColor(0.35f, 0.35f, 0.38f, 0.85f);
		if (bLocal && bAlive) { Line = BorderCol * 1.2f; Line.A = 1.f; }

		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetPadding(FMargin(2.f));
		Frame->SetBrush(FTacticalUIStyle::MakePanelBrush(PortraitFill, 2.f, OutlineW, &Line));
		Frame->SetBrushColor(FLinearColor::White);

		UOverlay* Inner = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UBorder* Avatar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		FLinearColor Fill = (SPS->GetTeamId() == 0)
			? FLinearColor(0.08f, 0.22f, 0.24f, 1.f)
			: FLinearColor(0.12f, 0.16f, 0.28f, 1.f);
		if (!bAlive) Fill = FLinearColor(0.12f, 0.12f, 0.13f, 1.f);
		Avatar->SetBrush(FTacticalUIStyle::MakePanelBrush(Fill, 1.f, 0.f));
		Avatar->SetBrushColor(FLinearColor::White);
		if (UOverlaySlot* AvSlot = Inner->AddChildToOverlay(Avatar))
		{
			AvSlot->SetHorizontalAlignment(HAlign_Fill);
			AvSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UTextBlock* Initial = MatchHeaderChrome::MakeText(
			WidgetTree, NAME_None, 16, true,
			bAlive ? FLinearColor(0.9f, 0.92f, 0.95f, 1.f) : FLinearColor(0.45f, 0.45f, 0.48f, 1.f));
		Initial->SetText(FText::FromString(Label.Left(1).ToUpper()));
		if (UOverlaySlot* TxSlot = Inner->AddChildToOverlay(Initial))
		{
			TxSlot->SetHorizontalAlignment(HAlign_Center);
			TxSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (!bAlive)
		{
			UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.5f));
			if (UOverlaySlot* DimSlot = Inner->AddChildToOverlay(Dim))
			{
				DimSlot->SetHorizontalAlignment(HAlign_Fill);
				DimSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}

		Frame->SetContent(Inner);
		Size->SetContent(Frame);
		if (UHorizontalBoxSlot* BoxSlot = Row->AddChildToHorizontalBox(Size))
		{
			BoxSlot->SetPadding(FMargin(MatchHeaderChrome::PortraitGap * 0.5f, 0.f));
			BoxSlot->SetVerticalAlignment(VAlign_Top);
		}
	};

	for (AShooterPlayerState* SPS : TeamA) AddSlot(PortraitRowA, SPS, LocalPS);
	for (AShooterPlayerState* SPS : TeamB) AddSlot(PortraitRowB, SPS, LocalPS);
}

void UMatchHeaderWidget::EnsureValorantTopBar(bool bIncludeSDExtras)
{
	if (bBuiltValorantTopBar || !WidgetTree) return;

	// Full-width column so equal side fills keep the score module at true screen center
	// even when left/right portrait counts differ (Valorant/CS layout).
	UVerticalBox* RootCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ValorantHeaderRoot"));

	UHorizontalBox* TopRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopBarRow"));

	PortraitRowA = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PortraitRowA"));
	PortraitRowB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PortraitRowB"));

	USpacer* LeftFill = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("TopLeftFill"));
	USpacer* RightFill = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("TopRightFill"));

	// ---- Center module (dark charcoal: timer / scores / alive) ----
	USizeBox* CenterSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CenterSize"));
	CenterSize->SetWidthOverride(MatchHeaderChrome::CenterWidth);

	UBorder* CenterPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CenterPanel"));
	CenterPanel->SetPadding(FMargin(0.f));
	CenterPanel->SetBrush(FTacticalUIStyle::MakePanelBrush(CenterPanelColor, 2.f, 0.f));
	CenterPanel->SetBrushColor(FLinearColor::White);

	UVerticalBox* CenterCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CenterCol"));

	UBorder* TimerBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TimerBar"));
	TimerBar->SetPadding(FMargin(6.f, 5.f));
	TimerBar->SetBrush(FTacticalUIStyle::MakePanelBrush(TimerBarColor, 0.f, 0.f));
	TimerBar->SetBrushColor(FLinearColor::White);
	UTextBlock* TimerText = MatchHeaderChrome::MakeText(WidgetTree, TEXT("VText_Timer"), 17, true, FLinearColor(0.95f, 0.96f, 0.97f, 1.f));
	TimerText->SetText(FText::FromString(TEXT("0:00")));
	TimerBar->SetContent(TimerText);

	UHorizontalBox* ScoreRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ScoreRow"));

	auto MakeTeamCol = [this](FName ScoreName, FName AliveName, const FLinearColor& ScoreCol) -> UVerticalBox*
	{
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock* Score = MatchHeaderChrome::MakeText(WidgetTree, ScoreName, 40, true, ScoreCol);
		Score->SetText(FText::AsNumber(0));
		if (UVerticalBoxSlot* SS = Col->AddChildToVerticalBox(Score))
		{
			SS->SetHorizontalAlignment(HAlign_Center);
			SS->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		UHorizontalBox* AliveRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UWidget* Glyph = MakePersonSilhouette(FLinearColor(0.82f, 0.84f, 0.88f, 1.f));
		if (Glyph)
		{
			if (UHorizontalBoxSlot* GS = AliveRow->AddChildToHorizontalBox(Glyph))
			{
				GS->SetVerticalAlignment(VAlign_Center);
				GS->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
			}
		}
		UTextBlock* Alive = MatchHeaderChrome::MakeText(WidgetTree, AliveName, 12, true, FLinearColor(0.88f, 0.89f, 0.92f, 1.f));
		Alive->SetText(FText::AsNumber(0));
		if (UHorizontalBoxSlot* AS = AliveRow->AddChildToHorizontalBox(Alive))
		{
			AS->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* ARS = Col->AddChildToVerticalBox(AliveRow))
		{
			ARS->SetHorizontalAlignment(HAlign_Center);
			ARS->SetPadding(FMargin(0.f, 2.f, 0.f, 8.f));
		}
		return Col;
	};

	UVerticalBox* ColA = MakeTeamCol(TEXT("VText_TeamAScore"), TEXT("VText_TeamAAlive"), ScoreColorA);
	UVerticalBox* ColB = MakeTeamCol(TEXT("VText_TeamBScore"), TEXT("VText_TeamBAlive"), ScoreColorB);

	if (UHorizontalBoxSlot* CA = ScoreRow->AddChildToHorizontalBox(ColA))
	{
		CA->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CA->SetHorizontalAlignment(HAlign_Center);
	}
	if (UHorizontalBoxSlot* CB = ScoreRow->AddChildToHorizontalBox(ColB))
	{
		CB->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CB->SetHorizontalAlignment(HAlign_Center);
	}

	if (UVerticalBoxSlot* TB = CenterCol->AddChildToVerticalBox(TimerBar))
	{
		TB->SetHorizontalAlignment(HAlign_Fill);
	}
	if (UVerticalBoxSlot* SR = CenterCol->AddChildToVerticalBox(ScoreRow))
	{
		SR->SetHorizontalAlignment(HAlign_Fill);
	}

	// Soft vertical fade under the charcoal block (matches ref)
	UBorder* Fade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CenterFade"));
	Fade->SetBrush(FTacticalUIStyle::MakePanelBrush(FLinearColor(0.02f, 0.02f, 0.03f, 0.28f), 0.f, 0.f));
	Fade->SetBrushColor(FLinearColor::White);
	USizeBox* FadeSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	FadeSize->SetHeightOverride(12.f);
	FadeSize->SetContent(Fade);
	if (UVerticalBoxSlot* FS = CenterCol->AddChildToVerticalBox(FadeSize))
	{
		FS->SetHorizontalAlignment(HAlign_Fill);
	}

	CenterPanel->SetContent(CenterCol);
	CenterSize->SetContent(CenterPanel);

	// Equal Fill | portraitsA (right) | center | portraitsB (left) | Equal Fill
	if (UHorizontalBoxSlot* LF = TopRow->AddChildToHorizontalBox(LeftFill))
	{
		LF->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	if (UHorizontalBoxSlot* PA = TopRow->AddChildToHorizontalBox(PortraitRowA))
	{
		PA->SetVerticalAlignment(VAlign_Top);
		PA->SetHorizontalAlignment(HAlign_Right);
		PA->SetPadding(FMargin(0.f, 2.f, 8.f, 0.f));
	}
	if (UHorizontalBoxSlot* CS = TopRow->AddChildToHorizontalBox(CenterSize))
	{
		CS->SetVerticalAlignment(VAlign_Top);
		CS->SetHorizontalAlignment(HAlign_Center);
	}
	if (UHorizontalBoxSlot* PB = TopRow->AddChildToHorizontalBox(PortraitRowB))
	{
		PB->SetVerticalAlignment(VAlign_Top);
		PB->SetHorizontalAlignment(HAlign_Left);
		PB->SetPadding(FMargin(8.f, 2.f, 0.f, 0.f));
	}
	if (UHorizontalBoxSlot* RF = TopRow->AddChildToHorizontalBox(RightFill))
	{
		RF->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	if (UVerticalBoxSlot* TR = RootCol->AddChildToVerticalBox(TopRow))
	{
		TR->SetHorizontalAlignment(HAlign_Fill);
		TR->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	if (bIncludeSDExtras)
	{
		UHorizontalBox* StatusRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SDStatusRow"));
		UTextBlock* Round = MatchHeaderChrome::MakeText(WidgetTree, TEXT("VText_Round"), 13, false, FTacticalUIStyle::TextMuted);
		UTextBlock* Bomb = MatchHeaderChrome::MakeText(WidgetTree, TEXT("VText_BombState"), 13, true, FTacticalUIStyle::TextGold);
		UTextBlock* Role = MatchHeaderChrome::MakeText(WidgetTree, TEXT("VText_Role"), 13, true, FTacticalUIStyle::Text);
		if (UHorizontalBoxSlot* R1 = StatusRow->AddChildToHorizontalBox(Round))
		{
			R1->SetPadding(FMargin(8.f, 0.f));
		}
		if (UHorizontalBoxSlot* R2 = StatusRow->AddChildToHorizontalBox(Bomb))
		{
			R2->SetPadding(FMargin(8.f, 0.f));
		}
		if (UHorizontalBoxSlot* R3 = StatusRow->AddChildToHorizontalBox(Role))
		{
			R3->SetPadding(FMargin(8.f, 0.f));
		}
		if (UVerticalBoxSlot* SRS = RootCol->AddChildToVerticalBox(StatusRow))
		{
			SRS->SetHorizontalAlignment(HAlign_Center);
			SRS->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}

		UTextBlock* Prompt = MatchHeaderChrome::MakeText(WidgetTree, TEXT("VText_Prompt"), 15, true, FTacticalUIStyle::TextGold);
		Prompt->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* PS = RootCol->AddChildToVerticalBox(Prompt))
		{
			PS->SetHorizontalAlignment(HAlign_Center);
			PS->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		UProgressBar* Progress = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("VProgress_Action"));
		Progress->SetVisibility(ESlateVisibility::Collapsed);
		USizeBox* ProgSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ProgSize->SetWidthOverride(220.f);
		ProgSize->SetHeightOverride(8.f);
		ProgSize->SetContent(Progress);
		if (UVerticalBoxSlot* PrS = RootCol->AddChildToVerticalBox(ProgSize))
		{
			PrS->SetHorizontalAlignment(HAlign_Center);
			PrS->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
	}

	// Mount into existing root or become root — always fill width at top so center stays screen-centered
	if (!WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = RootCol;
	}
	else if (UOverlay* Overlay = Cast<UOverlay>(WidgetTree->RootWidget))
	{
		for (int32 i = 0; i < Overlay->GetChildrenCount(); ++i)
		{
			if (UWidget* Child = Overlay->GetChildAt(i))
			{
				Child->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		if (UOverlaySlot* OverlaySlot = Overlay->AddChildToOverlay(RootCol))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			OverlaySlot->SetVerticalAlignment(VAlign_Top);
			OverlaySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 0.f));
		}
	}
	else if (UPanelWidget* Panel = Cast<UPanelWidget>(WidgetTree->RootWidget))
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			if (UWidget* Child = Panel->GetChildAt(i))
			{
				Child->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		if (UVerticalBox* VRoot = Cast<UVerticalBox>(Panel))
		{
			if (UVerticalBoxSlot* VS = VRoot->AddChildToVerticalBox(RootCol))
			{
				VS->SetHorizontalAlignment(HAlign_Fill);
				VS->SetVerticalAlignment(VAlign_Top);
			}
		}
		else if (UHorizontalBox* HRoot = Cast<UHorizontalBox>(Panel))
		{
			if (UHorizontalBoxSlot* HS = HRoot->AddChildToHorizontalBox(RootCol))
			{
				HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				HS->SetHorizontalAlignment(HAlign_Fill);
				HS->SetVerticalAlignment(VAlign_Top);
			}
		}
		else
		{
			Panel->AddChild(RootCol);
		}
	}
	else
	{
		WidgetTree->RootWidget = RootCol;
	}

	bBuiltValorantTopBar = true;
	UE_LOG(LogTemp, Log, TEXT("[MatchHeader] Built Valorant top bar (SDExtras=%d)"), bIncludeSDExtras ? 1 : 0);
}

// ---------------- TDM ----------------

bool UTDMHeaderWidget::Initialize()
{
	const bool bOk = Super::Initialize();
	EnsureValorantTopBar(false);
	return bOk;
}

void UTDMHeaderWidget::ResolveWidgets()
{
	auto PickText = [this](const TCHAR* Preferred, const TCHAR* Fallback) -> UTextBlock*
	{
		if (UTextBlock* Found = Cast<UTextBlock>(GetWidgetFromName(Preferred))) return Found;
		return Cast<UTextBlock>(GetWidgetFromName(Fallback));
	};
	Text_TeamAScore = PickText(TEXT("VText_TeamAScore"), TEXT("Text_TeamAScore"));
	Text_TeamBScore = PickText(TEXT("VText_TeamBScore"), TEXT("Text_TeamBScore"));
	Text_TeamAAlive = PickText(TEXT("VText_TeamAAlive"), TEXT("Text_TeamAAlive"));
	Text_TeamBAlive = PickText(TEXT("VText_TeamBAlive"), TEXT("Text_TeamBAlive"));
	Text_Timer = PickText(TEXT("VText_Timer"), TEXT("Text_Timer"));
	Text_ScoreToWin = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_ScoreToWin")));
	Text_MatchState = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_MatchState")));
	PortraitRowA = Cast<UHorizontalBox>(GetWidgetFromName(TEXT("PortraitRowA")));
	PortraitRowB = Cast<UHorizontalBox>(GetWidgetFromName(TEXT("PortraitRowB")));
}

void UTDMHeaderWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureValorantTopBar(false);
	ResolveWidgets();
	ApplyScoreColors();
	// TacticalBatch1: hide persistent countdown (radio prompts later)
	if (Text_Timer) Text_Timer->SetVisibility(ESlateVisibility::Collapsed);
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
	EnsureValorantTopBar(false);
	ResolveWidgets();
	if (!GameState.IsValid()) return;

	GameState->OnTeamScoreChanged.AddUniqueDynamic(this, &ThisClass::HandleTeamScoreChanged);
	GameState->OnTeamAliveChanged.AddUniqueDynamic(this, &ThisClass::HandleTeamAliveChanged);
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
	RefreshAlive();
	RefreshMatchState();
	RefreshPortraitRows();
	LastDisplayedSeconds = -1;
	RefreshTimer();
	if (Text_Timer) Text_Timer->SetVisibility(ESlateVisibility::Collapsed);

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
		GameState->OnTeamAliveChanged.RemoveDynamic(this, &ThisClass::HandleTeamAliveChanged);
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

void UTDMHeaderWidget::RefreshAlive()
{
	if (!GameState.IsValid()) return;
	const int32 AliveA = GameState->GetTeamAliveCount(0);
	const int32 AliveB = GameState->GetTeamAliveCount(1);
	if (Text_TeamAAlive) Text_TeamAAlive->SetText(FText::AsNumber(AliveA));
	if (Text_TeamBAlive) Text_TeamBAlive->SetText(FText::AsNumber(AliveB));
	RefreshPortraitRows();
	OnAliveCountsUpdated(AliveA, AliveB);
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
	ScoreColorA = TeamAColor;
	ScoreColorB = TeamBColor;
	if (Text_TeamAScore) Text_TeamAScore->SetColorAndOpacity(FSlateColor(TeamAColor));
	if (Text_TeamBScore) Text_TeamBScore->SetColorAndOpacity(FSlateColor(TeamBColor));
}

void UTDMHeaderWidget::HandleTeamScoreChanged(uint8 TeamId, int32 NewScore)
{
	(void)TeamId;
	(void)NewScore;
	RefreshScores();
}

void UTDMHeaderWidget::HandleTeamAliveChanged(uint8 TeamId, int32 AliveCount)
{
	(void)TeamId;
	(void)AliveCount;
	RefreshAlive();
}

void UTDMHeaderWidget::HandleMatchStateChanged(EShooterMatchState NewState)
{
	(void)NewState;
	LastDisplayedSeconds = -1;
	RefreshMatchState();
	RefreshAlive();
	RefreshTimer();
}

void UTDMHeaderWidget::HandleLocalTeamChanged(uint8 TeamId)
{
	RefreshPortraitRows();
	OnLocalTeamChanged(TeamId);
}

// ---------------- SD ----------------
