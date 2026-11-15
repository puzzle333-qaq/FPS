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
class UStaticMesh;
class UArrowComponent;

UENUM(BlueprintType)
enum class EDoorHingeEdge : uint8
{
	NegativeY UMETA(DisplayName = "Negative Y"),
	PositiveY UMETA(DisplayName = "Positive Y"),
	NegativeX UMETA(DisplayName = "Negative X"),
	PositiveX UMETA(DisplayName = "Positive X")
};

/**
 * Reusable CQB door for doorframes (Lvl_boat etc).
 * - Swap DoorLeafMesh in Details (any static mesh).
 * - Look up + F = slow quiet push open.
 * - Look down + F = fast loud kick open.
 * - Always swings away from the interacting player (two-way).
 * Optional HP break (shoot to shatter) still supported.
 */
UCLASS()
class FPS_API ABreakableDoor : public AActor, public IInteractable, public IPlayerInterface
{
	GENERATED_BODY()

public:
	ABreakableDoor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	// IInteractable
	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	// IPlayerInterface
	virtual bool DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator) override;

	UFUNCTION(BlueprintCallable, Category = "FPS|Door")
	void Auth_SetOpen(bool bNewOpen);

	UFUNCTION(BlueprintCallable, Category = "FPS|Door")
	void Auth_PushOpen(APawn* InstigatorPawn);

	UFUNCTION(BlueprintCallable, Category = "FPS|Door")
	void Auth_KickOpen(APawn* InstigatorPawn);

	UFUNCTION(BlueprintCallable, Category = "FPS|Door")
	void Auth_Close(APawn* InstigatorPawn);

	UFUNCTION(BlueprintPure, Category = "FPS|Door")
	bool IsOpen() const { return bOpen; }

	UFUNCTION(BlueprintPure, Category = "FPS|Door")
	bool IsDestroyed() const { return bDestroyed; }

	UFUNCTION(BlueprintPure, Category = "FPS|Door")
	float GetHealth() const { return Health; }

	/** Look-pitch mode: up = Push, down = Kick. */
	UFUNCTION(BlueprintPure, Category = "FPS|Door")
	EDoorOpenMode ResolveOpenMode(APawn* InteractingPawn) const;

	/** +1 / -1 swing sign so the leaf moves away from the pawn. */
	UFUNCTION(BlueprintPure, Category = "FPS|Door")
	float ResolveOpenDirectionAwayFrom(APawn* InteractingPawn) const;

	/** Spawn a test door in front of Pawn (PIE). */
	static ABreakableDoor* SpawnInFrontOf(APawn* Pawn, float DistCm = 180.f);

	bool IsAimedAtBy(APawn* Pawn) const;
	bool ReserveInteraction(APawn* Pawn);
	void ReleaseInteraction(APawn* Pawn);
	void CommitAnimatedInteraction(APawn* Pawn, bool bKick, bool bClosing, float RemainingSeconds=0.f);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Door")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Hinge pivot; DoorLeaf rotates on this. Place actor origin on the frame hinge edge. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Door")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Door")
	TObjectPtr<UStaticMeshComponent> DoorLeaf;

	/** Stays on the actor root. Never swings. Leave empty if the frame is already a level Static Mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Door")
	TObjectPtr<UStaticMeshComponent> DoorFrame;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Door")
	TObjectPtr<UArrowComponent> HingeAxis;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Door")
	TObjectPtr<UBoxComponent> InteractBox;

	/** Swap this to any door leaf mesh (ship doors, warehouse, etc.). Applied in OnConstruction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Mesh")
	TObjectPtr<UStaticMesh> DoorLeafMesh;

	/** Optional frame mesh on the root. It does not rotate. The level's own frame mesh should stay where it is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Mesh")
	TObjectPtr<UStaticMesh> DoorFrameMesh;

	/**
	 * Which edge of the leaf bounds sits on the hinge.
	 * The green arrow is the axis. Put that arrow on the real hinge; the frame Static Mesh stays in the level.
	 * Do not attach this actor as a child of the frame, or the frame instance is removed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Mesh")
	EDoorHingeEdge HingeEdge = EDoorHingeEdge::NegativeY;

	/** Shift the leaf so Hinge Edge lies on the green arrow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Mesh")
	bool bSnapLeafToHingeEdge = true;

	/** Extra lift after the snap (cm). X/Y are filled from the mesh bounds while snap is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Mesh")
	FVector DoorLeafRelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Mesh")
	FRotator DoorLeafRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Mesh")
	FVector DoorLeafRelativeScale = FVector(1.f);

	/** Interact / hit box half-extents around leaf (cm). Auto-fit from mesh bounds if bAutoFitInteractBox. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Mesh")
	FVector InteractBoxExtent = FVector(6.f, 50.f, 110.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Mesh")
	FVector InteractBoxRelativeLocation = FVector(0.f, 0.f, 110.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Mesh")
	bool bAutoFitInteractBox = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Open")
	float OpenAngleDegrees = 90.f;

	/** Push opens fully but slowly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Open", meta = (ClampMin = "0.5"))
	float PushInterpSpeed = 2.2f;

	/** Kick opens fast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Open", meta = (ClampMin = "1"))
	float KickInterpSpeed = 16.f;

	/** Close speed (quiet). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Open", meta = (ClampMin = "0.5"))
	float CloseInterpSpeed = 3.5f;

	/**
	 * Manual override for swing sign when bAutoOpenAwayFromPlayer is false.
	 * +1 = positive yaw, -1 = negative yaw.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Open")
	float OpenDirection = 1.f;

	/** If true, each open chooses swing direction away from the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Open")
	bool bAutoOpenAwayFromPlayer = true;

	/** Pitch (degrees, look up positive) at/above this = Push. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Aim", meta = (ClampMin = "0", ClampMax = "80"))
	float LookUpPitchThreshold = 8.f;

	/** Pitch at/below -this = Kick. Between -threshold and +threshold defaults to Push. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Aim", meta = (ClampMin = "0", ClampMax = "80"))
	float LookDownPitchThreshold = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Break")
	bool bCanBreak = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Break", meta = (ClampMin = "1", EditCondition = "bCanBreak"))
	float MaxHealth = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Break")
	int32 DebrisCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|Break")
	float DebrisLifeSeconds = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|FX")
	TObjectPtr<USoundBase> PushSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|FX")
	TObjectPtr<USoundBase> KickSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|FX")
	TObjectPtr<USoundBase> CloseSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|FX")
	TObjectPtr<USoundBase> BreakSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|FX", meta = (ClampMin = "0", ClampMax = "2"))
	float PushSoundVolume = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|FX", meta = (ClampMin = "0", ClampMax = "2"))
	float KickSoundVolume = 1.2f;

	/** AI hear loudness 0..1 (kick uses gunshot range band). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|FX", meta = (ClampMin = "0", ClampMax = "1"))
	float KickNoiseLoudness = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Door|FX", meta = (ClampMin = "0", ClampMax = "1"))
	float PushNoiseLoudness = 0.15f;

	UPROPERTY(ReplicatedUsing = OnRep_Open)
	bool bOpen = false;

	UPROPERTY(ReplicatedUsing = OnRep_Destroyed)
	bool bDestroyed = false;

	UPROPERTY(Replicated)
	float Health = 150.f;

	/** Signed target yaw degrees (includes open direction). */
	UPROPERTY(ReplicatedUsing = OnRep_TargetOpen)
	float TargetOpenDegrees = 0.f;

	UFUNCTION()
	void OnRep_Open();

	UFUNCTION()
	void OnRep_TargetOpen();

	UFUNCTION()
	void OnRep_Destroyed();

	void ApplyDoorMeshAndBox();
	void ApplyOpenVisual(bool bSnap);
	void ApplyDestroyedVisual(bool bSpawnDebris);
	void Auth_DestroyDoor(AActor* InstigatorActor);
	void SpawnDebrisLocal();
	void Auth_SetTargetOpenSigned(float SignedDegrees, float InterpSpeed, USoundBase* Sound, float VolumeMultiplier, float NoiseLoudness, AActor* NoiseInstigator, bool bKickNoise);

	float CurrentYaw = 0.f;
	UPROPERTY(Replicated) TObjectPtr<APawn> InteractionOperator;
	UPROPERTY(Replicated) bool bInteractionCommitted = false;
	UPROPERTY(Replicated) float ReplicatedInterpSpeed = 2.2f;
 UPROPERTY(Replicated) float SwingStartTime = 0.f;
 UPROPERTY(Replicated) float SwingDuration = 0.f;
 UPROPERTY(Replicated) float SwingFromYaw = 0.f;
	UFUNCTION(NetMulticast, Reliable) void Multicast_ContactSound(USoundBase* Sound, float Volume);
	float TargetYaw = 0.f;
	float ActiveInterpSpeed = 6.f;
};
