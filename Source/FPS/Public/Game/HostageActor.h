#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/HostageProtectInterface.h"
#include "Interface/Interactable.h"
#include "Interface/PlayerInterface.h"
#include "HostageActor.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * Placeable hostage stub. Tag "Hostage" + IHostageProtectInterface (muzzle dip).
 * Damage via IPlayerInterface::DoDamage. Follow/cover AI later.
 */
UCLASS()
class FPS_API AHostageActor : public AActor, public IHostageProtectInterface, public IInteractable, public IPlayerInterface
{
	GENERATED_BODY()

public:
	AHostageActor();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "FPS|Hostage")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "FPS|Hostage")
	bool IsHostageAlive() const { return bAlive; }

	UFUNCTION(BlueprintPure, Category = "FPS|Hostage")
	uint8 GetLastDamageTeam() const { return LastDamageTeam; }

	// IInteractable
	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	// IPlayerInterface — only DoDamage used; others return defaults
	virtual bool DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Hostage")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Hostage")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostage", meta = (ClampMin = "1"))
	float MaxHealth = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Hostage")
	float Health = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Hostage")
	bool bAlive = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Hostage")
	uint8 LastDamageTeam = 255;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostage")
	bool bFollowing = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> FollowTarget;

	void Die(AActor* Killer);
};
