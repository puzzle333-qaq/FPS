#include "Game/CompliancePoseComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace {
 UAnimSequence* Clip(const TCHAR* Name)
 {
  return LoadObject<UAnimSequence>(nullptr,*(FString(TEXT("/Game/FPS/Animations/Compliance/"))+Name+TEXT(".")+Name));
 }
 float Length(const TCHAR* Name){auto* A=Clip(Name);return A?A->GetPlayLength():0.f;}
}
UCompliancePoseComponent::UCompliancePoseComponent(){PrimaryComponentTick.bCanEverTick=false;SetIsReplicatedByDefault(true);}
void UCompliancePoseComponent::BeginPlay(){Super::BeginPlay();Refresh();}
void UCompliancePoseComponent::EndPlay(const EEndPlayReason::Type Reason){if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(PhaseTimer);Super::EndPlay(Reason);}
void UCompliancePoseComponent::SetMesh(USkeletalMeshComponent* InMesh){Mesh=InMesh;Refresh();}
bool UCompliancePoseComponent::PrepareCompatibleAnimation(UAnimSequence* Animation,USkeleton* Skeleton)
{
#if WITH_EDITOR
 return Animation&&Skeleton&&Animation->ReplaceSkeleton(Skeleton,false);
#else
 return false;
#endif
}
float UCompliancePoseComponent::GetHandsUpDuration()const{return Length(TEXT("A_HandsUpEnter"))+Length(TEXT("A_KneelEnter"));}
float UCompliancePoseComponent::GetBindingDuration()const{return Length(TEXT("A_TargetKneelBind"));}
bool UCompliancePoseComponent::IsHandsUpSettled()const
{
 const auto* GS=GetWorld()?GetWorld()->GetGameState():nullptr;
 return State.Pose==ECompliancePose::Kneel||(State.Pose==ECompliancePose::HandsUp&&GetWorld()&&(GS?GS->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds())-State.ServerStartTime>=GetHandsUpDuration());
}
void UCompliancePoseComponent::SetPose(ECompliancePose Pose,float PlayRate)
{
 if(!GetOwner()->HasAuthority()||(State.Pose==Pose&&FMath::IsNearlyEqual(State.PlayRate,PlayRate)))return;
 State.Pose=Pose;State.PlayRate=FMath::Max(.01f,PlayRate);State.ServerStartTime=GetWorld()->GetTimeSeconds();Refresh();GetOwner()->ForceNetUpdate();
}
void UCompliancePoseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(UCompliancePoseComponent,State);}
void UCompliancePoseComponent::Refresh()
{
 if(!GetWorld())return;
 GetWorld()->GetTimerManager().ClearTimer(PhaseTimer);
 if(!Mesh||State.Pose==ECompliancePose::None)return;
 const auto* GS=GetWorld()->GetGameState();
 float Elapsed=FMath::Max(0.f,(GS?GS->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds())-State.ServerStartTime)*State.PlayRate;
 TArray<const TCHAR*> Intro;const TCHAR* Loop=TEXT("A_Calm");
 switch(State.Pose){
 case ECompliancePose::Scared:Loop=TEXT("A_Scared");break;
 case ECompliancePose::Flee:Loop=TEXT("A_Flee");break;
 case ECompliancePose::Crawl:Loop=TEXT("A_Crawl");break;
 case ECompliancePose::HandsUp:Intro={TEXT("A_HandsUpEnter"),TEXT("A_KneelEnter")};Loop=TEXT("A_KneelHandsUp");break;
 case ECompliancePose::Bound:Loop=TEXT("A_KneelBound");break;
 case ECompliancePose::Kneel:Loop=TEXT("A_KneelHandsUp");break;
 case ECompliancePose::Binding:Intro={TEXT("A_TargetKneelBind")};Loop=TEXT("A_KneelBound");break;
 case ECompliancePose::TableHide:Intro={TEXT("A_TableHideEnter")};Loop=TEXT("A_TableHide");break;
 case ECompliancePose::ProneHide:Intro={TEXT("A_ProneEnter")};Loop=TEXT("A_ProneHide");break;
 default:break;}
 UAnimSequence* Chosen=nullptr;bool bLoop=true;float Next=0.f;
 for(const TCHAR* Name:Intro){auto* A=Clip(Name);if(!A)continue;const float Duration=A->GetPlayLength();if(Elapsed<Duration){Chosen=A;bLoop=false;Next=Duration-Elapsed;break;}Elapsed-=Duration;}
 if(!Chosen)Chosen=Clip(Loop);
 if(!Chosen)return;
 if(auto* Asset=Mesh->GetSkeletalMeshAsset())if(auto* Skeleton=Asset->GetSkeleton())Skeleton->AddCompatibleSkeleton(Chosen->GetSkeleton());
 Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 Mesh->PlayAnimation(Chosen,bLoop);
 Mesh->SetPlayRate(State.PlayRate);
 Mesh->SetPosition(bLoop?FMath::Fmod(Elapsed,FMath::Max(.01f,Chosen->GetPlayLength())):Elapsed,false);
 if(Next>0.f)GetWorld()->GetTimerManager().SetTimer(PhaseTimer,this,&ThisClass::Refresh,Next/State.PlayRate,false);
}
