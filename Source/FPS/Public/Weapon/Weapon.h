

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Weapon.generated.h"

enum EPhysicalSurface : int;

UENUM(BlueprintType)
enum class EFireType : uint8
{
	Single UMETA(DisplayName = "Single"),
	Automatic UMETA(DisplayName = "Automatic")
};

UCLASS()
class FPS_API AWeapon : public AActor
{
	GENERATED_BODY()
	
public:	
	AWeapon();
	virtual void OnRep_Instigator() override;

	USkeletalMeshComponent* GetMesh1P() const;
	USkeletalMeshComponent* GetMesh3P() const;

	void AttachToOwingPawn() const;
	void WeaponTrace(FHitResult& OutHit, float TraceLength);

	UPROPERTY(EditAnywhere,BlueprintReadOnly , Category = "FPS|WeaponType")
	FGameplayTag WeaponType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Aiming")
	float AimFeildOfView;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|FireType")
	EFireType FireType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|FireType")
	float FireTime;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Trace")
	float TraceRadius;

	void Local_Fire(const FVector& ImpactPoint, const FVector& ImpactNormal,TEnumAsByte<EPhysicalSurface>ImpactSurfaceType,bool bIsFirstPerson);

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintImplementableEvent)
	void FireEffect(const FVector& ImpactPoint, const FVector& ImpactNormal, EPhysicalSurface ImpactSurfaceType, bool bIsFirstPerson);

	//Weapon Mesh 1st person view
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	TObjectPtr<USkeletalMeshComponent> Mesh1P;

	//Weapon Mesh 3rd person view
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	TObjectPtr<USkeletalMeshComponent> Mesh3P;

private:


	void SetMeshVisibility(APawn* OwningPawn) const;
};
