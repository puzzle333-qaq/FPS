#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GlowStick.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UProjectileMovementComponent;

/** Dropped chem light. Glows on the ground, then goes out. */
UCLASS()
class FPS_API AGlowStick : public AActor
{
	GENERATED_BODY()

public:
	AGlowStick();

	virtual void BeginPlay() override;

	void Launch(const FVector& Direction, float Speed);

protected:
	UPROPERTY(VisibleAnywhere, Category = "FPS|GlowStick")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "FPS|GlowStick")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "FPS|GlowStick")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(VisibleAnywhere, Category = "FPS|GlowStick")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UFUNCTION()
	void OnStopped(const FHitResult& Hit);
};
