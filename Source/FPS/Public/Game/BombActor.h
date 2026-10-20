#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/SearchDestroyTypes.h"
#include "BombActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;

// 爆破模式的 C4：地上可捡、携带、安放后倒计时。逻辑在 ASearchDestroyGameMode。
UCLASS()
class FPS_API ABombActor : public AActor
{
	GENERATED_BODY()

public:
	ABombActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	APawn* GetCarrier() const { return Carrier.Get(); }

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	bool IsPlanted() const { return bPlanted; }

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	bool IsDropped() const { return !bPlanted && !Carrier; }

	void GiveTo(APawn* NewCarrier);
	void DropAt(const FVector& WorldLocation);
	void PlantAt(const FVector& WorldLocation, EBombSiteId Site);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Bomb")
	void OnBombPickedUp(APawn* NewCarrier);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Bomb")
	void OnBombDropped();

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Bomb")
	void OnBombPlanted(EBombSiteId Site);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Bomb")
	void OnBombExploded();

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Bomb")
	void OnBombDefused();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PickedUp(APawn* NewCarrier);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Dropped();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Planted(EBombSiteId Site);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Exploded();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Defused();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Bomb")
	TObjectPtr<USphereComponent> PickupSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Bomb")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Bomb")
	FVector CarryOffset = FVector(0.f, 20.f, 40.f);

	UPROPERTY(ReplicatedUsing = OnRep_Carrier)
	TObjectPtr<APawn> Carrier;

	UPROPERTY(ReplicatedUsing = OnRep_Planted)
	bool bPlanted = false;

	UFUNCTION()
	void OnRep_Carrier();

	UFUNCTION()
	void OnRep_Planted();

	void ApplyCarryAttach();
	void ApplyWorldDrop();
	void SetPickupEnabled(bool bEnabled);
};
