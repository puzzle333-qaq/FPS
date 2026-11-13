#include "Ui/DeploymentDroneComponent.h"
#include "ShooterPlayerController.h"
#include "ShooterCharacter.h"
#include "CombatComponent.h"
#include "Game/CoopCqbGameState.h"
#include "Game/HostileAICharacter.h"
#include "HAL/IConsoleManager.h"
#include "Vision/NightVisionProfile.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneObjectBindingID.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "TimerManager.h"

namespace {
 const FVector WP11(4400,2000,1800),WP12(4400,800,300),Door(4400,0,220);
 int32 MessageUsers=0;bool SavedMessages=true;
 FRotator LookAtDoor(const FVector& P){return (Door-P).Rotation();}
#if WITH_EDITOR
 FAutoConsoleCommandWithWorld SpawnQATarget(TEXT("fps.UI.SpawnQATarget"),TEXT("Editor QA: spawn a replicated target to check deployment visibility."),
  FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World){
   if(!World||!World->IsGameWorld()||World->GetNetMode()==NM_Client)return;
   UClass* Class=LoadClass<AHostileAICharacter>(nullptr,TEXT("/Game/FPS/Coop/BP_Hostile.BP_Hostile_C"));
   FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   if(auto* Actor=World->SpawnActor<AHostileAICharacter>(Class,FVector(5800,-4100,120),FRotator::ZeroRotator,P))Actor->Tags.Add(TEXT("UIQA_Target"));
  }));
#endif
}
UDeploymentDroneComponent::UDeploymentDroneComponent()
{
 PrimaryComponentTick.bCanEverTick=true;
 PrimaryComponentTick.TickGroup=TG_PrePhysics;
}
float UDeploymentDroneComponent::GetLoopSeconds()const
{
 return SequencePlayer?FMath::Fmod(float(SequencePlayer->GetCurrentTime().AsSeconds()),13.5f):0.f;
}
void UDeploymentDroneComponent::SetPhase(EDeploymentViewPhase Next)
{
 Phase=Next;PhaseTime=0;
 const FString Name=StaticEnum<EDeploymentViewPhase>()->GetNameStringByValue(int64(Next));
 UE_LOG(LogTemp,Log,TEXT("[DeploymentView] %s phase=%s time=%.3f"),*GetNameSafe(GetOwner()),*Name,GetWorld()->GetTimeSeconds());
 // Opt-in packaged-build evidence; no dependency on Shipping console/log support.
 if(FParse::Param(FCommandLine::Get(),TEXT("UIQA")))
 {
  const FVector P=DroneCamera?DroneCamera->GetActorLocation():FVector::ZeroVector;
  const FString Line=FString::Printf(TEXT("{\"event\":\"%s\",\"time\":%.3f,\"net_mode\":%d,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f}\n"),*Name,GetWorld()->GetTimeSeconds(),int32(GetNetMode()),P.X,P.Y,P.Z);
  const FString File=FPaths::ProjectSavedDir()/FString::Printf(TEXT("UIQA_Deployment_%u.jsonl"),FPlatformProcess::GetCurrentProcessId());
  FFileHelper::SaveStringToFile(Line,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
  if(Next==EDeploymentViewPhase::Orbit||Next==EDeploymentViewPhase::Raise)
   FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("UIQA_%u_%s.png"),FPlatformProcess::GetCurrentProcessId(),*Name),true,false);
 }
}
void UDeploymentDroneComponent::StartOrbit(AShooterPlayerController* PC)
{
 ULevelSequence* Asset=LoadObject<ULevelSequence>(nullptr,TEXT("/Game/FPS/UI/Sequences/LS_Factory_Recon_V3.LS_Factory_Recon_V3"));
 if(!Asset||!Asset->GetMovieScene()||Asset->GetMovieScene()->GetBindings().Num()==0)return;
 FActorSpawnParameters Spawn;Spawn.Owner=PC;Spawn.ObjectFlags|=RF_Transient;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 DroneCamera=GetWorld()->SpawnActor<ACameraActor>(FVector(-1000,-4800,1800),FRotator(-27.4183f,28.6105f,0),Spawn);
 if(!DroneCamera)return;
 DroneCamera->GetCameraComponent()->FieldOfView=70;
 DroneCamera->GetCameraComponent()->bConstrainAspectRatio=false;
 FMovieSceneSequencePlaybackSettings Settings;Settings.LoopCount.Value=-1;Settings.bDisableCameraCuts=true;
 Settings.FinishCompletionStateOverride=EMovieSceneCompletionModeOverride::ForceKeepState;
 ALevelSequenceActor* Actor=nullptr;
 SequencePlayer=ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(),Asset,Settings,Actor);SequenceActor=Actor;
 if(!SequencePlayer||!SequenceActor){DroneCamera->Destroy();DroneCamera=nullptr;return;}
 const FGuid ID=Asset->GetMovieScene()->GetBindings()[0].GetObjectGuid();
 SequenceActor->SetBinding(FMovieSceneObjectBindingID(UE::MovieScene::FFixedObjectBindingID(ID,MovieSceneSequenceID::Root)),{DroneCamera},false);
 SequencePlayer->PlayLooping(-1);
 PC->SetViewTarget(DroneCamera);
 PC->LockDeploymentPresentation();
 if(MessageUsers++==0){SavedMessages=GAreScreenMessagesEnabled;GAreScreenMessagesEnabled=false;}
 bScreenMessagesReserved=true;
 SetPhase(EDeploymentViewPhase::Orbit);
 UpdateVision(PC);
}
void UDeploymentDroneComponent::UpdateVision(AShooterPlayerController* PC)
{
 if(!DroneCamera)return;
 UCameraComponent* Camera=DroneCamera->GetCameraComponent();Camera->PostProcessBlendWeight=1;
 if(auto* Pawn=Cast<AShooterCharacter>(PC->GetPawn()))
 {
  if(Pawn->GetTacticalVisionMode()!=ETacticalVisionMode::NVG)Pawn->SetTacticalVisionMode(ETacticalVisionMode::NVG);
  if(UCameraComponent* PlayerCamera=Pawn->FindComponentByClass<UCameraComponent>())
  {Camera->PostProcessSettings=PlayerCamera->PostProcessSettings;return;}
 }
 // Before possession, use the very same profile and material as the first-person NVG.
 NightVisionProfile::Apply(Camera->PostProcessSettings);
 if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/FPS/Materials/M_NVG_Phosphor.M_NVG_Phosphor")))Camera->AddOrUpdateBlendable(Material,1);
}
void UDeploymentDroneComponent::StartReturn()
{
 if(SequencePlayer)SequencePlayer->Pause();
 ReturnFrom=DroneCamera->GetActorLocation();ReturnRotation=DroneCamera->GetActorRotation();
 ReturnDuration=FMath::GetMappedRangeValueClamped(FVector2D(0,8000),FVector2D(1.5,2),FVector::Distance(ReturnFrom,WP11));
 SetPhase(EDeploymentViewPhase::Return);
}
void UDeploymentDroneComponent::TickComponent(float D,ELevelTick Type,FActorComponentTickFunction* Tick)
{
 Super::TickComponent(D,Type,Tick);
 auto* PC=Cast<AShooterPlayerController>(GetOwner());if(!PC||!PC->IsLocalController())return;
 const auto* GS=GetWorld()->GetGameState<ACoopCqbGameState>();
 if(!GS||GS->bRangeHub||!GetWorld()->GetMapName().Contains(TEXT("Factory")))return;
 const ECoopCqbPhase GamePhase=GS->GetCoopPhase();
 if(GamePhase==ECoopCqbPhase::PostMission){if(Phase!=EDeploymentViewPhase::Inactive&&Phase!=EDeploymentViewPhase::Complete){SetPhase(EDeploymentViewPhase::Complete);Cleanup(PC);}return;}
 if(GamePhase==ECoopCqbPhase::InProgress){if(Phase!=EDeploymentViewPhase::Inactive&&Phase!=EDeploymentViewPhase::Complete){SetPhase(EDeploymentViewPhase::Complete);Cleanup(PC);}return;}
 if(Phase==EDeploymentViewPhase::Inactive)StartOrbit(PC);
 if(!DroneCamera)return;
 PhaseTime+=D;
 if(!HasLeftRecon())
 {
  if(PC->GetViewTarget()!=DroneCamera)PC->SetViewTarget(DroneCamera);
  UpdateVision(PC);
 }
 if(Phase==EDeploymentViewPhase::Orbit)
 {
  if(GamePhase==ECoopCqbPhase::Briefing&&PC->IsMissionSceneReady())StartReturn();
 }
 else if(Phase==EDeploymentViewPhase::Return)
 {
  const float A=FMath::Clamp(PhaseTime/ReturnDuration,0.f,1.f),S=FMath::SmoothStep(0.f,1.f,A);
  const FVector P=FMath::Lerp(ReturnFrom,WP11,S);
  const FQuat R=FQuat::Slerp(ReturnRotation.Quaternion(),LookAtDoor(WP11).Quaternion(),S);
  DroneCamera->SetActorLocationAndRotation(P,R);
  if(A>=1)SetPhase(EDeploymentViewPhase::Descend);
 }
 else if(Phase==EDeploymentViewPhase::Descend)
 {
  const float A=FMath::Clamp(PhaseTime/1.5f,0.f,1.f),S=FMath::SmoothStep(0.f,1.f,A);
  const FVector P=FMath::Lerp(WP11,WP12,S);DroneCamera->SetActorLocationAndRotation(P,LookAtDoor(P));
  if(A>=1)SetPhase(EDeploymentViewPhase::Signal);
 }
 else if(Phase==EDeploymentViewPhase::Signal)
 {
  DroneCamera->GetCameraComponent()->PostProcessSettings.FilmGrainIntensity=1.f;
  if(PhaseTime>=.15f&&!bBlackFrame){PC->PlayerCameraManager->StartCameraFade(0,1,.01f,FLinearColor::Black,false,true);bBlackFrame=true;}
  if(PhaseTime>=.3f)
  {
   PC->HideDeploymentOverlayForRaise();
   if(SequencePlayer)SequencePlayer->Stop();
   PC->SetViewTarget(PC->GetPawn());
   PC->PlayerCameraManager->StartCameraFade(1,0,.08f,FLinearColor::Black,false,false);
   SetPhase(EDeploymentViewPhase::Raise);
  }
 }
 else if(Phase==EDeploymentViewPhase::Raise)
 {
  if(PC->GetPawn()&&PC->GetViewTarget()!=PC->GetPawn())PC->SetViewTarget(PC->GetPawn());
  UCombatComponent* Combat=UCombatComponent::FindCombatComponent(PC->GetPawn());
  if(!Combat)return;
  if(!bRaiseStarted){bRaiseStarted=Combat->BeginDeploymentRaise();if(bRaiseStarted)PhaseTime=0;}
  if(bRaiseStarted&&PhaseTime>.05f&&!Combat->IsDeploymentRaisePlaying())
  {SetPhase(EDeploymentViewPhase::AwaitingMission);PC->Server_DeploymentViewReady();}
 }
 else if(Phase==EDeploymentViewPhase::AwaitingMission&&PhaseTime>.5f)
 {
  // Safe when the loading acknowledgement/replication arrives after the first request.
  PhaseTime=0;PC->Server_DeploymentViewReady();
 }
}
void UDeploymentDroneComponent::Cleanup(AShooterPlayerController* PC,bool bRestorePlayerView)
{
 if(IsValid(SequencePlayer))SequencePlayer->Stop();SequencePlayer=nullptr;
 if(bRestorePlayerView&&IsValid(PC)&&IsValid(PC->PlayerCameraManager))
 {
  if(PC->GetPawn())PC->SetViewTarget(PC->GetPawn());
  if(bBlackFrame)PC->PlayerCameraManager->StopCameraFade();
  if(FParse::Param(FCommandLine::Get(),TEXT("UIQA")))
   GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(PC,[PC]{
    const FString Line=FString::Printf(TEXT("{\"event\":\"Gameplay\",\"own_pawn_view\":%s,\"move_locked\":%s,\"look_locked\":%s}\n"),
     PC->GetViewTarget()==PC->GetPawn()?TEXT("true"):TEXT("false"),PC->IsMoveInputIgnored()?TEXT("true"):TEXT("false"),PC->IsLookInputIgnored()?TEXT("true"):TEXT("false"));
    FFileHelper::SaveStringToFile(Line,*(FPaths::ProjectSavedDir()/FString::Printf(TEXT("UIQA_Deployment_%u.jsonl"),FPlatformProcess::GetCurrentProcessId())),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("UIQA_%u_Gameplay.png"),FPlatformProcess::GetCurrentProcessId()),true,false);
   }));
 }
 bBlackFrame=false;
 if(IsValid(SequenceActor))SequenceActor->Destroy();SequenceActor=nullptr;
 if(IsValid(DroneCamera))DroneCamera->Destroy();DroneCamera=nullptr;
 if(bScreenMessagesReserved){bScreenMessagesReserved=false;if(--MessageUsers==0)GAreScreenMessagesEnabled=SavedMessages;}
}
void UDeploymentDroneComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 Cleanup(Cast<AShooterPlayerController>(GetOwner()),false);Super::EndPlay(Reason);
}
