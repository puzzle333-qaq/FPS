#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RestraintActionComponent.generated.h"
USTRUCT()
struct FRestraintActionState
{
 GENERATED_BODY()
 UPROPERTY() TObjectPtr<AActor> Target;
 UPROPERTY() float StartedAt=0.f;
 UPROPERTY() float Duration=3.6f;
};
/** Owning arms and the replicated operator body perform the same timed restraint action. */
UCLASS(ClassGroup=NPC,meta=(BlueprintSpawnableComponent))
class FPS_API URestraintActionComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 URestraintActionComponent();
 bool BeginRestraint(AActor* Target,float Duration);
 void EndRestraint();
 bool IsActive()const{return IsValid(State.Target);}
 float GetDuration()const{return State.Duration;}
 void CancelForCombatAction();
 UFUNCTION(BlueprintPure,Category=Interaction) bool IsRestraining()const{return IsActive();}
 UPROPERTY(EditDefaultsOnly,Category=Interaction) TSoftObjectPtr<class UAnimSequence> FirstPersonAnimation;
 UPROPERTY(EditDefaultsOnly,Category=Interaction) TSoftObjectPtr<UAnimSequence> ThirdPersonAnimation;
 UPROPERTY(EditDefaultsOnly,Category=Interaction) TSoftObjectPtr<class USkeletalMesh> FirstPersonSleevesMesh;
 // Authored FK-only kneeling pair; no synthetic spine or arm stretching.
 UPROPERTY(EditDefaultsOnly,Category=Interaction) FVector OperatorAlignment=FVector(-45.6933f,1.8707f,0);
 virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)override;
 virtual void EndPlay(const EEndPlayReason::Type Reason)override;
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const override;
private:
 UPROPERTY(ReplicatedUsing=Refresh) FRestraintActionState State;
 UFUNCTION() void Refresh();
 UFUNCTION(Server,Reliable) void Server_Cancel();
 bool bPresented=false;
 UPROPERTY(Transient) TSubclassOf<class UAnimInstance> SavedArmsClass;
 UPROPERTY(Transient) TSubclassOf<UAnimInstance> SavedBodyClass;
 UPROPERTY(Transient) TWeakObjectPtr<class AWeapon> HiddenWeapon;
 UPROPERTY(Transient) TWeakObjectPtr<class USceneComponent> SavedArmsParent;
 UPROPERTY(Transient) TWeakObjectPtr<class APlayerController> LockedController;
 UPROPERTY(Transient) TWeakObjectPtr<class AActor> IgnoredTarget;
 UPROPERTY(Transient) TWeakObjectPtr<class USkeletalMeshComponent> SavedSleevesComponent;
 UPROPERTY(Transient) TObjectPtr<USkeletalMesh> SavedSleevesMesh;
 int32 SavedSleevesLOD=0;
 bool bSleevesWereVisible=true;
 FTransform SavedArmsTransform;
 FVector SavedCameraLocation=FVector::ZeroVector;
 FVector SavedCameraWorld=FVector::ZeroVector;
 FRotator SavedControlRotation=FRotator::ZeroRotator;
 bool bCameraSaved=false;
};
