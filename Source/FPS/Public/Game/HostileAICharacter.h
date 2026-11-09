#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interface/PlayerInterface.h"
#include "Game/HostileSquadTypes.h"
#include "Interface/Interactable.h"
#include "HostileAICharacter.generated.h"

class UStatsComponent;
class USkeletalMeshComponent;

/**
 * Minimal PVE hostile: takes IPlayerInterface::DoDamage like TargetDummy,
 * walks/shoots via AHostileAIController. No PlayerState.
 */
UCLASS()
class FPS_API AHostileAICharacter : public ACharacter, public IPlayerInterface, public IInteractable
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

	/** BP_Hostile plays the rifle Blueprint here: gun mesh, tracer, gunshot. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Hostile")
	void OnHostileFired(FVector Muzzle, FVector Impact);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_HostileFired(FVector Muzzle, FVector Impact);

	UFUNCTION(BlueprintPure, Category = "FPS|Hostile")
	UStatsComponent* GetStats() const { return Stats; }

	UFUNCTION(BlueprintPure, Category = "FPS|Hostile")
	EHostileMorale GetMorale() const { return Morale; }

	void SetMorale(EHostileMorale NewMorale);
 bool IsArrested() const { return bArrested; }
 virtual bool CanInteract_Implementation(APawn* Pawn) const override;
 virtual FText GetInteractPrompt_Implementation(APawn* Pawn) const override;
 virtual void Interact_Implementation(APawn* Pawn) override;
 UPROPERTY(ReplicatedUsing=OnRep_Arrested, BlueprintReadOnly, Category="Mission") bool bArrested = false;
 UFUNCTION() void OnRep_Arrested();

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

	/**
	 * Squad job for this body. A spawner with two or more hostiles overwrites it:
	 * Suppressor, Closer, Flanker, then repeat.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile")
	EHostileSquadRole SquadRole = EHostileSquadRole::Closer;

	/** Set when a spawner assigned SquadRole. A one-bot wave is then returned to Closer. */
	bool bAutoSquadRole = false;

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

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "FPS|Hostile")
	EHostileMorale Morale = EHostileMorale::Fighting;

	UFUNCTION()
	void OnRep_Dead();
};
