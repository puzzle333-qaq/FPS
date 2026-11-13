#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ThrownProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
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
	Flash    UMETA(DisplayName = "Flash"),
	Stinger  UMETA(DisplayName = "Stinger")
};

class UTexture2D;

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

	// 击杀播报图标。不填 = 用扔出它的投掷武器的 WeaponIcon
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FPS|Throwable|KillFeed")
	TObjectPtr<UTexture2D> KillFeedIcon;

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
	void ApplyFlashWash();
	void ApplyStingerWash();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDetonate();

	FTimerHandle FuseTimer;
	bool bDetonated = false;
	bool bLaunched = false;
};

/** MVP flashbang projectile — washes nearby NVG/vision via ShooterCharacter::AddVisionFlashWash. */
UCLASS()
class FPS_API AThrownFlash : public AThrownProjectile
{
	GENERATED_BODY()
public:
	AThrownFlash();

	/** After flash, hostiles stay in a high-surrender window (server). */
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Throwable|Flash", meta = (ClampMin = "1"))
	float FlashSurrenderWindowMin = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Throwable|Flash", meta = (ClampMin = "1"))
	float FlashSurrenderWindowMax = 20.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Throwable")
	TObjectPtr<USkeletalMeshComponent> FlashMesh;

	virtual void OnDetonate_Implementation() override;
};

/** MVP molotov projectile — impact burn stand-in (radial DoDamage). Full fire pool later. */
UCLASS()
class FPS_API AThrownMolotov : public AThrownProjectile
{
	GENERATED_BODY()
public:
	AThrownMolotov();
};

/** Stinger / sting grenade: locks look / AI turn. No damage, no whiteout. */
UCLASS()
class FPS_API AThrownStinger : public AThrownProjectile
{
	GENERATED_BODY()
public:
	AThrownStinger();

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Throwable|Stinger", meta = (ClampMin = "0.05"))
	float LockDuration = 0.7f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Throwable|Stinger", meta = (ClampMin = "0"))
	float RecoverDuration = 0.9f;

protected:
	virtual void OnDetonate_Implementation() override;
};
