#include "Game/DoorGestureComponent.h"
#include "Game/RestraintActionComponent.h"
#include "World/BreakableDoor.h"
#include "ShooterCharacter.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "Stats/StatsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
UDoorGestureComponent::UDoorGestureComponent()
{
 SetIsReplicatedByDefault(true);PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.bStartWithTickEnabled=false;
 OpenLeftAnimation=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/FPS/Animations/Compliance/A_DoorOpenLeft3P.A_DoorOpenLeft3P")));
 OpenRightAnimation=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/FPS/Animations/Compliance/A_DoorOpenRight3P.A_DoorOpenRight3P")));
 KickAnimation=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/FPS/Animations/Compliance/A_DoorKick3P.A_DoorKick3P")));
}
bool UDoorGestureComponent::BeginDoorAction(ABreakableDoor* Door,bool bKick,bool bLeft,bool bClosing)
{
 auto* C=Cast<AShooterCharacter>(GetOwner());
 if(!C||!C->HasAuthority()||IsActive()||!IsValid(Door)||Door->IsDestroyed())return false;
 if(auto* Restraint=C->FindComponentByClass<URestraintActionComponent>();Restraint&&Restraint->IsActive())return false;
 if(auto* Combat=C->GetCombatComponent();Combat&&Combat->CurrentWeapon&&Combat->CurrentWeapon->WeaponStatus!=EWeaponStatus::Idle)return false;
 if(bKick&&!Door->IsAimedAtBy(C))return false;
 bLeft=C->GetActorTransform().InverseTransformPosition(Door->GetActorLocation()).Y<0;
 auto* Clip=(bKick?KickAnimation:bLeft?OpenLeftAnimation:OpenRightAnimation).LoadSynchronous();if(!Clip)return false;
 if(!Door->ReserveInteraction(C))return false;
 if(auto* Combat=C->GetCombatComponent()){Combat->Initiate_FireWeapon_Released();Combat->Initiate_Aim_Released();}
 State.Door=Door;State.Gesture=bKick?EDoorGesture::Kick:bLeft?EDoorGesture::OpenLeft:EDoorGesture::OpenRight;
 State.bReverse=bClosing;
 State.Duration=FMath::Max(.2f,bKick?KickDuration:OpenDuration);State.ServerStart=GetWorld()->GetTimeSeconds();++State.Serial;
 auto* Move=C->GetCharacterMovement();State.ResumeMovementMode=Move->MovementMode;State.ResumeCustomMode=Move->CustomMovementMode;
 Refresh();
 float Contact=bKick?KickContactTime:bClosing?Clip->GetPlayLength()-HandleContactTime:HandleContactTime;
 Contact=FMath::Clamp(Contact/Clip->GetPlayLength()*State.Duration,.01f,State.Duration-.05f);
 GetWorld()->GetTimerManager().SetTimer(ContactTimer,this,&ThisClass::ContactDoor,Contact,false);
 C->ForceNetUpdate();return true;
}
void UDoorGestureComponent::Refresh()
{
 if(!GetWorld())return;GetWorld()->GetTimerManager().ClearTimer(FinishTimer);
 if(State.Gesture==EDoorGesture::None){FinishPresentation();return;}
 auto* C=Cast<AShooterCharacter>(GetOwner());if(!IsValid(C))return;
 auto* Clip=(State.Gesture==EDoorGesture::Kick?KickAnimation:State.Gesture==EDoorGesture::OpenLeft?OpenLeftAnimation:OpenRightAnimation).LoadSynchronous();if(!Clip)return;
 const auto* GS=GetWorld()->GetGameState();const float Elapsed=FMath::Max(0.f,(GS?GS->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds())-State.ServerStart);
 if(Elapsed>=State.Duration){FinishPresentation();return;}
 auto* Body=C->GetMesh();
 if(!bPlaying)
 {
  SavedBodyClass=Body->GetAnimClass();bPlaying=true;SetComponentTickEnabled(true);
  if(C->HasAuthority()||C->IsLocallyControlled())
  {C->GetCharacterMovement()->StopMovementImmediately();C->GetCharacterMovement()->DisableMovement();}
  if(C->IsLocallyControlled())if(auto* PC=Cast<APlayerController>(C->GetController()))
  {
   LockedController=PC;PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);bPawnInputWasEnabled=C->InputEnabled();C->DisableInput(PC);
  }
 }
 if(auto* Combat=C->GetCombatComponent();Combat&&Combat->CurrentWeapon&&!HiddenGun.IsValid())
 {HiddenGun=Combat->CurrentWeapon->GetMesh3P();if(HiddenGun.IsValid()){bGunWasHidden=HiddenGun->bHiddenInGame;HiddenGun->SetHiddenInGame(true);}}
 if(auto* Mesh=Body->GetSkeletalMeshAsset())Mesh->GetSkeleton()->AddCompatibleSkeleton(Clip->GetSkeleton());
 const float Rate=Clip->GetPlayLength()/State.Duration;
 Body->PlayAnimation(Clip,false);Body->SetPlayRate(State.bReverse?-Rate:Rate);Body->SetPosition(State.bReverse?Clip->GetPlayLength()-Elapsed*Rate:Elapsed*Rate,false);
 GetWorld()->GetTimerManager().SetTimer(FinishTimer,this,&ThisClass::StopGesture,State.Duration-Elapsed,false);
}
void UDoorGestureComponent::ContactDoor()
{
 if(!GetOwner()->HasAuthority()||!IsActive())return;
 if(!IsValid(State.Door)||State.Door->IsDestroyed()){StopGesture();return;}
 if(const auto* S=UStatsComponent::FindStatsComponent(GetOwner());S&&S->DeathState!=EDeathState::NotDead){StopGesture();return;}
 State.Door->CommitAnimatedInteraction(Cast<APawn>(GetOwner()),State.Gesture==EDoorGesture::Kick,State.bReverse,FMath::Max(.08f,State.Duration-(GetWorld()->GetTimeSeconds()-State.ServerStart)));
}
void UDoorGestureComponent::FinishPresentation()
{
 if(!bPlaying)return;bPlaying=false;SetComponentTickEnabled(false);
 if(auto* C=Cast<AShooterCharacter>(GetOwner());IsValid(C))
 {
  const auto* S=UStatsComponent::FindStatsComponent(C);const bool Alive=!S||S->DeathState==EDeathState::NotDead;
  if(Alive)
  {
   C->GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);C->GetMesh()->SetAnimInstanceClass(SavedBodyClass);
   if(C->HasAuthority()||C->IsLocallyControlled())C->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(State.ResumeMovementMode),State.ResumeCustomMode);
   if(auto* Combat=C->GetCombatComponent())Combat->RefreshAnimsAfterMesh3PSwap(false);
  }
  if(LockedController.IsValid()){LockedController->SetIgnoreMoveInput(false);LockedController->SetIgnoreLookInput(false);if(bPawnInputWasEnabled)C->EnableInput(LockedController.Get());}
 }
 else if(LockedController.IsValid()){LockedController->SetIgnoreMoveInput(false);LockedController->SetIgnoreLookInput(false);}
 LockedController.Reset();if(HiddenGun.IsValid())HiddenGun->SetHiddenInGame(bGunWasHidden);HiddenGun.Reset();
}
void UDoorGestureComponent::StopGesture()
{
 if(GetWorld()){GetWorld()->GetTimerManager().ClearTimer(FinishTimer);GetWorld()->GetTimerManager().ClearTimer(ContactTimer);}
 FinishPresentation();
 if(IsValid(GetOwner())&&GetWorld()&&GetOwner()->HasAuthority())
 {if(IsValid(State.Door))State.Door->ReleaseInteraction(Cast<APawn>(GetOwner()));State.Door=nullptr;State.Gesture=EDoorGesture::None;GetOwner()->ForceNetUpdate();}
}
void UDoorGestureComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
 Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
 const auto* S=UStatsComponent::FindStatsComponent(GetOwner());
 if(!IsValid(State.Door)||State.Door->IsDestroyed()||(S&&S->DeathState!=EDeathState::NotDead))StopGesture();
}
void UDoorGestureComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 if(GetWorld()){GetWorld()->GetTimerManager().ClearTimer(FinishTimer);GetWorld()->GetTimerManager().ClearTimer(ContactTimer);}
 FinishPresentation();
 if(IsValid(State.Door)&&GetOwner()->HasAuthority())State.Door->ReleaseInteraction(Cast<APawn>(GetOwner()));
 Super::EndPlay(Reason);
}
void UDoorGestureComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(UDoorGestureComponent,State);}
