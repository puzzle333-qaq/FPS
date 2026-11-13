#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BloodPool.generated.h"

class UDecalComponent;
class USphereComponent;
class UMaterialInterface;

/** Spreading wet blood puddle. Stepping in it marks bloody feet. Client-visual, spawned from hit FX. */
UCLASS()
class FPS_API ABloodPool : public AActor
{
	GENERATED_BODY()

public:
	ABloodPool();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Place or merge a puddle on the floor under a flesh hit. */
	static void NotifyFloorHit(UWorld* World, const FVector& FloorLocation, const FVector& FloorNormal);

	void AddVolume(float ExtraRadius);

	UPROPERTY(EditDefaultsOnly, Category = "FPS|BloodPool")
	TSoftObjectPtr<UMaterialInterface> PuddleMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|BloodPool", meta = (ClampMin = "10"))
	float StartRadius = 28.f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|BloodPool", meta = (ClampMin = "20"))
	float MaxRadius = 110.f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|BloodPool", meta = (ClampMin = "5"))
	float MergeRadius = 85.f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|BloodPool", meta = (ClampMin = "0.05"))
	float SpreadTime = 0.85f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|BloodPool", meta = (ClampMin = "1"))
	float LifeSpanSeconds = 28.f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|BloodPool", meta = (ClampMin = "1"))
	int32 MaxActivePools = 24;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|BloodPool", meta = (ClampMin = "1"))
	int32 StepsOnEnter = 8;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|BloodPool", meta = (ClampMin = "0.05"))
	float StepMarkCooldown = 0.35f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "FPS|BloodPool")
	TObjectPtr<UDecalComponent> Decal;

	UPROPERTY(VisibleAnywhere, Category = "FPS|BloodPool")
	TObjectPtr<USphereComponent> StepTrigger;

	float TargetRadius = 28.f;
	float CurrentRadius = 8.f;
	float Age = 0.f;
	TMap<TWeakObjectPtr<AActor>, float> NextMarkTime;

	void UpdateDecalSize();
	void TryMarkOverlaps(float Now);
	static void PrunePools();
	static TArray<TWeakObjectPtr<ABloodPool>> ActivePools;
	static UMaterialInterface* ResolveMaterial(const ABloodPool* CDO);
};
