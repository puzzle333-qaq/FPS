#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/Interactable.h"
#include "Interface/PlayerInterface.h"
#include "BreakableDoor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UBoxComponent;
class USoundBase;

/**
 * Interactive door: F to open/close (IInteractable), optional HP for wooden doors.
 * Bullets damage via IPlayerInterface::DoDamage (same path as TargetDummy).
 * Skip true hole-cutting; at 0 HP hide leaf + spawn simple debris.
 */
UCLASS()
class FPS_API ABreakableDoor : public AActor, public IInteractable, public IPlayerInterface
{
	GENERATED_BODY()

public:
	ABreakableDoor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	// IInteractable
	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	// IPlayerInterface — only DoDamage used for doors; other events use generated defaults
	virtual bool DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator) override;

	UFUNCTION(BlueprintCallable, Category = "FPS|Door")
	void Auth_SetOpen(bool bNewOpen);

	UFUNCTION(BlueprintPure, Category = "FPS|Door")
	bool IsOpen() const { return bOpen; }

	UFUNCTION(BlueprintPure, Category = "FPS|Door")
	bool IsDestroyed() const { return bDestroyed; }

	UFUNCTION(BlueprintPure, Category = "FPS|Door")
	float GetHealth() const { return Health; }

	/** Spawn a test door in front of Pawn (PIE). Returns spawned actor or null. */
	static ABreakableDoor* SpawnInFrontOf(APawn* Pawn, float DistCm = 180.f);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Door")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Hinge pivot; DoorLeaf rotates on this. Place actor so hinge sits on the frame edge. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Door")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Door")
	TObjectPtr<UStaticMeshComponent> DoorLeaf;

	/** Visibility + weapon hit box covering the leaf. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Door")
	TObjectPtr<UBoxComponent> InteractBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Open")
	float OpenAngleDegrees = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Open", meta = (ClampMin = "1"))
	float OpenInterpSpeed = 6.f;

	/** +1 or -1 for swing direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Open")
	float OpenDirection = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Break")
	bool bCanBreak = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Break", meta = (ClampMin = "1", EditCondition = "bCanBreak"))
	float MaxHealth = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Break")
	int32 DebrisCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Break")
	float DebrisLifeSeconds = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|FX")
	TObjectPtr<USoundBase> OpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|FX")
	TObjectPtr<USoundBase> CloseSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|FX")
	TObjectPtr<USoundBase> BreakSound;

	UPROPERTY(ReplicatedUsing = OnRep_Open)
	bool bOpen = false;

	UPROPERTY(ReplicatedUsing = OnRep_Destroyed)
	bool bDestroyed = false;

	UPROPERTY(Replicated)
	float Health = 150.f;

	UFUNCTION()
	void OnRep_Open();

	UFUNCTION()
	void OnRep_Destroyed();

	void ApplyOpenVisual(bool bSnap);
	void ApplyDestroyedVisual(bool bSpawnDebris);
	void Auth_DestroyDoor(AActor* InstigatorActor);
	void SpawnDebrisLocal();

	float CurrentYaw = 0.f;
	float TargetYaw = 0.f;
};
