#include "ShooterPlayerController.h"
#include "Ui/MissionFlowWidget.h"
#include "World/MissionDeployPoint.h"
#include "Game/CoopCqbGameMode.h"
#include "Game/CoopCqbGameState.h"
#include "CombatComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Engine/LevelStreaming.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

void AShooterPlayerController::InitializeMissionInput()
{
 if (!IsLocalController()) return;
 if (!TabletAction)
 {
  TabletAction = NewObject<UInputAction>(this, TEXT("IA_MissionTablet"));
  TabletAction->ValueType = EInputActionValueType::Boolean;
  TabletIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_MissionTablet"));
  TabletIMC->MapKey(TabletAction, EKeys::Tab);
 }
 if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
  if (!Subsystem->HasMappingContext(TabletIMC)) Subsystem->AddMappingContext(TabletIMC, 5);
 if (auto* Input = Cast<UEnhancedInputComponent>(InputComponent))
  Input->BindAction(TabletAction, ETriggerEvent::Started, this, &ThisClass::ToggleMissionTablet);
}
void AShooterPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
 GetWorldTimerManager().ClearTimer(MissionServiceTimer);
 if (BoundMissionState) BoundMissionState->OnMissionUpdated.RemoveDynamic(this, &ThisClass::OnMissionStateUpdated);
 if (MissionWidget) { MissionWidget->RemoveFromParent(); MissionWidget = nullptr; }
 if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
  if (TabletIMC) Subsystem->RemoveMappingContext(TabletIMC);
 Super::EndPlay(EndPlayReason);
}
void AShooterPlayerController::ServiceMissionReadiness()
{
 if (!IsLocalController()) return;
 auto* GS = GetWorld()->GetGameState<ACoopCqbGameState>();
 if (!GS) return;
 if (BoundMissionState != GS)
 {
  if (BoundMissionState) BoundMissionState->OnMissionUpdated.RemoveDynamic(this, &ThisClass::OnMissionStateUpdated);
  BoundMissionState = GS;
  GS->OnMissionUpdated.AddUniqueDynamic(this, &ThisClass::OnMissionStateUpdated);
  OnMissionStateUpdated();
 }
 if (GS->bRangeHub) { GetWorldTimerManager().ClearTimer(MissionServiceTimer); return; }
 if (bMissionLoadAcknowledged || !GetPawn() || !GetPawn()->HasActorBegunPlay() || IsAsyncLoading()) return;
 for (const ULevelStreaming* Level : GetWorld()->GetStreamingLevels())
 {
  if (!Level) continue;
  if (Level->ShouldBeLoaded() && !Level->IsLevelLoaded()) return;
  if (Level->ShouldBeVisible() && !Level->IsLevelVisible()) return;
 }
 if (auto* WP = GetWorld()->GetSubsystem<UWorldPartitionSubsystem>()) if (!WP->IsAllStreamingCompleted()) return;
 bMissionLoadAcknowledged = true;
 Server_MissionLoaded();
 GetWorldTimerManager().ClearTimer(MissionServiceTimer);
}
void AShooterPlayerController::Server_MissionLoaded_Implementation()
{
 if (auto* GM = GetWorld()->GetAuthGameMode<ACoopCqbGameMode>()) GM->Auth_OperatorReady(this);
}
void AShooterPlayerController::OnMissionStateUpdated()
{
 if (!IsLocalController() || !BoundMissionState) return;
 auto* GS = BoundMissionState.Get();
 if (GS->bRangeHub)
 {
  if (MissionWidget && MissionWidget->Panel == EMissionPanel::Loading) CloseMissionPanel();
  return;
 }
 if (GS->GetCoopPhase() == ECoopCqbPhase::Waiting || GS->GetCoopPhase() == ECoopCqbPhase::Briefing)
  ShowMissionPanel(uint8(EMissionPanel::Loading));
 else if (GS->GetCoopPhase() == ECoopCqbPhase::PostMission)
  ShowMissionPanel(uint8(EMissionPanel::Debrief));
 else if (MissionWidget && MissionWidget->Panel == EMissionPanel::Loading) CloseMissionPanel();
 else if (MissionWidget) MissionWidget->Refresh();
}
void AShooterPlayerController::ToggleMissionTablet()
{
 if (MissionWidget)
 {
  if (MissionWidget->Panel == EMissionPanel::Tablet) CloseMissionPanel();
  return;
 }
 const auto* GS = GetWorld()->GetGameState<ACoopCqbGameState>();
 if (!GS || GS->bRangeHub || !GS->IsMissionLive() || bRangeBenchOpen) return;
 ShowMissionPanel(uint8(EMissionPanel::Tablet));
}
void AShooterPlayerController::ShowMissionPanel(uint8 Panel, AMissionDeployPoint* Deploy)
{
 if (!IsLocalController()) return;
 if (bRangeBenchOpen) LeaveRangeBench();
 const auto NewPanel = EMissionPanel(Panel);
 if (MissionWidget && MissionWidget->Panel == NewPanel && MissionWidget->Deploy == Deploy)
 {
  MissionWidget->Refresh(); return;
 }
 if (!MissionWidget)
 {
  MissionWidget = CreateWidget<UMissionFlowWidget>(this, UMissionFlowWidget::StaticClass());
  if (!MissionWidget) return;
  MissionWidget->Configure(NewPanel, Deploy);
  MissionWidget->AddToViewport(60);
 }
 else MissionWidget->Configure(NewPanel, Deploy);
 if (!bMissionInputMuted)
 {
  SetIgnoreMoveInput(true); SetIgnoreLookInput(true); bMissionInputMuted = true;
  StopSprinting();
  if (APawn* ControlledPawn = GetPawn())
  {
   if (auto* Combat = UCombatComponent::FindCombatComponent(ControlledPawn))
   {
    Combat->Initiate_FireWeapon_Released(); Combat->Initiate_Aim_Released();
   }
   ControlledPawn->DisableInput(this);
   if (auto* Char = Cast<ACharacter>(ControlledPawn)) Char->GetCharacterMovement()->StopMovementImmediately();
  }
 }
 FInputModeGameAndUI Mode;
 Mode.SetWidgetToFocus(MissionWidget->TakeWidget());
 Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
 Mode.SetHideCursorDuringCapture(false);
 SetInputMode(Mode); bShowMouseCursor = true;
}
void AShooterPlayerController::CloseMissionPanel()
{
 if (!MissionWidget) return;
 MissionWidget->RemoveFromParent(); MissionWidget = nullptr;
 if (bMissionInputMuted)
 {
  SetIgnoreMoveInput(false); SetIgnoreLookInput(false); bMissionInputMuted = false;
  if (APawn* ControlledPawn = GetPawn()) if (bPawnAlive && !bMatchOver) ControlledPawn->EnableInput(this);
 }
 SetInputMode(FInputModeGameOnly()); bShowMouseCursor = false;
}
void AShooterPlayerController::Client_OpenMissionSelection_Implementation(AMissionDeployPoint* Deploy)
{
 if (IsValid(Deploy)) ShowMissionPanel(uint8(EMissionPanel::Selection), Deploy);
}
void AShooterPlayerController::Server_SelectMission_Implementation(AMissionDeployPoint* Deploy, int32 Index)
{
 if (IsValid(Deploy) && bPawnAlive) Deploy->Auth_SelectMission(this, Index);
}
void AShooterPlayerController::Server_CancelMissionDeploy_Implementation(AMissionDeployPoint* Deploy)
{
 if (IsValid(Deploy)) Deploy->Auth_Cancel(this);
}
