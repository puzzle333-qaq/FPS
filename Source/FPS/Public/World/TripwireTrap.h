#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/Interactable.h"
#include "TripwireTrap.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UBoxComponent;
class USoundBase;
class AController;

/**
 * Criminal tripwire: armed during setup, triggers on pawn overlap.
 * Police (or anyone outside PC mode) can disarm with F before it fires.
 * Explosion uses DoDamage directly (no armor scale).
 */
UCLASS()
class FPS_API ATripwireTrap : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ATripwireTrap();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	UFUNCTION(BlueprintPure, Category = "FPS|Trap")
	bool IsArmed() const { return bArmed && !bTriggered && !bDisarmed; }

	UFUNCTION(BlueprintPure, Category = "FPS|Trap")
	bool IsTriggered() const { return bTriggered; }

	UFUNCTION(BlueprintPure, Category = "FPS|Trap")
	bool IsDisarmed() const { return bDisarmed; }

	void Auth_SetPlacer(AController* Placer);
	AController* GetPlacer() const { return PlacerController; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap|Damage", meta = (ClampMin = "0"))
	float ExplosionDamage = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap|Damage", meta = (ClampMin = "1"))
	float InnerRadius = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap|Damage", meta = (ClampMin = "1"))
	float DamageRadius = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap", meta = (ClampMin = "40"))
	float HalfSpan = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap", meta = (ClampMin = "1"))
	float WireHeight = 55.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap|Limits", meta = (ClampMin = "1"))
	int32 MaxPerPlayer = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap|FX")
	TObjectPtr<USoundBase> TriggerSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Trap|FX")
	TObjectPtr<USoundBase> DisarmSound;

	/** Spawn armed tripwire in front of pawn. bIgnoreSetup for PIE console. */
	static ATripwireTrap* Auth_TryPlace(APawn* Pawn, bool bIgnoreSetup, FString* OutError);
	static int32 CountPlacedBy(UWorld* World, AController* Placer);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Trap")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Trap")
	TObjectPtr<UStaticMeshComponent> PostLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Trap")
	TObjectPtr<UStaticMeshComponent> PostRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Trap")
	TObjectPtr<UStaticMeshComponent> WireMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Trap")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	bool bArmed = true;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	bool bTriggered = false;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	bool bDisarmed = false;

	UPROPERTY(Replicated)
	TObjectPtr<AController> PlacerController;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnRep_State();

	void Auth_Trigger(AActor* Victim);
	void Auth_Disarm(APawn* InteractingPawn);
	void ApplyExplosionDamage(AActor* PreferredVictim);
	void ApplyVisualState();
	bool IsPolicePawn(APawn* Pawn) const;
	void LayoutGeometry();
};
