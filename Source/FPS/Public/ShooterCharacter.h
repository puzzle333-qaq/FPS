// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include <GameFramework/SpringArmComponent.h>
#include <Camera/CameraComponent.h>
#include "GameFramework/Character.h"
#include "CombatComponent.h"
#include "Stats/StatsComponent.h"
#include "Combat/HitZoneVitalsComponent.h"
#include "ShooterType/ShooterType.h"
#include "Interface/PlayerInterface.h"
#include "Engine/NetSerialization.h"
#include "ShooterCharacter.generated.h"

class UCombatComponent;
class UPouchInventoryComponent;
class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UInputMappingContext;
class UTimelineComponent;
class UAnimMontage;
class AWeapon;
class USoundBase;
class USoundAttenuation;
class USkeletalMesh;
class UMaterialInterface;
class UNiagaraSystem;
class UWidgetComponent;
namespace WeaponObstacle { struct FProbeResult; }

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWeaponFirstReplicated, AWeapon*, Weapon);
// 第 10 步：侧瞄开/关（只在本地）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCantedSightChanged, bool, bCanted);
// Hit feedback (local only): victim damage ring / shooter hit confirm
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDamageIndicatorSignature, FVector, InstigatorLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHitConfirmedSignature);
// COD 风格命中反馈：bHeadshot = 爆头，bKilled = 这一枪打死了
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHitConfirmedDetailedSignature, bool, bHeadshot, bool, bKilled);

UENUM(BlueprintType)
enum class EHitReactDirection : uint8
{
	Front,
	Back,
	Left,
	Right
};


// 脚步：按地面 PhysicalSurface 选音效。默认指向 FPS_Controller 自带 Cue；可在 BP 覆盖。
USTRUCT(BlueprintType)
struct FFootstepSurfaceSounds
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps")
	TSoftObjectPtr<USoundBase> Default;

	// SurfaceType2 Concrete - 项目里用 Tile Cue
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps")
	TSoftObjectPtr<USoundBase> Concrete;

	// SurfaceType5 Metal
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps")
	TSoftObjectPtr<USoundBase> Metal;

	// SurfaceType4 Wood（暂无无专用 wav 时回落到 Default）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps")
	TSoftObjectPtr<USoundBase> Wood;

	// SurfaceType3 Glass
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps")
	TSoftObjectPtr<USoundBase> Glass;
};

UCLASS()
class FPS_API AShooterCharacter : public ACharacter,public IPlayerInterface
{
	GENERATED_BODY()

public:
 FSimpleMulticastDelegate OnEquipmentHudChanged;
 int32 GetGlowStickCount() const { return GlowStickCount; }
 UFUNCTION() void OnRep_GlowStickCount();

	AShooterCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual bool IsLocallyControlled() const override;
	virtual void OnRep_PlayerState() override;
	virtual void Landed(const FHitResult& Hit) override;

	// 跳跃耐力：耐力不够就不能跳；真正起跳时在服务器扣耐力
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnJumped_Implementation() override;

	// 给 AnimBP 用：绝对地面速度 + 是否该播 Walk BlendSpace（不是 Velocity/MaxWalkSpeed 归一化）
	UFUNCTION(BlueprintPure, Category = "FPS|Locomotion")
	float GetGroundSpeed() const { return GroundSpeed; }

	UFUNCTION(BlueprintPure, Category = "FPS|Locomotion")
	bool IsUsingWalkGait() const { return bUseWalkGait; }

	UFUNCTION(BlueprintPure, Category = "FPS|Locomotion")
	float GetLocomotionPlayRate() const { return LocomotionPlayRate; }

	// 控制器在改 MaxWalkSpeed 后调用，立刻刷新步态
	void RefreshLocomotionGait();

	// 脚步：着地且速度够时按距离播；蹲/趴更轻更疏，冲刺更密更响。滑铲/腾空不播。
	void UpdateFootsteps(float DeltaTime);

	// 下蹲开始/结束时（服务器和本地客户端都会调用）刷新速度
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	/** PlayerInterface */
	virtual FName GetWeaponAttachPoint_Implementation(const FGameplayTag& WeaponType)const override;
	virtual USkeletalMeshComponent* GetMesh1P_Implementation() const override;
	virtual USkeletalMeshComponent* GetMesh3P_Implementation() const override;
	virtual void WeaponReplicated_Implementation() override;
	virtual AWeapon* GetCurrentWeapon_Implementation() override;
	virtual int32 GetReserveAmmo_Implementation() const override;
	virtual void Notify_ReloadWeapon_Implementation() override;
	virtual void Notify_ThrowWeapon_Implementation() override;
	virtual bool DoDamage_Implementation(float DamageAmout,AActor* DamageInstigator) override;
	/** PlayerInterface */

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable)
	FRotator GetFixedAimRotation()const;

	UPROPERTY(BlueprintReadOnly,Category = "FPS|FABRIK")
	FTransform FABRIK_SocketTransform;

	// First-person left-hand FABRIK target in Mesh1P component space (GripSocket snap).
	UPROPERTY(BlueprintReadOnly, Category = "FPS|FABRIK")
	FTransform FABRIK_SocketTransform1P;

	// First-person FABRIK alpha. 1 when a grip/weapon hand socket exists; 0 while reloading / melee.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|FABRIK")
	float FABRIK_Alpha1P = 0.f;

	// 第三人称左手 FABRIK 强度。近战为 0，ABP 的 FABRIK Alpha 应绑这个。
	UPROPERTY(BlueprintReadOnly, Category = "FPS|FABRIK")
	float FABRIK_Alpha = 0.f;
 UPROPERTY(BlueprintReadOnly, Category="FPS|SingleBody|ADS") FTransform BodyAdsRightHand;
 UPROPERTY(BlueprintReadOnly, Category="FPS|SingleBody|ADS") float BodyAdsAlpha=0.f;
 UPROPERTY(BlueprintReadOnly, Category="FPS|SingleBody|ADS") float BodySupportHandAlpha=0.f;
 UPROPERTY(BlueprintReadOnly, Category="FPS|SingleBody") float BodyFirstPersonPoseAlpha=0.f;
 UPROPERTY(BlueprintReadOnly, Category="FPS|SingleBody") FTransform BodyLeftHandTarget;
 UPROPERTY(BlueprintReadOnly, Category="FPS|SingleBody") FVector BodyLeftElbowTarget;
 bool CanUseFirstPersonBodyPose() const;
 void UpdateBodyAdsTarget();

	// 动画曲线名。手臂蒙太奇里这条曲线为 1 时左手 IK 完全关掉（让左手去拿弹匣/拉栓），0 时正常扶枪
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|FABRIK")
	FName DisableLeftHandIKCurveName = TEXT("DisableLHandIK");

	// 勾上：换弹/拉栓期间不管有没有曲线，左手 IK 都淡出到 0
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|FABRIK")
	bool bDisableLeftHandIKDuringReload = true;

	// 左手 IK 淡入淡出速度（越大越快，10 大约 0.2~0.3 秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|FABRIK", meta = (ClampMin = "0"))
	float LeftHandIKBlendSpeed = 10.f;

	void SetWeaponAttachDebug(bool bEnabled) { bWeaponAttachDebug = bEnabled; }
	void DrawWeaponAttachDebug() const;

	UFUNCTION(BlueprintCallable)
	bool HasCurrentWeapon() const;

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void EquipMainWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void EquipSeconedaryWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void EquipMeleeWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void EquipPistolWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void VersionWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Heal")
	void HealPlayer(float InHealAmout);

	// Anim notify entry. The montage-end callback also commits once; the server applies the heal only once.
	UFUNCTION(BlueprintCallable, Category = "FPS|Heal")
	void Notify_HealPlayer();

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Heal")
	TObjectPtr<UAnimMontage> HealMontage;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Heal")
	float HealAmout;

	UPROPERTY(BlueprintAssignable)
	FWeaponFirstReplicated OnWeaponFirstReplicated;

	bool HasWeaponFirstReplicated() const { return bWeaponFirstReplicated; }

	UPROPERTY(EditDefaultsOnly, Category = "FPS|HitReact")
	TArray<TObjectPtr<UAnimMontage>> HitReacts;

	// Directional hit reacts (attacker relative to victim facing). Empty array -> falls back to HitReacts.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|HitReact")
	TArray<TObjectPtr<UAnimMontage>> HitReactsFront;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|HitReact")
	TArray<TObjectPtr<UAnimMontage>> HitReactsBack;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|HitReact")
	TArray<TObjectPtr<UAnimMontage>> HitReactsLeft;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|HitReact")
	TArray<TObjectPtr<UAnimMontage>> HitReactsRight;

	// Victim's own client: someone damaged me from this location (drives the 4-arc ring).
	UPROPERTY(BlueprintAssignable, Category = "FPS|HitFeedback")
	FDamageIndicatorSignature OnDamageIndicator;

	// Shooter's own client: my shot did damage (drives the hit marker).
	UPROPERTY(BlueprintAssignable, Category = "FPS|HitFeedback")
	FHitConfirmedSignature OnHitConfirmed;

	UFUNCTION(Client, Unreliable)
	void Client_ShowDamageIndicator(FVector_NetQuantize InstigatorLocation);

	UFUNCTION(Client, Unreliable)
	void Client_ConfirmHit(bool bHeadshot);

	// 致死一枪：给开枪的人发（可靠，击杀提示不能丢）
	UFUNCTION(Client, Reliable)
	void Client_ConfirmKill(bool bHeadshot);

	// 命中/击杀/爆头（准星和蓝图都可以绑）。击杀时 OnHitConfirmed 也会广播，兼容旧蓝图
	UPROPERTY(BlueprintAssignable, Category = "FPS|HitFeedback")
	FHitConfirmedDetailedSignature OnHitConfirmedDetailed;

	// 服务器：CombatComponent 在 DoDamage 前设，受害者 DoDamage 里读（这一发是不是打头）
	bool bPendingHeadshotHit = false;

	EHitReactDirection ComputeHitReactDirection(const FVector& SourceLocation) const;
	const TArray<TObjectPtr<UAnimMontage>>& GetHitReactPool(EHitReactDirection Direction) const;

	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	// 观战：本机正在用这个角色的第一人称视角看它（只影响本机显示：1P 手臂/枪，隐藏 3P）
	UFUNCTION(BlueprintCallable, Category = "FPS|Spectate")
	void SetFirstPersonViewedLocally(bool bViewed);

	// PIE/debug: toggle local third-person so you can see own Mesh3P / FactionLook.
	UFUNCTION(BlueprintCallable, Category = "FPS|Debug")
	void SetDebugThirdPerson(bool bThirdPerson);

	UFUNCTION(BlueprintCallable, Category = "FPS|Debug")
	void ToggleDebugThirdPerson();

	UFUNCTION(BlueprintPure, Category = "FPS|Debug")
	bool IsDebugThirdPerson() const { return bDebugThirdPerson; }

	UFUNCTION(BlueprintPure, Category = "FPS|Debug")
	bool IsSingleBodyPilot() const { return bUseSingleBodyPilot; }

	// PIE/debug: Off / NVG / Thermal camera post-process (+ battery). Console Vision / CycleVision.
	UFUNCTION(BlueprintCallable, Category = "FPS|Vision")
	void SetTacticalVisionMode(ETacticalVisionMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "FPS|Vision")
	void CycleTacticalVisionMode();

	UFUNCTION(BlueprintCallable, Category = "FPS|Vision")
	void ToggleNVG();

	// Tap N toggles tubes, hold N switches housing, Alt+N toggles local IR assist.
	UFUNCTION(BlueprintCallable, Category = "FPS|Vision")
	void CycleNVGHousing();
 UFUNCTION(BlueprintCallable, Category="FPS|Vision") void ToggleNVGInfrared();
 UFUNCTION(BlueprintPure, Category="FPS|Vision") bool IsNVGInfraredEnabled()const{return bNVGInfraredEnabled&&TacticalVisionMode==ETacticalVisionMode::NVG&&VisionBatterySeconds>0.f;}
 float GetVisionBatteryDrainMultiplier()const{return TacticalVisionMode==ETacticalVisionMode::NVG&&NVGHousing==ENVGHousing::Quad?1.5f:1.f;}


	UFUNCTION(BlueprintPure, Category = "FPS|Vision")
	ETacticalVisionMode GetTacticalVisionMode() const { return TacticalVisionMode; }

	UFUNCTION(BlueprintPure, Category = "FPS|Vision")
	ENVGHousing GetNVGHousing() const { return NVGHousing; }

	UFUNCTION(BlueprintPure, Category = "FPS|Vision")
	float GetVisionBatteryNormalized() const;

	// Call from flashbang BP (or console FlashWash) - NVG washes out harder.
	UFUNCTION(BlueprintCallable, Category = "FPS|Vision")
	void AddVisionFlashWash(float Amount);

	// 0..1 whiteout, or ghosting when this character has flash goggles.
	void ReceiveFlashbang(float Strength);

	UFUNCTION(Client, Reliable)
	void Client_ReceiveFlashbang(float Strength);

	UFUNCTION(BlueprintCallable, Category = "FPS|Vision")
	void SetHasFlashGoggles(bool bEnable);

	UFUNCTION(Server, Reliable)
	void Server_SetHasFlashGoggles(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "FPS|Vision")
	bool HasFlashGoggles() const { return bHasFlashGoggles; }

	UFUNCTION(BlueprintPure, Category = "FPS|HitFeedback")
	float GetHitBloodAlpha() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Vision")
	float GetFlashWhiteAlpha() const { return FlashWhite; }

	void AddHitBlood();

	UFUNCTION(BlueprintCallable, Category = "FPS|Vision")
	void RefillVisionBattery(float Amount = -1.f);

	// Radar accessory. Console GrantRadar.
	UFUNCTION(BlueprintCallable, Category = "FPS|Radar")
	void SetHasRadar(bool bEnable);
	UFUNCTION(Server, Reliable)
	void Server_SetHasRadar(bool bEnable);
	UFUNCTION(BlueprintPure, Category = "FPS|Radar")
	bool HasRadar() const { return bHasRadar; }

	// Watch check: play optional FP montage + briefly show remaining match time.
	UFUNCTION(BlueprintCallable, Category = "FPS|Watch")
	void CheckWatch();

	// Bloody footprints (local decals). Auto on HP drop; console MarkBlood.
	UFUNCTION(BlueprintCallable, Category = "FPS|Blood")
	void MarkBloodyFeet(int32 ExtraSteps = -1);

	// Hit spray + floor blood stain (Niagara + decal). Call on every machine that saw the hit.
	UFUNCTION(BlueprintCallable, Category = "FPS|Blood")
	void PlayBloodHitFX(FVector ImpactPoint, FVector ImpactNormal);

	// Same FX for dummies / any hit location (no character required).
	static void SpawnBloodImpactFX(UObject* WorldContext, FVector ImpactPoint, FVector ImpactNormal, AActor* IgnoreActor = nullptr);

	UFUNCTION(BlueprintCallable, Category = "FPS|Obstacle")
	void SetDebugWeaponObstacle(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "FPS|Spectate")
	bool IsFirstPersonViewedLocally() const { return bFirstPersonViewedLocally; }

	// 本机是否该显示这个角色的第一人称（自己控制 或 被本机观战）。ABP 里可替代 IsLocallyControlled
	UFUNCTION(BlueprintPure, Category = "FPS|Spectate")
	bool UsesFirstPersonVisuals() const { return IsLocallyControlled() || bFirstPersonViewedLocally; }

	UPROPERTY(Transient, BlueprintReadOnly, Category = "FPS|Spectate")
	bool bFirstPersonViewedLocally = false;

	// Local-only debug third-person camera (not replicated).
	bool bDebugThirdPerson = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Debug")
	bool bUseSingleBodyPilot = false;

 // neck_01 remains a stable positional anchor when its hidden descendants are scaled to zero.
 UPROPERTY(EditDefaultsOnly, Category = "FPS|SingleBody|Camera")
 FName SingleBodyCameraAnchorBone = TEXT("neck_01");
 /** Eye offset in actor space; bone rotation must never rotate the player's view. Units: cm. */
 UPROPERTY(EditDefaultsOnly, Category = "FPS|SingleBody|Camera")
 FVector SingleBodyCameraEyeOffset = FVector(0.f, 0.f, 14.f);
 UPROPERTY(EditDefaultsOnly, Category = "FPS|SingleBody|Camera", meta = (ClampMin = "0", Units = "cm"))
 float SingleBodyCameraProbeRadius = 4.f;

 void UpdateSingleBodyCameraLocation();
 bool bSingleBodyCameraFollowing = false;
 FVector SingleBodyCameraRestoreLocation = FVector::ZeroVector;
 FVector SingleBodyCameraLastLocation = FVector::ZeroVector;

	float DebugThirdPersonArmLength = 250.f;
	float SavedSpringArmLength = 0.f;
	bool bSavedSpringArmLength = false;

	// ===== Tactical vision (NVG / Thermal) - local camera only =====
	/** Local infrared assist: never replicated and enabled only for the owning NVG view. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Vision")
	TObjectPtr<class USpotLightComponent> NVGIlluminator;
 UPROPERTY(Transient) bool bNVGInfraredEnabled=false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Vision")
	ETacticalVisionMode TacticalVisionMode = ETacticalVisionMode::Off;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Vision")
	ENVGHousing NVGHousing = ENVGHousing::Dual;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Vision", meta = (ClampMin = "5"))
	float VisionBatteryMaxSeconds = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Vision", meta = (ClampMin = "0.1"))
	float VisionBatteryDrainPerSecond = 1.f;

	// Optional PP materials (Material Domain = Post Process). Soft-load; Engine CustomDepth used as thermal fallback.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Vision")
	TSoftObjectPtr<UMaterialInterface> NVGPostProcessMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Vision")
	TSoftObjectPtr<UMaterialInterface> ThermalPostProcessMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Vision")
	TSoftObjectPtr<UMaterialInterface> ThermalBodyOverlayMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Vision", meta = (ClampMin = "500"))
	float ThermalOverlayRangeCm = 8000.f;

	// 枪身视觉后坐总开关（第 4 步）。关掉时不加任何偏移，Mesh1P 完全不受影响，方便对比手感
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Recoil|Visual")
	bool bEnableVisualRecoil = true;

	// ---------- 程序化武器晃动（转视角拖拽 / 走路晃 / 落地下沉），叠在视觉后坐同一条 Mesh1P 偏移上 ----------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Sway")
	bool bEnableWeaponSway = true;

	// 转视角拖拽：每 1 度/秒 的视角速度给多少度反向偏转
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Sway", meta = (ClampMin = "0"))
	float SwayLookRotationScale = 0.006f;

	// 转视角拖拽：每 1 度/秒 给多少厘米横向位移
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Sway", meta = (ClampMin = "0"))
	float SwayLookLocationScale = 0.0025f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Sway", meta = (ClampMin = "0"))
	float SwayMaxRotation = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Sway", meta = (ClampMin = "0"))
	float SwayMaxLocation = 1.5f;

	// 越大越跟手（拖拽越短）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Sway", meta = (ClampMin = "0"))
	float SwaySmoothSpeed = 9.f;

	// 走路晃动：竖直 / 水平幅度（厘米，按跑步速度 RunSpeed=1 计）和频率（弧度/秒）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Sway", meta = (ClampMin = "0"))
	float BobVerticalAmount = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Sway", meta = (ClampMin = "0"))
	float BobHorizontalAmount = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Sway", meta = (ClampMin = "0"))
	float BobFrequency = 9.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Sway", meta = (ClampMin = "1"))
	float BobReferenceSpeed = 500.f;

	// 落地下沉：每 1000 cm/s 落地速度下沉多少厘米，最多多少；回弹速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Sway", meta = (ClampMin = "0"))
	float LandingDipAmount = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Sway", meta = (ClampMin = "0"))
	float LandingDipMax = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Sway", meta = (ClampMin = "0"))
	float LandingDipRecoverySpeed = 7.f;

	// 开镜时以上所有晃动乘这个
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Sway", meta = (ClampMin = "0", ClampMax = "1"))
	float SwayAimMultiplier = 0.2f;

	// 每发开火由 CombatComponent 调用：给第一人称手臂加一次视觉后坐（镜头空间的位移和旋转）
	void AddVisualRecoil(const FVector& KickLocation, const FRotator& KickRotation);
	UCombatComponent* GetCombatComponent() const { return Combat; }
	UPouchInventoryComponent* GetPouch() const { return Pouch; }
	float GetDefaultFieldOfView() const { return DefaultFOV; }
	// 设置里的 FOV（只改本机镜头）
	void SetDefaultFieldOfView(float InFOV);
	float GetAimFOVInterpSpeed() const { return AimFOVInterpSpeed; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ===== 左右探头（Lean）=====
	// 平滑后的探头值：-1 = 左探，0 = 不探，1 = 右探。所有端都有（3P 动画蓝图读它驱动脊椎）
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Lean")
	float LeanAlpha = 0.f;

	// 目标探头值（服务器同步给其他玩家；本地玩家自己算）
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "FPS|Lean")
	float LeanTarget = 0.f;

	UFUNCTION(BlueprintPure, Category = "FPS|Lean")
	float GetLeanAlpha() const { return LeanAlpha; }

	// 1P 相机横移距离（cm）。Q/E 用这个距离，Alt+A/D 再乘 AltPeekLeanScale。侧倾角度两边一样。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Lean")
	float LeanCameraOffset = 30.f;

	// Alt+A / Alt+D 往侧面跨出的距离倍数。不改变探头角度。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Lean", meta = (ClampMin = "1.0"))
	float AltPeekLeanScale = 2.f;

	UFUNCTION(BlueprintPure, Category = "FPS|Lean")
	bool IsAltPeekCrouching() const { return bAltPeekCrouch; }

	// 1P 相机侧倾角度（度）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Lean")
	float LeanRollDegrees = 12.f;

	// 插值速度（越大越快）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Lean")
	float LeanInterpSpeed = 10.f;

	// 3P 动画蓝图建议用的脊椎侧倾角度（ABP 里 LeanAlpha * 这个值）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Lean")
	float LeanSpineDegrees = 20.f;

	// true = 按一下切换，false = 按住探头
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Lean")
	bool bLeanToggleMode = false;

	// 防穿墙：探测球半径
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Lean")
	float LeanWallProbeRadius = 12.f;

	// 留空时运行时自动创建并绑定 Q / E
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> LeanLeftAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> LeanRightAction;

	// ===== Prone (趴下/趴行) =====
	// Server-authoritative: the owner predicts locally, the server validates and replicates bIsProne.
	// Prone = CMC crouch with a smaller CrouchedHalfHeight, so capsule/network move prediction reuse crouch.
	UFUNCTION(BlueprintPure, Category = "FPS|Prone")
	bool IsProne() const { return bIsProne; }

	// bExitToCrouch: when leaving prone, go to crouch instead of standing.
	UFUNCTION(BlueprintCallable, Category = "FPS|Prone")
	void RequestSetProne(bool bNewProne, bool bExitToCrouch = false);

	// True while prone (replicated to everyone). 3P ABP: drive the Prone state from this.
	UPROPERTY(ReplicatedUsing = OnRep_IsProne, BlueprintReadOnly, Category = "FPS|Prone")
	bool bIsProne = false;

	// Smoothed 0..1 prone blend (all machines). Use for camera / optional ABP blends.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Prone")
	float ProneAlpha = 0.f;

	// Prone AND moving (ground speed > ProneCrawlSpeedThreshold). ABP: crawl full-body BS_Prone when true;
	// when false (prone, stopped) use prone idle body + layered upper-body gun hold. 3P FABRIK only off while crawling.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Prone")
	bool bProneCrawling = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Prone", meta = (ClampMin = "0.0"))
	float ProneCrawlSpeedThreshold = 10.f;

	// Empty = runtime IA bound to Z (like Lean Q/E).
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> ProneAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Prone", meta = (ClampMin = "10.0"))
	float ProneCapsuleHalfHeight = 34.f;

	// Extra 1P camera drop while prone (SpringArm relative Z), on top of the capsule shrinking.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Prone")
	float ProneCameraZOffset = -50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Prone", meta = (ClampMin = "0.0"))
	float ProneCameraInterpSpeed = 6.f;

	// Authored crawl speed of the prone BlendSpace (LocomotionPlayRate = GroundSpeed / this).
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Locomotion", meta = (ClampMin = "1.0"))
	float ProneAnimReferenceSpeed = 100.f;

	// Third-person: hide the weapon while prone (UnarmedProne anims hold no gun). False = keep it in hand, FABRIK off.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Prone")
	bool bHideWeapon3PWhileProne = false;

	// ===== Slide (滑铲) =====
	// Pressing a crouch key (Ctrl hold / C toggle) above SlideMinSpeed slides instead of crouching.
	// Server-authoritative like prone: owner predicts, Server_SetSliding validates, bIsSliding replicates.
	UFUNCTION(BlueprintPure, Category = "FPS|Slide")
	bool IsSliding() const { return bIsSliding; }

	// Called by the controller on a crouch key press. Returns true if a slide started (then do not crouch).
	bool TryStartSlide();

	// Whether the slide should end crouched (C, or Ctrl still held) or standing (Ctrl released).
	void SetSlideEndCrouch(bool bCrouch) { bSlideEndCrouch = bCrouch; }

	// Jump while sliding: end the slide and stand up.
	void CancelSlideToStand();

	UPROPERTY(ReplicatedUsing = OnRep_IsSliding, BlueprintReadOnly, Category = "FPS|Slide")
	bool bIsSliding = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Slide")
	float SlideAlpha = 0.f;

	// Tactical mode: slide disabled. Leave code paths intact for possible future modes.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Slide")
	bool bEnableSlide = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Slide")
	float SlideMinSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Slide", meta = (ClampMin = "0.1"))
	float SlideDuration = 0.9f;

	// Extra horizontal speed added at slide start (cm/s).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Slide")
	float SlideSpeedBoost = 150.f;

	// Linear deceleration while sliding (cm/s^2). ~ (800+150)/0.9 ≈ 1000 stops near the end.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Slide")
	float SlideDeceleration = 800.f;

	// The slide ends early below this speed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Slide")
	float SlideEndSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Slide")
	float SlideCameraZOffset = -15.f;

	// COD 式两段减速：前 SlideDecelSwitchTime 秒用 Early（保速冲出去），之后用 Late（干脆收尾）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Slide")
	float SlideDecelEarly = 300.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Slide")
	float SlideDecelLate = 1200.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Slide")
	float SlideDecelSwitchTime = 0.25f;
	// 滑铲中速度方向朝视角转的最大角速度（度/秒），0 = 不能转向
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Slide")
	float SlideSteerRate = 75.f;
	// 滑铲中开镜时间倍率
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Slide")
	float SlideAdsTimeMultiplier = 1.3f;

	// Option A：滑铲前半段禁止立刻开镜（冲刺姿势还在混合）；按键预输入，到期自动开镜。默认开。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Slide|ADS")
	bool bSlideAdsDelayEnabled = false;   // 默认关：用 ABP 混合缩短（方案 B）；需要时再开

	// 滑铲开始后多久才能开镜。<=0 则用 0.5 * SlideDuration。默认 0.35。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Slide|ADS", meta = (ClampMin = "0", EditCondition = "bSlideAdsDelayEnabled"))
	float SlideToAdsDelay = 0.35f;

	UFUNCTION(BlueprintPure, Category = "FPS|Slide|ADS")
	float GetSlideAdsSettleRemaining() const;
	// 滑铲镜头侧倾（度），开镜时减到 SlideCameraRollAds
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Slide")
	float SlideCameraRoll = -4.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Slide")
	float SlideCameraRollAds = -1.5f;
	// 滑铲结束后摩擦力恢复用时（秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Slide")
	float SlideFrictionRecoverTime = 0.2f;
	// 滑铲结束/滑铲跳后保留的惯性，落地后在这么多秒内衰减回正常速度
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Slide")
	float MomentumCarryDecayTime = 0.35f;
	// 蹲下/站起时镜头高度平滑速度
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Movement")
	float CrouchCameraInterpSpeed = 14.f;

	// 控制器读：滑铲后保留的速度上限（0 = 没有）
	float GetCarriedSpeed() const { return CarriedSpeed; }
	float GetLandingSpeedMultiplier() const;
	float GetInjurySpeedMultiplier() const;
	float GetBandageSpeedMultiplier() const;
	float GetArmorSpeedMultiplier() const;
	float GetWeaponBlockAlpha() const { return WeaponBlockAlpha; }
	bool IsWeaponBlocked() const { return WeaponBlockAlpha > 0.55f; }
	bool IsWeaponFireBlocked() const;
	friend class FWeaponObstacleFireGateTest;
	float GetMuzzleDipAlpha() const { return MuzzleDipAlpha; }
	bool IsMuzzleDipped() const { return MuzzleDipAlpha > 0.35f; }
	bool IsArmInjured() const { return bArmInjured; }
	bool IsLegInjured() const { return bLegInjured; }
	UStatsComponent* GetStats() const { return Stats; }
	UHitZoneVitalsComponent* GetHitZoneVitals() const { return Vitals; }

	/** 1 when healthy, <1 when arm is injured (slower reload / raise). */
	float GetArmInjuryAnimPlayRate() const;

	/** 0 = cannot turn (full stinger), 1 = normal. Refresh (does not stack). */
	float GetStingerLookScale() const;
	void ApplyStingerEffect(float Strength, float LockDuration, float RecoverDuration);

	UFUNCTION(Client, Reliable)
	void Client_ReceiveStinger(float Strength, float LockDuration, float RecoverDuration);

	/** Server: hit react, bloody feet, optional StartDeath + OnPlayerKilled. */
	void Auth_OnHitZoneDamaged(AActor* DamageInstigator, bool bLethal);
	/** HUD: head/torso marked after hit; arm/leg also true while injured. */
	bool IsHitZoneMarked(EHitZone Zone) const;
	bool IsBandaging() const { return bIsBandaging; }
	EBodyArmorType GetBodyArmor() const { return BodyArmor; }

	// Authority: apply arm/leg injury from hit zone (server).
	void Auth_ApplyHitZoneInjury(EHitZone Zone);

	UFUNCTION(Client, Unreliable)
	void Client_FlashHitZone(EHitZone Zone);

	void ClearHitZoneMarks();

	// Scale ballistic torso damage by armor. Explosives/fire should call DoDamage directly (no armor).
	float ScaleDamageByArmor(float Damage, EHitZone Zone, bool bPistolOrShotgun) const;

	// PIE/debug: 0=None 1=Light 2=Heavy. Replicated via Server_SetBodyArmor.
	UFUNCTION(BlueprintCallable, Category = "FPS|Armor")
	void SetBodyArmorLocal(EBodyArmorType NewArmor);
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="FPS|Look")
 TObjectPtr<class UPoliceAppearanceComponent> PoliceAppearance;
	UFUNCTION(Server, Reliable)
	void Server_SetBodyArmor(EBodyArmorType NewArmor);

	// PIE: PickFaction 0 = Police (default spawn), 1 = Criminal. 1P Bandits gloves stay locked across weapon swaps.
	UFUNCTION(BlueprintCallable, Category = "FPS|Look")
	void SetFactionLookLocal(ECharacterLookFaction NewLook);
	UFUNCTION(Server, Reliable)
	void Server_SetFactionLook(ECharacterLookFaction NewLook);
	UFUNCTION(BlueprintPure, Category = "FPS|Look")
	ECharacterLookFaction GetFactionLook() const { return FactionLook; }

	/** True when 1P should stay on Bandits gloves (spawn default Police, PickFaction, or Mesh1P already Bandits). */
	UFUNCTION(BlueprintPure, Category = "FPS|Look")
	bool IsUsingBanditsFirstPersonArms() const;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Movement|Landing")
	float LandingSlowDuration = 0.35f;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Movement|Landing", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float LandingSlowMultiplier = 0.55f;
	float LandingSlowUntil = -1.f;

	// ===== Locational injury / bandage / barrel obstacle (tactical) =====
	UPROPERTY(ReplicatedUsing = OnRep_ArmInjured, BlueprintReadOnly, Category = "FPS|Injury")
	bool bArmInjured = false;
	UPROPERTY(ReplicatedUsing = OnRep_LegInjured, BlueprintReadOnly, Category = "FPS|Injury")
	bool bLegInjured = false;

	/** Local/owner HUD marks for head/torso (set by Client_FlashHitZone). */
	bool bHeadHitMarked = false;
	bool bTorsoHitMarked = false;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Injury")
	float LegInjurySpeedMultiplier = 0.45f;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Injury")
	float ArmInjuryAdsTimeMultiplier = 1.8f;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Injury")
	float ArmInjurySwayMultiplier = 2.5f;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Injury", meta = (ClampMin = "0.2", ClampMax = "1"))
	float ArmInjuryReloadPlayRate = 0.65f;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Injury")
	float LimpBobAmount = 1.2f;

	// Hold HealAction (~6s). Clears arm+leg only (not HP). Key = character HealAction binding.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Bandage", meta = (ClampMin = "1"))
	float BandageHoldDuration = 6.f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Stinger", meta = (ClampMin = "0.05"))
	float StingerLockDuration = 0.7f;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Stinger", meta = (ClampMin = "0"))
	float StingerRecoverDuration = 0.9f;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Bandage")
	float BandageSpeedMultiplier = 0.35f;

	// ===== Body armor (tactical) =====
	// Default None. For PIE: set on character BP defaults, or console `Armor 0|1|2`.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_BodyArmor, Category = "FPS|Armor")
	EBodyArmorType BodyArmor = EBodyArmorType::None;

	// Equipped radar tablet / tactical map accessory.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_HasRadar, Category = "FPS|Radar")
	bool bHasRadar = false;

	// Flash goggles: flashbang ghosts the screen instead of whiteout. Console `Goggles 1`.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "FPS|Vision")
	bool bHasFlashGoggles = false;

	// Soft ref; defaults to RAR WatchCheck montage if null.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Watch")
	TSoftObjectPtr<UAnimMontage> WatchCheckMontage;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Watch", meta = (ClampMin = "1"))
	float WatchTimeDisplaySeconds = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Blood")
	TSoftObjectPtr<UMaterialInterface> BloodFootprintDecal;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Blood")
	TSoftObjectPtr<UMaterialInterface> BloodHitDecal;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Blood")
	TSoftObjectPtr<UNiagaraSystem> BloodHitNiagara;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Blood", meta = (ClampMin = "8"))
	float BloodHitDecalSize = 48.f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Blood", meta = (ClampMin = "1"))
	float BloodHitDecalLifeSpan = 22.f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Blood", meta = (ClampMin = "4"))
	float BloodFootprintSize = 18.f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Blood", meta = (ClampMin = "1"))
	float BloodFootprintLifeSpan = 14.f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Blood", meta = (ClampMin = "1"))
	int32 BloodyFootstepsOnHit = 10;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Obstacle")
	bool bDebugWeaponObstacle = false;

	// Multipliers applied to torso (chest/abdomen) ballistic damage only.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Armor", meta = (ClampMin = "0.05", ClampMax = "1"))
	float LightArmorTorsoMultiplier = 0.65f;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Armor", meta = (ClampMin = "0.05", ClampMax = "1"))
	float HeavyArmorTorsoMultiplier = 0.50f;
	// Heavy vs pistol / shotgun torso (much stronger).
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Armor", meta = (ClampMin = "0.05", ClampMax = "1"))
	float HeavyArmorPistolShotgunTorsoMultiplier = 0.25f;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Armor", meta = (ClampMin = "0.2", ClampMax = "1"))
	float HeavyArmorMoveSpeedMultiplier = 0.78f;
 UPROPERTY(EditDefaultsOnly,Category="FPS|Armor",meta=(ClampMin="0.2",ClampMax="1"))
 float BallisticMaskMoveSpeedMultiplier = 0.95f;
	/** Criminals with no armor run faster than armored police (MVP). Applied in GetArmorSpeedMultiplier. */
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Armor", meta = (ClampMin = "1.0", ClampMax = "1.5"))
	float UnarmoredCriminalSpeedMultiplier = 1.12f;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Armor", meta = (ClampMin = "1"))
	float HeavyArmorAdsTimeMultiplier = 1.45f;

	// ===== 3P faction look + optional Bandits 1P arms =====
	// Default spawn = Police (Bandits Preset_13 + black gloves + gorka sleeves). Criminal = Preset_05. SetFactionLookLocal / PickFaction still override.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_FactionLook, Category = "FPS|Look")
	ECharacterLookFaction FactionLook = ECharacterLookFaction::Police; // default Bandits Preset_13 + 1P arms; customization can override

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Look")
	TSoftObjectPtr<USkeletalMesh> CriminalMesh3P;

	/** Single-body Bandits police look (same UE5 Epic skeleton as CriminalMesh3P). Preferred over CharGen modular. */
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Look")
	TSoftObjectPtr<USkeletalMesh> PoliceMesh3P;

	/** Legacy CharGen modular parts - kept for BP experiments; not used when PoliceMesh3P loads. */
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Look")
	TSoftObjectPtr<USkeletalMesh> PoliceHeadMesh3P;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Look")
	TSoftObjectPtr<USkeletalMesh> PoliceBodyDetMesh3P;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Look")
	TSoftObjectPtr<USkeletalMesh> PoliceBodyUpMesh3P;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Look")
	TSoftObjectPtr<USkeletalMesh> PolicePantsMesh3P;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Look")
	TSoftObjectPtr<USkeletalMesh> PoliceJacketMesh3P;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Look")
	TSoftObjectPtr<UMaterialInterface> PoliceJacketMaterial;

	/** Unused at runtime (TPS_Char_AnimBP T-poses on ShooterCharacter). Kept for optional BP override experiments. */
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Look")
	TSoftClassPtr<UAnimInstance> CharGenAnimBP3P;


	/** When true, FactionLook Criminal/Police also swaps Mesh1P to Bandits gloved arms (sockets patched in C++). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Look")
	bool bUseBanditsArms1P = true;

	/** Criminal 1P arms (black gloves). */
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Look")
	TSoftObjectPtr<USkeletalMesh> CriminalArmsMesh1P;

	/** Police 1P arms (green gloves). */
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Look")
	TSoftObjectPtr<USkeletalMesh> PoliceArmsMesh1P;

	/** When true, attach Bandits cloth as Mesh1P LeaderPose sleeves (Preset_05 bomber / Preset_13 gorka). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Look")
	bool bUseBanditsSleeves1P = true;

	/** Criminal 1P sleeve cloth (Bomber matching Preset_05). */
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Look")
	TSoftObjectPtr<USkeletalMesh> CriminalSleeveMesh1P;

	/** Optional material override for criminal sleeves (Bomber brown). */
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Look")
	TSoftObjectPtr<UMaterialInterface> CriminalSleeveMaterial;

	/** Police 1P sleeve cloth (Gorka beige matching Preset_13). */
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Look")
	TSoftObjectPtr<USkeletalMesh> PoliceSleeveMesh1P;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Obstacle")
	float WeaponBlockRaisePitch = 85.f;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Obstacle")
	float WeaponBlockRetract = -36.f;
 UPROPERTY(EditDefaultsOnly, Category = "FPS|Obstacle")
 float WeaponBlockSideOffset = 32.f;
 UPROPERTY(EditDefaultsOnly, Category = "FPS|Obstacle")
 float WeaponBlockLowerOffset = 34.f;
 UPROPERTY(EditDefaultsOnly, Category = "FPS|Obstacle")
 float WeaponBlockRoll = 0.f;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Obstacle")
	float WeaponBlockInterpSpeed = 10.f;

	// Close-range aim at teammate/hostage: gently lower muzzle (not holster). Off while ADS.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|MuzzleDip", meta = (ClampMin = "50", ClampMax = "500"))
	float MuzzleDipRange = 250.f;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|MuzzleDip", meta = (ClampMin = "0", ClampMax = "40"))
	float MuzzleDipPitch = 12.f;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|MuzzleDip")
	float MuzzleDipInterpSpeed = 12.f;

	// 开镜时间倍率（滑铲中慢慢靠到 SlideAdsTimeMultiplier，避免中途突变卡枪）
	float AdsTimeScaleCurrent = 1.f;

	// 3P full-body slide. Montage wins if set; else SlideAnim is played as a dynamic montage in SlideSlotName.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Slide")
	TObjectPtr<UAnimMontage> SlideMontage;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Slide")
	TSoftObjectPtr<UAnimSequenceBase> SlideAnim;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Slide")
	// Body-only slot: ABP must layer arms/spine back from locomotion/weapon pose (see CrouchSlide_REPORT.md).
	FName SlideSlotName = TEXT("BodySlot");

	// Optional 1P arms montage. Design rule: arms keep weapon hold during slide -> leave EMPTY.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Slide")
	TObjectPtr<UAnimMontage> SlideMontage1P;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Health")
	TObjectPtr<UStatsComponent>Stats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|HitZone")
	TObjectPtr<UHitZoneVitalsComponent> Vitals;



	UPROPERTY(VisibleAnywhere,BlueprintReadOnly , Category = "FPS|Combat")
	TObjectPtr<UCombatComponent> Combat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Pouch")
	TObjectPtr<UPouchInventoryComponent> Pouch;

	UFUNCTION(BlueprintImplementableEvent)
	void OnAim(bool bIsAiming);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Aiming")
	float DefaultFOV;

	// 勾上：开镜 FOV 由 C++ 每帧插值（蓝图 OnAim 里就不要再改 FOV 了）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Aiming")
	bool bDriveAimFOVInCode = true;

	// 开镜/关镜时 FOV 的插值速度（越大越快，0 = 瞬间切换）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Aiming", meta = (ClampMin = "0"))
	float AimFOVInterpSpeed = 20.f;

	// 勾上：开镜时手臂位置由 C++ 按 AimAlpha 在腰射位置和开镜位置之间插值（蓝图 OnAim 里就不要再移动 Mesh1P）
	// 不勾：和以前一样，C++ 只算 AdsMesh1PLoc，由蓝图时间轴去移动
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Aiming")
	bool bDriveAdsInCode = true;

	// 开镜进度 0（腰射）~ 1（完全开镜）。按武器的开镜时间推进，手臂位置和 FOV 都按它混合
	UFUNCTION(BlueprintPure, Category = "FPS|Aiming")
	float GetAimAlpha() const { return AimAlpha; }


public:
	// ---------- 侧瞄（第 10 步） ----------
	// 当前是否在用侧瞄（只在本地有意义）。HUD 想单独处理时读它；准星本来就在 bAiming 时隐藏
	UFUNCTION(BlueprintPure, Category = "FPS|Aiming|Canted")
	bool IsCantedAiming() const { return bCantedAim; }

	// 侧瞄切换时广播（本地）
	UPROPERTY(BlueprintAssignable, Category = "FPS|Aiming|Canted")
	FCantedSightChanged OnCantedSightChanged;
protected:

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly)
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|TurnInPlace")
	float AO_Yaw;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Strafing")
	float MovementOffsetYaw;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Locomotion")
	float GroundSpeed = 0.f;

	// true = 静步/开镜/当前 MaxWalkSpeed 偏慢，站立 Locomotion 应采 Walk BS
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Locomotion")
	bool bUseWalkGait = false;

	// AnimBP BlendSpace Player Play Rate MUST bind this: PlayRate = Clamp(GroundSpeed / RefSpeed, Min, Max)
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Locomotion")
	float LocomotionPlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Locomotion")
	float WalkGaitSpeedThreshold = 360.f;

	// Authored sample speed (cm/s) of the BlendSpace currently playing. PlayRate = GroundSpeed / this.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Locomotion", meta = (ClampMin = "1.0"))
	float WalkAnimReferenceSpeed = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Locomotion", meta = (ClampMin = "1.0"))
	float JogAnimReferenceSpeed = 375.f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Locomotion", meta = (ClampMin = "1.0"))
	float CrouchAnimReferenceSpeed = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Locomotion", meta = (ClampMin = "0.05"))
	float MinLocomotionPlayRate = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Locomotion", meta = (ClampMin = "0.1"))
	float MaxLocomotionPlayRate = 2.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|TurnInPlace")
	ETurningInPlace TurningStatus;
	
	UFUNCTION(NetMulticast,Reliable)
	void Multicast_HitReact(EHitReactDirection Direction, int32 MontageIndex);

	UFUNCTION()
	void OnDeathStarted();

	UFUNCTION(BlueprintImplementableEvent)
	void DeathEffect();

	// ---------- 死亡视角（只在自己操控的角色上） ----------
	// 死后镜头落到地面上方的高度（厘米）、下落用时（秒）、仰望天空时的歪头角度
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|DeathCam")
	float DeathCamGroundHeight = 18.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|DeathCam", meta = (ClampMin = "0.05"))
	float DeathCamMoveTime = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|DeathCam")
	float DeathCamRoll = 8.f;

	// 眨眼次数、每次眨眼时长（秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|DeathCam", meta = (ClampMin = "0"))
	int32 DeathBlinkCount = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|DeathCam", meta = (ClampMin = "0.05"))
	float DeathBlinkTime = 0.35f;

	// 眨完眼后多久开始彻底变黑、变黑用时
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|DeathCam", meta = (ClampMin = "0"))
	float DeathFadeDelay = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|DeathCam", meta = (ClampMin = "0.05"))
	float DeathFadeDuration = 1.5f;

	// 进入死亡视角时调用（可在蓝图里加眼皮 UMG、音效等）
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|DeathCam")
	void OnDeathView();

	void StartDeathView();

	// ---------- 脚步音效 ----------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps")
	FFootstepSurfaceSounds FootstepSounds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps")
	TSoftObjectPtr<USoundAttenuation> FootstepAttenuation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps", meta = (ClampMin = "0"))
	float FootstepMinSpeed = 20.f;

	// 每一步累计的水平位移（厘米）。冲刺更短=更密，蹲趴更长=更疏。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps", meta = (ClampMin = "10"))
	float FootstepDistanceRun = 155.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps", meta = (ClampMin = "10"))
	float FootstepDistanceWalk = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps", meta = (ClampMin = "10"))
	float FootstepDistanceSprint = 175.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps", meta = (ClampMin = "10"))
	float FootstepDistanceCrouch = 95.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps", meta = (ClampMin = "10"))
	float FootstepDistanceProne = 70.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps", meta = (ClampMin = "0"))
	float FootstepVolumeRun = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps", meta = (ClampMin = "0"))
	float FootstepVolumeWalk = 0.55f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps", meta = (ClampMin = "0"))
	float FootstepVolumeSprint = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps", meta = (ClampMin = "0"))
	float FootstepVolumeCrouch = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps", meta = (ClampMin = "0"))
	float FootstepVolumeProne = 0.22f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Footsteps")
	bool bEnableFootsteps = true;

private:

	// CombatComponent 开镜状态变化时调用
	void HandleAimStateChanged(bool bIsAiming);
	// 让控制器按 冲刺/静步/下蹲/开镜 重新算速度
	void RefreshControllerMovementSpeed();
	void UpdateLocomotionGait(bool bNotifyCombat);

	EPhysicalSurface TraceFootstepSurface() const;
	USoundBase* ResolveFootstepSound(EPhysicalSurface Surface) const;
	void PlayFootstepAtFeet(USoundBase* Sound, float Volume);

	float FootstepDistanceAccum = 0.f;
	bool bFootstepLeft = false;


	void Input_LeanLeft_Started();
	void Input_LeanLeft_Completed();
	void Input_LeanRight_Started();
	void Input_LeanRight_Completed();
	void HandleLeanInput(int32 Dir, bool bPressed);
	void EnsureLeanInput(class UEnhancedInputComponent* InputComp);
	void UpdateLean(float DeltaTime);
	bool IsLeanBlocked() const;

	UFUNCTION(Server, Reliable)
	void Server_SetLeanTarget(float NewTarget);

	// ---- Prone ----
	UFUNCTION()
	void OnRep_IsProne(bool bOldProne);

	UFUNCTION(Server, Reliable)
	void Server_SetProne(bool bNewProne, bool bExitToCrouch);

	// Server refused a predicted change: snap the owner back to the server value.
	UFUNCTION(Client, Reliable)
	void Client_ProneRejected(bool bServerProne);

	void ApplyProneState(bool bNewProne, bool bExitToCrouch);
	bool CanEnterProne() const;
	bool CanExitProne(bool bToCrouch) const;
	void Input_Prone();
	void EnsureProneInput();
	void UpdateProne(float DeltaTime);
	void ApplyProneWeaponVisibility();

	// ---- Slide ----
	UFUNCTION()
	void OnRep_IsSliding(bool bOldSliding);

	UFUNCTION(Server, Reliable)
	void Server_SetSliding(bool bNewSliding);

	UFUNCTION(Client, Reliable)
	void Client_SlideRejected();

	bool CanStartSlide(float SpeedScale) const;
	void ApplySlideState(bool bNewSliding);
	void EndSlideLocal();
	void UpdateSlide(float DeltaTime);
	void PlaySlideAnim(bool bPlay);

	bool bSlideEndCrouch = true;
	float SlideElapsed = 0.f;
	float SavedGroundFriction = 8.f;
	float SavedBrakingDecelWalking = 2048.f;
	bool bSlideSavedMovement = false;
	float CarriedSpeed = 0.f;
	float FrictionRecoverElapsed = -1.f;
	float CrouchCameraOffsetZ = 0.f;
	void UpdateMomentumCarry(float DeltaTime);
	void OnStartCrouchCameraFix(float ScaledHalfHeightAdjust);
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveSlideMontage;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> ProneRuntimeIMC;

	float DefaultCrouchedHalfHeight = 0.f;
	FVector SpringArmBaseLocation = FVector::ZeroVector;
	bool bSpringArmBaseCaptured = false;

	// 当前按住/切换的方向：-1 左 0 无 1 右
	int32 LeanInputDir = 0;
	bool bLeanLeftHeld = false;
	bool bLeanRightHeld = false;
	bool bAltPeekCrouch = false;
	int32 AltPeekDir = 0;
	float LeanDistanceScale = 1.f;
	double LastFireModeSwitchTime = -1.0;
	void UpdateAltPeek();
	FVector CameraBaseLocation = FVector::ZeroVector;
	FRotator CameraBaseRotation = FRotator::ZeroRotator;
	bool bCameraBaseCaptured = false;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> LeanRuntimeIMC;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> FireModeRuntimeIMC;
	void EnsureFireModeInput(class UEnhancedInputComponent* InputComp);

	UPROPERTY(ReplicatedUsing=OnRep_GlowStickCount)
	int32 GlowStickCount = 10;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> GlowStickAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> GlowStickIMC;

	void EnsureGlowStickInput();
	void Input_DropGlowStick();

	UFUNCTION(Server, Reliable)
	void Server_DropGlowStick();
	void Input_ReloadWeapon_Started();
	void Input_ReloadWeapon_Completed();
	void Input_ReloadWeapon(); // legacy alias -> Started
	void OnReloadHoldThreshold();
	void OnReloadSingleClickTimeout();
	void ClearReloadGestureTimers();
	FTimerHandle ReloadHoldTimer;
	FTimerHandle ReloadDoubleClickTimer;
	bool bReloadKeyHeld = false;
	bool bReloadHoldTriggered = false;
	bool bReloadAwaitingDouble = false;
	bool bReloadDoubleClick = false;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Reload", meta = (ClampMin = "0.05"))
	float ReloadHoldThreshold = 0.35f;
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Reload", meta = (ClampMin = "0.05"))
	float ReloadDoubleClickWindow = 0.28f;
	void Input_SwitchFireMode();
	void Input_SwitchZoom();
	// 第 12 步：侧瞄是切档循环里的一档（主镜各档 -> 侧瞄 -> 第一档），不再有单独按键
	void SetCantedAim(bool bNewCanted);
	// 侧瞄时 Mesh1P 的相对旋转和位置（让侧瞄插槽正对屏幕中心）。算不出返回 false
	bool ComputeCantedMeshTransform(FQuat& OutRelRotation, FVector& OutRelLocation);
	bool bCantedAim = false;
	void Input_FireWeapon_Pressed();
	void Input_FireWeapon_Released();
	void Input_Aim_Pressed();
	void Input_Aim_Released();
	void Input_EquipMainWeapon();
	void Input_EquipSeconedaryWeapon();
	void Input_EquipMeleeWeapon();
	void Input_EquipPistolWeapon();
	void Input_Heal();
	void Input_VersionWeapon();
 void Input_BodyInspect();
 UPROPERTY(Transient) TObjectPtr<UInputAction> BodyInspectAction;
 UPROPERTY(Transient) TObjectPtr<UInputMappingContext> BodyInspectIMC;
	bool PlayHealMontage(USkeletalMeshComponent* HealMesh, bool bBindEnd);
	void OnHealMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void HideEquippedWeaponForHeal();
	void RestoreWeaponAfterHeal();

	UFUNCTION(Server, Reliable)
	void Server_BeginHeal(float InHealAmout);

	UFUNCTION(Server, Reliable)
	void Server_FinishHeal(bool bCompleted);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayHealMontage();
	void ConfigureWeaponHitCollision();

	void CalculateFABRIKSocket(float DeltaTime);
	void CalculateTurnInPlaceParameters(float DeltaTime);
	void TurnInPlace(float DeltaTime);
	void ConfigureAimTickOrder();
 void RefreshHybridBodyVisibility();
	void UpdateAimSocketAlignment(float DeltaTime);
	float GetAimTimelineAlpha(float DeltaTime);
	// 本地玩家每帧把镜头 FOV 插值到开镜目标或默认值
	void UpdateAimFieldOfView(float DeltaTime);

	bool bWeaponAttachDebug = false;
	bool bWeaponFirstReplicated;
	FRotator StartingAimRotation;

	float InterpAO_Yaw;
	float AimAlignAlpha = 0.f;

	// 开镜进度（线性）和开镜时用的平滑 FOV
	float AimAlpha = 0.f;
	float SmoothedAimFOV = 90.f;
	void UpdateAimAlpha(float DeltaTime);
	// 算出"瞄准点正好在屏幕中心"时 Mesh1P 的相对位置；算不出（没枪/没瞄准点）返回 false
	bool ComputeAdsMeshLocation(FVector& OutLocation);

	// ---------- 枪身视觉后坐 ----------
	// 先把上一帧加在 Mesh1P 上的偏移撤掉，让开镜对准按"没有后坐"的位置算，算完再加回去
	void RemoveVisualRecoilFromMesh();
	void ApplyVisualRecoilToMesh(float DeltaTime);
	void UpdateWeaponSway(float DeltaTime);

	FVector SwayLocCurrent = FVector::ZeroVector;
	FRotator SwayRotCurrent = FRotator::ZeroRotator;
	FRotator LastSwayControlRot = FRotator::ZeroRotator;
	bool bHasLastSwayControlRot = false;
	float BobPhase = 0.f;
	float BobWeight = 0.f;
	float LandingDipTarget = 0.f;
	float LandingDipCurrent = 0.f;
	float LastFallingVelocityZ = 0.f;
	float SwayDipZ = 0.f;

	FVector VisualRecoilLocTarget = FVector::ZeroVector;    // 被踢到的目标（会慢慢回 0）
	FRotator VisualRecoilRotTarget = FRotator::ZeroRotator;
	FVector VisualRecoilLocCurrent = FVector::ZeroVector;   // 当前实际偏移（追向目标）
	FRotator VisualRecoilRotCurrent = FRotator::ZeroRotator;
	FVector AppliedRecoilLocParent = FVector::ZeroVector;   // 上一帧真正写进 Mesh1P 的量
	FQuat AppliedRecoilQuat = FQuat::Identity;
	FVector LastWrittenMeshLoc = FVector::ZeroVector;
	FQuat LastWrittenMeshQuat = FQuat::Identity;
	bool bVisualRecoilWritten = false;

	bool bIsHealing = false;
	TWeakObjectPtr<AWeapon> HealHiddenWeapon;
	bool bServerHealing = false;
	float PendingHealAmout = 0.f;
	float ServerHealAmout = 0.f;
	FTimerHandle HealLocalTimer;
	FTimerHandle HealServerTimer;

	void AbortLocalHeal();
	void AbortServerHeal();

	void UpdateWeaponObstacle(float DeltaTime);
	WeaponObstacle::FProbeResult ProbeWeaponObstacle() const;
	void SpawnBloodFootprint();
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_NotifyBloodyFeet();
	void UpdateMuzzleDip(float DeltaTime);
	bool IsProtectedAimTarget(const AActor* HitActor) const;
	void UpdateBandage(float DeltaTime);
	void UpdateTacticalVision(float DeltaTime);
	void ApplyTacticalVisionEffects();
	void ClearTacticalVisionEffects();
	void RefreshThermalOverlays();
	void ClearThermalOverlays();
	void Input_Bandage_Started();
	void Input_Bandage_Completed();
	void BeginBandage();
	void AbortBandage(bool bCompleted);
	UFUNCTION(Server, Reliable)
	void Server_BeginBandage();
	UFUNCTION(Server, Reliable)
	void Server_FinishBandage(bool bCompleted);
	UFUNCTION()
	void OnRep_ArmInjured();
	UFUNCTION()
	void OnRep_LegInjured();
	UFUNCTION()
	void OnRep_BodyArmor();
	UFUNCTION()
	void OnRep_HasRadar();
	UFUNCTION()
	void OnRep_FactionLook();

	void ApplyFactionLookMeshes();
	void ApplyFactionArms1P();
	void ApplyFactionSleeves1P();
	void EnsureArms1PGripSockets(USkeletalMesh* ArmsMesh, USkeletalMesh* SourceMesh);
	/** Strip mesh-only sockets that shadow animated gun bones (ik_hand_gun). Never promote those bones. */
	void EnsureAnimatedGunAttachSockets(USkeletalMesh* ArmsMesh);
	/** Copy Manny/default Mesh3P grip sockets onto Bandits body (weapon_r / RifleSocket* / PistolSocket*). */
	void EnsureMesh3PGripSockets(USkeletalMesh* BodyMesh, USkeletalMesh* SourceMesh, USkeletalMeshComponent* LiveSourceComp = nullptr);
	void EnsureFactionLookDefaults();
	void SetModularLookVisible(bool bVisible);
	USkeletalMesh* LoadSoftMesh(const TSoftObjectPtr<USkeletalMesh>& Soft) const;
	UMaterialInterface* LoadSoftMaterial(const TSoftObjectPtr<UMaterialInterface>& Soft) const;

	UPROPERTY()
	TObjectPtr<USkeletalMesh> DefaultMesh3PAsset;
	UPROPERTY()
	TSubclassOf<UAnimInstance> DefaultMesh3PAnimClass;
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Mesh3P_Head;
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Mesh3P_BodyUp;
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Mesh3P_Pants;
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Mesh3P_Jacket;
	UPROPERTY(VisibleAnywhere, Category = "FPS|Name")
	TObjectPtr<UWidgetComponent> Nameplate;
	void RefreshNameplate();
	bool bCapturedDefaultMesh3P = false;
	UPROPERTY()
	TObjectPtr<USkeletalMesh> DefaultArmsMesh1PAsset;
	bool bCapturedDefaultArms1P = false;

	bool bIsBandaging = false;
	bool bBandageHoldInput = false;
	float BandageHoldElapsed = 0.f;

	float StingerStrength = 0.f;
	float StingerLockUntil = 0.f;
	float StingerRecoverUntil = 0.f;
	float WeaponBlockAlpha = 0.f;
	FVector WeaponBlockLocCurrent = FVector::ZeroVector;
	FRotator WeaponBlockRotCurrent = FRotator::ZeroRotator;
	int32 BloodyFootstepsRemaining = 0;
	float WatchDisplayUntil = -1.f;
	float MuzzleDipAlpha = 0.f;
	FRotator MuzzleDipRotCurrent = FRotator::ZeroRotator;

	float VisionBatterySeconds = 60.f;
	float VisionFlashWash = 0.f;
	float FlashWhite = 0.f;
	float FlashGhost = 0.f;
	float HitBloodFlash = 0.f;
	float HitBloodTail = 0.f;
	bool bGhostPostProcess = false;
	void UpdateScreenEffects(float DeltaTime);
	void ApplyFlashGhostPostProcess();
	float VisionOverlayRefreshAcc = 0.f;
	ETacticalVisionMode AppliedVisionMode = ETacticalVisionMode::Off;
	TWeakObjectPtr<UMaterialInterface> ActiveVisionBlendable;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedThermalOverlayMat;
	TArray<TWeakObjectPtr<USkeletalMeshComponent>> ThermalOverlayMeshes;

	// Mesh1P relative pose captured at BeginPlay. Used when the blueprint hip variables are absent.
	FVector AuthoredHipMeshLocation = FVector::ZeroVector;
	FRotator AuthoredHipMeshRotation = FRotator::ZeroRotator;

	TWeakObjectPtr<AWeapon> MissingAimSocketWeapon;
	TWeakObjectPtr<UTimelineComponent> AimTimeline;


	//1st preson view(arms)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Mesh", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> Mesh1P;

	/** Bandits jacket/bomber MasterPose follower for FP sleeves (OnlyOwnerSee). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Mesh", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> Mesh1P_Sleeves;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpringArmComponent> SpringArm;



	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> FireWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> ReloadWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> AimWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> EquipMainWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> EquipSeconedaryWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> EquipMeleeWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> EquipPistolWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction>HealAction;

	// 单发/自动切换键（IA_SwitchFireMode）
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction>SwitchFireModeAction;

	// 可变倍率瞄具切档键（IA_SwitchZoom）
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction>SwitchZoomAction;


	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction>VersionWeaponAction;


};
