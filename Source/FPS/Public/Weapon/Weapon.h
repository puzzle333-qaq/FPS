

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Engine/Texture2D.h"
#include "Interface/RecoilModifierInterface.h"
#include "Weapon/AttachmentData.h"
#include "Weapon.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class USpotLightComponent;

// 一个已装配件在本地生成的模型（1P/3P 各一个）。只在本地存在，不同步
USTRUCT()
struct FAttachmentVisual
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAttachmentData> Data;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh1P;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh3P;
};

// 配件变化时广播，UI 可以绑它刷新
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAttachmentsChanged, AWeapon*, Weapon);

enum EPhysicalSurface : int;

// One shot's kick in degrees. Positive pitch looks up. Positive yaw looks right, negative yaw looks left.
USTRUCT(BlueprintType)
struct FRecoilStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil", meta = (DisplayName = "Pitch (Up +, Down -)"))
	float Pitch = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil", meta = (DisplayName = "Yaw (Right +, Left -)"))
	float Yaw = 0.f;

	FRecoilStep() = default;
	FRecoilStep(float InPitch, float InYaw) : Pitch(InPitch), Yaw(InYaw) {}
};

UENUM(BlueprintType)
enum class EFireType : uint8
{
	Single UMETA(DisplayName = "Single"),
	Automatic UMETA(DisplayName = "Automatic")
};

UENUM(BlueprintType)
enum class EWeaponStatus : uint8
{
	Idle,
	Firing,
	Reloading,
	Cycling,
	UnEquipped,

};

// Long gun, pistol, or knife. Main and secondary slots only accept a long gun.
UENUM(BlueprintType)
enum class EWeaponKind : uint8
{
	LongGun UMETA(DisplayName = "Long Gun"),
	Pistol UMETA(DisplayName = "Pistol"),
	Melee UMETA(DisplayName = "Knife")
};

UCLASS()
class FPS_API AWeapon : public AActor, public IRecoilModifier   // 实现后座接口，CombatComponent 会自动读到配件倍率
{
	GENERATED_BODY()
	
public:	
	AWeapon();
	virtual void OnRep_Instigator() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ================= 配件系统 =================

	// 只在服务器调用：装上配件（同槽位的旧配件会被替换）
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Attachment")
	bool EquipAttachment(UAttachmentData* Attachment);

	// 只在服务器调用：卸下某个槽位的配件
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Attachment")
	bool RemoveAttachment(EAttachmentSlot Slot);

	// 只在服务器调用：开/关手电（没有手电配件时无效）
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Attachment")
	void ToggleFlashlight();

	UFUNCTION(BlueprintPure, Category = "FPS|Attachment")
	UAttachmentData* GetAttachmentInSlot(EAttachmentSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Attachment")
	TArray<UAttachmentData*> GetEquippedAttachments() const { return ObjectPtrDecay(EquippedAttachments); }

	// 所有配件后座倍率相乘
	UFUNCTION(BlueprintPure, Category = "FPS|Attachment")
	float GetRecoilMultiplier() const;

	// 开镜 FOV：瞄具配件覆盖了就用配件的，否则用枪自己的 AimFeildOfView。蓝图 OnAim 请用这个
	UFUNCTION(BlueprintPure, Category = "FPS|Aiming")
	float GetEffectiveAimFOV() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Attachment")
	bool HasFlashlight() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Attachment")
	bool IsFlashlightOn() const { return bFlashlightOn; }

	UPROPERTY(BlueprintAssignable, Category = "FPS|Attachment")
	FAttachmentsChanged OnAttachmentsChanged;

	// IRecoilModifier
	virtual FRecoilScale GetAttachmentRecoilScale_Implementation() const override;

	// 枪生成时（服务器）自动装上的配件
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Attachment")
	TArray<TObjectPtr<UAttachmentData>> DefaultAttachments;

	USkeletalMeshComponent* GetMesh1P() const;
	USkeletalMeshComponent* GetMesh3P() const;
	UMaterialInstanceDynamic* GetReticleDynamicMaterialInstance();



	void AttachToOwingPawn();
	void AttachToPawnSocket(USkeletalMeshComponent* PawnMesh, FName SocketName);
	void DetachFromPawn();
	void SetHiddenForHeal(bool bHideWeapon);
	void WeaponTrace(FHitResult& OutHit, float TraceLength);

	UPROPERTY(EditAnywhere,BlueprintReadOnly , Category = "FPS|WeaponType")
	FGameplayTag WeaponType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|WeaponType")
	EWeaponKind WeaponKind;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Aiming")
	float AimFeildOfView;

	// Socket on the weapon skeleton that should sit on the camera's view axis while aiming.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Aiming")
	FName AimSocketName;

	bool HasAimSocket() const;
	bool GetAimSocketWorldLocation(FVector& OutLocation) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|FireType")
	EFireType FireType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|FireType")
	float FireTime;

	// Shot order. Past the last entry, the spray repeats that last step.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil")
	TArray<FRecoilStep> RecoilPattern;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil", meta = (ClampMin = "0"))
	float RecoilPitchScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil", meta = (ClampMin = "0"))
	float RecoilYawScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil", meta = (ClampMin = "0"))
	float AimRecoilMultiplier;

	// Degrees per second given back after RecoilRecoveryDelay, once the trigger is up.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil", meta = (ClampMin = "0"))
	float RecoilRecoverySpeed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil", meta = (ClampMin = "0"))
	float RecoilRecoveryDelay;

	// A new burst starts at the first step after the trigger has been up this long.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil", meta = (ClampMin = "0"))
	float RecoilResetTime;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Trace")
	float TraceRadius;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Damage")
	float Damage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Damage")
	float HeadshotMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Damage")
	float LimbMultiplier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Icon")
	TObjectPtr<UTexture2D> WeaponIcon;

	void Local_Fire(const FVector& ImpactPoint, const FVector& ImpactNormal,TEnumAsByte<EPhysicalSurface>ImpactSurfaceType,bool bIsFirstPerson);
	void Auth_Fire();

	void Rep_Fire(int32 AuthAmmo);

	UPROPERTY(EditAnywhere, Category = "FPS|Ammo")
	int32 Ammo;

	UPROPERTY(EditAnywhere, Category = "FPS|Ammo")
	int32 MagCapacity;

	UPROPERTY(EditAnywhere, Category = "FPS|Ammo")
	int32 StartingCarriedAmmo;

	EWeaponStatus WeaponStatus;

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
	void CaptureAuthoredMeshRelative();

	// ---------- 配件内部 ----------
	// 已装配件列表。TMap 不能网络同步，所以用数组，每个槽位最多一个
	UPROPERTY(ReplicatedUsing = OnRep_EquippedAttachments)
	TArray<TObjectPtr<UAttachmentData>> EquippedAttachments;

	UPROPERTY(ReplicatedUsing = OnRep_FlashlightOn)
	bool bFlashlightOn = false;

	UFUNCTION()
	void OnRep_EquippedAttachments();

	UFUNCTION()
	void OnRep_FlashlightOn();

	// 按 EquippedAttachments 重新生成所有配件模型和手电
	void RebuildAttachmentVisuals();
	// 手电挂到 1P 还是 3P 模型上（本地玩家看 1P，其他人看 3P）
	void UpdateFlashlightAttachment();
	void ApplyFlashlightState();
	UStaticMeshComponent* SpawnAttachmentMesh(USkeletalMeshComponent* Parent, const UAttachmentData* Data, bool bFirstPerson);
	const FAttachmentVisual* FindVisual(EAttachmentSlot Slot) const;

	UPROPERTY(Transient)
	TArray<FAttachmentVisual> AttachmentVisuals;

	UPROPERTY(Transient)
	TObjectPtr<USpotLightComponent> Flashlight;



	FTransform AuthoredMesh1PRelative = FTransform::Identity;
	FTransform AuthoredMesh3PRelative = FTransform::Identity;
	bool bCapturedAuthoredRelative = false;

	int32 Sequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> ReticleMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynMatInst_Reticle;
};
