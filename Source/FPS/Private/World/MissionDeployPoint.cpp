#include "World/MissionDeployPoint.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "ShooterGameInstance.h"
#include "ShooterPlayerController.h"
#include "Game/CoopCqbGameState.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "GameFramework/PlayerState.h"
#include "Game/MissionRadioComponent.h"
#include "Ui/ShooterLoadingScreen.h"
#include "Misc/ConfigCacheIni.h"

AMissionDeployPoint::AMissionDeployPoint()
{
	PrimaryActorTick.bCanEverTick = false;
 BriefingRadio=CreateDefaultSubobject<UMissionRadioComponent>(TEXT("BriefingRadio"));
 bReplicates = true;
 bAlwaysRelevant = true;

	InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
	SetRootComponent(InteractBox);
	InteractBox->SetBoxExtent(FVector(90.f, 50.f, 45.f));
	InteractBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractBox->SetCanEverAffectNavigation(false);

	TableMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TableMesh"));
	TableMesh->SetupAttachment(InteractBox);
	TableMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Prompt = NSLOCTEXT("MissionDeploy", "Prompt", "按 F 选择任务");
	MissionMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/FPS/Level/Testmap.Testmap")));
	MissionGameMode = TSoftClassPtr<AGameModeBase>(FSoftObjectPath(TEXT("/Script/FPS.CoopCqbGameMode")));
}

bool AMissionDeployPoint::IsRoomHost(const APlayerController* PC) const
{
	const UWorld* World = GetWorld();
	if (!World || !IsValid(PC)) return false;
	if (World->GetNetMode() == NM_Standalone) return PC->IsLocalController();
	return World->GetNetMode() != NM_Client && PC->IsLocalController();
}

bool AMissionDeployPoint::CanInteract_Implementation(APawn* InteractingPawn) const
{
	const APlayerController* PC = IsValid(InteractingPawn) ? Cast<APlayerController>(InteractingPawn->GetController()) : nullptr;
	return IsRoomHost(PC) && GetDestinationCount() > 0;
}

FText AMissionDeployPoint::GetInteractPrompt_Implementation(APawn* InteractingPawn) const
{
	return IsValid(InteractingPawn) ? Prompt : FText::GetEmpty();
}

void AMissionDeployPoint::Interact_Implementation(APawn* InteractingPawn)
{
	if (!HasAuthority() || !IsValid(InteractingPawn) || GetDestinationCount() == 0) return;

 if (auto* PC = Cast<AShooterPlayerController>(InteractingPawn->GetController())) PC->Client_OpenMissionSelection(this);
}

void AMissionDeployPoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(AMissionDeployPoint, Missions);
 DOREPLIFETIME(AMissionDeployPoint, CountdownEndServerTime);
 DOREPLIFETIME(AMissionDeployPoint, SelectedMission);
 DOREPLIFETIME(AMissionDeployPoint, DeploymentError);
 DOREPLIFETIME(AMissionDeployPoint, DeploymentRoster);
 DOREPLIFETIME(AMissionDeployPoint, ReadyPlayers);
}
int32 AMissionDeployPoint::GetDestinationCount() const { return Missions.IsEmpty() ? (MissionMap.IsNull() ? 0 : 1) : Missions.Num(); }
FMissionDestination AMissionDeployPoint::GetDestination(int32 Index) const
{
 FMissionDestination D;
 if(Missions.IsValidIndex(Index))D=Missions[Index];
 if (Missions.IsEmpty() && Index == 0)
 {
  D.Title = NSLOCTEXT("Mission", "TestMission", "CQB · 现场处置");
  D.Briefing = NSLOCTEXT("Mission", "TestBriefing", "控制现场，处置全部嫌疑人，解救人质并完成标定目标。尽量活捉，确保小队存活。隐藏目标为可选加分。 ");
  D.Map = MissionMap; D.GameMode = MissionGameMode;
 }
 if(D.Map.ToSoftObjectPath().GetLongPackageName().Contains(TEXT("Factory")))
 {
  D.Title=FText::FromString(ShooterLoadingScreen::GetTaskName(TEXT("Factory")));
  FString Text;
  GConfig->GetString(TEXT("FPS.MissionUI"),TEXT("FactoryBriefing"),Text,GGameIni);
  Text.ReplaceInline(TEXT("\\n"),TEXT("\n"));D.Briefing=FText::FromString(Text);
 }
 return D;
}
void AMissionDeployPoint::OnRep_Deployment() { OnDeploymentUpdated.Broadcast(); }

bool AMissionDeployPoint::Auth_SelectMission(APlayerController* PC, int32 Index)
{
 if (!HasAuthority() || !IsRoomHost(PC) || !IsValid(PC) || !PC->GetPawn() || Index < 0 || Index >= GetDestinationCount()) return false;
 if (FVector::DistSquared(PC->GetPawn()->GetActorLocation(), GetActorLocation()) > FMath::Square(500.f)) return false;
 const auto* GS = GetWorld()->GetGameState<ACoopCqbGameState>();
 if (GS && !GS->bRangeHub) return false;
 for (TActorIterator<AMissionDeployPoint> It(GetWorld()); It; ++It)
  if (*It!=this&&It->SelectedMission!=INDEX_NONE) return false;
 const FMissionDestination D = GetDestination(Index);
 if (D.Map.IsNull() || !FPackageName::DoesPackageExist(D.Map.ToSoftObjectPath().GetLongPackageName())) return false;
 SelectedMission = Index;
 DeploymentError.Reset();
 DeploymentLeader = PC;
 StopCountdown();ReadyPlayers.Reset();UpdateRoster();
 if(BriefingRadio)
 {
  if(D.Map.ToSoftObjectPath().GetLongPackageName().Contains(TEXT("Factory")))BriefingRadio->PlayImmediate(EMissionVoiceCue::Briefing);
  else BriefingRadio->StopBriefing();
 }
 OnRep_Deployment(); ForceNetUpdate();
 Multicast_ShowCountdown();
 GetWorldTimerManager().SetTimer(RosterTimer,this,&ThisClass::ServiceReadyRoster,.25f,true);
 return true;
}
void AMissionDeployPoint::Auth_Cancel(APlayerController* PC)
{
 if (!HasAuthority() || !IsRoomHost(PC) || DeploymentLeader.Get() != PC || SelectedMission==INDEX_NONE) return;
 StopCountdown();GetWorldTimerManager().ClearTimer(RosterTimer);
 BriefingRadio->StopBriefing();
 SelectedMission = INDEX_NONE; DeploymentLeader.Reset();ReadyPlayers.Reset();DeploymentRoster.Reset();
 OnRep_Deployment(); ForceNetUpdate();
}
void AMissionDeployPoint::Multicast_ShowCountdown_Implementation()
{
 for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
  if (auto* PC = Cast<AShooterPlayerController>(It->Get()); PC && PC->IsLocalController()) PC->Client_OpenMissionSelection(this);
}
void AMissionDeployPoint::DeploySquad()
{
 if(UpdateRoster()||ReadyPlayers.Num()!=DeploymentRoster.Num()||DeploymentRoster.IsEmpty())
 {
  StopCountdown();DeploymentError=TEXT("队伍发生变化，请重新按 Y 就绪。");OnRep_Deployment();ForceNetUpdate();return;
 }
 if (!DeploymentLeader.IsValid())
 {
  DeploymentError = TEXT("发起人已离线，出发取消");
  CountdownEndServerTime = -1; OnRep_Deployment(); ForceNetUpdate(); return;
 }
 auto* GI = Cast<UShooterGameInstance>(GetGameInstance());
 int32 Operators = 0;
 for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) if (It->IsValid()) ++Operators;
 const FMissionDestination D = GetDestination(SelectedMission);
 const TSoftClassPtr<AGameModeBase> Mode = D.GameMode.IsNull() ? MissionGameMode : D.GameMode;
 if (!GI || !GI->TravelSquadTo(D.Map, Mode, {FString::Printf(TEXT("ExpectedOperators=%d"), FMath::Max(1, Operators))}))
 {
  DeploymentError = TEXT("地图切换失败，请重试");
  StopCountdown();ReadyPlayers.Reset();OnRep_Deployment(); ForceNetUpdate();
 }
 else GetWorldTimerManager().ClearTimer(RosterTimer);
}

bool AMissionDeployPoint::IsPlayerReady(const APlayerController* PC) const
{
 return PC&&PC->PlayerState&&ReadyPlayers.Contains(PC->PlayerState);
}
bool AMissionDeployPoint::UpdateRoster()
{
 TArray<TObjectPtr<APlayerState>> Current;
 for(auto It=GetWorld()->GetPlayerControllerIterator();It;++It)
  if(auto* PC=It->Get();PC&&PC->PlayerState&&!PC->PlayerState->IsOnlyASpectator())Current.AddUnique(PC->PlayerState);
 bool Changed=Current.Num()!=DeploymentRoster.Num();
 for(const auto& PS:Current)if(!DeploymentRoster.Contains(PS)){Changed=true;break;}
 if(Changed){DeploymentRoster=MoveTemp(Current);ReadyPlayers.Reset();}
 return Changed;
}
bool AMissionDeployPoint::Auth_ToggleReady(APlayerController* PC)
{
 if(!HasAuthority()||SelectedMission==INDEX_NONE||!IsValid(PC)||!PC->GetPawn()||!PC->PlayerState)return false;
 const auto* GS=GetWorld()->GetGameState<ACoopCqbGameState>();
 if(GS?!GS->bRangeHub:!GetWorld()->GetMapName().Contains(TEXT("Lvl_Range")))return false;
 if(UpdateRoster())StopCountdown();
 if(!DeploymentRoster.Contains(PC->PlayerState))return false;
 if(ReadyPlayers.Contains(PC->PlayerState)){ReadyPlayers.Remove(PC->PlayerState);StopCountdown();}
 else ReadyPlayers.Add(PC->PlayerState);
 DeploymentError.Reset();
 if(ReadyPlayers.Num()==DeploymentRoster.Num()&&!DeploymentRoster.IsEmpty())BeginCountdown();
 OnRep_Deployment();ForceNetUpdate();return true;
}
void AMissionDeployPoint::BeginCountdown()
{
 if(CountdownEndServerTime>0)return;
 const auto* GS=GetWorld()->GetGameState<ACoopCqbGameState>();
 float Seconds=FMath::Max(1.f,CountdownDuration);
 if(GetDestination(SelectedMission).Map.ToSoftObjectPath().GetLongPackageName().Contains(TEXT("Factory")))
 {
  if(BriefingRadio->Transmission.Cue==EMissionVoiceCue::Briefing)
   Seconds=FMath::Max(Seconds,BriefingRadio->GetRemainingSeconds()+.15f);
 }
 CountdownEndServerTime=(GS?GS->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds())+Seconds;
 GetWorldTimerManager().SetTimer(DeployTimer,this,&ThisClass::DeploySquad,Seconds,false);
}
void AMissionDeployPoint::StopCountdown()
{
 GetWorldTimerManager().ClearTimer(DeployTimer);CountdownEndServerTime=-1;
}
void AMissionDeployPoint::ServiceReadyRoster()
{
 if(!DeploymentLeader.IsValid())
 {
  BriefingRadio->StopBriefing();
  StopCountdown();SelectedMission=INDEX_NONE;ReadyPlayers.Reset();DeploymentRoster.Reset();
  DeploymentError=TEXT("发起人已离线，部署取消。");GetWorldTimerManager().ClearTimer(RosterTimer);
  OnRep_Deployment();ForceNetUpdate();return;
 }
 if(UpdateRoster())
 {
  StopCountdown();DeploymentError=TEXT("队伍发生变化，请全队重新按 Y 就绪。");
  OnRep_Deployment();ForceNetUpdate();Multicast_ShowCountdown();
 }
}
void AMissionDeployPoint::EndPlay(const EEndPlayReason::Type Reason)
{
 GetWorldTimerManager().ClearTimer(DeployTimer);GetWorldTimerManager().ClearTimer(RosterTimer);
 Super::EndPlay(Reason);
}
