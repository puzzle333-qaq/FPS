#include "Game/RestraintActionComponent.h"
#include "Game/DoorGestureComponent.h"
#include "ShooterCharacter.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
URestraintActionComponent::URestraintActionComponent()
{
 SetIsReplicatedByDefault(true);PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.bStartWithTickEnabled=false;PrimaryComponentTick.TickGroup=TG_PostPhysics;
 FirstPersonAnimation=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/FPS/Animations/Compliance/A_OperatorKneelBind1P.A_OperatorKneelBind1P")));
 ThirdPersonAnimation=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/FPS/Animations/Compliance/A_OperatorKneelBind3P.A_OperatorKneelBind3P")));
}
bool URestraintActionComponent::BeginRestraint(AActor* Target,float Duration)
{
 auto* C=Cast<AShooterCharacter>(GetOwner());
 if(!GetOwner()->HasAuthority()||!C||IsActive()||!IsValid(Target))return false;
 auto* Combat=C->GetCombatComponent();
 if(Combat&&Combat->CurrentWeapon&&Combat->CurrentWeapon->WeaponStatus!=EWeaponStatus::Idle)return false;
 if(Combat){Combat->Initiate_FireWeapon_Released();Combat->CancelInspect();}
 auto* Clip=FirstPersonAnimation.LoadSynchronous();if(!Clip||!ThirdPersonAnimation.LoadSynchronous())return false;
 if(auto* Door=GetOwner()->FindComponentByClass<UDoorGestureComponent>();Door&&Door->IsActive())return false;
 C->GetCharacterMovement()->StopMovementImmediately();
 C->GetCapsuleComponent()->IgnoreActorWhenMoving(Target,true);IgnoredTarget=Target;
 const auto* TargetCapsule=Target->FindComponentByClass<UCapsuleComponent>();
 const float Floor=Target->GetActorLocation().Z-(TargetCapsule?TargetCapsule->GetScaledCapsuleHalfHeight():88.f);
 FVector Aligned=Target->GetActorLocation()+Target->GetActorRotation().RotateVector(OperatorAlignment);Aligned.Z=Floor+C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f;
 FHitResult Hit;C->SetActorLocationAndRotation(Aligned,Target->GetActorRotation(),true,&Hit);
 State.Target=Target;State.Duration=FMath::Clamp(Duration,3.f,4.f);State.StartedAt=GetWorld()->GetTimeSeconds();Refresh();GetOwner()->ForceNetUpdate();return true;
}
void URestraintActionComponent::EndRestraint(){if(!IsValid(GetOwner())||!GetWorld()||!GetOwner()->HasAuthority())return;State.Target=nullptr;Refresh();GetOwner()->ForceNetUpdate();}
void URestraintActionComponent::CancelForCombatAction(){if(!IsActive())return;if(GetOwner()->HasAuthority())EndRestraint();else Server_Cancel();}
void URestraintActionComponent::Server_Cancel_Implementation(){EndRestraint();}
void URestraintActionComponent::Refresh()
{
 auto* C=Cast<AShooterCharacter>(GetOwner());if(!IsValid(C))return;
 auto* Arms=C->GetMesh1P_Implementation();auto* Body=C->GetMesh();auto* Combat=C->GetCombatComponent();
 if(IsActive())
 {
  if(!bPresented)
  {
   SavedArmsClass=Arms?Arms->GetAnimClass():nullptr;SavedBodyClass=Body?Body->GetAnimClass():nullptr;bPresented=true;
   if(Arms){SavedArmsParent=Arms->GetAttachParent();SavedArmsTransform=Arms->GetRelativeTransform();Arms->AttachToComponent(Body,FAttachmentTransformRules::SnapToTargetNotIncludingScale);Arms->SetRelativeTransform(FTransform::Identity);}
   if(C->UsesFirstPersonVisuals())
   {
    TInlineComponentArray<USkeletalMeshComponent*> Parts(C);
    for(auto* Part:Parts)if(Part->GetFName()==TEXT("Mesh1P_Sleeves"))
    {
     SavedSleevesComponent=Part;SavedSleevesMesh=Part->GetSkeletalMeshAsset();SavedSleevesLOD=Part->GetForcedLOD();bSleevesWereVisible=Part->IsVisible();
     if(auto* SleevesMesh=FirstPersonSleevesMesh.LoadSynchronous()){Part->SetSkeletalMesh(SleevesMesh);Part->SetForcedLOD(1);Part->SetLeaderPoseComponent(Arms,true);}else Part->SetVisibility(false);
     break;
    }
   }
   if(C->IsLocallyControlled())
   {
    if(auto* Camera=C->FindComponentByClass<UCameraComponent>()){SavedCameraLocation=Camera->GetRelativeLocation();SavedCameraWorld=Camera->GetComponentLocation();bCameraSaved=true;}
    if(auto* PC=Cast<APlayerController>(C->GetController())){LockedController=PC;SavedControlRotation=PC->GetControlRotation();PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);FRotator Aim=SavedControlRotation;Aim.Yaw=State.Target->GetActorRotation().Yaw;PC->SetControlRotation(Aim);}
   }
   SetComponentTickEnabled(true);
  }
  if(Combat&&Combat->CurrentWeapon){HiddenWeapon=Combat->CurrentWeapon;HiddenWeapon->SetHiddenForHeal(true);}
  const auto* GS=GetWorld()->GetGameState();const float Elapsed=FMath::Max(0.f,(GS?GS->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds())-State.StartedAt);
  auto Play=[&](USkeletalMeshComponent* Mesh,UAnimSequence* Animation)
  {
   if(!Mesh||!Animation)return;
   if(auto* Asset=Mesh->GetSkeletalMeshAsset())if(auto* Skeleton=Asset->GetSkeleton())Skeleton->AddCompatibleSkeleton(Animation->GetSkeleton());
   Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
   Mesh->PlayAnimation(Animation,false);const float Rate=Animation->GetPlayLength()/State.Duration;Mesh->SetPlayRate(Rate);Mesh->SetPosition(FMath::Min(Animation->GetPlayLength(),Elapsed*Rate),false);
  };
  if(C->IsLocallyControlled()||C->UsesFirstPersonVisuals())Play(Arms,FirstPersonAnimation.LoadSynchronous());
  Play(Body,ThirdPersonAnimation.LoadSynchronous());
 }
 else if(bPresented)
 {
  bPresented=false;SetComponentTickEnabled(false);
  if(bCameraSaved){if(auto* Camera=C->FindComponentByClass<UCameraComponent>())Camera->SetRelativeLocation(SavedCameraLocation);bCameraSaved=false;}
  if(LockedController.IsValid()){LockedController->SetControlRotation(SavedControlRotation);LockedController->SetIgnoreMoveInput(false);LockedController->SetIgnoreLookInput(false);LockedController.Reset();}
  if(IgnoredTarget.IsValid())C->GetCapsuleComponent()->IgnoreActorWhenMoving(IgnoredTarget.Get(),false);IgnoredTarget.Reset();
  if(Arms&&SavedArmsParent.IsValid()){Arms->AttachToComponent(SavedArmsParent.Get(),FAttachmentTransformRules::KeepRelativeTransform);Arms->SetRelativeTransform(SavedArmsTransform);}
  if(SavedSleevesComponent.IsValid()){SavedSleevesComponent->SetSkeletalMesh(SavedSleevesMesh);SavedSleevesComponent->SetForcedLOD(SavedSleevesLOD);SavedSleevesComponent->SetLeaderPoseComponent(Arms,true);SavedSleevesComponent->SetVisibility(bSleevesWereVisible);}
  SavedSleevesComponent.Reset();SavedSleevesMesh=nullptr;
  if(Arms){Arms->SetAnimationMode(EAnimationMode::AnimationBlueprint);Arms->SetAnimInstanceClass(SavedArmsClass);}
  if(Body){Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);Body->SetAnimInstanceClass(SavedBodyClass);}
  if(HiddenWeapon.IsValid())HiddenWeapon->SetHiddenForHeal(false);HiddenWeapon.Reset();
  if(Combat)Combat->RefreshAnimsAfterMesh3PSwap();
 }
}
void URestraintActionComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
 Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
 auto* C=Cast<AShooterCharacter>(GetOwner());if(!C||!IsActive())return;
 if(bCameraSaved)if(auto* Camera=C->FindComponentByClass<UCameraComponent>())
 {
  const auto* GS=GetWorld()->GetGameState();const float Time=(GS?GS->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds())-State.StartedAt;
  const float In=FMath::SmoothStep(0.f,.3f,Time);
  const float Out=FMath::SmoothStep(0.f,.5f,State.Duration-Time);
  FVector Eyes=C->GetMesh()->GetSocketLocation(TEXT("head"))+C->GetActorForwardVector()*5.f+FVector(0,0,7.f);
  const FVector TargetForward=State.Target->GetActorForwardVector();
  const float TargetDistance=FVector::DotProduct(Eyes-State.Target->GetActorLocation(),TargetForward);
  if(TargetDistance>-65.f)Eyes-=TargetForward*(TargetDistance+65.f);
  const FVector Original=Camera->GetAttachParent()?Camera->GetAttachParent()->GetComponentTransform().TransformPosition(SavedCameraLocation):SavedCameraWorld;
  const FVector Position=FMath::Lerp(Original,FMath::Lerp(SavedCameraWorld,Eyes,In),Out);
  Camera->SetWorldLocation(Position);
  if(LockedController.IsValid())
  {
   const FVector Hands=(C->GetMesh()->GetSocketLocation(TEXT("hand_l"))+C->GetMesh()->GetSocketLocation(TEXT("hand_r")))*.5f;
   const float Forward=FMath::Max(6.f,FVector::DotProduct(Hands-Position,C->GetActorForwardVector()));
   FRotator Aim=SavedControlRotation;Aim.Yaw=State.Target->GetActorRotation().Yaw;
   const float Pitch=FMath::Clamp(-FMath::RadiansToDegrees(FMath::Atan2(Position.Z-Hands.Z,Forward)),-85.f,20.f);
   Aim.Pitch=FMath::Lerp(SavedControlRotation.Pitch,Pitch,In*Out);LockedController->SetControlRotation(Aim);
  }
 }
}
void URestraintActionComponent::EndPlay(const EEndPlayReason::Type Reason){State.Target=nullptr;Refresh();if(LockedController.IsValid()){LockedController->SetIgnoreMoveInput(false);LockedController->SetIgnoreLookInput(false);}Super::EndPlay(Reason);}
void URestraintActionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(URestraintActionComponent,State);}
