#include "ShooterPlayerController.h"
#include "Ui/DeploymentDroneComponent.h"
#include "Camera/CameraActor.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Ui/MissionFlowWidget.h"
#include "Ui/MissionTabletActor.h"
#include "Ui/SettingsScreen.h"
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
#include "EngineUtils.h"
#include "Character/PoliceAppearanceComponent.h"
#include "ShooterCharacter.h"
#include "ShooterGameInstance.h"

void AShooterPlayerController::Server_DebriefAction_Implementation(bool bRetry)
{
 if(!HasAuthority())return;
 GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this,bRetry]()
 {
  if(!HasAuthority()||!IsLocalController())return;
  auto* GS=GetWorld()->GetGameState<ACoopCqbGameState>();
  auto* GM=GetWorld()->GetAuthGameMode<ACoopCqbGameMode>();
  if(!GS||!GM||!GS->IsDebriefOpen())return;
  if(bRetry)GM->Auth_RetryMission();else GM->Auth_ReturnToRange();
 }));
}

void AShooterPlayerController::Server_SetPoliceProtection_Implementation(EPoliceFaceProtection Face,EBodyArmorType Armor)
{
 if(!HasAuthority())return;
 if(auto* Operator=Cast<AShooterCharacter>(GetPawn());Operator&&Operator->PoliceAppearance)
  Operator->PoliceAppearance->Server_SetProtection_Implementation(Face,Armor);
}

#if WITH_EDITOR
static TAutoConsoleVariable<float> UIReadyDelay(TEXT("fps.UI.ReadyDelay"),0.f,TEXT("QA only: local readiness delay in seconds."),ECVF_Cheat);
static TAutoConsoleVariable<float> UIReadyDelayClient(TEXT("fps.UI.ReadyDelayClient"),-1.f,TEXT("QA: override remote client's local readiness delay."),ECVF_Cheat);
static TAutoConsoleVariable<float> UIReadyDelayListen(TEXT("fps.UI.ReadyDelayListen"),-1.f,TEXT("QA: override listen host's local readiness delay."),ECVF_Cheat);
#endif

void AShooterPlayerController::InitializeMissionInput()
{
 if (!IsLocalController()) return;
 if (!TabletAction)
 {
  TabletAction = NewObject<UInputAction>(this, TEXT("IA_MissionTablet"));
  TabletAction->ValueType = EInputActionValueType::Boolean;
  TabletIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_MissionTablet"));
  TabletIMC->MapKey(TabletAction, EKeys::Tab);
  MissionReadyAction=NewObject<UInputAction>(this,TEXT("IA_MissionReady"));
  MissionReadyAction->ValueType=EInputActionValueType::Boolean;
  TabletIMC->MapKey(MissionReadyAction,EKeys::Y);
  TabletScrollUpAction=NewObject<UInputAction>(this,TEXT("IA_TabletScrollUp"));
  TabletScrollDownAction=NewObject<UInputAction>(this,TEXT("IA_TabletScrollDown"));
  TabletScrollUpAction->ValueType=TabletScrollDownAction->ValueType=EInputActionValueType::Boolean;
  TabletScrollUpAction->bConsumeInput=TabletScrollDownAction->bConsumeInput=false;
  TabletIMC->MapKey(TabletScrollUpAction,EKeys::MouseScrollUp);
  TabletIMC->MapKey(TabletScrollUpAction,EKeys::PageUp);
  TabletIMC->MapKey(TabletScrollDownAction,EKeys::MouseScrollDown);
  TabletIMC->MapKey(TabletScrollDownAction,EKeys::PageDown);
 }
 if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
  if (!Subsystem->HasMappingContext(TabletIMC)) Subsystem->AddMappingContext(TabletIMC, 5);
 if (auto* Input = Cast<UEnhancedInputComponent>(InputComponent))
 {
  Input->BindAction(TabletAction, ETriggerEvent::Started, this, &ThisClass::ToggleMissionTablet);
  Input->BindAction(MissionReadyAction,ETriggerEvent::Started,this,&ThisClass::ToggleMissionReady);
  Input->BindAction(TabletScrollUpAction,ETriggerEvent::Started,this,&ThisClass::ScrollTabletUp);
  Input->BindAction(TabletScrollDownAction,ETriggerEvent::Started,this,&ThisClass::ScrollTabletDown);
 }
}
void AShooterPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
 GetWorldTimerManager().ClearTimer(MissionServiceTimer);
 if (BoundMissionState) BoundMissionState->OnMissionUpdated.RemoveDynamic(this, &ThisClass::OnMissionStateUpdated);
 if (MissionWidget) { MissionWidget->RemoveFromParent(); MissionWidget = nullptr; }
 if (MissionTablet) { MissionTablet->Close(false); MissionTablet = nullptr; }
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
 float Delay=0;
#if WITH_EDITOR
 Delay=UIReadyDelay.GetValueOnGameThread();
 const float RoleDelay=GetNetMode()==NM_Client?UIReadyDelayClient.GetValueOnGameThread():UIReadyDelayListen.GetValueOnGameThread();
 if(RoleDelay>=0)Delay=RoleDelay;
#endif
 if(FParse::Param(FCommandLine::Get(),TEXT("UIQA")))FParse::Value(FCommandLine::Get(),TEXT("UIReadyDelay="),Delay);
 if(GetWorld()->GetTimeSeconds()<FMath::Max(0.f,Delay))return;
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
 if(MissionTablet)
 {
  if(!GS->IsMissionLive())CloseMissionPanel();
  else MissionTablet->RefreshMission();
 }
 if (GS->bRangeHub)
 {
  if (MissionWidget && MissionWidget->Panel == EMissionPanel::Loading) CloseMissionPanel();
  return;
 }
 if (GS->GetCoopPhase() == ECoopCqbPhase::Waiting || GS->GetCoopPhase() == ECoopCqbPhase::Briefing)
 {
  ShowMissionPanel(uint8(EMissionPanel::Loading));
  if(DeploymentDrone&&DeploymentDrone->HasLeftRecon())HideDeploymentOverlayForRaise();
 }
 else if (GS->GetCoopPhase() == ECoopCqbPhase::PostMission)
  ShowMissionPanel(uint8(EMissionPanel::Debrief));
 else if (MissionWidget && MissionWidget->Panel == EMissionPanel::Loading) CloseMissionPanel();
 else if (MissionWidget) MissionWidget->Refresh();
}
void AShooterPlayerController::ToggleMissionTablet()
{
 if(MissionTablet){MissionTablet->Close();return;}
 if (MissionWidget)
 {
  if (MissionWidget->Panel == EMissionPanel::Tablet) CloseMissionPanel();
  return;
 }
 const auto* GS = GetWorld()->GetGameState<ACoopCqbGameState>();
 if (!GS || GS->bRangeHub || !GS->IsMissionLive() || !bPawnAlive || bMatchOver || bSpectating || bRangeBenchOpen || IsPauseMenuOpen() || (SettingsScreen && SettingsScreen->IsInViewport())) return;
 ShowMissionPanel(uint8(EMissionPanel::Tablet));
}
void AShooterPlayerController::ScrollTabletUp(){if(MissionTablet)MissionTablet->ScrollBriefing(-160);}
void AShooterPlayerController::ScrollTabletDown(){if(MissionTablet)MissionTablet->ScrollBriefing(160);}
void AShooterPlayerController::ShowMissionPanel(uint8 Panel, AMissionDeployPoint* Deploy)
{
 if (!IsLocalController()) return;
 if (bRangeBenchOpen) LeaveRangeBench();
 const auto NewPanel = EMissionPanel(Panel);
 if(NewPanel==EMissionPanel::Debrief)EnsureCombatHudKeepAlive();
 if(NewPanel==EMissionPanel::Tablet)
 {
  if(MissionTablet){MissionTablet->RefreshMission();return;}
  FActorSpawnParameters Spawn;Spawn.Owner=GetPawn();Spawn.ObjectFlags|=RF_Transient;
  Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  MissionTablet=GetWorld()->SpawnActor<AMissionTabletActor>(AMissionTabletActor::StaticClass(),FTransform::Identity,Spawn);
  if(!MissionTablet||!MissionTablet->OpenFor(this))
  {
   if(MissionTablet)MissionTablet->Close(false);
   MissionTablet=nullptr;return;
  }
 }
 else if(MissionTablet)CloseMissionPanel();
 if(NewPanel!=EMissionPanel::Tablet)
 {
 if(MissionWidget&&MissionWidget->Panel!=NewPanel){MissionWidget->RemoveFromParent();MissionWidget=nullptr;}
 if (MissionWidget && MissionWidget->Panel == NewPanel && MissionWidget->Deploy == Deploy)
 {
  MissionWidget->Refresh(); return;
 }
 if (!MissionWidget)
 {
  MissionWidget = CreateWidget<UMissionFlowWidget>(this, UMissionFlowWidget::StaticClass());
  if (!MissionWidget) return;
  MissionWidget->Configure(NewPanel, Deploy);
  MissionWidget->AddToViewport(400);
 }
 else MissionWidget->Configure(NewPanel, Deploy);
 }
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
 if(NewPanel==EMissionPanel::Tablet)
 {
  SetInputMode(FInputModeGameOnly());bShowMouseCursor=false;
  return;
 }
 FInputModeGameAndUI Mode;
 Mode.SetWidgetToFocus(MissionWidget->TakeWidget());
 Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
 Mode.SetHideCursorDuringCapture(false);
 SetInputMode(Mode); bShowMouseCursor = true;
}
void AShooterPlayerController::CloseMissionPanel()
{
 if (!MissionWidget && !MissionTablet) return;
 if(MissionTablet){MissionTablet->Close(false);MissionTablet=nullptr;}
 if(MissionWidget){MissionWidget->RemoveFromParent();MissionWidget=nullptr;}
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
void AShooterPlayerController::ToggleMissionReady()
{
 if(!IsLocalController())return;
 if(MissionWidget&&MissionWidget->Panel==EMissionPanel::Selection&&IsValid(MissionWidget->Deploy))
 {
  Server_ToggleMissionReady(MissionWidget->Deploy);return;
 }
 for(TActorIterator<AMissionDeployPoint> It(GetWorld());It;++It)
  if(It->SelectedMission!=INDEX_NONE){Server_ToggleMissionReady(*It);return;}
}
void AShooterPlayerController::Server_ToggleMissionReady_Implementation(AMissionDeployPoint* Deploy)
{
 if(bPawnAlive&&IsValid(Deploy))Deploy->Auth_ToggleReady(this);
}

void AShooterPlayerController::Server_DeploymentViewReady_Implementation()
{
 if(auto* GM=GetWorld()->GetAuthGameMode<ACoopCqbGameMode>())GM->Auth_DeploymentViewReady(this);
}
void AShooterPlayerController::HideDeploymentOverlayForRaise()
{
 if(MissionWidget&&MissionWidget->Panel==EMissionPanel::Loading)MissionWidget->SetVisibility(ESlateVisibility::Collapsed);
}
void AShooterPlayerController::LockDeploymentPresentation()
{
 if(!IsLocalController())return;
 const auto* GS=GetWorld()->GetGameState<ACoopCqbGameState>();
 if(!GS||GS->bRangeHub||!GetWorld()->GetMapName().Contains(TEXT("Factory"))||GS->IsMissionLive()||GS->GetCoopPhase()==ECoopCqbPhase::PostMission)return;
 ShowMissionPanel(uint8(EMissionPanel::Loading));
 // ClientRestart resets these native counters after initial possession. Restore
 // our existing lock without adding a second contribution to the counter.
 if(!IsMoveInputIgnored())SetIgnoreMoveInput(true);
 if(!IsLookInputIgnored())SetIgnoreLookInput(true);
 if(GetPawn())GetPawn()->DisableInput(this);
 if(DeploymentDrone&&DeploymentDrone->HasLeftRecon())HideDeploymentOverlayForRaise();
}
void AShooterPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
 Super::ClientRestart_Implementation(NewPawn);LockDeploymentPresentation();
}
void AShooterPlayerController::AutoManageActiveCameraTarget(AActor* Suggested)
{
 if(DeploymentDrone&&!DeploymentDrone->HasLeftRecon()&&DeploymentDrone->GetDroneCamera())
  Super::AutoManageActiveCameraTarget(DeploymentDrone->GetDroneCamera());
 else Super::AutoManageActiveCameraTarget(Suggested);
}
