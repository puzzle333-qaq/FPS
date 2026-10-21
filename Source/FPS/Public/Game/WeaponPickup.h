#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/Interactable.h"
#include "Loadout/LoadoutTypes.h"
#include "WeaponPickup.generated.h"

class UBoxComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class AWeapon;
class UAttachmentData;

// 掉在地上的枪（爆破死亡掉落 / 换枪丢下）。F 拾取，同槽位的枪会被换下来丢在地上。
UCLASS()
class FPS_API AWeaponPickup : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AWeaponPickup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 服务器：在 Location 附近生成并贴地
	static AWeaponPickup* SpawnDropped(UWorld* World, TSubclassOf<AWeaponPickup> PickupClass, const FWeaponPreset& Preset, const FVector& Location, float Yaw, AActor* IgnoreActor);

	UFUNCTION(BlueprintPure, Category = "FPS|Pickup")
	TSubclassOf<AWeapon> GetWeaponClass() const { return WeaponClass; }

	FWeaponPreset GetPreset() const;

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

protected:
	// 根：射线命中用（只挡 Visibility）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Pickup")
	TObjectPtr<UBoxComponent> InteractBox;

	// 从枪蓝图的 3P 骨骼模型复制
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Pickup")
	TObjectPtr<USkeletalMeshComponent> VisualMesh;

	// 找不到枪模型时显示的小方块
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Pickup")
	TObjectPtr<UStaticMeshComponent> FallbackMesh;

	UPROPERTY(ReplicatedUsing = OnRep_Weapon)
	TSubclassOf<AWeapon> WeaponClass;

	UPROPERTY(Replicated)
	TArray<TObjectPtr<UAttachmentData>> Attachments;

	UFUNCTION()
	void OnRep_Weapon();

	void RefreshVisual();
	void SnapToGround(const FVector& Location, AActor* IgnoreActor);
};
