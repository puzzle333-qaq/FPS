#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interface/PlayerInterface.h"
#include "Game/HostileSquadTypes.h"
#include "Interface/Interactable.h"
#include "Combat/HitZoneVitalsComponent.h"
#include "ShooterType/ShooterType.h"
#include "HostileAICharacter.generated.h"

class AWeapon;
class UStatsComponent;
class USkeletalMeshComponent;

/**
 * Minimal PVE hostile: takes IPlayerInterface::DoDamage like TargetDummy,
 * walks/shoots via AHostileAIController. No PlayerState.
 */
UCLASS()
class FPS_API AHostileAICharacter : public ACharacter, public IPlayerInterface, public IInteractable
{
	GENERATED_BODY()
 friend class FMissionObjectiveFlowTest;
 bool bDeploymentHidden=false;
 void RefreshDeploymentVisibility();

public:
	AHostileAICharacter(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// IPlayerInterface — only DoDamage used; others use generated defaults
	virtual bool DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator) override;
	virtual USkeletalMeshComponent* GetMesh3P_Implementation() const override;

	UFUNCTION(BlueprintPure, Category = "FPS|Hostile")
	bool IsHostileAlive() const;

	/** BP_Hostile plays the rifle Blueprint here: gun mesh, tracer, gunshot. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Hostile")
	void OnHostileFired(FVector Muzzle, FVector Impact);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_HostileFired(FVector Muzzle, FVector Impact);

	UFUNCTION(BlueprintPure, Category = "FPS|Hostile")
	UStatsComponent* GetStats() const { return Stats; }

	UFUNCTION(BlueprintPure, Category = "FPS|Hostile")
	UHitZoneVitalsComponent* GetHitZoneVitals() const { return Vitals; }

	UFUNCTION(BlueprintPure, Category = "FPS|Hostile")
	EBodyArmorType GetBodyArmor() const { return BodyArmor; }

	void Auth_Die(AActor* Killer);
	void Auth_ApplyHitZoneInjury(EHitZone Zone);

	UFUNCTION(BlueprintPure, Category = "FPS|Injury")
	bool IsArmInjured() const { return bArmInjured; }
	UFUNCTION(BlueprintPure, Category = "FPS|Injury")
	bool IsLegInjured() const { return bLegInjured; }

	void MarkBloodyFeet(int32 ExtraSteps = -1);
	void ApplyStingerEffect(float Strength, float LockDuration, float RecoverDuration);
	float GetStingerLookScale() const;

	/** Server: flashbang window where surrender weights / police warning apply. */
	void Auth_BeginFlashSurrenderWindow(float WindowMinSeconds, float WindowMaxSeconds);
	UFUNCTION(BlueprintPure, Category = "FPS|Hostile")
	bool IsInFlashSurrenderWindow() const;
	float GetArmInjurySpreadScale() const;
	float GetArmInjuryTurnScale() const;
	void RefreshInjuryMovement();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_FleshHit(FVector ImpactPoint, FVector ImpactNormal);

	UFUNCTION(BlueprintPure, Category = "FPS|Hostile")
	EHostileMorale GetMorale() const { return Morale; }

	void SetMorale(EHostileMorale NewMorale);
 void Auth_ReceiveWarning(APawn* Speaker);
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="FPS|Hostile") TObjectPtr<class UCompliancePoseComponent> CompliancePose;
 void DropCarriedWeapon();
 UFUNCTION()void OnRep_Morale();
 UPROPERTY(Replicated,BlueprintReadOnly,Category=Mission) bool bBeingRestrained=false;
 UPROPERTY(Transient) TWeakObjectPtr<APawn> RestrainingOfficer;
 float RestraintCompleteAt=0.f;
 bool CanOfficerRestrain(const APawn* Pawn)const;
 void UpdateRestraint();
 bool IsArrested() const { return bArrested; }
 virtual bool CanInteract_Implementation(APawn* Pawn) const override;
 virtual FText GetInteractPrompt_Implementation(APawn* Pawn) const override;
 virtual void Interact_Implementation(APawn* Pawn) override;
 UPROPERTY(ReplicatedUsing=OnRep_Arrested, BlueprintReadOnly, Category="Mission") bool bArrested = false;
 UFUNCTION() void OnRep_Arrested();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile", meta = (ClampMin = "1"))
	float MaxHealth = 100.f;

	/** Damage dealt per shot to players. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile", meta = (ClampMin = "1"))
	float ShotDamage = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile", meta = (ClampMin = "100"))
	float SightRange = 3500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile", meta = (ClampMin = "100"))
	float ShootRange = 2800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile", meta = (ClampMin = "0.05"))
	float FireInterval = 0.22f;

	/**
	 * Squad job for this body. A spawner with two or more hostiles overwrites it:
	 * Suppressor, Closer, Flanker, then repeat.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile")
	EHostileSquadRole SquadRole = EHostileSquadRole::Closer;

	/** Set when a spawner assigned SquadRole. A one-bot wave is then returned to Closer. */
	bool bAutoSquadRole = false;

	/** Bandit (or any) body. Empty = a random Bandit preset at spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_BodyMesh, Category = "FPS|Hostile")
	TSoftObjectPtr<USkeletalMesh> BodyMesh;

	/** Gun to spawn in the hands. Empty = a random rifle from the built-in pool. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile")
	TSubclassOf<AWeapon> WeaponClass;

	static FName HostileTag;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Hostile")
	TObjectPtr<UStatsComponent> Stats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|HitZone")
	TObjectPtr<UHitZoneVitalsComponent> Vitals;

	UPROPERTY(ReplicatedUsing = OnRep_ArmInjured)
	bool bArmInjured = false;
	UPROPERTY(ReplicatedUsing = OnRep_LegInjured)
	bool bLegInjured = false;

	UFUNCTION() void OnRep_ArmInjured();
	UFUNCTION() void OnRep_LegInjured();

	float BaseWalkSpeed = 320.f;
	float BaseWalkSpeedCrouched = 160.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile|Injury", meta = (ClampMin = "0.1", ClampMax = "1"))
	float LegInjurySpeedMultiplier = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile|Injury", meta = (ClampMin = "1"))
	float ArmInjurySpreadMultiplier = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile|Injury", meta = (ClampMin = "0.1", ClampMax = "1"))
	float ArmInjuryTurnMultiplier = 0.55f;
	int32 BloodyFootstepsRemaining = 0;
	int32 BloodyFootstepsOnHit = 8;
	float FootstepDistanceAccum = 0.f;
	bool bFootstepLeft = false;
	float StingerStrength = 0.f;
	float StingerLockUntil = 0.f;
	float StingerRecoverUntil = 0.f;
	float StingerLockDuration = 0.7f;
	float StingerRecoverDuration = 0.9f;
	float FlashSurrenderWindowUntil = 0.f;

	void SpawnBloodFootprint();
	virtual void Tick(float DeltaSeconds) override;

	void ApplyDeadVisual();

	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;

	UPROPERTY(ReplicatedUsing=OnRep_Morale, BlueprintReadOnly, Category = "FPS|Hostile")
	EHostileMorale Morale = EHostileMorale::Fighting;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "FPS|Hostile")
	EBodyArmorType BodyArmor = EBodyArmorType::None;

	UFUNCTION()
	void OnRep_Dead();

	UFUNCTION()
	void OnRep_BodyMesh();

	UFUNCTION()
	void OnRep_CarriedWeapon();

	void ChooseDefaultLookAndWeapon();
	void ApplyBodyMesh();
	void SpawnCarriedWeapon();
	void AttachCarriedWeapon();

	UPROPERTY(ReplicatedUsing = OnRep_CarriedWeapon)
	TObjectPtr<AWeapon> CarriedWeapon;
};
