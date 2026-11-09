#include "Ui/MissionFlowWidget.h"
#include "Ui/TacticalUIStyle.h"
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

TSharedRef<SWidget> UMissionFlowWidget::RebuildWidget()
{
 SetIsFocusable(true);
 PanelBrush = FTacticalUIStyle::MakePanelBrush(FTacticalUIStyle::Panel, 12.f, 1.f);
 DimBrush = FTacticalUIStyle::MakePanelBrush(FTacticalUIStyle::BgDim, 0.f, 0.f);
 ButtonStyle = FTacticalUIStyle::MakeButtonStyle();
 return SNew(SBorder).BorderImage(&DimBrush).Padding(32)
 [
  SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
  [
   SNew(SBox).WidthOverride(860).HeightOverride(760)
   [
    SNew(SBorder).BorderImage(&PanelBrush).Padding(36)
    [ SNew(SScrollBox) + SScrollBox::Slot() [ SAssignNew(Rows, SVerticalBox) ] ]
   ]
  ]
 ];
}
void UMissionFlowWidget::ReleaseSlateResources(bool bReleaseChildren)
{
 Super::ReleaseSlateResources(bReleaseChildren); Rows.Reset(); Clock.Reset(); FirstButton.Reset();
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
 Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 14)
 [ SNew(STextBlock).Text(FText::FromString(Text)).Font(FTacticalUIStyle::MakeFont(Size, Size >= 24))
  .ColorAndOpacity(Size >= 24 ? FTacticalUIStyle::TextGold : FTacticalUIStyle::Text).AutoWrapText(true) ];
}
void UMissionFlowWidget::AddButton(const FText& Label, TFunction<void()> Action)
{
 TSharedPtr<SButton> NewButton;
 Rows->AddSlot().AutoHeight().Padding(0, 4)
 [ SAssignNew(NewButton, SButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(18, 12))
  .OnClicked_Lambda([Action=MoveTemp(Action)]() { Action(); return FReply::Handled(); })
  [ SNew(STextBlock).Text(Label).Font(FTacticalUIStyle::MakeFont(18)).ColorAndOpacity(FTacticalUIStyle::Text) ] ];
 if (!FirstButton) FirstButton = NewButton;
}
void UMissionFlowWidget::Refresh()
{
 if (!Rows) return;
 Rows->ClearChildren(); Clock.Reset(); FirstButton.Reset();
 auto* PC = Cast<AShooterPlayerController>(GetOwningPlayer());
 TWeakObjectPtr<AShooterPlayerController> WeakPC(PC);
 if (Panel == EMissionPanel::Selection)
 {
  AddLine(TEXT("行动部署 / MISSION SELECT"), 28);
  if (!Deploy) { AddLine(TEXT("任务台不可用")); return; }
  if (Deploy->CountdownEndServerTime > 0)
  {
   AddLine(Deploy->GetDestination(Deploy->SelectedMission).Title.ToString(), 24);
   AddLine(TEXT("全队即将出发。倒计时结束后开始加载，所有队员就绪后任务才会开始。"));
   Rows->AddSlot().AutoHeight().Padding(0, 12) [ SAssignNew(Clock, STextBlock).Font(FTacticalUIStyle::MakeFont(36, true)).ColorAndOpacity(FTacticalUIStyle::TextGold) ];
   TWeakObjectPtr<AMissionDeployPoint> WeakDeploy(Deploy);
   AddButton(NSLOCTEXT("Mission", "CancelDeploy", "取消出发（发起人）"), [WeakPC, WeakDeploy]() { if (WeakPC.IsValid()) WeakPC->Server_CancelMissionDeploy(WeakDeploy.Get()); });
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
  AddLine(TEXT("行动准备 / SQUAD LOADING"), 28);
  AddLine(GS->GetCoopPhase() == ECoopCqbPhase::Briefing ? TEXT("全员加载完成，正在准备行动…") : GS->MissionStatus, 24);
  AddLine(TEXT("等待地图、角色及场景流送就绪。准备完成前禁止移动与开火。"));
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
  Rows->AddSlot().AutoHeight().Padding(0, 12) [ SAssignNew(Clock, STextBlock).Font(FTacticalUIStyle::MakeFont(20)).ColorAndOpacity(FTacticalUIStyle::TextGold) ];
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
 if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Tab)
 {
  if (Panel == EMissionPanel::Tablet || (Panel == EMissionPanel::Selection && Deploy && Deploy->CountdownEndServerTime < 0))
   if (auto* PC = Cast<AShooterPlayerController>(GetOwningPlayer())) PC->CloseMissionPanel();
  return FReply::Handled();
 }
 return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
