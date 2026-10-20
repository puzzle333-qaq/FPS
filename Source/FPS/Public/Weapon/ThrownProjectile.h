#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ThrownProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UNiagaraSystem;
class USoundBase;

// Shared by frag / smoke / molotov / flash. C++ applies frag radial damage;
// other effects override OnDetonate in Blueprint.
UENUM(BlueprintType)
enum class EThrownEffect : uint8
{
	Frag     UMETA(DisplayName = "Frag"),
	Smoke    UMETA(DisplayName = "Smoke"),
	Molotov  UMETA(DisplayName = "Molotov"),
	Flash    UMETA(DisplayName = "Flash")
};

UCLASS()
class FPS_API AThrownProjectile : public AActor
{
	GENERATED_BODY()

public:
	AThrownProjectile();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "FPS|Throwable")
	void Launch(const FVector& Direction, float Speed);

	UFUNCTION(BlueprintCallable, Category = "FPS|Throwable")
	void Detonate();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Throwable")
	EThrownEffect EffectType = EThrownEffect::Frag;

	// Seconds after launch. Frag uses a fuse; impact weapons set this high and tick bDetonateOnImpact.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Throwable", meta = (ClampMin = "0.05"))
	float FuseTime = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Throwable")
	bool bDetonateOnImpact = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Throwable|Damage", meta = (ClampMin = "0"))
	float Damage = 100.f;

	// Full damage inside this radius (cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Throwable|Damage", meta = (ClampMin = "0"))
	float InnerRadius = 150.f;

	// Zero damage at this radius (cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Throwable|Damage", meta = (ClampMin = "1"))
	float DamageRadius = 550.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Throwable|Damage")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Throwable|Movement", meta = (ClampMin = "0"))
	float GravityScale = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Throwable|Movement")
	bool bShouldBounce = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Throwable|Movement", meta = (ClampMin = "0", ClampMax = "1"))
	float Bounciness = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Throwable|Movement", meta = (ClampMin = "0", ClampMax = "1"))
	float Friction = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Throwable|FX")
	TObjectPtr<UNiagaraSystem> ExplosionFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Throwable|FX")
	TObjectPtr<USoundBase> ExplosionSound;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Throwable")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Throwable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Throwable")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UFUNCTION(BlueprintNativeEvent, Category = "FPS|Throwable")
	void OnDetonate();
	virtual void OnDetonate_Implementation();

	UFUNCTION()
	void OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void ApplyThrownEffect();
	void ApplyFragDamage();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDetonate();

	FTimerHandle FuseTimer;
	bool bDetonated = false;
	bool bLaunched = false;
};
