#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/PlayerInterface.h"
#include "GasCan.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USoundBase;
class AController;

/**
 * Placeable gas can. Shoot it to explode (DoDamage = no armor).
 * Criminals place during setup; PIE console SpawnGasCan anytime.
 */
UCLASS()
class FPS_API AGasCan : public AActor, public IPlayerInterface
{
	GENERATED_BODY()

public:
	AGasCan();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	virtual bool DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator) override;

	void Auth_SetPlacer(AController* Placer);
	AController* GetPlacer() const { return PlacerController; }

	UFUNCTION(BlueprintPure, Category = "FPS|Trap")
	bool IsExploded() const { return bExploded; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap|Health", meta = (ClampMin = "1"))
	float MaxHealth = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap|Damage", meta = (ClampMin = "0"))
	float ExplosionDamage = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap|Damage", meta = (ClampMin = "1"))
	float InnerRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap|Damage", meta = (ClampMin = "1"))
	float DamageRadius = 400.f;

	/** Brief lingering fire ticks after boom (seconds). 0 = explosion only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap|Fire", meta = (ClampMin = "0"))
	float FireDuration = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap|Fire", meta = (ClampMin = "0"))
	float FireTickDamage = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap|Fire", meta = (ClampMin = "0.1"))
	float FireTickInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap|Fire", meta = (ClampMin = "1"))
	float FireRadius = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap|Limits", meta = (ClampMin = "1"))
	int32 MaxPerPlayer = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap|FX")
	TObjectPtr<USoundBase> ExplosionSound;

	static AGasCan* Auth_TryPlace(APawn* Pawn, bool bIgnoreSetup, FString* OutError);
	static int32 CountPlacedBy(UWorld* World, AController* Placer);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Trap")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Trap")
	TObjectPtr<UStaticMeshComponent> CanMesh;

	UPROPERTY(Replicated)
	float Health = 35.f;

	UPROPERTY(ReplicatedUsing = OnRep_Exploded)
	bool bExploded = false;

	UPROPERTY(Replicated)
	TObjectPtr<AController> PlacerController;

	FTimerHandle FireTimer;
	float FireTimeLeft = 0.f;

	UFUNCTION()
	void OnRep_Exploded();

	void Auth_Explode(AActor* DamageInstigator);
	void ApplyExplosionDamage(AActor* DamageInstigator);
	void Auth_FireTick();
	void ApplyVisualState();
};
