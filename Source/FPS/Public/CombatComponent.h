#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/Texture2D.h"
#include "CombatComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class UWeaponData;
class AWeapon;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FReticleChanged, UMaterialInstanceDynamic*, ReticleMaterialInst);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCurrentReserveAmmoChanged, int32, RoundsInServe, int32, RoundsInWeapon, UTexture2D*, WeaponIcon);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class FPS_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:		
	UCombatComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "FPS|Combat")
	static UCombatComponent* FindCombatComponent(const AActor* Actor){ return(IsValid(Actor)?Actor->FindComponentByClass<UCombatComponent>():nullptr); }

	//Cycle to the next weapon in the inventory
	void Initiate_CycleWeapon();
	void Initiate_FireWeapon_Pressed();
	void Initiate_FireWeapon_Released();
	void Initiate_ReloadWeapon();
	void Initiate_Aim_Pressed();
	void Initiate_Aim_Released();

	void Notify_ReloadWeapon();

	// Moves rounds from reserve into the magazine. Reload input does not call this.
	// A montage notify calls it at the insert frame.
	UFUNCTION(BlueprintCallable, Category = "FPS|Combat")
	bool ApplyReload();

	UPROPERTY(BlueprintAssignable)
	FReticleChanged OnReticleChanged;


	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category = "FPS|Weapon")
	TObjectPtr<UWeaponData> WeaponData;


	void Equip(AWeapon* Weapon);
	void SpawnInventory();
	void DestroyInventory();

	UPROPERTY(BlueprintReadOnly,Replicated)
	bool bAiming;

	UPROPERTY(Transient, BlueprintReadOnly , ReplicatedUsing = OnRep_CurrentWeapon)
	TObjectPtr<AWeapon> CurrentWeapon;	

	UPROPERTY(BlueprintAssignable)
	FCurrentReserveAmmoChanged OnCurrentReserveAmmoChanged;

	void InitializeWeaponWidget();
	void NotifyAmmoChanged();

	UPROPERTY(ReplicatedUsing = OnRep_CurrentReserveAmmo)
	int32 CurrentReserveAmmo;



protected:

	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon")
	float TraceLength;
private:	

	TMap<FGameplayTag, int32> ReserveAmmo;
	bool bTriggerPressed;
	FTimerHandle FireTimer;
	void FireTimerFinished();

	UFUNCTION()
	void OnRep_CurrentWeapon(AWeapon* LastWeapon);

	UPROPERTY(Transient,Replicated)
	TArray<AWeapon*> Inventory;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon")
	TArray<TSubclassOf<AWeapon>> DefaultWeaponClass;
	
	AWeapon* SpawnWeapon(TSubclassOf<AWeapon> WeaponClass) const;

	UFUNCTION(Server,Reliable)
	void Server_Aim(bool bPressed);

	UFUNCTION(Server, Reliable)
	void Server_FireWeapon(const FHitResult& Hit);

	UFUNCTION(NetMulticast,Reliable)
	void Multicast_FireWeapon(const FHitResult& Hit ,int32 AuthAmmo);
	
	void Local_Aim(bool bPressed);

	void Local_FireWeapon();
	bool GetReloadMontage(bool bFirstPerson, UAnimMontage*& OutMontage, UAnimInstance*& OutAnim) const;
	bool PlayReloadMontage(bool bFirstPerson);
	void OnReloadMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION(Server, Reliable)
	void Server_ReloadWeapon();

	UFUNCTION(Client,Reliable)
	void Client_ReloadWeapon(int32 NewWeaponAmmo, int32 NewCarryAmmo);

	UFUNCTION(Server, Reliable)
	void Server_EndReload();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ReloadWeapon();

	UFUNCTION()
	void OnRep_CurrentReserveAmmo();

	int32 AdvanceWeaponIndex();
	int32 LocalWeaponIndex;

	void Local_CycleWeapon(int32 WeaponIndex, bool bFirstPerson);
	void ApplyEquippedWeaponAnim(AWeapon* Weapon);
	void PlayEquipMontage(AWeapon* Weapon, bool bFirstPerson);
	void OnEquipMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION(Server,Reliable)
	void Server_CycleWeapon(int32 WeaponIndex);

	UFUNCTION(NetMulticast,Reliable)
	void Multicast_CycleWeapon(int32 WeaponIndex);
};
