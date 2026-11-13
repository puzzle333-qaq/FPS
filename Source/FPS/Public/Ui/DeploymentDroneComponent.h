#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeploymentDroneComponent.generated.h"
class ACameraActor;
class ALevelSequenceActor;
class ULevelSequencePlayer;
class AShooterPlayerController;
UENUM(BlueprintType)
enum class EDeploymentViewPhase:uint8 { Inactive,Orbit,Return,Descend,Signal,Raise,AwaitingMission,Complete };
/** Local presentation. The server starts combat only after every operator's Raise completes. */
UCLASS()
class FPS_API UDeploymentDroneComponent:public UActorComponent
{
 GENERATED_BODY()
public:
 UDeploymentDroneComponent();
 virtual void TickComponent(float,ELevelTick,FActorComponentTickFunction*)override;
 virtual void EndPlay(const EEndPlayReason::Type)override;
 UFUNCTION(BlueprintPure) EDeploymentViewPhase GetPresentationPhase()const{return Phase;}
 bool HasLeftRecon()const{return Phase==EDeploymentViewPhase::Raise||Phase==EDeploymentViewPhase::AwaitingMission||Phase==EDeploymentViewPhase::Complete;}
 bool HasFinishedRaise()const{return Phase==EDeploymentViewPhase::AwaitingMission||Phase==EDeploymentViewPhase::Complete;}
 UFUNCTION(BlueprintPure) float GetLoopSeconds()const;
 UFUNCTION(BlueprintPure) ACameraActor* GetDroneCamera()const{return DroneCamera;}
private:
 void StartOrbit(AShooterPlayerController*);
 void StartReturn();
 void UpdateVision(AShooterPlayerController*);
 void SetPhase(EDeploymentViewPhase);
 void Cleanup(AShooterPlayerController*,bool bRestorePlayerView=true);
 UPROPERTY(Transient) TObjectPtr<ACameraActor> DroneCamera;
 UPROPERTY(Transient) TObjectPtr<ALevelSequenceActor> SequenceActor;
 UPROPERTY(Transient) TObjectPtr<ULevelSequencePlayer> SequencePlayer;
 EDeploymentViewPhase Phase=EDeploymentViewPhase::Inactive;
 FVector ReturnFrom=FVector::ZeroVector;
 FRotator ReturnRotation=FRotator::ZeroRotator;
 float PhaseTime=0,ReturnDuration=1.5f;
 bool bScreenMessagesReserved=false,bBlackFrame=false,bRaiseStarted=false;
};
