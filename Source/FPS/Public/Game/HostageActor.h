#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/HostageProtectInterface.h"
#include "Interface/Interactable.h"
#include "Interface/PlayerInterface.h"
#include "HostageActor.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
UENUM(BlueprintType)
enum class EHostageBehavior : uint8 { Calm, Scared, SeekingCover, EnteringCover, Hiding, Emerging, Complying, Compliant, Binding, Secured };

/**
 * Placeable hostage stub. Tag "Hostage" + IHostageProtectInterface (muzzle dip).
 * Damage via IPlayerInterface::DoDamage. Follow/cover AI later.
 */
UCLASS()
class FPS_API AHostageActor : public AActor, public IHostageProtectInterface, public IInteractable, public IPlayerInterface
{
	GENERATED_BODY()
 friend class FDirectedComplianceTest;
 friend class FMissionObjectiveFlowTest;

public:
	AHostageActor();

	virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UFUNCTION(BlueprintCallable,Category="FPS|Hostage") void Auth_ReceiveWarning(APawn* Speaker);
 void Auth_Frighten(const FVector& Threat);
 UFUNCTION(BlueprintPure,Category="FPS|Hostage") EHostageBehavior GetBehavior()const{return Behavior;}
 UFUNCTION(BlueprintPure,Category="FPS|Hostage") bool IsBehindAndClose(const APawn* Pawn) const;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="FPS|Hostage") TObjectPtr<class UCompliancePoseComponent> CompliancePose;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="FPS|Hostage") TObjectPtr<class AHostageHidePoint> PreferredHidePoint;
 UPROPERTY(EditAnywhere,Category="FPS|Hostage",meta=(ClampMin="3",ClampMax="4")) float BindDuration=3.6f;
 UPROPERTY(EditAnywhere,Category="FPS|Hostage") float WarningGraceSeconds=1.8f;
 UPROPERTY(ReplicatedUsing=OnRep_Behavior,BlueprintReadOnly,Category="FPS|Hostage") EHostageBehavior Behavior=EHostageBehavior::Calm;
 UFUNCTION() void OnRep_Behavior();
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 UFUNCTION(BlueprintPure, Category="Mission") bool IsRescued() const { return bRescued; }
 UPROPERTY(ReplicatedUsing=OnRep_RescueState, BlueprintReadOnly, Category="Mission") bool bRescued = false;
 UFUNCTION() void OnRep_RescueState();
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mission") TObjectPtr<USkeletalMeshComponent> CivilianMesh;
 UPROPERTY(EditAnywhere, Category="Mission") TSoftObjectPtr<class UAnimSequence> RescuedCrouchAnimation;

	UFUNCTION(BlueprintPure, Category = "FPS|Hostage")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "FPS|Hostage")
	bool IsHostageAlive() const { return bAlive; }

	UFUNCTION(BlueprintPure, Category = "FPS|Hostage")
	uint8 GetLastDamageTeam() const { return LastDamageTeam; }

	// IInteractable
	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	// IPlayerInterface — only DoDamage used; others return defaults
	virtual bool DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Hostage")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Hostage")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostage", meta = (ClampMin = "1"))
	float MaxHealth = 100.f;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Hostage")
	float Health = 100.f;

	UPROPERTY(ReplicatedUsing=OnRep_RescueState, VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Hostage")
	bool bAlive = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Hostage")
	uint8 LastDamageTeam = 255;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostage")
	bool bFollowing = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> FollowTarget;

	void Die(AActor* Killer);
 void UpdateBehavior();
 void StartCompliance();
 bool FindCoverPath();
 bool AdvancePath(float Speed);
 void SetBehavior(EHostageBehavior Next);
 void SetLowProfile(bool bLow);
 bool bLowProfile=false;
 float StateDeadline=0.f;
 float PathBlockedSeconds=0.f;
 FVector LastThreat=FVector::ZeroVector;
 TArray<FVector> EscapePath;
 int32 PathIndex=0;
 UPROPERTY(Transient) TWeakObjectPtr<APawn> WarningSpeaker;
 UPROPERTY(Transient) TWeakObjectPtr<APawn> BindingOfficer;
 UPROPERTY(Transient) TWeakObjectPtr<AHostageHidePoint> ActiveHidePoint;
 FTimerHandle BehaviorTimer;
};
