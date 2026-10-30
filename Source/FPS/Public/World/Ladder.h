#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ladder.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UBoxComponent;

/**
 * VaultMantle: placeable ladder. Origin = ladder foot on the climb side.
 * +X (red arrow) points from the climber INTO the ladder/wall. Climb top = origin Z + Height.
 * Walk into the trigger facing the ladder and press Space to attach.
 */
UCLASS(Blueprintable)
class FPS_API ALadder : public AActor
{
	GENERATED_BODY()

public:
	ALadder();
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "50"))
	float Height = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "20"))
	float Width = 60.f;

	/** Distance from the ladder plane to the climber capsule center (behind the ladder, -X). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder")
	float StandOff = 45.f;

	/** How far past the top (+X) the climber is placed when exiting at the top. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder")
	float TopExitForward = 60.f;

	/** Hide/disable the placeholder cube (e.g. when you add your own ladder mesh in BP). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder")
	bool bShowPlaceholderMesh = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	TObjectPtr<UBoxComponent> Trigger;

	/** Capsule center while climbing with feet at FeetZ (world). */
	FVector GetClimbLocation(float FeetZ, float CapsuleHalfHeight) const;
	float GetBottomZ() const { return GetActorLocation().Z; }
	float GetTopZ() const { return GetActorLocation().Z + Height; }
	FVector GetTopExitLocation(float CapsuleHalfHeight) const;
	FVector GetClimbForward() const;
};
