#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interface/PlayerInterface.h"
#include "HostileAICharacter.generated.h"

class UStatsComponent;
class USkeletalMeshComponent;

/**
 * Minimal PVE hostile: takes IPlayerInterface::DoDamage like TargetDummy,
 * walks/shoots via AHostileAIController. No PlayerState.
 */
UCLASS()
class FPS_API AHostileAICharacter : public ACharacter, public IPlayerInterface
{
	GENERATED_BODY()

public:
	AHostileAICharacter(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// IPlayerInterface — only DoDamage used; others use generated defaults
	virtual bool DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator) override;

	UFUNCTION(BlueprintPure, Category = "FPS|Hostile")
	bool IsHostileAlive() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Hostile")
	UStatsComponent* GetStats() const { return Stats; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile", meta = (ClampMin = "1"))
	float MaxHealth = 100.f;

	/** Damage dealt per shot to players. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile", meta = (ClampMin = "1"))
	float ShotDamage = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile", meta = (ClampMin = "100"))
	float SightRange = 3500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile", meta = (ClampMin = "100"))
	float ShootRange = 2800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile", meta = (ClampMin = "0.05"))
	float FireInterval = 0.22f;

	/** Soft mesh; falls back to engine mannequin / none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile")
	TSoftObjectPtr<USkeletalMesh> BodyMesh;

	static FName HostileTag;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Hostile")
	TObjectPtr<UStatsComponent> Stats;

	void Auth_Die(AActor* Killer);
	void ApplyDeadVisual();

	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;

	UFUNCTION()
	void OnRep_Dead();
};
