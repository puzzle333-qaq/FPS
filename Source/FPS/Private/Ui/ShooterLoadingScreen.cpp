#include "Ui/ShooterLoadingScreen.h"
#include "Ui/TacticalUIStyle.h"

#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UObject/UObjectGlobals.h"
#include "MoviePlayer.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

namespace
{
	FDelegateHandle GPreLoadHandle;
	FDelegateHandle GPostLoadHandle;
	TWeakObjectPtr<UGameInstance> GOwnerGI;
	TSharedPtr<SWidget> GViewportWidget;
	FString GLoadingMapPackage;
	bool GRegistered = false;

	FString FriendlyMapName(const FString& MapPath)
	{
		FString Name = MapPath;
		int32 Slash = INDEX_NONE;
		if (Name.FindLastChar(TEXT('/'), Slash))
		{
			Name = Name.Mid(Slash + 1);
		}
		Name.RemoveFromStart(TEXT("Lvl_"));
		Name.RemoveFromStart(TEXT("LVL_"));
		return Name.IsEmpty() ? TEXT("地图") : Name;
	}

	class SShooterLoadingScreenWidget : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SShooterLoadingScreenWidget) {}
			SLATE_ARGUMENT(FString, MapName)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			MapPackage = InArgs._MapName;
			DisplayProgress = 0.05f;
			MaxDisplayedProgress = 0.05f;
			AnimTime = 0.f;

			BgBrush = FTacticalUIStyle::MakePanelBrush(FTacticalUIStyle::BgDim, 0.f, 0.f);
			PanelBrush = FTacticalUIStyle::MakePanelBrush(FTacticalUIStyle::Panel, 8.f, 1.f);

			const FProgressBarStyle& DefaultPB = FCoreStyle::Get().GetWidgetStyle<FProgressBarStyle>("ProgressBar");
			OwnedBarStyle = DefaultPB;
			OwnedBarStyle.BackgroundImage = FTacticalUIStyle::MakePanelBrush(FTacticalUIStyle::Row, 3.f, 1.f);
			OwnedBarStyle.FillImage = FTacticalUIStyle::MakePanelBrush(FTacticalUIStyle::Accent, 3.f, 0.f);
			OwnedBarStyle.MarqueeImage = OwnedBarStyle.FillImage;

			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(&BgBrush)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(520.f)
					[
						SNew(SBorder)
						.BorderImage(&PanelBrush)
						.Padding(FMargin(28.f, 24.f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Left)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("加载中…")))
								.Font(FTacticalUIStyle::MakeFont(22, true))
								.ColorAndOpacity(FSlateColor(FTacticalUIStyle::TextGold))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.f, 8.f, 0.f, 0.f)
							[
								SNew(STextBlock)
								.Text(this, &SShooterLoadingScreenWidget::GetSubtitleText)
								.Font(FTacticalUIStyle::MakeFont(14, false))
								.ColorAndOpacity(FSlateColor(FTacticalUIStyle::TextMuted))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.f, 20.f, 0.f, 0.f)
							[
								SNew(SBox)
								.HeightOverride(14.f)
								[
									SNew(SProgressBar)
									.Style(&OwnedBarStyle)
									.Percent(this, &SShooterLoadingScreenWidget::GetProgressPercent)
								]
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.f, 10.f, 0.f, 0.f)
							.HAlign(HAlign_Right)
							[
								SNew(STextBlock)
								.Text(this, &SShooterLoadingScreenWidget::GetPercentText)
								.Font(FTacticalUIStyle::MakeFont(13, false))
								.ColorAndOpacity(FSlateColor(FTacticalUIStyle::Text))
							]
						]
					]
				]
			];
		}

		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
		{
			SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
			AnimTime += InDeltaTime;

			float Raw = 0.f;
			bool bHaveRaw = false;
			if (!MapPackage.IsEmpty())
			{
				const float AsyncPct = GetAsyncLoadPercentage(FName(*MapPackage));
				if (AsyncPct >= 0.f)
				{
					Raw = FMath::Clamp(AsyncPct / 100.f, 0.f, 0.99f);
					bHaveRaw = true;
				}
			}

			if (bHaveRaw)
			{
				// Never let a later lower package % pull the bar backward
				MaxDisplayedProgress = FMath::Max(MaxDisplayedProgress, Raw);
			}
			else
			{
				// Indeterminate: crawl forward only (never oscillate / regress)
				const float Crawl = FMath::Min(MaxDisplayedProgress + InDeltaTime * 0.07f, 0.88f);
				MaxDisplayedProgress = FMath::Max(MaxDisplayedProgress, Crawl);
			}

			// Displayed bar is monotonic — never lower than previous frame
			const float Smoothed = FMath::FInterpTo(DisplayProgress, MaxDisplayedProgress, InDeltaTime, 8.f);
			DisplayProgress = FMath::Max(DisplayProgress, Smoothed);
		}

	private:
		TOptional<float> GetProgressPercent() const
		{
			return DisplayProgress;
		}

		FText GetPercentText() const
		{
			const int32 Pct = FMath::Clamp(FMath::RoundToInt(DisplayProgress * 100.f), 0, 99);
			return FText::FromString(FString::Printf(TEXT("%d%%"), Pct));
		}

		FText GetSubtitleText() const
		{
			return FText::FromString(FString::Printf(TEXT("正在进入 %s"), *FriendlyMapName(MapPackage)));
		}

		FString MapPackage;
		float DisplayProgress = 0.f;
		float MaxDisplayedProgress = 0.05f;
		float AnimTime = 0.f;
		FProgressBarStyle OwnedBarStyle;
		FSlateBrush BgBrush;
		FSlateBrush PanelBrush;
	};

	TSharedRef<SWidget> MakeLoadingWidget(const FString& MapName)
	{
		return SNew(SShooterLoadingScreenWidget).MapName(MapName);
	}

	void HideViewportFallback()
	{
		if (GViewportWidget.IsValid() && GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(GViewportWidget.ToSharedRef());
		}
		GViewportWidget.Reset();
	}

	void BeginLoading(const FString& MapName)
	{
		GLoadingMapPackage = MapName;
		UE_LOG(LogTemp, Log, TEXT("[Loading] Begin %s"), *MapName);

		const TSharedRef<SWidget> Widget = MakeLoadingWidget(MapName);

		FLoadingScreenAttributes Attr;
		Attr.bAutoCompleteWhenLoadingCompletes = true;
		Attr.MinimumLoadingScreenDisplayTime = 0.35f;
		Attr.bAllowEngineTick = true;
		Attr.WidgetLoadingScreen = Widget;

		if (IsMoviePlayerEnabled())
		{
			HideViewportFallback();
			GetMoviePlayer()->SetupLoadingScreen(Attr);
			return;
		}

		// PIE / editor: MoviePlayer is usually off — keep a viewport overlay so travel isn't a black freeze.
		if (GEngine && GEngine->GameViewport)
		{
			HideViewportFallback();
			GViewportWidget = Widget;
			GEngine->GameViewport->AddViewportWidgetContent(GViewportWidget.ToSharedRef(), 10000);
		}
	}

	void EndLoading(UWorld* /*World*/)
	{
		UE_LOG(LogTemp, Log, TEXT("[Loading] End %s"), *GLoadingMapPackage);
		GLoadingMapPackage.Reset();
		HideViewportFallback();
		if (IsMoviePlayerEnabled() && GetMoviePlayer())
		{
			GetMoviePlayer()->StopMovie();
		}
	}

	void OnPreLoadMap(const FString& MapName)
	{
		if (!GOwnerGI.IsValid()) return;
		BeginLoading(MapName);
	}

	void OnPostLoadMap(UWorld* World)
	{
		if (!GOwnerGI.IsValid()) return;
		EndLoading(World);
	}
}

void ShooterLoadingScreen::Register(UGameInstance* GameInstance)
{
	if (GRegistered) return;
	GOwnerGI = GameInstance;
	GPreLoadHandle = FCoreUObjectDelegates::PreLoadMap.AddStatic(&OnPreLoadMap);
	GPostLoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddStatic(&OnPostLoadMap);
	GRegistered = true;
	UE_LOG(LogTemp, Log, TEXT("[Loading] Registered map-travel loading screen"));
}

void ShooterLoadingScreen::Hide()
{
	GLoadingMapPackage.Reset();
	HideViewportFallback();
	if (IsMoviePlayerEnabled() && GetMoviePlayer())
	{
		GetMoviePlayer()->StopMovie();
	}
}

void ShooterLoadingScreen::Unregister()
{
	if (!GRegistered) return;
	FCoreUObjectDelegates::PreLoadMap.Remove(GPreLoadHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(GPostLoadHandle);
	GPreLoadHandle.Reset();
	GPostLoadHandle.Reset();
	HideViewportFallback();
	GOwnerGI.Reset();
	GRegistered = false;
}

