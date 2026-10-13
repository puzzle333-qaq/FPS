

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

// 枪上的一个配件槽位（在枪蓝图的 Class Defaults -> FPS|Attachment -> AttachmentSlots 里配置）
// 每一行 = “这把枪有一个什么槽、挂在哪个 Socket、只允许装哪些配件、默认装哪个”
USTRUCT(BlueprintType)
struct FWeaponAttachmentSlot
{
	GENERATED_BODY()

	// 槽位类型，例如 Attachment.Slot.Barrel
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment", meta = (Categories = "Attachment.Slot"))
	FGameplayTag SlotTag;

	// 配件挂在哪个 Socket。先在枪骨骼上找；枪上没有时，再在已经装上的配件上找。
	// 护木/枪管上的握把插槽就这样用：护木挂枪的 Barrel_sckt，握把再挂护木的 AFGrip_sckt。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment")
	FName SocketName;

	// 白名单：这个槽只能装这里列出的配件（配件自己的 SlotTag 也必须和本槽一致）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment")
	TArray<TObjectPtr<UAttachmentData>> AllowedAttachments;

	// 枪生成时默认装的配件（可以空着；必须在白名单里，否则会被拒绝）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment")
	TObjectPtr<UAttachmentData> DefaultAttachment;

	// 勾上后这个槽不能被卸空（比如弹匣），只能换成别的
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment")
	bool bRequired = false;
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
	// 开枪后正在拉栓/泵动。和 Reloading 一样：不能开火、换弹、切枪
	BoltCycling,

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

	// 只在服务器调用：装上配件（同槽位的旧配件会被替换）。不在白名单里会返回 false
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Attachment")
	bool EquipAttachment(UAttachmentData* Attachment);

	// 只在服务器调用：卸下某个槽位的配件（bRequired 的槽不能卸）
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Attachment")
	bool RemoveAttachment(FGameplayTag SlotTag);

	// ---------- 给 UI 用的查询 ----------
	// 这把枪有哪些槽位
	UFUNCTION(BlueprintPure, Category = "FPS|Attachment")
	TArray<FWeaponAttachmentSlot> GetAttachmentSlots() const { return AttachmentSlots; }

	// 某个槽位可以装哪些配件（没有这个槽就返回空数组）
	UFUNCTION(BlueprintPure, Category = "FPS|Attachment")
	TArray<UAttachmentData*> GetAllowedAttachmentsForSlot(FGameplayTag SlotTag) const;

	// 这个配件能不能装到这把枪上（有对应槽位 且 在白名单里）
	UFUNCTION(BlueprintPure, Category = "FPS|Attachment")
	bool CanEquipAttachment(const UAttachmentData* Attachment) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Attachment")
	bool HasAttachmentSlot(FGameplayTag SlotTag) const { return FindSlot(SlotTag) != nullptr; }

	// 只在服务器调用：开/关手电（没有手电配件时无效）
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Attachment")
	void ToggleFlashlight();

	UFUNCTION(BlueprintPure, Category = "FPS|Attachment")
	UAttachmentData* GetAttachmentInSlot(FGameplayTag SlotTag) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Attachment")
	TArray<UAttachmentData*> GetEquippedAttachments() const { return ObjectPtrDecay(EquippedAttachments); }

	// 所有配件后座倍率相乘
	UFUNCTION(BlueprintPure, Category = "FPS|Attachment")
	float GetRecoilMultiplier() const;

	// 镜外开镜 FOV：瞄具覆盖了（且 > 0）就用配件的 AimFOV，否则用枪自己的。都 <= 0 时返回 0（不变焦）。
	// 画中画的镜片倍率不在这里。可变倍率但没配画面时，整屏按当前档变焦。
	UFUNCTION(BlueprintPure, Category = "FPS|Aiming")
	float GetEffectiveAimFOV() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Attachment")
	bool HasFlashlight() const;

	// 当前装在瞄具槽上的配件数据（没装返回 nullptr）
	UFUNCTION(BlueprintPure, Category = "FPS|Attachment")
	UAttachmentData* GetOpticAttachment() const;

	// 瞄具在第一人称枪上生成的模型。画中画只替换它的画面槽材质。
	UStaticMeshComponent* GetOpticMesh1P() const;

	// 开镜时是不是用镜片画中画（UOpticScopeComponent）：
	// Magnified，或配了画面材质的 Variable。画面材质用 LensMaterial，没填则用画面槽上现成的材质。
	// 模型上有 LensSocket，或至少有 SightSocket。
	// 红点、全息、机瞄走整屏镜头 FOV。不再按武器类型判断。
	UFUNCTION(BlueprintPure, Category = "FPS|Aiming")
	bool UsesLensPiP() const;

	// 镜片内拍摄 FOV：Variable = 当前档的 ZoomLevelsFOV；Magnified = 按 Magnification 换算。镜外镜头不进这里。
	float GetPiPCaptureFOV(float BaseFOV) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Attachment")
	bool IsFlashlightOn() const { return bFlashlightOn; }

	UPROPERTY(BlueprintAssignable, Category = "FPS|Attachment")
	FAttachmentsChanged OnAttachmentsChanged;

	// IRecoilModifier
	virtual FRecoilScale GetAttachmentRecoilScale_Implementation() const override;

	// 这把枪的配件槽位表：有哪些槽、挂哪个 Socket、能装什么、默认装什么
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment", meta = (TitleProperty = "SlotTag"))
	TArray<FWeaponAttachmentSlot> AttachmentSlots;

	// 按槽位类型找到槽位配置，找不到返回 nullptr
	const FWeaponAttachmentSlot* FindSlot(const FGameplayTag& SlotTag) const;

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

	// 开镜所需时间（秒），不装配件时。配件的 AdsTimeMultiplier 会乘上去
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Aiming", meta = (ClampMin = "0"))
	float AimDownSightTime;

	// 第 8 步：腰射时手臂/枪的位置偏移（厘米，相对角色 Mesh1P 默认位置，在相机空间里）。
	// X 负数 = 往后拉近镜头，Y = 左右，Z = 上下。开镜时会平滑过渡到瞄准位置，这个偏移只影响腰射。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Aiming|Hip")
	FVector HipMesh1POffset = FVector::ZeroVector;

	// 第 8 步：腰射时的旋转偏移（度）。开镜时过渡回角色默认旋转，所以不影响瞄准对齐
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Aiming|Hip")
	FRotator HipMesh1PRotationOffset = FRotator::ZeroRotator;

	// 第 8 步：开镜时瞄准点（AimSocket/SightSocket）离眼睛（镜头）的距离，厘米，沿镜头前方。
	// 0 = 保持原来的距离不变（现在的行为）。数值越小枪越贴脸
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Aiming|ADS", meta = (ClampMin = "0"))
	float AdsEyeDistance = 0.f;


	// 实际开镜时间 = AimDownSightTime × 所有已装配件的 AdsTimeMultiplier
	UFUNCTION(BlueprintPure, Category = "FPS|Aiming")
	float GetEffectiveAdsTime() const;

	// 可变倍率瞄具有几档（不是可变倍率返回 0）
	UFUNCTION(BlueprintPure, Category = "FPS|Aiming")
	int32 GetZoomLevelCount() const;

	// 当前倍率档（本地状态，不同步：只影响自己的镜头 FOV）
	UPROPERTY(BlueprintReadOnly, Transient, Category = "FPS|Aiming")
	int32 CurrentZoomIndex = 0;

	// Socket on the weapon skeleton that should sit on the camera's view axis while aiming.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Aiming")
	FName AimSocketName;

	bool HasAimSocket() const;
	bool GetAimSocketWorldLocation(FVector& OutLocation) const;

	// 扶枪点的世界变换。bFirstPerson 用 1P 枪和握把，否则用 3P。
	// 握把挂住且模型上有 FabrikSocketName 时用握把，否则用枪骨骼上的 FABRIK。
	bool GetFabrikWorldTransform(bool bFirstPerson, FTransform& OutWorld) const;

	// 枪口挂点。枪口配件优先，其次枪管，最后是枪自己的 Muzzle Socket。
	UFUNCTION(BlueprintPure, Category = "FPS|Attachment")
	bool GetMuzzleAttachPoint(bool bFirstPerson, USceneComponent*& OutComponent, FName& OutSocketName) const;

	// 当前射击模式。同步给其他玩家（拥有者自己本地先改，跳过拥有者避免被旧值盖回）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "FPS|FireType")
	EFireType FireType;

	// 能否按键在单发/自动之间切换。步枪、冲锋枪勾上；手枪、狙击不勾
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|FireType")
	bool bCanSwitchFireMode = false;

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

	// ---------- 枪身视觉后坐（只影响第一人称手臂和枪的画面，不影响准星和弹道） ----------
	// 每发位移（厘米，镜头空间）：X = 往后退，Y = 左右随机幅度，Z = 往上抬。会乘 Pitch/Yaw 后坐倍率
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil|Visual")
	FVector VisualRecoilLocation;

	// 每发旋转：Pitch / Yaw = 每 1 度图案后坐对应枪身转多少度；Roll = 左右随机滚转幅度（度）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil|Visual")
	FRotator VisualRecoilRotation;

	// 开镜时视觉后坐再乘这个（开镜要稳，别把瞄具踢出画面）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil|Visual", meta = (ClampMin = "0"))
	float VisualRecoilAimMultiplier;

	// 枪身被踢出去的速度（越大越“脆”）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil|Visual", meta = (ClampMin = "0"))
	float VisualRecoilKickSpeed;

	// 枪身回到原位的速度
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil|Visual", meta = (ClampMin = "0"))
	float VisualRecoilRecoverySpeed;

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

	// 这把枪是不是已经换过弹（决定第一次换弹播 FirstReloadMontage）。只在本地/服务器各自记，不同步
	bool bHasReloadedOnce = false;

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
	const FAttachmentVisual* FindVisual(const FGameplayTag& SlotTag) const;
	FName GetFallbackMuzzleSocketName() const;
	bool TryAttachmentMuzzle(const FGameplayTag& SlotTag, bool bFirstPerson, USceneComponent*& OutComponent, FName& OutSocketName) const;

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
