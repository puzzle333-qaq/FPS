#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShooterType/ShooterType.h"
#include "HitZoneVitalsComponent.generated.h"

class UStatsComponent;

/**
 * Pure hit-zone death: head can be lethal, torso dies after N weighted hits
 * (N depends on armor), limbs never kill. HP in StatsComponent is derived only.
 */
UCLASS(ClassGroup = (FPS), meta = (BlueprintSpawnableComponent))
class FPS_API UHitZoneVitalsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHitZoneVitalsComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "FPS|HitZone")
	static UHitZoneVitalsComponent* Find(AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "FPS|HitZone")
	bool IsAlive() const;

	UFUNCTION(BlueprintPure, Category = "FPS|HitZone")
	float GetTorsoHitCount() const { return TorsoHitCount; }

	UFUNCTION(BlueprintPure, Category = "FPS|HitZone")
	float GetTorsoHitsToKill() const;

	/** Weighted ballistic hit. Returns true if the owner died from this hit. */
	bool ApplyBallisticHit(EHitZone Zone, AActor* Instigator, bool bPistolOrShotgun, float HitWeight);

	/** Inner radius is lethal; outside adds scaled torso-hit equivalents. */
	bool ApplyExplosiveHit(AActor* Instigator, float Distance, float InnerRadius, float OuterRadius);

	/** Fire tick: torso-hit equivalent only (not automatically lethal). */
	bool ApplyFireTick(AActor* Instigator);

	/** Fallback for leftover untyped DoDamage callers: one torso-weight hit. */
	bool ApplyUnspecifiedHit(AActor* Instigator, float TorsoWeight = 1.f);

	/** Server: mark combat (took damage or fired) so torso recovery resets. */
	void NoteCombatEvent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitZone|Torso", meta = (ClampMin = "1"))
	float TorsoHitsToKillNone = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitZone|Torso", meta = (ClampMin = "1"))
	float TorsoHitsToKillLight = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitZone|Torso", meta = (ClampMin = "1"))
	float TorsoHitsToKillHeavy = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitZone|Weights", meta = (ClampMin = "0.05"))
	float RifleHitWeight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitZone|Weights", meta = (ClampMin = "0.05"))
	float PistolHitWeight = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitZone|Weights", meta = (ClampMin = "0.05"))
	float ShotgunPelletWeight = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitZone|Weights", meta = (ClampMin = "0.05"))
	float MeleeHitWeight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitZone|Head")
	bool bHeadIsLethal = true;

	/** No helmet in loadout yet. Leave false; if true, head can spend saves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "FPS|HitZone|Head")
	bool bHasHelmet = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitZone|Head", meta = (ClampMin = "0"))
	int32 HelmetMaxSaves = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "FPS|HitZone|Head")
	int32 HelmetSavesRemaining = 0;

	/** Torso-hit equivalents just outside the inner (lethal) blast radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitZone|Explosion", meta = (ClampMin = "0"))
	float ExplosionMaxTorsoWeight = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitZone|Fire", meta = (ClampMin = "0"))
	float FireTickTorsoWeight = 1.f;

	/** Players default on; AI should set this false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitZone|Recovery")
	bool bOutOfCombatTorsoRecovery = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitZone|Recovery", meta = (ClampMin = "0"))
	float OutOfCombatDelay = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitZone|Recovery", meta = (ClampMin = "0.1"))
	float TorsoRecoverInterval = 10.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_TorsoHitCount, Category = "FPS|HitZone")
	float TorsoHitCount = 0.f;

	float LastCombatTime = -1000.f;
	float RecoverAccum = 0.f;

	UFUNCTION()
	void OnRep_TorsoHitCount();

	EBodyArmorType ResolveArmor() const;
	UStatsComponent* GetStats() const;
	void SyncDerivedHealth();
	bool AddTorsoHits(float Weight, AActor* Instigator);
	bool KillOwner(AActor* Instigator);
	bool ConsumeHelmetSave();
};

namespace HitZoneCombat
{
	/** Rifle / pistol / shotgun / melee weight from the firing weapon. */
	float ResolveHitWeight(bool bMelee, bool bPistolOrShotgun, int32 PelletsPerShot, float FalloffScale = 1.f);

	bool ApplyBallisticToActor(AActor* Victim, AActor* Instigator, EHitZone Zone, bool bPistolOrShotgun, float HitWeight, bool bApplyInjury);
	bool ApplyExplosiveToActor(AActor* Victim, AActor* Instigator, float Distance, float InnerRadius, float OuterRadius);
	bool ApplyFireTickToActor(AActor* Victim, AActor* Instigator);

	bool IsHitZoneVictim(AActor* Actor);
}
