#include "Game/MissionRadioComponent.h"
#include "Game/CoopCqbGameState.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UMissionRadioComponent::UMissionRadioComponent()
{
 PrimaryComponentTick.bCanEverTick=false;
 SetIsReplicatedByDefault(true);
}
void UMissionRadioComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(UMissionRadioComponent,Transmission);
}
void UMissionRadioComponent::BeginPlay()
{
 Super::BeginPlay();
 if(Transmission.Serial>0)OnRep_Transmission();
}
void UMissionRadioComponent::QueueCue(EMissionVoiceCue Cue,bool Once)
{
 if(!GetOwner()->HasAuthority()||Cue==EMissionVoiceCue::None)return;
 if(Once&&OnceCues.Contains(Cue))return;
 if(Pending.Contains(Cue)||(Transmission.Cue==Cue&&GetRemainingSeconds()>0))return;
 if(Pending.Num()>=24)return;
 if(Once)OnceCues.Add(Cue);
 Pending.Add(Cue);
 if(!GetWorld()->GetTimerManager().IsTimerActive(DispatchTimer))DispatchNext();
}
void UMissionRadioComponent::PlayImmediate(EMissionVoiceCue Cue)
{
 if(!GetOwner()->HasAuthority())return;
 GetWorld()->GetTimerManager().ClearTimer(DispatchTimer);
 Pending.Reset();Pending.Add(Cue);DispatchNext();
}
void UMissionRadioComponent::StopBriefing()
{
 if(GetOwner()->HasAuthority()&&Transmission.Cue==EMissionVoiceCue::Briefing)PlayImmediate(EMissionVoiceCue::None);
}
void UMissionRadioComponent::DispatchNext()
{
 if(Pending.IsEmpty())return;
 const auto* GS=GetWorld()->GetGameState();
 Transmission.Cue=Pending[0];Pending.RemoveAt(0);
 Transmission.StartServerTime=(GS?GS->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds())+.25f;
 ++Transmission.Serial;
 CurrentEndTime=Transmission.StartServerTime+MissionVoice::Duration(Transmission.Cue);
 OnRep_Transmission();GetOwner()->ForceNetUpdate();
 if(Transmission.Cue!=EMissionVoiceCue::None)
  GetWorld()->GetTimerManager().SetTimer(DispatchTimer,this,&ThisClass::DispatchNext,MissionVoice::Duration(Transmission.Cue)+.4f,false);
 UE_LOG(LogTemp,Log,TEXT("[MissionRadio] cue=%s serial=%d start=%.3f"),MissionVoice::AssetName(Transmission.Cue),Transmission.Serial,Transmission.StartServerTime);
}
float UMissionRadioComponent::GetRemainingSeconds() const
{
 const auto* GS=GetWorld()->GetGameState();
 const float Now=GS?GS->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
 float Seconds=FMath::Max(0.f,CurrentEndTime-Now);
 for(auto Cue:Pending)Seconds+=MissionVoice::Duration(Cue)+.4f;
 return Seconds;
}
void UMissionRadioComponent::OnRep_Transmission()
{
 if(GetNetMode()==NM_DedicatedServer)return;
 GetWorld()->GetTimerManager().ClearTimer(LocalStartTimer);
 if(LocalRadio){LocalRadio->Stop();LocalRadio=nullptr;}
 if(Transmission.Cue==EMissionVoiceCue::None)return;
 const auto* GS=GetWorld()->GetGameState();
 const float Delay=Transmission.StartServerTime-(GS?GS->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds());
 if(Delay>0)GetWorld()->GetTimerManager().SetTimer(LocalStartTimer,this,&ThisClass::StartLocalPlayback,Delay,false);
 else StartLocalPlayback();
}
void UMissionRadioComponent::StartLocalPlayback()
{
 if(Transmission.Cue==EMissionVoiceCue::None)return;
 bool LocalPlayer=false;
 for(auto It=GetWorld()->GetPlayerControllerIterator();It;++It)if(It->IsValid()&&It->Get()->IsLocalController()){LocalPlayer=true;break;}
 if(!LocalPlayer)return;
 const auto* GS=GetWorld()->GetGameState();
 const float Offset=FMath::Max(0.f,float((GS?GS->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds())-Transmission.StartServerTime));
 if(Offset>=MissionVoice::Duration(Transmission.Cue))return; // Do not replay stale transmissions for late joiners.
 if(auto* Sound=MissionVoice::LoadSound(Transmission.Cue))
 {
  LocalRadio=UGameplayStatics::SpawnSound2D(this,Sound,.9f,1.f,Offset,nullptr,false,true);
  LastLocalCue=Transmission.Cue;++LocalPlaybackCount;
 }
}
bool UMissionRadioComponent::IsRadioPlaying() const{return LocalRadio&&LocalRadio->IsPlaying();}
void UMissionRadioComponent::Multicast_OperatorWarning_Implementation(APawn* Speaker,EMissionVoiceCue Cue)
{
 if(GetNetMode()==NM_DedicatedServer||!IsValid(Speaker))return;
 if(!ShoutAttenuation)
 {
  ShoutAttenuation=NewObject<USoundAttenuation>(this);
  ShoutAttenuation->Attenuation.bAttenuate=true;
  ShoutAttenuation->Attenuation.bSpatialize=true;
  ShoutAttenuation->Attenuation.AttenuationShapeExtents=FVector(180);
  ShoutAttenuation->Attenuation.FalloffDistance=1800;
 }
 if(auto* Sound=MissionVoice::LoadSound(Cue))
 {
  UGameplayStatics::SpawnSoundAttached(Sound,Speaker->GetRootComponent(),NAME_None,FVector(0,0,65),FRotator::ZeroRotator,
   EAttachLocation::KeepRelativeOffset,true,.9f,1.f,0,ShoutAttenuation,nullptr,true);
  LastWarningSpeaker=Speaker;++LocalWarningCount;
 }
}
void UMissionRadioComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 GetWorld()->GetTimerManager().ClearTimer(DispatchTimer);GetWorld()->GetTimerManager().ClearTimer(LocalStartTimer);
 if(LocalRadio)LocalRadio->Stop();
 Super::EndPlay(Reason);
}
