#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DoorGestureComponent.generated.h"
UENUM()
enum class EDoorGesture : uint8 { None, OpenLeft, OpenRight, Kick };
USTRUCT()
struct FDoorGestureState
{
 GENERATED_BODY()
 UPROPERTY() EDoorGesture Gesture=EDoorGesture::None;
 UPROPERTY() TObjectPtr<class ABreakableDoor> Door;
 UPROPERTY() float ServerStart=0.f;
 UPROPERTY() float Duration=2.2f;
 UPROPERTY() int32 Serial=0;
 UPROPERTY() bool bReverse=false;
 UPROPERTY() uint8 ResumeMovementMode=1;
 UPROPERTY() uint8 ResumeCustomMode=0;
};
/** Server-timed body-only action: lock input, contact the leaf, then restore. Mesh1P is never animated. */
UCLASS(ClassGroup=Interaction,meta=(BlueprintSpawnableComponent))
class FPS_API UDoorGestureComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UDoorGestureComponent();
 bool BeginDoorAction(class ABreakableDoor* Door,bool bKick,bool bLeft,bool bClosing=false);
 void StopGesture();
 bool IsActive()const{return State.Gesture!=EDoorGesture::None;}
 virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)override;
 virtual void EndPlay(const EEndPlayReason::Type Reason)override;
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const override;
 UPROPERTY(EditDefaultsOnly,Category=Interaction) TSoftObjectPtr<class UAnimSequence> OpenLeftAnimation;
 UPROPERTY(EditDefaultsOnly,Category=Interaction) TSoftObjectPtr<UAnimSequence> OpenRightAnimation;
 UPROPERTY(EditDefaultsOnly,Category=Interaction) TSoftObjectPtr<UAnimSequence> KickAnimation;
 UPROPERTY(EditDefaultsOnly,Category=Interaction) float OpenDuration=1.f;
 UPROPERTY(EditDefaultsOnly,Category=Interaction) float KickDuration=2.2f;
 // Contact times measured in source-animation seconds before playback-rate scaling.
 UPROPERTY(EditDefaultsOnly,Category=Interaction) float HandleContactTime=.8f;
 UPROPERTY(EditDefaultsOnly,Category=Interaction) float KickContactTime=2.166667f;
private:
 UPROPERTY(ReplicatedUsing=Refresh) FDoorGestureState State;
 UPROPERTY(Transient) TSubclassOf<class UAnimInstance> SavedBodyClass;
 UPROPERTY(Transient) TWeakObjectPtr<class USkeletalMeshComponent> HiddenGun;
 UPROPERTY(Transient) TWeakObjectPtr<class APlayerController> LockedController;
 bool bPlaying=false;
 bool bGunWasHidden=false;
 bool bPawnInputWasEnabled=true;
 FTimerHandle FinishTimer;
 FTimerHandle ContactTimer;
 UFUNCTION() void Refresh();
 void ContactDoor();
 void FinishPresentation();
};
