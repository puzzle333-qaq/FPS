#include "Ui/MissionFlowWidget.h"
#include "Ui/TacticalUIStyle.h"
#include "Ui/OperationUIStyle.h"
#include "Ui/ShooterLoadingScreen.h"
#include "ShooterPlayerController.h"
#include "World/MissionDeployPoint.h"
#include "Game/CoopCqbGameState.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "TimerManager.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerState.h"
#include "Ui/MissionDebriefSlate.h"

TSharedRef<SWidget> UMissionFlowWidget::RebuildWidget()
{
 SetIsFocusable(true);
 if(Panel==EMissionPanel::Debrief){DebriefScreen=MakeMissionDebriefScreen(Cast<AShooterPlayerController>(GetOwningPlayer()));return DebriefScreen.ToSharedRef();}
 PanelBrush = FTacticalUIStyle::MakePanelBrush(FTacticalUIStyle::Panel, 12.f, 1.f);
 DimBrush = FTacticalUIStyle::MakePanelBrush(FTacticalUIStyle::BgDim, 0.f, 0.f);
 ButtonStyle = FTacticalUIStyle::MakeButtonStyle();
 if(Panel==EMissionPanel::Tablet){
  PanelBrush=FOperationUIStyle::MakePanelBrush(FOperationUIStyle::Panel,2,1);
  DimBrush=FOperationUIStyle::MakePanelBrush(FOperationUIStyle::Panel,0,0);
  ButtonStyle.Normal=FOperationUIStyle::MakePanelBrush(FOperationUIStyle::Panel,2,1);
  ButtonStyle.Hovered=FOperationUIStyle::MakePanelBrush(FOperationUIStyle::Dim,2,1);
  ButtonStyle.Pressed=ButtonStyle.Normal;
 }
 ReconBrush=FOperationUIStyle::MakePanelBrush(FLinearColor::Transparent,0,1,&FOperationUIStyle::TextMuted);
 return SNew(SOverlay)
 +SOverlay::Slot()
 [SAssignNew(LegacyPanel,SBorder).BorderImage(&DimBrush).Padding(32)
  [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
   [SNew(SBox).WidthOverride(860).HeightOverride(760)
    [SNew(SBorder).BorderImage(&PanelBrush).Padding(36)
     [SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Rows,SVerticalBox)]]]]]]
 +SOverlay::Slot().Padding(FMargin(48,40))
 [SAssignNew(ReconPanel,SBorder).BorderImage(&ReconBrush).Padding(24).Visibility(EVisibility::Collapsed)
  [SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight()
    [SAssignNew(ReconMap,STextBlock).Font(FOperationUIStyle::MakeFont(28,true)).ColorAndOpacity(FOperationUIStyle::Text)
     .ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))]
   +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,8,0,0))
    [SAssignNew(ReconTask,STextBlock).Font(FOperationUIStyle::MakeFont(20)).ColorAndOpacity(FOperationUIStyle::Text)
     .ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f)).AutoWrapText(true)]
   +SVerticalBox::Slot().FillHeight(1)[SNew(SBox)]
   +SVerticalBox::Slot().AutoHeight()
    [SNew(SHorizontalBox)
     +SHorizontalBox::Slot().AutoWidth()
      [SNew(STextBlock).Text(FText::FromString(TEXT("等待队友就绪"))).Font(FOperationUIStyle::MakeFont(16)).ColorAndOpacity(FOperationUIStyle::TextMuted)
       .ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))]
     +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(8,0,0,0))
      [SAssignNew(ReconCount,STextBlock).Font(FOperationUIStyle::MakeFont(16)).ColorAndOpacity(FOperationUIStyle::Text)
       .ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))]]]];

}
void UMissionFlowWidget::ReleaseSlateResources(bool bReleaseChildren)
{
 DebriefScreen.Reset();
 Super::ReleaseSlateResources(bReleaseChildren); Rows.Reset(); Clock.Reset(); FirstButton.Reset(); LegacyPanel.Reset();ReconPanel.Reset();ReconMap.Reset();ReconTask.Reset();ReconCount.Reset();
}
void UMissionFlowWidget::NativeConstruct()
{
 Super::NativeConstruct();
 Refresh();
 GetWorld()->GetTimerManager().SetTimer(ClockTimer, this, &ThisClass::UpdateClock, .1f, true);
}
void UMissionFlowWidget::NativeDestruct()
{
 GetWorld()->GetTimerManager().ClearTimer(ClockTimer);
 if (Deploy) Deploy->OnDeploymentUpdated.RemoveDynamic(this, &ThisClass::Refresh);
 Super::NativeDestruct();
}
void UMissionFlowWidget::Configure(EMissionPanel InPanel, AMissionDeployPoint* InDeploy)
{
 if (Deploy) Deploy->OnDeploymentUpdated.RemoveDynamic(this, &ThisClass::Refresh);
 Panel = InPanel; Deploy = InDeploy;
 if (Deploy) Deploy->OnDeploymentUpdated.AddUniqueDynamic(this, &ThisClass::Refresh);
 Refresh();
}
void UMissionFlowWidget::AddLine(const FString& Text, int32 Size)
{
 const bool Modern=Panel==EMissionPanel::Tablet;
 const int32 FontSize=Size>=32?40:Size>=24?28:Size>=18?20:Size>=15?16:Size>=13?13:12;
 Rows->AddSlot().AutoHeight().Padding(0, 0, 0, Modern?16:14)
 [ SNew(STextBlock).Text(FText::FromString(Text)).Font(Modern?FOperationUIStyle::MakeFont(FontSize,Size>=24):FTacticalUIStyle::MakeFont(Size, Size >= 24))
  .ColorAndOpacity(Modern?FOperationUIStyle::Text:(Size >= 24 ? FTacticalUIStyle::TextGold : FTacticalUIStyle::Text)).AutoWrapText(true)
  .ShadowOffset(Modern?FVector2D(0,1):FVector2D::ZeroVector).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f)) ];
}
void UMissionFlowWidget::AddButton(const FText& Label, TFunction<void()> Action)
{
 TSharedPtr<SButton> NewButton;
 Rows->AddSlot().AutoHeight().Padding(0, 4)
 [ SAssignNew(NewButton, SButton).ButtonStyle(&ButtonStyle).ContentPadding(Panel==EMissionPanel::Tablet?FMargin(16,8):FMargin(18,12))
  .OnClicked_Lambda([Action=MoveTemp(Action)]() { Action(); return FReply::Handled(); })
  [ SNew(STextBlock).Text(Label).Font(Panel==EMissionPanel::Tablet?FOperationUIStyle::MakeFont(16):FTacticalUIStyle::MakeFont(18)).ColorAndOpacity(Panel==EMissionPanel::Tablet?FOperationUIStyle::Text:FTacticalUIStyle::Text)
   .ShadowOffset(Panel==EMissionPanel::Tablet?FVector2D(0,1):FVector2D::ZeroVector).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f)) ] ];
 if (!FirstButton) FirstButton = NewButton;
}
void UMissionFlowWidget::Refresh()
{
 if (!Rows) return;
 if(LegacyPanel)LegacyPanel->SetVisibility(Panel==EMissionPanel::Loading?EVisibility::Collapsed:EVisibility::Visible);
 if(ReconPanel)ReconPanel->SetVisibility(Panel==EMissionPanel::Loading?EVisibility::HitTestInvisible:EVisibility::Collapsed);
 Rows->ClearChildren(); Clock.Reset(); FirstButton.Reset();
 auto* PC = Cast<AShooterPlayerController>(GetOwningPlayer());
 TWeakObjectPtr<AShooterPlayerController> WeakPC(PC);
 if (Panel == EMissionPanel::Selection)
 {
  AddLine(TEXT("行动部署 / MISSION SELECT"), 28);
  if (!Deploy) { AddLine(TEXT("任务台不可用")); return; }
  const bool bHost = Deploy->IsRoomHost(PC);
  if (Deploy->SelectedMission!=INDEX_NONE)
  {
   AddLine(Deploy->GetDestination(Deploy->SelectedMission).Title.ToString(), 24);
   AddLine(Deploy->GetDestination(Deploy->SelectedMission).Briefing.ToString(),16);
   AddLine(FString::Printf(TEXT("全队就绪：%d / %d"),Deploy->ReadyPlayers.Num(),Deploy->DeploymentRoster.Num()),20);
   for(const auto& PS:Deploy->DeploymentRoster)
    if(PS)AddLine(FString::Printf(TEXT("%s  %s"),Deploy->ReadyPlayers.Contains(PS)?TEXT("[已就绪]"):TEXT("[等待 Y]"),*PS->GetPlayerName()),16);
   if(Deploy->CountdownEndServerTime>0)
   {
    AddLine(TEXT("全员已就绪。简介播放完成、倒计时结束后出发。"),16);
    Rows->AddSlot().AutoHeight().Padding(0,12)[SAssignNew(Clock,STextBlock).Font(FTacticalUIStyle::MakeFont(36,true)).ColorAndOpacity(FTacticalUIStyle::TextGold)];
   }
   else AddLine(TEXT("任务简介已开始播放。每位队员按 Y 就绪，全部就绪后开始倒计时。"),16);
   if(!Deploy->DeploymentError.IsEmpty())AddLine(Deploy->DeploymentError,16);
   AddButton(Deploy->IsPlayerReady(PC)?NSLOCTEXT("Mission","Unready","取消就绪（Y）"):NSLOCTEXT("Mission","Ready","准备就绪（Y）"),[WeakPC](){if(WeakPC.IsValid())WeakPC->ToggleMissionReady();});
   if (bHost)
   {
    TWeakObjectPtr<AMissionDeployPoint> WeakDeploy(Deploy);
    AddButton(NSLOCTEXT("Mission", "CancelDeploy", "取消出发（房主）"), [WeakPC, WeakDeploy]() { if (WeakPC.IsValid()) WeakPC->Server_CancelMissionDeploy(WeakDeploy.Get()); });
   }
   AddButton(NSLOCTEXT("Mission","HideSelection","暂时收起（Esc）"),[WeakPC](){if(WeakPC.IsValid())WeakPC->CloseMissionPanel();});
  }
  else if (!bHost)
  {
   AddLine(TEXT("只有房主可以选择任务。"));
   AddButton(NSLOCTEXT("Mission", "Close", "返回 / Esc"), [WeakPC]() { if (WeakPC.IsValid()) WeakPC->CloseMissionPanel(); });
  }
  else
  {
   if (!Deploy->DeploymentError.IsEmpty()) AddLine(Deploy->DeploymentError);
   for (int32 i = 0; i < Deploy->GetDestinationCount(); ++i)
   {
    const auto D = Deploy->GetDestination(i);
    AddLine(D.Briefing.ToString(), 16);
    TWeakObjectPtr<AMissionDeployPoint> WeakDeploy(Deploy);
    AddButton(D.Title, [WeakPC, WeakDeploy, i]() { if (WeakPC.IsValid()) WeakPC->Server_SelectMission(WeakDeploy.Get(), i); });
   }
   AddButton(NSLOCTEXT("Mission", "Close", "返回 / Esc"), [WeakPC]() { if (WeakPC.IsValid()) WeakPC->CloseMissionPanel(); });
  }
  UpdateClock(); return;
 }
 const auto* GS = GetWorld()->GetGameState<ACoopCqbGameState>();
 if (!GS) return;
 if (Panel == EMissionPanel::Loading)
 {
  const FString Map=GetWorld()->GetMapName();
  if(ReconMap)ReconMap->SetText(FText::FromString(ShooterLoadingScreen::GetMapName(Map)));
  if(ReconTask)ReconTask->SetText(FText::FromString(ShooterLoadingScreen::GetTaskName(Map)));
  if(ReconCount)ReconCount->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"),GS->ReadyOperators,FMath::Max(1,GS->ExpectedOperators))));
  return;
 }
 const FMissionReport& R = GS->Report;
 const bool bDebrief = Panel == EMissionPanel::Debrief;
 AddLine(bDebrief ? TEXT("行动结算 / AFTER ACTION") : TEXT("任务平板 / OBJECTIVES"), 28);
 if (bDebrief)
 {
  AddLine(FString::Printf(TEXT("%s    评级 %s    得分 %d / %d"), GS->DidOperatorsWin() ? TEXT("任务成功") : TEXT("任务失败"), *R.Grade, R.Score, R.MaximumScore), 26);
  if (GS->GetWinReason() == ECoopCqbWinReason::LoadTimeout) AddLine(TEXT("队员加载超时，任务已取消。"));
  else if (GS->GetWinReason() == ECoopCqbWinReason::HostageLost) AddLine(TEXT("人质死亡，任务失败。"));
  else if (GS->GetWinReason() == ECoopCqbWinReason::TeamWiped) AddLine(TEXT("小队全员阵亡。"));
 }
 AddLine(FString::Printf(TEXT("01  标定任务     %d / %d    %s    +%d"), R.MarkedCompleted, R.MarkedTotal,
  R.bPrimaryComplete ? TEXT("现场已控制") : TEXT("控制现场并完成全部必要目标"), R.PrimaryPoints + R.MarkedPoints));
 AddLine(FString::Printf(TEXT("02  击倒嫌疑人   %d    +%d"), R.Neutralized, R.NeutralizedPoints));
 AddLine(FString::Printf(TEXT("03  活捉嫌疑人   %d    +%d    待处置 %d"), R.Arrested, R.ArrestedPoints, FMath::Max(0, R.SuspectsTotal - R.Neutralized - R.Arrested)));
 AddLine(FString::Printf(TEXT("04  解救人质     %d / %d    +%d"), R.Rescued, R.HostagesTotal, R.HostagePoints));
 AddLine(FString::Printf(TEXT("05  小队存活     %d / %d    +%d"), R.Survivors, R.OperatorsTotal, R.SurvivalPoints));
 AddLine(FString::Printf(TEXT("06  隐藏任务（可选）   已发现并完成 %d    +%d"), R.HiddenCompleted, R.HiddenPoints));
 if (R.Penalties) AddLine(FString::Printf(TEXT("行动扣分：-%d  / 人质损失 %d，击倒已投降嫌疑人 %d"), R.Penalties, R.HostagesLost, R.SurrenderedKilled));
 for (const FText& Description : GS->ObjectiveDescriptions) AddLine(Description.ToString(), 16);
 AddLine(TEXT("投降不等于拘捕：靠近按 F 拘捕。人质解救后原地蹲下。"), 16);
 if (bDebrief)
 {
  Rows->AddSlot().AutoHeight().Padding(0, 12) [ SAssignNew(Clock, STextBlock).Font(Panel==EMissionPanel::Tablet?FOperationUIStyle::MakeFont(20):FTacticalUIStyle::MakeFont(20)).ColorAndOpacity(Panel==EMissionPanel::Tablet?FOperationUIStyle::Text:FTacticalUIStyle::TextGold) ];
  AddLine(TEXT("结算展示结束后全队自动返回靶场。"), 16);
 }
 else AddButton(NSLOCTEXT("Mission", "CloseTablet", "收起平板 / Tab"), [WeakPC]() { if (WeakPC.IsValid()) WeakPC->CloseMissionPanel(); });
 UpdateClock();
}
void UMissionFlowWidget::UpdateClock()
{
 if (!Clock) return;
 const auto* GS = GetWorld()->GetGameState<ACoopCqbGameState>();
 const auto* TimeGS = GetWorld()->GetGameState();
 const double Now = TimeGS ? TimeGS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
 float Remaining = 0;
 if (Panel == EMissionPanel::Selection && Deploy) Remaining = Deploy->CountdownEndServerTime - Now;
 else if (GS) Remaining = GS->IsDebriefOpen() ? GS->ReturnEndServerTime - Now : GS->GetEvacSecondsRemaining();
 const FText Seconds = FText::AsNumber(FMath::Max(0, FMath::CeilToInt(Remaining)));
 Clock->SetText(FText::Format(Panel == EMissionPanel::Selection
  ? NSLOCTEXT("Mission", "DeploySeconds", "{0} 秒后出发") : NSLOCTEXT("Mission", "ReturnSeconds", "{0} 秒"), Seconds));
}
FReply UMissionFlowWidget::NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event)
{
 if (FirstButton) return FReply::Handled().SetUserFocus(FirstButton.ToSharedRef(), EFocusCause::SetDirectly);
 return Super::NativeOnFocusReceived(Geometry, Event);
}
FReply UMissionFlowWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
 if(Panel==EMissionPanel::Debrief){if(SkipMissionDebriefAnimation(DebriefScreen))return FReply::Handled();return Super::NativeOnPreviewKeyDown(Geometry,Event);}
 if(Panel==EMissionPanel::Selection&&Event.GetKey()==EKeys::Y)
 {
  if(!Event.IsRepeat())if(auto* PC=Cast<AShooterPlayerController>(GetOwningPlayer()))PC->ToggleMissionReady();
  return FReply::Handled();
 }
 if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Tab)
 {
  if (Panel == EMissionPanel::Tablet || Panel==EMissionPanel::Selection)
   if (auto* PC = Cast<AShooterPlayerController>(GetOwningPlayer())) PC->CloseMissionPanel();
  return FReply::Handled();
 }
 return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
