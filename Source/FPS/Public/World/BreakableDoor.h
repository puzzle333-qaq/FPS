#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/Interactable.h"
#include "Interface/PlayerInterface.h"
#include "Game/CoopCqbTypes.h"
#include "BreakableDoor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UBoxComponent;
class USoundBase;

/**
 * Interactive door: F to open/close (IInteractable), optional HP for wooden doors.
 * Aim zones (CQB): Upper=gentle push, Handle=normal open/close, Bottom=kick open.
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

	UFUNCTION(BlueprintCallable, Category = "FPS|Door")
	void Auth_GentlePush(APawn* InstigatorPawn);

	UFUNCTION(BlueprintCallable, Category = "FPS|Door")
	void Auth_KickOpen(APawn* InstigatorPawn);

	UFUNCTION(BlueprintPure, Category = "FPS|Door")
	bool IsOpen() const { return bOpen; }

	UFUNCTION(BlueprintPure, Category = "FPS|Door")
	bool IsDestroyed() const { return bDestroyed; }

	UFUNCTION(BlueprintPure, Category = "FPS|Door")
	float GetHealth() const { return Health; }

	/** Resolve aim zone from pawn camera vs door leaf height. */
	UFUNCTION(BlueprintPure, Category = "FPS|Door")
	EDoorAimZone ResolveAimZone(APawn* InteractingPawn) const;

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

	/** Partial open for gentle push (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Open", meta = (ClampMin = "5", ClampMax = "60"))
	float GentlePushAngleDegrees = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Open", meta = (ClampMin = "1"))
	float OpenInterpSpeed = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Open", meta = (ClampMin = "1"))
	float KickInterpSpeed = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Open", meta = (ClampMin = "0.1"))
	float GentleInterpSpeed = 2.5f;

	/** +1 or -1 for swing direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Open")
	float OpenDirection = 1.f;

	/** Relative Z on leaf (cm from leaf origin) separating bottom / handle / upper. Leaf ~220 tall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Aim")
	float BottomZoneMaxZ = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Aim")
	float UpperZoneMinZ = 150.f;

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

	/** Current target open angle (degrees * direction). Gentle push may be partial. */
	UPROPERTY(ReplicatedUsing = OnRep_TargetOpen)
	float TargetOpenDegrees = 0.f;

	UFUNCTION()
	void OnRep_Open();

	UFUNCTION()
	void OnRep_TargetOpen();

	UFUNCTION()
	void OnRep_Destroyed();

	void ApplyOpenVisual(bool bSnap);
	void ApplyDestroyedVisual(bool bSpawnDebris);
	void Auth_DestroyDoor(AActor* InstigatorActor);
	void SpawnDebrisLocal();
	void Auth_SetTargetOpen(float DegreesAbs, float InterpSpeed);

	float CurrentYaw = 0.f;
	float TargetYaw = 0.f;
	float ActiveInterpSpeed = 6.f;
};
