#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroppedHostileWeapon.generated.h"
UCLASS()
class FPS_API ADroppedHostileWeapon : public AActor
{
 GENERATED_BODY()
public:
 ADroppedHostileWeapon();
 void Initialize(class USkeletalMeshComponent* Source,const FVector& ThrowDirection);
 virtual void BeginPlay()override;
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out)const override;
 UPROPERTY(VisibleAnywhere) TObjectPtr<class USphereComponent> Collision;
 UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Visual;
private:
 UPROPERTY(ReplicatedUsing=RefreshVisual) TObjectPtr<class USkeletalMesh> GunMesh;
 UPROPERTY(ReplicatedUsing=RefreshVisual) TArray<TObjectPtr<class UMaterialInterface>> GunMaterials;
 UPROPERTY(ReplicatedUsing=RefreshVisual) FVector VisualScale=FVector::OneVector;
 UFUNCTION()void RefreshVisual();
};
