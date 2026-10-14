// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include <GameFramework/SpringArmComponent.h>
#include <Camera/CameraComponent.h>
#include "GameFramework/Character.h"
#include "CombatComponent.h"
#include "Stats/StatsComponent.h"
#include "ShooterType/ShooterType.h"
#include "Interface/PlayerInterface.h"
#include "Engine/NetSerialization.h"
#include "ShooterCharacter.generated.h"

class UCombatComponent;
class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UInputMappingContext;
class UTimelineComponent;
class UAnimMontage;
class AWeapon;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWeaponFirstReplicated, AWeapon*, Weapon);
// 第 10 步：侧瞄开/关（只在本地）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCantedSightChanged, bool, bCanted);
// Hit feedback (local only): victim damage ring / shooter hit confirm
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDamageIndicatorSignature, FVector, InstigatorLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHitConfirmedSignature);

UENUM(BlueprintType)
enum class EHitReactDirection : uint8
{
	Front,
	Back,
	Left,
	Right
};

UCLASS()
class FPS_API AShooterCharacter : public ACharacter,public IPlayerInterface
{
	GENERATED_BODY()

public:
	AShooterCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

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
	virtual bool DoDamage_Implementation(float DamageAmout,AActor* DamageInstigator) override;
	/** PlayerInterface */

	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

	UFUNCTION(BlueprintCallable)
	FRotator GetFixedAimRotation()const;

	UPROPERTY(BlueprintReadOnly,Category = "FPS|FABRIK")
	FTransform FABRIK_SocketTransform;

	// 第一人称左手目标，手臂网格 hand_r 的骨骼空间。ABP_FirstPerson 的 FABRIK 读它。
	UPROPERTY(BlueprintReadOnly, Category = "FPS|FABRIK")
	FTransform FABRIK_SocketTransform1P;

	// 第一人称 FABRIK 强度。没有可用插槽时为 0，左手留在持枪动画上。
	UPROPERTY(BlueprintReadOnly, Category = "FPS|FABRIK")
	float FABRIK_Alpha1P = 0.f;

	// 动画曲线名。手臂蒙太奇里这条曲线为 1 时左手 IK 完全关掉（让左手去拿弹匣/拉栓），0 时正常扶枪
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|FABRIK")
	FName DisableLeftHandIKCurveName = TEXT("DisableLHandIK");

	// 勾上：换弹/拉栓期间不管有没有曲线，左手 IK 都淡出到 0
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|FABRIK")
	bool bDisableLeftHandIKDuringReload = true;

	// 左手 IK 淡入淡出速度（越大越快，10 大约 0.2~0.3 秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|FABRIK", meta = (ClampMin = "0"))
	float LeftHandIKBlendSpeed = 10.f;

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
	void Client_ConfirmHit();

	EHitReactDirection ComputeHitReactDirection(const FVector& SourceLocation) const;
	const TArray<TObjectPtr<UAnimMontage>>& GetHitReactPool(EHitReactDirection Direction) const;

	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	// 枪身视觉后坐总开关（第 4 步）。关掉时不加任何偏移，Mesh1P 完全不受影响，方便对比手感
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Recoil|Visual")
	bool bEnableVisualRecoil = false;

	// 每发开火由 CombatComponent 调用：给第一人称手臂加一次视觉后坐（镜头空间的位移和旋转）
	void AddVisualRecoil(const FVector& KickLocation, const FRotator& KickRotation);
	UCombatComponent* GetCombatComponent() const { return Combat; }
	float GetDefaultFieldOfView() const { return DefaultFOV; }
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

	// 1P 相机横移距离（cm）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Lean")
	float LeanCameraOffset = 30.f;

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

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Health")
	TObjectPtr<UStatsComponent>Stats;



	UPROPERTY(VisibleAnywhere,BlueprintReadOnly , Category = "FPS|Combat")
	TObjectPtr<UCombatComponent> Combat;

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


private:

	// CombatComponent 开镜状态变化时调用
	void HandleAimStateChanged(bool bIsAiming);
	// 让控制器按 冲刺/静步/下蹲/开镜 重新算速度
	void RefreshControllerMovementSpeed();
	void UpdateLocomotionGait(bool bNotifyCombat);

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

	// 当前按住/切换的方向：-1 左 0 无 1 右
	int32 LeanInputDir = 0;
	bool bLeanLeftHeld = false;
	bool bLeanRightHeld = false;
	FVector CameraBaseLocation = FVector::ZeroVector;
	FRotator CameraBaseRotation = FRotator::ZeroRotator;
	bool bCameraBaseCaptured = false;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> LeanRuntimeIMC;
	void Input_ReloadWeapon();
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
	void Input_EquipMainWeapon();
	void Input_EquipSeconedaryWeapon();
	void Input_EquipMeleeWeapon();
	void Input_EquipPistolWeapon();
	void Input_Heal();
	void Input_VersionWeapon();
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
	void UpdateAimSocketAlignment(float DeltaTime);
	float GetAimTimelineAlpha(float DeltaTime);
	// 本地玩家每帧把镜头 FOV 插值到开镜目标或默认值
	void UpdateAimFieldOfView(float DeltaTime);

	bool bWeaponFirstReplicated;
	FRotator StartingAimRotation;

	float InterpAO_Yaw;
	float AimAlignAlpha = 0.f;

	// 开镜进度（线性）和开镜时用的平滑 FOV
	float AimAlpha = 0.f;
	float SmoothedAimFOV = 90.f;
	void UpdateAimAlpha(float DeltaTime);
	// 算出“瞄准点正好在屏幕中心”时 Mesh1P 的相对位置；算不出（没枪/没瞄准点）返回 false
	bool ComputeAdsMeshLocation(FVector& OutLocation);

	// ---------- 枪身视觉后坐 ----------
	// 先把上一帧加在 Mesh1P 上的偏移撤掉，让开镜对准按“没有后坐”的位置算，算完再加回去
	void RemoveVisualRecoilFromMesh();
	void ApplyVisualRecoilToMesh(float DeltaTime);

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

	// Mesh1P relative pose captured at BeginPlay. Used when the blueprint hip variables are absent.
	FVector AuthoredHipMeshLocation = FVector::ZeroVector;
	FRotator AuthoredHipMeshRotation = FRotator::ZeroRotator;

	TWeakObjectPtr<AWeapon> MissingAimSocketWeapon;
	TWeakObjectPtr<UTimelineComponent> AimTimeline;


	//1st preson view(arms)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Mesh", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> Mesh1P;

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
