#include "Ui/ShooterLoadingScreen.h"
#include "Ui/OperationUIStyle.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"

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
	void TraceLoading(const TCHAR* Event)
	{
		if (!FParse::Param(FCommandLine::Get(), TEXT("UIQA"))) return;
		const FString Line = FString::Printf(TEXT("{\"event\":\"%s\",\"map\":\"%s\"}\n"), Event, *GLoadingMapPackage);
		FFileHelper::SaveStringToFile(Line, *(FPaths::ProjectSavedDir()/FString::Printf(TEXT("UIQA_Loading_%u.jsonl"), FPlatformProcess::GetCurrentProcessId())),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}


 class SShooterLoadingScreenWidget : public SCompoundWidget
 {
 public:
  SLATE_BEGIN_ARGS(SShooterLoadingScreenWidget){} SLATE_ARGUMENT(FString,MapName) SLATE_END_ARGS()
  void Construct(const FArguments& A)
  {
   MapPackage=FPackageName::ObjectPathToPackageName(A._MapName);
   Bg=FOperationUIStyle::MakePanelBrush(FLinearColor::Black,0,0);
   Bar.BackgroundImage=FOperationUIStyle::MakePanelBrush(FOperationUIStyle::Outline,0,0);
   Bar.FillImage=FOperationUIStyle::MakePanelBrush(FOperationUIStyle::Text.CopyWithNewOpacity(.8f),0,0);
   Bar.MarqueeImage=Bar.FillImage;
   ChildSlot[SNew(SOverlay)
    +SOverlay::Slot()[SNew(SBorder).BorderImage(&Bg).Padding(0)]
    +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(48,0,48,40))
    [SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,0,0,8))
      [SNew(STextBlock).Text(FText::FromString(TEXT("行 动 准 备"))).Font(FOperationUIStyle::MakeFont(13)).ColorAndOpacity(FOperationUIStyle::TextMuted)
       .ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))]
     +SVerticalBox::Slot().AutoHeight()
      [SNew(STextBlock).Text(FText::FromString(ShooterLoadingScreen::GetTaskName(MapPackage))).Font(FOperationUIStyle::MakeFont(20)).ColorAndOpacity(FOperationUIStyle::Text)
       .ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))]]
    +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(48,0,48,40))
     [SNew(STextBlock).Text_Lambda([this]{
       FString Dots=TEXT("…");
       if(SinceChange>5.f)Dots=FString::ChrN(1+FMath::FloorToInt(AnimationTime*1.5f)%3,TCHAR(0x00B7));
       return FText::FromString(TEXT("正在部署")+Dots);
      }).Font(FOperationUIStyle::MakeFont(16)).ColorAndOpacity(FOperationUIStyle::Text)
       .ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))]
    +SOverlay::Slot().VAlign(VAlign_Bottom)
     [SNew(SBox).HeightOverride(2)[SNew(SProgressBar).Style(&Bar).BorderPadding(FVector2D::ZeroVector)
       .BarFillStyle(EProgressBarFillStyle::Scale).Percent_Lambda([this]{return TOptional<float>(Progress);})]]];
  }
  virtual void Tick(const FGeometry& G,double T,float D)override
  {
   SCompoundWidget::Tick(G,T,D);AnimationTime+=D;SinceChange+=D;
   const float Raw=GetAsyncLoadPercentage(FName(*MapPackage));
   // Unknown means hold. There is no clock-driven progress, initial head start or 99% cap.
   if(Raw>=0.f)
   {
    const float Measured=FMath::Clamp(Raw/100.f,0.f,1.f);
    if(Measured>Progress){Progress=Measured;SinceChange=0;}
   }
  }
 private:
  FString MapPackage;
  float Progress=0,AnimationTime=0,SinceChange=0;
  FSlateBrush Bg;
  FProgressBarStyle Bar;
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

#if WITH_EDITOR
 // Editor-only layout/stall inspection, using an actually idle async-loader (unknown -> hold).
 FAutoConsoleCommand LoadingPreview(TEXT("fps.UI.PreviewLoading"),TEXT("Toggle the deployment loading widget for visual QA."),
  FConsoleCommandDelegate::CreateLambda([] {
   if(GViewportWidget.IsValid()){HideViewportFallback();return;}
   if(GEngine&&GEngine->GameViewport){GViewportWidget=MakeLoadingWidget(TEXT("/Game/Maps/Lvl_Factory_Whitebox"));GEngine->GameViewport->AddViewportWidgetContent(GViewportWidget.ToSharedRef(),10000);}
  }));
#endif
	void BeginLoading(const FString& MapName)
	{
		GLoadingMapPackage = MapName;
		TraceLoading(TEXT("Begin"));
		UE_LOG(LogTemp, Log, TEXT("[Loading] Begin %s"), *MapName);

		const TSharedRef<SWidget> Widget = MakeLoadingWidget(MapName);

		FLoadingScreenAttributes Attr;
		Attr.bAutoCompleteWhenLoadingCompletes = true;
		Attr.MinimumLoadingScreenDisplayTime = 0.f;
		// Slate-only blocking-travel chrome. No MoviePaths/video or drone footage.
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
		TraceLoading(TEXT("End"));
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


FString ShooterLoadingScreen::GetTaskName(const FString& MapPath)
{
 FString Task=MapPath.Contains(TEXT("Factory"))?TEXT("清剿行动 — 清光匪徒"):TEXT("行动部署");
 if(GConfig)GConfig->GetString(TEXT("FPS.MissionUI"),MapPath.Contains(TEXT("Factory"))?TEXT("FactoryTask"):TEXT("DefaultTask"),Task,GGameIni);
 return Task;
}
FString ShooterLoadingScreen::GetMapName(const FString& MapPath)
{
 FString Name=MapPath.Contains(TEXT("Factory"))?TEXT("工厂"):TEXT("行动区域");
 if(GConfig)GConfig->GetString(TEXT("FPS.MissionUI"),MapPath.Contains(TEXT("Factory"))?TEXT("FactoryMapName"):TEXT("DefaultMapName"),Name,GGameIni);
 return Name;
}
