#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/Texture2D.h"
#include "Interface/RecoilModifierInterface.h"
#include "Weapon/AttachmentData.h"
#include "CombatComponent.generated.h"

UENUM(BlueprintType)
enum class EWeaponSlot : uint8
{
	Main,
	Secondary,
	Pistol,
	Melee
};

class UAnimInstance;
class UAnimMontage;
class UWeaponData;
class AWeapon;

// 开镜状态改变时广播（普通 C++ 委托，不给蓝图用）
DECLARE_MULTICAST_DELEGATE_OneParam(FAimStateChanged, bool /*bIsAiming*/);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FReticleChanged, UMaterialInstanceDynamic*, ReticleMaterialInst);

// 当前武器换了（服务器 Equip 或客户端 OnRep 时广播），配件菜单用它刷新
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCurrentWeaponChanged, AWeapon*, NewWeapon);

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
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void Initiate_EquipMainWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void Initiate_EquipSeconedaryWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void Initiate_EquipMeleeWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void Initiate_EquipPistolWeapon();

	void Notify_ReloadWeapon();

	// ================= 配件（给 UI / 输入调用，本地调用即可，内部会发给服务器） =================
	UFUNCTION(BlueprintCallable, Category = "FPS|Attachment")
	void Initiate_EquipAttachment(UAttachmentData* Attachment);

	UFUNCTION(BlueprintCallable, Category = "FPS|Attachment")
	void Initiate_RemoveAttachment(FGameplayTag SlotTag);

	UFUNCTION(BlueprintCallable, Category = "FPS|Attachment")
	void Initiate_ToggleFlashlight();

	// 当前枪的开镜 FOV（已考虑瞄具配件）；没有枪时返回 Fallback
	UFUNCTION(BlueprintPure, Category = "FPS|Aiming")
	float GetCurrentAimFOV(float Fallback = 90.f) const;

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

	// 角色订阅它：开镜/关镜时刷新蓝图镜头和移动速度
	FAimStateChanged OnAimStateChanged;

	// 切枪时自动关镜（本地 + 服务器）
	void StopAimForWeaponSwitch();

	// 慢走步态时把 Strafe_Walking 写进 AnimInstance.Strafe_Standing（站立 Jog BS 没有 Walk 采样）
	void RefreshLocomotionBlendSpaces(bool bUseWalkGait);

	// Current weapon has a Walk strafe BlendSpace filled in DA_WeaponData
	bool HasStrafeWalkingAnim() const;

	UPROPERTY(Transient, BlueprintReadOnly , ReplicatedUsing = OnRep_CurrentWeapon)
	TObjectPtr<AWeapon> CurrentWeapon;	

	UPROPERTY(BlueprintAssignable)
	FCurrentReserveAmmoChanged OnCurrentReserveAmmoChanged;

	UPROPERTY(BlueprintAssignable)
	FCurrentWeaponChanged OnCurrentWeaponChanged;

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
	float ServerNextFireTime = 0.f;
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
	void ApplyFireRecoil();
	void TickRecoil(float DeltaTime);
	void ResetRecoilPattern();
	FRecoilScale ResolveRecoilScale() const;

	// X is yaw (right), Y is pitch (up) still waiting to be given back.
	FVector2D UnrecoveredRecoil = FVector2D::ZeroVector;
	int32 RecoilShotIndex = 0;
	float SecondsSinceRecoilShot = 0.f;
	bool GetReloadMontage(bool bFirstPerson, UAnimMontage*& OutMontage, UAnimInstance*& OutAnim) const;
	bool PlayReloadMontage(bool bFirstPerson);
	void OnReloadMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void OnObservedReloadMontageEnded(UAnimMontage* Montage, bool bInterrupted);

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

	// Main and secondary resolve to the first and second long gun. Pistol and melee take the first match.
	AWeapon* FindWeaponForSlot(EWeaponSlot Slot) const;
	void Initiate_EquipSlot(EWeaponSlot Slot);

	UFUNCTION(Server, Reliable)
	void Server_EquipSlot(EWeaponSlot Slot);

	// 客户端 -> 服务器：给当前枪装/卸配件、开关手电
	UFUNCTION(Server, Reliable)
	void Server_EquipAttachment(UAttachmentData* Attachment);

	UFUNCTION(Server, Reliable)
	void Server_RemoveAttachment(FGameplayTag SlotTag);

	UFUNCTION(Server, Reliable)
	void Server_ToggleFlashlight();
};
