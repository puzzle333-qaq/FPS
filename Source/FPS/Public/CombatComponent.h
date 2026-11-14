#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/Texture2D.h"
#include "Interface/RecoilModifierInterface.h"
#include "Weapon/AttachmentData.h"
#include "WeaponData.h"
#include "Weapon/Weapon.h"
#include "CombatComponent.generated.h"

UENUM(BlueprintType)
enum class EWeaponSlot : uint8
{
	Main,
	Secondary,
	Pistol,
	Melee,
	Throwable,
	Bomb
};

class UAnimInstance;
class UAnimMontage;
class USkeletalMesh;
class UAnimSequence;
class UWeaponData;
class AWeapon;

// 开镜状态改变时广播（普通 C++ 委托，不给蓝图用）
DECLARE_MULTICAST_DELEGATE_OneParam(FAimStateChanged, bool /*bIsAiming*/);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FReticleChanged, UMaterialInstanceDynamic*, ReticleMaterialInst);

// 当前武器换了（服务器 Equip 或客户端 OnRep 时广播），配件菜单用它刷新
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCurrentWeaponChanged, AWeapon*, NewWeapon);

// 射击模式切换后广播（本地），UI 绑它显示 单发/自动
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFireModeChanged, AWeapon*, Weapon, EFireType, NewFireType);

// 可变倍率瞄具切档后广播（本地），UI 可以显示当前倍率档
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FZoomLevelChanged, int32, ZoomIndex, float, AimFOV);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWeaponFired);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCurrentReserveAmmoChanged, int32, RoundsInServe, int32, RoundsInWeapon, UTexture2D*, WeaponIcon);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class FPS_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:		
 DECLARE_MULTICAST_DELEGATE_OneParam(FInspectionChanged, bool);
 FInspectionChanged OnInspectionChanged;
 FSimpleMulticastDelegate OnInspectionHudChanged;
 // True means the request was accepted; an optional missing montage completes immediately.
 bool BeginDeploymentRaise();
 bool IsDeploymentRaisePlaying()const;
	UCombatComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "FPS|Combat")
	static UCombatComponent* FindCombatComponent(const AActor* Actor){ return(IsValid(Actor)?Actor->FindComponentByClass<UCombatComponent>():nullptr); }

	//Cycle to the next weapon in the inventory
	void Initiate_FireWeapon_Pressed();
	void Initiate_FireWeapon_Released();
	// bFastDrop=true: discard current mag (HOLD R). false: keep partial mag at back of queue (TAP R).
	void Initiate_ReloadWeapon(bool bFastDrop = false);

	/** After FactionLook swaps Mesh3P skeletal mesh, re-push idle/locomotion onto AnimBPs. */
	void RefreshAnimsAfterMesh3PSwap();

	/** After FactionLook swaps Mesh1P (Bandits arms), refresh cached default arms so OverrideArms restore keeps Bandits. */
	void RecacheDefaultFirstPersonArms();
	int32 GetSpareMagCount() const;

	/** HUD: fill fractions for current mag (index 0) then each spare mag. 1=full, 0=empty. */
	UFUNCTION(BlueprintPure, Category = "FPS|Combat")
	void GetHudMagazineFills(TArray<float>& OutFills) const;

	void SyncSpareMagRoundsForHud();
	void Initiate_Aim_Pressed();
	void Initiate_Aim_Released();

	UFUNCTION(BlueprintPure, Category = "FPS|Combat")
	bool IsReloading() const;

	// 本机：打断换弹（弹匣还没插进去时）。开火/冲刺调用。成功返回 true
	UFUNCTION(BlueprintCallable, Category = "FPS|Combat")
	bool CancelReload();

	// 服务器：直接打断换弹（冲刺状态 RPC 先到时用）
	void Auth_CancelReload();

	// 服务器：不经第一人称预测，直接走玩家那条开火/换弹权威逻辑。测试 BOT 用。
	void Auth_FireWeapon(const TArray<FHitResult>& Hits);
	void Auth_ReloadWeapon(EReloadType ReloadType);

	// 射击模式切换：本地先改，再发服务器。按武器 AllowedFireModes 循环。
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void Initiate_SwitchFireMode();

	// 可变倍率瞄具：开镜时切到下一档（只改本地镜头 FOV，不需要服务器）
	UFUNCTION(BlueprintCallable, Category = "FPS|Aiming")
	void Initiate_CycleZoom();

	UPROPERTY(BlueprintAssignable, Category = "FPS|Aiming")
	FZoomLevelChanged OnZoomLevelChanged;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Weapon")
	FFireModeChanged OnFireModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Weapon")
	FWeaponFired OnWeaponFired;
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void Initiate_EquipMainWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void Initiate_EquipSeconedaryWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void Initiate_EquipMeleeWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void Initiate_EquipPistolWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void Initiate_EquipThrowableWeapon();

	/** Key 3 in police/criminal MVP: equip throwable or cycle flash/HE / molotov/HE. */
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void Initiate_EquipOrCycleThrowable();

	// 动作表按武器类型查。闪光弹没有单独的行，手持和投掷都用手雷那一套。
	FGameplayTag ResolveAnimTag(const AWeapon* Weapon) const;

	// 数字键 1-5：5 留给治疗（返回 false 让角色走 HealPlayer）。
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	bool Initiate_Hotkey(int32 Hotkey);

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	bool AllowsHeal() const;

	
	// 第 9 步：检视武器。只在本地第一人称播放，不走网络
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void Initiate_VersionWeapon();

	// 打断检视（开火、开镜、换弹、切枪、冲刺都会调用）。没在检视时什么都不做
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void CancelInspect();

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	bool IsInspecting() const;

	void Notify_ReloadWeapon();
	void Notify_ThrowWeapon();

	// ================= 配件（给 UI / 输入调用，本地调用即可，内部会发给服务器） =================
	UFUNCTION(BlueprintCallable, Category = "FPS|Attachment")
	void Initiate_EquipAttachment(UAttachmentData* Attachment);

	UFUNCTION(BlueprintCallable, Category = "FPS|Attachment")
	void Initiate_RemoveAttachment(FGameplayTag SlotTag);

	// 整备界面用：给背包里指定的那把枪装/卸，不要求它正在手上
	void Initiate_EquipAttachmentOnWeapon(AWeapon* Weapon, UAttachmentData* Attachment);
	void Initiate_RemoveAttachmentOnWeapon(AWeapon* Weapon, FGameplayTag SlotTag);

	const TArray<AWeapon*>& GetInventory() const;

	UFUNCTION(BlueprintCallable, Category = "FPS|Attachment")
	void Initiate_ToggleFlashlight();

	// 第 11 步：激光开关
	UFUNCTION(BlueprintCallable, Category = "FPS|Attachment")
	void Initiate_ToggleLaser();

	// 第 13 步：直接把激光设成开/关（侧瞄进入时开）
	UFUNCTION(BlueprintCallable, Category = "FPS|Attachment")
	void Initiate_SetLaserOn(bool bOn);

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
	// 服务器：按控制器上的配装重新生成全部武器（进局收到配装后调用）
	void RebuildInventory();

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

	/** Owner-only: rounds in each spare mag (FIFO). Used by combat HUD fill bars. */
	UPROPERTY(ReplicatedUsing = OnRep_SpareMagRounds)
	TArray<int32> SpareMagRounds;

	UFUNCTION()
	void OnRep_SpareMagRounds();



protected:

	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon")
	float TraceLength;
private:	

	TMap<FGameplayTag, int32> ReserveAmmo;
	// Spare magazines per weapon type (FIFO). CurrentReserveAmmo = queue length for HUD.
	TMap<FGameplayTag, TArray<int32>> MagQueues;
	bool bPendingFastReloadDrop = false;
	void EnsureMagQueueForWeapon(AWeapon* Weapon);
	void SeedPouchFromInventory();
	void ClearPouch();
	bool ConsumeMagFromQueue(AWeapon* Weapon, bool bFastDrop);
	void PlayDryFireClick();
	double LastDryFireTime = -10.0;
	bool bTriggerPressed;
	int32 BurstShotsRemaining = 0;
	FTimerHandle FireTimer;
	FTimerHandle ThrowReleaseTimer;
	float ServerNextFireTime = 0.f;
	bool bThrowInProgress = false;
	bool bThrowHigh = true;
	bool bThrowCommitted = false;
	bool bThrowPinPulling = false;
	bool bThrowReleaseRequested = false;
	TWeakObjectPtr<UAnimInstance> ActiveThrowAnim;
	TWeakObjectPtr<UAnimMontage> ActiveThrowMontage;
	void FireTimerFinished();
	bool StopFireIfWeaponBlocked();
	friend class FWeaponObstacleFireGateTest;

	UFUNCTION(Server, Reliable)
	void Server_BeginThrow(bool bHighThrow);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BeginThrow(bool bHighThrow);

	UFUNCTION(Server, Reliable)
	void Server_ReleaseThrow();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ReleaseThrow();

	UFUNCTION(Server, Reliable)
	void Server_CommitThrow();

	UFUNCTION(Client, Reliable)
	void Client_ThrowAmmo(int32 AuthAmmo);

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
	void Server_FireWeapon(const TArray<FHitResult>& Hits);

	UFUNCTION(NetMulticast,Reliable)
	void Multicast_FireWeapon(const TArray<FHitResult>& Hits, int32 AuthAmmo);
	
	void Local_Aim(bool bPressed);

	void Local_FireWeapon();
	/** Local owner Multicast skips 3P — play fire on Mesh3P so ToggleThirdPerson sees it. */
	void PlayOwnerThirdPersonFireMontage();
	void Initiate_Throw(bool bHighThrow);
	void ReleaseThrow();
	void TryBeginThrowRelease();
	void PlayThrowMontage(bool bFirstPerson, bool bHighThrow);
	bool PlayThrowPinPull(bool bFirstPerson);
	void BeginThrowRelease();
	UAnimMontage* ResolveThrowPinPullMontage(bool bFirstPerson) const;
	void StopActiveThrowMontages();
	void CommitThrow();
	void SpawnThrownProjectile();
	void FinishThrow();
	void EquipFallbackAfterThrow();
	AWeapon* FindFallbackWeaponAfterThrow() const;
	void OnThrowMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void OnRemoteThrowPinPullEnded(UAnimMontage* Montage, bool bInterrupted);
	void ApplyFireRecoil();
	void TickRecoil(float DeltaTime);
	void ResetRecoilPattern();
	FRecoilScale ResolveRecoilScale() const;

	// X is yaw (right), Y is pitch (up) still waiting to be given back.
	FVector2D UnrecoveredRecoil = FVector2D::ZeroVector;
	int32 RecoilShotIndex = 0;
	float SecondsSinceRecoilShot = 0.f;
	bool GetReloadMontage(bool bFirstPerson, EReloadType ReloadType, UAnimMontage*& OutMontage, UAnimInstance*& OutAnim) const;
	bool PlayReloadMontage(bool bFirstPerson, EReloadType ReloadType);
	bool PlayReloadSequences(bool bFirstPerson, EReloadType ReloadType);
	// 观战：远端角色被本机第一人称观看时，额外在 1P 手臂/枪上播（不改弹药/状态）
	bool IsViewedFirstPersonLocally() const;
	void PlaySpectated1PFire();
	void PlaySpectated1PReload(EReloadType ReloadType);
	// 按当前弹匣和“是否第一次换弹”决定这次换弹的种类
	EReloadType ChooseReloadType() const;

	// ---------- 检视（第 9 步） ----------
	// 正在播的检视蒙太奇（手臂 / 枪身），打断时停掉
	TWeakObjectPtr<UAnimMontage> ActiveInspectMontage;
	TWeakObjectPtr<UAnimMontage> ActiveInspectWeaponMontage;
	TWeakObjectPtr<AWeapon> InspectWeapon;
	void OnReloadMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void OnObservedReloadMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION(Server, Reliable)
	void Server_ReloadWeapon(EReloadType ReloadType, bool bFastDrop);

	UFUNCTION(Client,Reliable)
	void Client_ReloadWeapon(AWeapon* ReloadedWeapon, int32 NewWeaponAmmo, int32 NewCarryAmmo);

	UFUNCTION(Server, Reliable)
	void Server_EndReload(bool bCompleted);

	// 本地客户端的 1P 换弹通知帧：让服务器在这一刻加子弹
	UFUNCTION(Server, Reliable)
	void Server_ReloadNotify();

	UFUNCTION(Server, Reliable)
	void Server_CancelReload();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_CancelReload();

	// 客户端 1P 换弹通知已发给服务器（弹匣算插进去了），之后不能再打断
	bool bLocalReloadNotified = false;

	// 冲刺中按开火：先停冲刺，缓冲一下再开第一枪
	FTimerHandle SprintFireBufferTimer;
	void OnSprintFireBufferElapsed();

	// 冲刺/后摇期间按了开镜：记住意图，时间一到自动开镜
	bool bAdsInputBuffered = false;
	FTimerHandle AdsBufferTimer;
	void ClearAdsBuffer();
	void OnAdsBufferElapsed();
	// 换弹期间按了开镜：换弹结束后再开（换弹中途绝不自动开镜）
	bool bAdsPendingAfterReload = false;
	void ClearAdsPendingAfterReload();
	void TryConsumeAdsAfterReload();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ReloadWeapon(EReloadType ReloadType);

	UFUNCTION()
	void OnRep_CurrentReserveAmmo();

	int32 AdvanceWeaponIndex();
	int32 LocalWeaponIndex;

	void Local_CycleWeapon(int32 WeaponIndex, bool bFirstPerson);
	void ApplyEquippedWeaponAnim(AWeapon* Weapon);
	// Swap/restore Mesh1P mesh+AnimBP for kit weapons (PSR). Never touches Mesh1P relative transform.
	void ApplyFirstPersonArmsForWeapon(AWeapon* Weapon);
	void CacheDefaultFirstPersonArms();
	bool bCachedDefaultArms = false;
	TObjectPtr<USkeletalMesh> DefaultArmsMesh1P;
	TSubclassOf<UAnimInstance> DefaultArmsAnimClass;
	void PlayEquipMontage(AWeapon* Weapon, bool bFirstPerson);
	void OnEquipMontageEnded(UAnimMontage* Montage, bool bInterrupted, TWeakObjectPtr<AWeapon> EquippingWeapon);
	// 切枪打断：停掉上一把的拔枪 montage（1P/3P 各记一份）
	void StopActiveEquipMontages();
	TWeakObjectPtr<UAnimInstance> ActiveEquipAnim[2];
	TWeakObjectPtr<UAnimMontage> ActiveEquipMontage[2];
	// 切枪打断换弹：停掉这把枪的所有换弹 montage（手臂/3P/枪身），不给子弹
	void StopReloadMontages(AWeapon* Weapon);

	UFUNCTION(Server,Reliable)
	void Server_CycleWeapon(int32 WeaponIndex);

	UFUNCTION(NetMulticast,Reliable)
	void Multicast_CycleWeapon(int32 WeaponIndex);

	// Main and secondary resolve to the first and second long gun. Pistol and melee take the first match.
	AWeapon* FindWeaponForSlot(EWeaponSlot Slot) const;
	void Initiate_EquipSlot(EWeaponSlot Slot);


	UFUNCTION(Server, Reliable)
	void Server_EquipSlot(EWeaponSlot Slot);

	// 客户端 -> 服务器：给背包里的一把枪装/卸配件、开关手电
	UFUNCTION(Server, Reliable)
	void Server_EquipAttachment(AWeapon* Weapon, UAttachmentData* Attachment);

	UFUNCTION(Server, Reliable)
	void Server_RemoveAttachment(AWeapon* Weapon, FGameplayTag SlotTag);

	UFUNCTION(Server, Reliable)
	void Server_ToggleFlashlight();

	UFUNCTION(Server, Reliable)
	void Server_ToggleLaser();

	UFUNCTION(Server, Reliable)
	void Server_SetLaserOn(bool bOn);

	UFUNCTION(Server, Reliable)
	void Server_SetFireMode(EFireType NewFireType);

	// 当前武器现在能不能切射击模式
	bool CanSwitchFireMode() const;
};

