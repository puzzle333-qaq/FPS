#include "Ui/MissionDebriefSlate.h"
#include "Ui/OperationUIStyle.h"
#include "Ui/MissionBestSaveGame.h"
#include "Ui/ShooterLoadingScreen.h"
#include "Game/CoopCqbGameState.h"
#include "ShooterPlayerController.h"
#include "ShooterGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Rendering/DrawElements.h"
#include "TimerManager.h"
#include "Styling/CoreStyle.h"

namespace {
FString FormatTime(float Seconds){const int32 T=FMath::Max(0,FMath::RoundToInt(Seconds*100));return FString::Printf(TEXT("%02d:%02d.%02d"),T/6000,(T/100)%60,T%100);}
TSharedRef<STextBlock> Label(const FString& S,int32 Size=16,FLinearColor Color=FOperationUIStyle::Text){return SNew(STextBlock).Text(FText::FromString(S)).Font(FOperationUIStyle::MakeFont(Size,Size>=28)).ColorAndOpacity(Color);}
class SScoreArc : public SLeafWidget {
public:
 SLATE_BEGIN_ARGS(SScoreArc){} SLATE_ATTRIBUTE(float,Fraction) SLATE_END_ARGS()
 void Construct(const FArguments& A){Fraction=A._Fraction;}
 virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(560);}
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool)const override{
  const float Ratio=FMath::Clamp(Fraction.Get(),0.f,1.f);TArray<FVector2D> Points;
  // A small structural gap remains at full score. Progress always starts at twelve o'clock.
  for(int32 I=0;I<=180;++I){float A=-PI/2+2*PI*Ratio*I/180;if(A>.60f&&A<.86f){if(Points.Num()>1)FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,FOperationUIStyle::Text,true,2.f);Points.Reset();continue;}Points.Add(FVector2D(280+274*FMath::Cos(A),280+274*FMath::Sin(A)));}
  if(Ratio>0&&Points.Num()>1)FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,FOperationUIStyle::Text,true,2.f);
  return Layer;
 }
 TAttribute<float> Fraction;
};
class SAfterActionScreen : public SCompoundWidget {
public:
 SLATE_BEGIN_ARGS(SAfterActionScreen){} SLATE_ARGUMENT(AShooterPlayerController*,Owner) SLATE_END_ARGS()
 void Construct(const FArguments& A){PC=A._Owner;GS=PC.IsValid()?PC->GetWorld()->GetGameState<ACoopCqbGameState>():nullptr;Started=FPlatformTime::Seconds();Black=FOperationUIStyle::MakePanelBrush(FLinearColor::Black,0,0);Button.SetNormal(FOperationUIStyle::MakePanelBrush(FLinearColor::Transparent,0,0));Button.SetHovered(FOperationUIStyle::MakePanelBrush(FLinearColor(.04,.04,.04),0,0));Button.SetPressed(Button.Hovered);
  ChildSlot[SNew(SBorder).BorderImage(&Black).Padding(0).ColorAndOpacity_Lambda([this]{return FLinearColor(1,1,1,FMath::Clamp(Age()/.2f,0.f,1.f));})[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1920).HeightOverride(1080)[SAssignNew(Canvas,SCanvas)]]]];
  Build();
 }
 virtual bool SupportsKeyboardFocus()const override{return true;}
 virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent&)override{if(Age()<2.2f){Skip();return FReply::Handled();}return FReply::Unhandled();}
 virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent&)override{Skip();return FReply::Handled();}
 void Skip(){Started=FPlatformTime::Seconds()-3;}
 float Age()const{return float(FPlatformTime::Seconds()-Started);}
 virtual void Tick(const FGeometry& G,double T,float D)override{SCompoundWidget::Tick(G,T,D);if(!Saved&&Age()>.35f&&GS.IsValid()&&GS->IsDebriefOpen()){ReadBest();Saved=true;Build();}}
 void ReadBest(){
  const FString Id=UWorld::RemovePIEPrefix(PC->GetWorld()->GetOutermost()->GetName());
  auto* Save=Cast<UMissionBestSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("MissionPersonalBest"),0));
  if(!Save)Save=NewObject<UMissionBestSaveGame>();
  if(const float* Previous=Save->BestSeconds.Find(Id))Best=FormatTime(*Previous);
  if(GS->DidOperatorsWin()&&GS->FinalMissionSeconds>0){float& Seconds=Save->BestSeconds.FindOrAdd(Id);if(Seconds<=0||GS->FinalMissionSeconds<Seconds)Seconds=GS->FinalMissionSeconds;}
  int32& Score=Save->BestScores.FindOrAdd(Id);Score=FMath::Max(Score,GS->Report.Score);UGameplayStatics::SaveGameToSlot(Save,TEXT("MissionPersonalBest"),0);
 }
 void Put(FVector2D Pos,FVector2D Size,TSharedRef<SWidget> Widget){Canvas->AddSlot().Position(Pos).Size(Size)[Widget];}
 TSharedRef<SWidget> Action(const FString& Name,TFunction<void()> Run,bool HostOnly=false){
  auto Control=SNew(SButton).ButtonStyle(&Button).ContentPadding(FMargin(12,8)).IsEnabled_Lambda([this,HostOnly]{return Age()>=2.2f&&(!HostOnly||(PC.IsValid()&&PC->HasAuthority()));}).OnClicked_Lambda([Run=MoveTemp(Run)]{Run();return FReply::Handled();});
  TWeakPtr<SButton> WeakControl=Control;
  Control->SetContent(SNew(STextBlock).Text(FText::FromString(Name)).Font(FOperationUIStyle::MakeFont(16)).ColorAndOpacity_Lambda([WeakControl]{auto B=WeakControl.Pin();return B&&B->IsHovered()?FOperationUIStyle::Text:FOperationUIStyle::TextMuted;}));
  return SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).ColorAndOpacity_Lambda([this]{return FLinearColor(1,1,1,FMath::Clamp((Age()-2.f)/.2f,0.f,1.f));})[SNew(SOverlay)+SOverlay::Slot()[Control]+SOverlay::Slot().HAlign(HAlign_Left)[SNew(SBox).WidthOverride(2)[SNew(SBorder).Padding(0).BorderImage(&Red).Visibility_Lambda([WeakControl]{auto B=WeakControl.Pin();return B&&B->IsHovered()?EVisibility::HitTestInvisible:EVisibility::Hidden;})]]];
 }
 void Build(){if(!GS.IsValid()||!Canvas)return;Canvas->ClearChildren();const auto& R=GS->Report;
  Put({48,40},{1000,44},Label(ShooterLoadingScreen::GetMapName(PC->GetWorld()->GetMapName()),28));
  Red=FOperationUIStyle::MakePanelBrush(FOperationUIStyle::Warning,0,0);Put({48,94},{64,2},SNew(SBorder).BorderImage(&Red));
  Put({48,112},{560,24},Label(TEXT("行动报告  /  AFTER ACTION"),13,FOperationUIStyle::TextMuted));
  Put({1392,40},{480,90},SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[SNew(STextBlock).Text_Lambda([this]{return FText::FromString(FormatTime(GS->FinalMissionSeconds));}).Font(FOperationUIStyle::MakeFont(28)).ColorAndOpacity(FOperationUIStyle::Text)]+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[Label(TEXT("个人最佳   ")+Best,16,FOperationUIStyle::TextMuted)]);
  Put({680,192},{560,560},SNew(SScoreArc).Fraction_Lambda([this]{float Ratio=GS->Report.MaximumScore>0?float(GS->Report.Score)/GS->Report.MaximumScore:0;return Ratio*(GS->DidOperatorsWin()?FMath::Clamp((Age()-1.2f)/.6f,0.f,1.f):1.f);}));
  Put({680,240},{560,420},SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([this]{return FText::FromString(GS->Report.Grade);}).Font(FOperationUIStyle::MakeFont(300,true)).ColorAndOpacity_Lambda([this]{auto C=FOperationUIStyle::Text;C.A=FMath::Clamp((Age()-1.8f)/.2f,0.f,1.f);return FSlateColor(C);}).RenderTransform_Lambda([this]{return FSlateRenderTransform(FScale2D(1.f+.06f*(1-FMath::Clamp((Age()-1.8f)/.2f,0.f,1.f))));}).RenderTransformPivot(FVector2D(.5))]);
  Put({680,776},{560,64},SNew(SBox).HAlign(HAlign_Center)[SNew(STextBlock).Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("%d / %d"),FMath::RoundToInt(GS->Report.Score*FMath::Clamp((Age()-.4f)/.6f,0.f,1.f)),GS->Report.MaximumScore));}).Font(FOperationUIStyle::MakeFont(40,true)).ColorAndOpacity(FOperationUIStyle::Text)]);
  FString Note=R.Penalties>0?FString::Printf(TEXT("%s  −%d"),R.SurrenderedKilled>0?TEXT("击杀已投降者 / 行动惩罚"):TEXT("平民伤亡 / 行动惩罚"),R.Penalties):R.HiddenCompleted>0?FString::Printf(TEXT("隐藏记录已达成 ×%d"),R.HiddenCompleted):TEXT("");
  Put({680,846},{560,40},SNew(SBox).HAlign(HAlign_Center)[Label(Note,13,R.Penalties>0?FOperationUIStyle::Warning:FOperationUIStyle::TextMuted)]);
  TSharedPtr<SVerticalBox> Receipt;Put({48,360},{560,600},SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Receipt,SVerticalBox)]);int32 RowIndex=0;
  auto Row=[&](FString Count,FString Name,int32 Points,bool RedRow=false){const int32 I=RowIndex++;Receipt->AddSlot().AutoHeight().Padding(0,0,0,12)[SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).ColorAndOpacity_Lambda([this,I]{return FLinearColor(1,1,1,FMath::Clamp((Age()-.2f-I*.12f)/.12f,0.f,1.f));})[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(72)[Label(Count,13,RedRow?FOperationUIStyle::Warning:FOperationUIStyle::TextMuted)]]+SHorizontalBox::Slot().FillWidth(1)[Label(Name,16,RedRow?FOperationUIStyle::Warning:FOperationUIStyle::Text)]+SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text_Lambda([this,Points,I]{return FText::AsNumber(FMath::RoundToInt(Points*FMath::Clamp((Age()-.2f-I*.12f)/.3f,0.f,1.f)));}).Font(FOperationUIStyle::MakeFont(16)).ColorAndOpacity(RedRow?FOperationUIStyle::Warning:FOperationUIStyle::Text)]]];};
  Row(R.bPrimaryComplete?TEXT("1/1"):TEXT("0/1"),TEXT("主目标"),R.PrimaryPoints,!R.bPrimaryComplete);
  for(const auto& Text:GS->ObjectiveDescriptions)Receipt->AddSlot().AutoHeight().Padding(72,0,0,8)[Label(Text.ToString(),13,FOperationUIStyle::TextMuted)];
  Row(FString::Printf(TEXT("%d/%d"),R.MarkedCompleted,R.MarkedTotal),TEXT("查封与标记"),R.MarkedPoints);
  Row(FString::Printf(TEXT("%d/%d"),R.Arrested,R.SuspectsTotal),TEXT("已控制嫌疑人"),R.ArrestedPoints);
  Row(FString::Printf(TEXT("%d/%d"),R.Neutralized,R.SuspectsTotal),TEXT("已击毙嫌疑人"),R.NeutralizedPoints);
  Row(FString::Printf(TEXT("%d/%d"),R.Rescued,R.HostagesTotal),TEXT("已解救平民"),R.HostagePoints);
  if(R.HostagesLost>0)Row(FString::FromInt(R.HostagesLost),TEXT("平民死亡"),0,true);
  Row(FString::Printf(TEXT("%d/%d"),R.Survivors,R.OperatorsTotal),TEXT("小队生还"),R.SurvivalPoints);
  Row(FString::Printf(TEXT("%d/%d"),R.HiddenCompleted,GS->HiddenRecordTotal),TEXT("隐藏记录"),R.HiddenPoints);
  for(const FText& Name:GS->CompletedHiddenRecords)Receipt->AddSlot().AutoHeight().Padding(72,0,0,8)[Label(TEXT("✓ ")+Name.ToString(),13,FOperationUIStyle::TextMuted)];
  if(GS->HiddenRecordTotal>R.HiddenCompleted)Receipt->AddSlot().AutoHeight().Padding(72,0,0,8)[Label(FString::Printf(TEXT("未达成的隐藏记录 ×%d"),GS->HiddenRecordTotal-R.HiddenCompleted),13,FOperationUIStyle::Dim)];
  if(R.Penalties>0)Row(TEXT("—"),TEXT("惩罚合计"),-R.Penalties,true);
  TWeakObjectPtr<AShooterPlayerController> Weak=PC;
  Put({48,1000},{640,40},SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[Action(TEXT("退出至主菜单"),[Weak]{if(Weak.IsValid())if(auto* GI=Weak->GetGameInstance<UShooterGameInstance>())GI->GoToMainMenu();})]+SHorizontalBox::Slot().AutoWidth().Padding(24,0)[Action(TEXT("设置"),[Weak]{if(Weak.IsValid())Weak->ShowSettings();})]);
  Put({1420,1000},{452,40},SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Action(TEXT("重试"),[Weak]{if(Weak.IsValid())Weak->Server_DebriefAction(true);},true)]+SHorizontalBox::Slot().FillWidth(1)[Action(TEXT("返回总部"),[Weak]{if(Weak.IsValid())Weak->Server_DebriefAction(false);},true)]);
  if(!PC->HasAuthority())Put({1420,960},{452,26},Label(TEXT("等待房主决定"),13,FOperationUIStyle::TextMuted));
 }
 TWeakObjectPtr<AShooterPlayerController> PC;TWeakObjectPtr<ACoopCqbGameState> GS;TSharedPtr<SCanvas> Canvas;FSlateBrush Black,Red;FButtonStyle Button;double Started=0;bool Saved=false;FString Best=TEXT("--:--.--");
};
}
TSharedRef<SWidget> MakeMissionDebriefScreen(AShooterPlayerController* Owner){return SNew(SAfterActionScreen).Owner(Owner);}
bool SkipMissionDebriefAnimation(const TSharedPtr<SWidget>& Screen){if(Screen){auto Panel=StaticCastSharedPtr<SAfterActionScreen>(Screen);if(Panel->Age()<2.2f){Panel->Skip();return true;}}return false;}
