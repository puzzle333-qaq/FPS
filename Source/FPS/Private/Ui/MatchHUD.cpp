#include "Ui/MatchHUD.h"

#include "Ui/MatchHeaderWidget.h"
#include "Ui/KillFeedWidget.h"
#include "Game/ShooterPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/NamedSlot.h"
#include "Components/ContentWidget.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
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

	if (ResultPanel)
	{
		ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
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
	if (Mode == EShooterGameMode::TDM)
	{
		if (TDMHeaderClass) return TDMHeaderClass;
		return LoadClass<UMatchHeaderWidget>(nullptr, TEXT("/Game/FPS/UI/WBP_TDMHeader.WBP_TDMHeader_C"));
	}
	if (Mode == EShooterGameMode::SearchDestroy)
	{
		if (SDHeaderClass) return SDHeaderClass;
		return LoadClass<UMatchHeaderWidget>(nullptr, TEXT("/Game/FPS/UI/WBP_SDHeader.WBP_SDHeader_C"));
	}
	return nullptr;
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
			OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			OverlaySlot->SetVerticalAlignment(VAlign_Fill);
		}
		return;
	}
	if (UCanvasPanel* CanvasHost = Cast<UCanvasPanel>(HeaderHost))
	{
		if (UCanvasPanelSlot* CanvasSlot = CanvasHost->AddChildToCanvas(Header))
		{
			CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			CanvasSlot->SetOffsets(FMargin(0.f));
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

void UMatchHUD::ShowMatchResult(uint8 WinningTeam)
{
	BindLocalPlayerState();
	const uint8 LocalTeam = BoundPlayerState.IsValid() ? BoundPlayerState->GetTeamId() : AShooterPlayerState::NoTeam;
	const bool bDraw = WinningTeam == AShooterPlayerState::NoTeam;
	const bool bWon = !bDraw && LocalTeam != AShooterPlayerState::NoTeam && LocalTeam == WinningTeam;

	if (Text_MatchResult)
	{
		Text_MatchResult->SetText(bDraw ? DrawText : (bWon ? VictoryText : DefeatText));
	}
	if (ResultPanel)
	{
		ResultPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	OnShowMatchResult(WinningTeam, bWon, bDraw);
}

void UMatchHUD::BindLocalPlayerState()
{
	APlayerController* PC = GetOwningPlayer();
	AShooterPlayerState* PS = IsValid(PC) ? PC->GetPlayerState<AShooterPlayerState>() : nullptr;
	BoundPlayerState = PS;
}

void UMatchHUD::RefreshRespawn()
{
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
