#include "Ui/MissionTabletWidget.h"
#include "Ui/OperationUIStyle.h"
#include "Ui/ShooterLoadingScreen.h"
#include "Game/CoopCqbGameState.h"
#include "Engine/Texture2D.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Rendering/DrawElements.h"

namespace TabletStyle
{
 const FLinearColor Background=FLinearColor::FromSRGBColor(FColor(242,243,244));
 const FLinearColor Text=FLinearColor::FromSRGBColor(FColor(23,25,28));
 const FLinearColor Secondary=FLinearColor::FromSRGBColor(FColor(90,96,102));
 const FLinearColor Line=FLinearColor::FromSRGBColor(FColor(216,219,222));
 const FLinearColor Red=FLinearColor::FromSRGBColor(FColor(179,57,46));
 TSharedRef<STextBlock> Label(const FText& Value,int32 Size,const FLinearColor& Color,bool Bold=false)
 {
  return SNew(STextBlock).Text(Value).Font(FOperationUIStyle::MakeFont(Size,Bold)).ColorAndOpacity(Color);
 }

 // Vector paths keep the three navigation symbols consistent at every render resolution.
 class SNavigationIcon : public SLeafWidget
 {
 public:
  SLATE_BEGIN_ARGS(SNavigationIcon){} SLATE_ARGUMENT(int32,Kind) SLATE_ARGUMENT(FLinearColor,Color) SLATE_END_ARGS()
  void Construct(const FArguments& A){Kind=A._Kind;Color=A._Color;}
  virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(28,28);}
  virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool)const override
  {
   const FVector2D Scale=G.GetLocalSize()/FVector2D(28,28);
   auto Path=[&](std::initializer_list<FVector2D> P){TArray<FVector2D> Points;for(const auto& V:P)Points.Add(V*Scale);FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,1.5f);};
   if(Kind==0){Path({{6,25},{6,21},{9,18},{19,18},{22,21},{22,25}});Path({{10,13},{8,10},{8,6},{11,3},{17,3},{20,6},{20,10},{18,13},{10,13}});}
   else if(Kind==1){Path({{7,3},{21,3},{21,25},{7,25},{7,3}});Path({{11,9},{17,9}});Path({{11,14},{17,14}});Path({{11,19},{15,19}});}
   else {Path({{3,7},{10,3},{18,7},{25,3},{25,22},{18,26},{10,22},{3,26},{3,7}});Path({{10,3},{10,22}});Path({{18,7},{18,26}});}
   return Layer;
  }
 private:int32 Kind=0;FLinearColor Color;
 };
}

TSharedRef<SWidget> UMissionTabletWidget::RebuildWidget()
{
 using namespace TabletStyle;
 SetIsFocusable(false);
 FString TypeLabel=TEXT("行动命令");
 if(GetWorld())GConfig->GetString(TEXT("FPS.MissionUI"),GetWorld()->GetMapName().Contains(TEXT("Factory"))?TEXT("FactoryTypeLabel"):TEXT("DefaultTypeLabel"),TypeLabel,GGameIni);
 BackgroundBrush=FOperationUIStyle::MakePanelBrush(Background,0,0);
 LineBrush=FOperationUIStyle::MakePanelBrush(Line,0,0);
 RedBrush=FOperationUIStyle::MakePanelBrush(Red,0,0);
 SelectedBrush=FOperationUIStyle::MakePanelBrush(Text,0,0);
 SelectedBrush.OutlineSettings.CornerRadii=FVector4(32,32,32,32);
 CircleBrush=FOperationUIStyle::MakePanelBrush(FLinearColor::Transparent,0,1.5f);
 CircleBrush.OutlineSettings.CornerRadii=FVector4(32,32,32,32);
 CircleBrush.OutlineSettings.Color=FLinearColor::FromSRGBColor(FColor(185,190,196));
 CheckBrush=FOperationUIStyle::MakePanelBrush(FLinearColor::Transparent,0,1.5f,&Secondary);
 ImageBrush.DrawAs=ESlateBrushDrawType::Image;
 ImageBrush.ImageSize=FVector2D(2000,780);
 auto Nav=SNew(SVerticalBox);
 const FText Names[]={NSLOCTEXT("Tablet","Roster","名册"),NSLOCTEXT("Tablet","Briefing","简报"),NSLOCTEXT("Tablet","Map","地图")};
 for(int32 I=0;I<3;++I)
 {
  const bool Selected=I==1;
  Nav->AddSlot().AutoHeight().Padding(16,I==0?48:24,16,0)
  [SNew(SBox).WidthOverride(64).HeightOverride(64)
   [SNew(SBorder).BorderImage(Selected?&SelectedBrush:&CircleBrush).Padding(0).HAlign(HAlign_Center).VAlign(VAlign_Center)
    [SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(TabletStyle::SNavigationIcon).Kind(I).Color(Selected?FLinearColor::White:Secondary)]
     +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,6,0,0)[Label(Names[I],12,Selected?FLinearColor::White:Secondary)]]]];
 }

 // Adapt the reference canvas height to the complete 21.327908 : 14.803008 screen face.
 // Width tokens remain 96 / flexible / 400 and the display covers the entire plane.
 return SNew(SBorder).BorderImage(&BackgroundBrush).Padding(0)
 [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
  [SNew(SBox).WidthOverride(1600).HeightOverride(1600.f*14.803008f/21.327908f)
   [SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(96)[Nav]]
    +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(1)[SNew(SBorder).BorderImage(&LineBrush)]]
    +SHorizontalBox::Slot().FillWidth(1)
    [SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()
     [SNew(SBox).HeightOverride(110)
      [SNew(SBorder).BorderImage(&BackgroundBrush).Padding(24,8)
       [SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()
        [SNew(SHorizontalBox)
         +SHorizontalBox::Slot().FillWidth(1)[Label(FText::FromString(TypeLabel),13,Secondary)]
         +SHorizontalBox::Slot().AutoWidth()[SAssignNew(Location,STextBlock).Font(FOperationUIStyle::MakeFont(20)).ColorAndOpacity(Secondary)]]
        +SVerticalBox::Slot().FillHeight(1).VAlign(VAlign_Bottom)
         [SAssignNew(Title,STextBlock).Font(FOperationUIStyle::MakeFont(44,true)).ColorAndOpacity(Text).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]]]]
     +SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(1)[SNew(SBorder).BorderImage(&LineBrush)]]
     +SVerticalBox::Slot().AutoHeight()
      [SNew(SBox).HeightOverride(430).Clipping(EWidgetClipping::ClipToBounds)
       [SNew(SOverlay)
        +SOverlay::Slot()[SNew(SImage).Image(&ImageBrush)]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
         [SNew(STextBlock).Text(NSLOCTEXT("Tablet","NoRecon","侦察图暂不可用")).Font(FOperationUIStyle::MakeFont(16)).ColorAndOpacity(Secondary)
          .Visibility_Lambda([this](){return ReconImage?EVisibility::Collapsed:EVisibility::HitTestInvisible;})]]]
     +SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(1)[SNew(SBorder).BorderImage(&LineBrush)]]
     +SVerticalBox::Slot().FillHeight(1).Padding(24)
      [SNew(SVerticalBox)
       +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)
        [SNew(SHorizontalBox)
         +SHorizontalBox::Slot().FillWidth(1)[Label(Names[1],13,Secondary)]
         +SHorizontalBox::Slot().AutoWidth()[Label(NSLOCTEXT("Tablet","ScrollHint","滚轮翻阅 · Shift + 滚轮查看目标"),12,Secondary)]]
       +SVerticalBox::Slot().FillHeight(1)
       [SAssignNew(BriefingScroll,SScrollBox)
        +SScrollBox::Slot()[SAssignNew(Briefing,STextBlock).Font(FOperationUIStyle::MakeFont(14)).ColorAndOpacity(Secondary).AutoWrapText(true).LineHeightPercentage(1.7f)]]]]
    +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(1)[SNew(SBorder).BorderImage(&LineBrush)]]
    +SHorizontalBox::Slot().AutoWidth()
    [SNew(SBox).WidthOverride(400)
     [SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight().Padding(24,32,24,16)[Label(NSLOCTEXT("Tablet","Objectives","目标"),20,Text,true)]
      +SVerticalBox::Slot().AutoHeight().Padding(24,0,24,24)
       [SNew(SBox).HeightOverride(30)
        [SNew(SBorder).BorderImage(&RedBrush).Padding(8,0).VAlign(VAlign_Center)
         [SAssignNew(CurrentObjective,STextBlock).Font(FOperationUIStyle::MakeFont(14)).ColorAndOpacity(FLinearColor::White).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]]]
      +SVerticalBox::Slot().FillHeight(1).Padding(24,0,24,24)
       [SAssignNew(ObjectiveScroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(Objectives,SVerticalBox)]]
      +SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(1)[SNew(SBorder).BorderImage(&LineBrush)]]
      +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0,32,32,32)
       [SNew(SBox).WidthOverride(200).HeightOverride(48)
        [SNew(SBorder).BorderImage(&RedBrush).Padding(0).HAlign(HAlign_Center).VAlign(VAlign_Center)
         [Label(NSLOCTEXT("Tablet","Close","关闭（Tab）"),16,FLinearColor::White)]]]]]]]];
}

void UMissionTabletWidget::NativeConstruct()
{
 Super::NativeConstruct();
 RefreshMission();
}

void UMissionTabletWidget::AddObjective(const FText& Name,const FText& Detail,bool Complete)
{
 using namespace TabletStyle;
 Objectives->AddSlot().AutoHeight().Padding(0,0,0,20)
 [SNew(SHorizontalBox)
  +SHorizontalBox::Slot().AutoWidth().Padding(0,1,12,0)
   [SNew(SBox).WidthOverride(20).HeightOverride(20).VAlign(VAlign_Top)
    [SNew(SBorder).BorderImage(&CheckBrush).Padding(0).HAlign(HAlign_Center).VAlign(VAlign_Center)
     [Label(Complete?FText::FromString(TEXT("✓")):FText::GetEmpty(),16,Text,true)]]]
  +SHorizontalBox::Slot().FillWidth(1)
   [SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Name).Font(FOperationUIStyle::MakeFont(16)).ColorAndOpacity(Complete?Secondary:Text).AutoWrapText(true)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)[Label(Detail,12,Secondary)]]];
}

void UMissionTabletWidget::RefreshMission()
{
 if(!Objectives)return;
 const FString Map=GetWorld()->GetMapName();
 const bool Factory=Map.Contains(TEXT("Factory"));
 Title->SetText(FText::FromString(ShooterLoadingScreen::GetTaskName(Map)));
 Location->SetText(FText::FromString(ShooterLoadingScreen::GetMapName(Map)));
 FString Body;
 GConfig->GetString(TEXT("FPS.MissionUI"),Factory?TEXT("FactoryBriefing"):TEXT("DefaultBriefing"),Body,GGameIni);
 Body.ReplaceInline(TEXT("\\n"),TEXT("\n"));
 Briefing->SetText(FText::FromString(Body));
 FString ImagePath;
 GConfig->GetString(TEXT("FPS.MissionUI"),Factory?TEXT("FactoryReconImage"):TEXT("DefaultReconImage"),ImagePath,GGameIni);
 if(!ImagePath.IsEmpty()&&(!ReconImage||ReconImage->GetPathName()!=ImagePath))ReconImage=LoadObject<UTexture2D>(nullptr,*ImagePath);
 ImageBrush.SetResourceObject(ReconImage);
 if(ReconImage)ImageBrush.ImageSize=FVector2D(ReconImage->GetSizeX(),ReconImage->GetSizeY());
 const auto* GS=GetWorld()->GetGameState<ACoopCqbGameState>();
 if(!GS)return;
 Objectives->ClearChildren();
 const FMissionReport& R=GS->Report;
 const int32 Resolved=R.Neutralized+R.Arrested;
 FString Active=TEXT("控制现场");
 bool FoundPending=false;
 auto Row=[&](const FString& Name,const FString& Detail,bool Done){AddObjective(FText::FromString(Name),FText::FromString(Detail),Done);if(!Done&&!FoundPending){Active=Name;FoundPending=true;}};
 Row(TEXT("控制现场"),R.bPrimaryComplete?TEXT("必要目标已完成"):TEXT("完成现场全部必要目标"),R.bPrimaryComplete);
 if(R.SuspectsTotal>0)Row(TEXT("处置嫌疑人"),FString::Printf(TEXT("已处置 %d / %d · 已拘捕 %d"),Resolved,R.SuspectsTotal,R.Arrested),Resolved>=R.SuspectsTotal);
 if(R.HostagesTotal>0)Row(TEXT("解救人质"),FString::Printf(TEXT("已解救 %d / %d"),R.Rescued,R.HostagesTotal),R.Rescued>=R.HostagesTotal);
 for(const FText& Description:GS->ObjectiveDescriptions)
 {
  FString Name=Description.ToString();const bool Complete=Name.StartsWith(TEXT("✓"));
  if(Complete||Name.StartsWith(TEXT("○")))Name=Name.RightChop(1).TrimStartAndEnd();
  Row(Name,Complete?TEXT("已完成"):TEXT("等待现场确认"),Complete);
 }
 if(R.HiddenCompleted>0)Row(TEXT("额外发现"),FString::Printf(TEXT("已完成 %d 项额外目标"),R.HiddenCompleted),true);
 CurrentObjective->SetText(FText::FromString(R.bPrimaryComplete?TEXT("必要目标已完成"):Active));
 InvalidateLayoutAndVolatility();
}

void UMissionTabletWidget::ScrollBriefing(float Amount,bool bObjectives)
{
 const auto Scroll=bObjectives?ObjectiveScroll:BriefingScroll;
 if(Scroll)Scroll->SetScrollOffset(FMath::Max(0.f,Scroll->GetScrollOffset()+Amount));
}

void UMissionTabletWidget::ReleaseSlateResources(bool Children)
{
 Super::ReleaseSlateResources(Children);
 Title.Reset();Location.Reset();Briefing.Reset();CurrentObjective.Reset();Objectives.Reset();BriefingScroll.Reset();ObjectiveScroll.Reset();
}
