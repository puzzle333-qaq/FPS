#include "Ui/MissionTabletActor.h"
#include "Ui/MissionTabletWidget.h"
#include "ShooterPlayerController.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/PostProcessComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

AMissionTabletActor::AMissionTabletActor()
{
 PrimaryActorTick.bCanEverTick=false;
 bReplicates=false;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DeviceRoot")));
 Hands=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TabletHands"));
 Hands->SetupAttachment(RootComponent);
 Hands->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Hands->SetCastShadow(false);
 Hands->SetOnlyOwnerSee(true);
 Hands->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 TabletMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TabletMesh"));
 TabletMesh->SetupAttachment(Hands,TEXT("Socket_Tablet"));
 TabletMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 TabletMesh->SetCastShadow(false);
 TabletMesh->SetOnlyOwnerSee(true);
 Screen=CreateDefaultSubobject<UWidgetComponent>(TEXT("BriefingScreen"));
 Screen->SetupAttachment(TabletMesh);
 Screen->SetWidgetSpace(EWidgetSpace::World);
 Screen->SetGeometryMode(EWidgetGeometryMode::Plane);
 Screen->SetPivot(FVector2D(.5,.5));
 Screen->SetDrawSize(FVector2D(1280,888)); // 60 px/cm across the complete rounded screen face.
 Screen->SetRelativeLocation(FVector(-1.22945,.701,10.89065)); // screen y=.601 + .1 cm.
 Screen->SetRelativeRotation(FRotator(0,90,0));
 Screen->SetRelativeScale3D(FVector(1,21.327908/1280.,14.803008/888.));
 Screen->SetBlendMode(EWidgetBlendMode::Opaque);
 Screen->SetTwoSided(false);
 Screen->SetBackgroundColor(FLinearColor::White);
 Screen->SetManuallyRedraw(true);
 Screen->SetRedrawTime(0); // RequestRenderUpdate only when data or scrolling changes.
 Screen->SetTickMode(ETickMode::Disabled);
 Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Screen->SetOnlyOwnerSee(true);
 Screen->SetCastShadow(false);
 Screen->SetWidgetClass(UMissionTabletWidget::StaticClass());
 auto* Glow=CreateDefaultSubobject<UPostProcessComponent>(TEXT("ScreenBloom"));
 Glow->SetupAttachment(RootComponent);
 Glow->bUnbound=true;
 Glow->Priority=-1; // Camera vision/flash overrides retain priority.
 Glow->Settings.bOverride_BloomThreshold=true;
 Glow->Settings.BloomThreshold=1.f;
 Glow->Settings.bOverride_BloomIntensity=true;
 Glow->Settings.BloomIntensity=.35f;
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/FPS_Devices/Mesh/SM_Tablet.SM_Tablet"));
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> Rig(TEXT("/Game/FPS_Devices/Mesh/SK_Hands_04.SK_Hands_04"));
 static ConstructorHelpers::FObjectFinder<UAnimSequence> Get(TEXT("/Game/FPS_Devices/Animations/FPS_Tablet/Anim_Tablet_Get.Anim_Tablet_Get"));
 static ConstructorHelpers::FObjectFinder<UAnimSequence> Hide(TEXT("/Game/FPS_Devices/Animations/FPS_Tablet/Anim_Tablet_Hide.Anim_Tablet_Hide"));
 static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/FPS_Devices/Animations/FPS_Tablet/Anim_Tablet_Idle.Anim_Tablet_Idle"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Display(TEXT("/Game/FPS/UI/Tablet/M_TabletDisplay.M_TabletDisplay"));
 TabletMesh->SetStaticMesh(Mesh.Object);
 Hands->SetSkeletalMesh(Rig.Object);
 RaiseAnimation=Get.Object;HideAnimation=Hide.Object;IdleAnimation=Idle.Object;
 if(Display.Succeeded())Screen->SetMaterial(0,Display.Object);
}

bool AMissionTabletActor::OpenFor(AShooterPlayerController* PC)
{
 if(!PC||!PC->IsLocalController()||!PC->GetPawn())return false;
 auto* Camera=PC->GetPawn()->FindComponentByClass<UCameraComponent>();
 if(!Camera||!TabletMesh->GetStaticMesh())return false;
 SetOwner(PC->GetPawn());
 SetInstigator(PC->GetPawn());
 // Hide only this viewer's equipment; preserve each component's original owner visibility.
 TArray<USceneComponent*> AttachedComponents;
 PC->GetPawn()->GetRootComponent()->GetChildrenComponents(true,AttachedComponents);
 for(USceneComponent* C:AttachedComponents)
  if(auto* M=Cast<UMeshComponent>(C))
  {
   HiddenMeshes.Emplace(M,bool(M->bOwnerNoSee));
   M->SetOwnerNoSee(true);
  }
 AttachToComponent(Camera,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
 int32 W=1920,H=1080;PC->GetViewportSize(W,H);
 const float Aspect=H>0?float(W)/H:16.f/9;
 const float FOV=PC->PlayerCameraManager?PC->PlayerCameraManager->GetFOVAngle():Camera->FieldOfView;
 const float HalfTan=FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(FOV,40.f,140.f)*.5f));
 const float Distance=FMath::Max(31.9132f/(2*HalfTan*.82f),21.7557f*Aspect/(2*HalfTan*.78f));
 const FTransform Desired(FRotator(0,90,0),FVector(Distance,1.22945,-10.89065));
 if(Hands->GetSkeletalMeshAsset()&&IdleAnimation&&Hands->DoesSocketExist(TEXT("Socket_Tablet")))
 {
  Hands->PlayAnimation(IdleAnimation,true);
  Hands->TickAnimation(0,false);
  Hands->RefreshBoneTransforms();
  // Calibrate using the authored idle pose, keeping the supplied Get/Hide/Idle animations.
  const FTransform Socket=Hands->GetSocketTransform(TEXT("Socket_Tablet"),RTS_Component);
  Hands->SetRelativeTransform(Socket.Inverse()*Desired);
  if(RaiseAnimation)
  {
   Hands->PlayAnimation(RaiseAnimation,false);
   GetWorldTimerManager().SetTimer(AnimationTimer,this,&ThisClass::PlayIdle,RaiseAnimation->GetPlayLength(),false);
  }
 }
 else
 {
  Hands->SetVisibility(false);
  TabletMesh->AttachToComponent(RootComponent,FAttachmentTransformRules::KeepRelativeTransform);
  TabletMesh->SetRelativeTransform(Desired);
 }
 Screen->SetOwnerPlayer(PC->GetLocalPlayer());
 Screen->InitWidget();
 if(auto* Widget=Cast<UMissionTabletWidget>(Screen->GetUserWidgetObject()))
 {
  Widget->SetOwningPlayer(PC);
  Widget->TakeWidget();
 }
 RefreshMission();
 return true;
}

void AMissionTabletActor::PlayIdle(){if(!bClosing&&IdleAnimation)Hands->PlayAnimation(IdleAnimation,true);}

void AMissionTabletActor::RefreshMission()
{
 if(bClosing)return;
 if(auto* W=Cast<UMissionTabletWidget>(Screen->GetUserWidgetObject()))W->RefreshMission();
 Screen->RequestRenderUpdate();
}

void AMissionTabletActor::ScrollBriefing(float Amount)
{
 const auto* PC=Cast<AShooterPlayerController>(GetInstigatorController());
 const bool Objectives=PC&&(PC->IsInputKeyDown(EKeys::LeftShift)||PC->IsInputKeyDown(EKeys::RightShift));
 if(auto* W=Cast<UMissionTabletWidget>(Screen->GetUserWidgetObject()))W->ScrollBriefing(Amount,Objectives);
 Screen->RequestRenderUpdate();
}

void AMissionTabletActor::Close(bool Animate)
{
 if(!Animate){Destroy();return;}
 if(bClosing)return;
 bClosing=true;
 GetWorldTimerManager().ClearTimer(AnimationTimer);
 if(Animate&&HideAnimation&&Hands->IsVisible())
 {
  Hands->PlayAnimation(HideAnimation,false);
  // Equipment returns after the device has left the view.
  TWeakObjectPtr<AShooterPlayerController> PC=Cast<AShooterPlayerController>(GetInstigatorController());
  GetWorldTimerManager().SetTimer(AnimationTimer,FTimerDelegate::CreateWeakLambda(this,[this,PC]()
  {
   if(PC.IsValid())PC->CloseMissionPanel();else Destroy();
  }),HideAnimation->GetPlayLength(),false);
 }
 else if(auto* PC=Cast<AShooterPlayerController>(GetInstigatorController()))PC->CloseMissionPanel();
 else Destroy();
}

void AMissionTabletActor::RestoreEquipment()
{
 for(const auto& Entry:HiddenMeshes)if(Entry.Key.IsValid())Entry.Key->SetOwnerNoSee(Entry.Value);
 HiddenMeshes.Reset();
}

void AMissionTabletActor::EndPlay(const EEndPlayReason::Type Reason)
{
 GetWorldTimerManager().ClearTimer(AnimationTimer);
 RestoreEquipment();
 Super::EndPlay(Reason);
}
