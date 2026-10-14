// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "WeaponData.h"
#include "Weapon/Weapon.h"
#include "Weapon/SniperScopeComponent.h"
#include "Weapon/OpticScopeComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include "FPSTraceChannels.h"
#include "UObject/UnrealType.h"
#include "ShooterPlayerController.h"
#include "ShooterGamePlayTags.h"

namespace
{
	template <typename T>
	T* FindBlueprintStruct(UObject* Object, const TCHAR* Name)
	{
		if (!IsValid(Object))
		{
			return nullptr;
		}

		FStructProperty* Property = FindFProperty<FStructProperty>(Object->GetClass(), Name);
		if (!Property || Property->Struct != TBaseStructure<T>::Get())
		{
			return nullptr;
		}

		return Property->ContainerPtrToValuePtr<T>(Object);
	}
}



AShooterCharacter::AShooterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;


	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->TargetArmLength = 0.f;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 15.f;
	SpringArm->bUsePawnControlRotation = true;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(SpringArm);
	FirstPersonCamera->bUsePawnControlRotation = false;

	//ArmMesh
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
	Mesh1P->SetupAttachment(FirstPersonCamera);
	Mesh1P->bOnlyOwnerSee = true;
	Mesh1P->bOwnerNoSee = false;
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->bReceivesDecals = false;
	Mesh1P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Mesh1P->PrimaryComponentTick.TickGroup = TG_PrePhysics;

	//BodyMeshB
	GetMesh()->bOnlyOwnerSee = false;
	GetMesh()->bOwnerNoSee = true;
	GetMesh()->bReceivesDecals = false;

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
	Combat->SetIsReplicated(true);

	Stats = CreateDefaultSubobject<UStatsComponent>("Stats");
	Stats->SetIsReplicated(true);

	DefaultFOV = 90.f;
	TurningStatus = ETurningInPlace::NotTurning;

	bWeaponFirstReplicated = false;

	ConfigureWeaponHitCollision();

	HealAmout = 30.f;
}

void AShooterCharacter::BeginPlay()
{
	if (GetNetMode() != NM_DedicatedServer && !FindComponentByClass<USniperScopeComponent>())
	{
		USniperScopeComponent* Scope = NewObject<USniperScopeComponent>(this, TEXT("SniperScope"));
		Scope->RegisterComponent();
	}
	// 倍镜画中画（配件瞄具 OpticType = Magnified 时用）
	if (GetNetMode() != NM_DedicatedServer && !FindComponentByClass<UOpticScopeComponent>())
	{
		UOpticScopeComponent* OpticScope = NewObject<UOpticScopeComponent>(this, TEXT("OpticScope"));
		OpticScope->RegisterComponent();
	}

	Super::BeginPlay();

	// A Blueprint collision override is applied after the constructor. Set the weapon channel again here.
	ConfigureWeaponHitCollision();

	FirstPersonCamera->SetFieldOfView(DefaultFOV);

	StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);

	if (IsValid(Mesh1P))
	{
		AuthoredHipMeshLocation = Mesh1P->GetRelativeLocation();
		AuthoredHipMeshRotation = Mesh1P->GetRelativeRotation();
	}

	ConfigureAimTickOrder();

	if (IsValid(Combat))
	{
		// 不管是按右键还是切枪自动关镜，都走这一个入口
		Combat->OnAimStateChanged.AddUObject(this, &ThisClass::HandleAimStateChanged);
	}
}

void AShooterCharacter::BeginDestroy()
{
	Super::BeginDestroy();

	if (IsValid(Combat))
	{
		Combat->DestroyInventory();
	}
}

FRotator AShooterCharacter::GetFixedAimRotation() const
{
	FRotator AimRotation = GetBaseAimRotation();
	if (AimRotation.Pitch > 90.f && !IsLocallyControlled())
	{	
		// map pitch form [270,360) to [-90,0]
		const FVector2D InRange(270.f, 360.f);
		const FVector2D OutRange(-90.f, 0.f);
		AimRotation.Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AimRotation.Pitch);
	}

	return AimRotation;

}

bool AShooterCharacter::HasCurrentWeapon() const
{
	return IsValid(Combat) && Combat->CurrentWeapon!=nullptr;
}

void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateLocomotionGait(true);

	CalculateTurnInPlaceParameters(DeltaTime);
	CalculateFABRIKSocket(DeltaTime);
	UpdateAimAlpha(DeltaTime);             // 开镜进度先推进，下面对准和 FOV 都用它
	RemoveVisualRecoilFromMesh();          // 开镜对准要按没有后坐的位置算
	UpdateAimSocketAlignment(DeltaTime);
	ApplyVisualRecoilToMesh(DeltaTime);    // 算完再把枪身后坐叠回去
	UpdateAimFieldOfView(DeltaTime);

	if (USniperScopeComponent* Scope = FindComponentByClass<USniperScopeComponent>())
	{
		Scope->UpdateView();
	}
	if (UOpticScopeComponent* OpticScope = FindComponentByClass<UOpticScopeComponent>())
	{
		OpticScope->UpdateView();
	}
}

void AShooterCharacter::CalculateTurnInPlaceParameters(float DeltaTime)
{
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	float Speed = Velocity.Size();
	bool bIsInAir = GetCharacterMovement()->IsFalling();

	if (Speed == 0.f && !bIsInAir)
	{
		FRotator CurrentAimRotation(0.f, GetBaseAimRotation().Yaw, 0.f);
		//StartingAimRotation initially set in beginplay
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;

		if (TurningStatus == ETurningInPlace::NotTurning)
		{
			InterpAO_Yaw = AO_Yaw;
		}
		
		TurnInPlace(DeltaTime);
	}

		
	if (Speed > 0.f || bIsInAir)
	{
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;

		FRotator AimRotation = GetBaseAimRotation();
		FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(GetVelocity());

		MovementOffsetYaw = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation).Yaw;
		TurningStatus = ETurningInPlace::NotTurning;
	}
	
	AO_Yaw *= -1.f;
}


void AShooterCharacter::TurnInPlace(float DeltaTime)
{
	if (AO_Yaw > 90.f)
	{
		TurningStatus = ETurningInPlace::Right;
	}
	else if (AO_Yaw < -90.f)
	{
		TurningStatus = ETurningInPlace::Left;
	}
	if (TurningStatus != ETurningInPlace::NotTurning)//are turning
	{
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 4.0f);
		AO_Yaw = InterpAO_Yaw;
		if (FMath::Abs(AO_Yaw) < 5.f)
		{
			TurningStatus = ETurningInPlace::NotTurning;
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		}
	}


}


void AShooterCharacter::CalculateFABRIKSocket(float DeltaTime)
{
	auto WriteHandSpace = [](USkeletalMeshComponent* PoseMesh, const FTransform& World, FTransform& Out)
	{
		if (!IsValid(PoseMesh))
		{
			Out = FTransform::Identity;
			return;
		}

		FVector OutLocation;
		FRotator OutRotation;
		PoseMesh->TransformToBoneSpace(
			TEXT("hand_r"),
			World.GetLocation(),
			World.Rotator(),
			OutLocation,
			OutRotation);
		Out.SetLocation(OutLocation);
		Out.SetRotation(OutRotation.Quaternion());
		Out.SetScale3D(FVector::OneVector);
	};

	if (!IsValid(Combat) || !IsValid(Combat->CurrentWeapon))
	{
		FABRIK_Alpha1P = 0.f;
		FABRIK_SocketTransform = FTransform::Identity;
		FABRIK_SocketTransform1P = FTransform::Identity;
		return;
	}

	// 握把配件上的 FABRIK 优先。没有握把，或握把模型上没有那个 Socket，才用枪骨骼上的 FABRIK。
	FTransform SocketWorld;
	if (Combat->CurrentWeapon->GetFabrikWorldTransform(false, SocketWorld))
	{
		WriteHandSpace(GetMesh(), SocketWorld, FABRIK_SocketTransform);
	}
	else
	{
		FABRIK_SocketTransform = FTransform::Identity;
	}

	// 狙击枪第一人称扶手跟动画走。步枪和手枪仍用 FABRIK。
	if (Combat->CurrentWeapon->WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Sniper))
	{
		FABRIK_SocketTransform1P = FTransform::Identity;
		FABRIK_Alpha1P = 0.f;
	}
	else if (Combat->CurrentWeapon->GetFabrikWorldTransform(true, SocketWorld))
	{
		WriteHandSpace(Mesh1P, SocketWorld, FABRIK_SocketTransform1P);

		// 目标强度 = 1 × (1 - 曲线值)。换弹/拉栓时直接把目标压到 0
		float TargetAlpha = 1.f;
		const UAnimInstance* ArmAnim = IsValid(Mesh1P) ? Mesh1P->GetAnimInstance() : nullptr;
		if (IsValid(ArmAnim) && !DisableLeftHandIKCurveName.IsNone())
		{
			TargetAlpha *= 1.f - FMath::Clamp(ArmAnim->GetCurveValue(DisableLeftHandIKCurveName), 0.f, 1.f);
		}
		const EWeaponStatus Status = Combat->CurrentWeapon->WeaponStatus;
		if (bDisableLeftHandIKDuringReload && (Status == EWeaponStatus::Reloading || Status == EWeaponStatus::BoltCycling))
		{
			TargetAlpha = 0.f;
		}

		// 平滑过渡，避免左手瞬移
		FABRIK_Alpha1P = LeftHandIKBlendSpeed > 0.f
			? FMath::FInterpTo(FABRIK_Alpha1P, TargetAlpha, DeltaTime, LeftHandIKBlendSpeed)
			: TargetAlpha;
	}
	else
	{
		FABRIK_SocketTransform1P = FTransform::Identity;
		FABRIK_Alpha1P = 0.f;
	}
}

void AShooterCharacter::ConfigureAimTickOrder()
{
	// The aim socket follows the arm bones, so read it after Mesh1P animates.
	if (IsValid(Mesh1P))
	{
		PrimaryActorTick.AddPrerequisite(Mesh1P, Mesh1P->PrimaryComponentTick);
	}
}

void AShooterCharacter::UpdateAimAlpha(float DeltaTime)
{
	if (!IsLocallyControlled()) return;

	const bool bDead = IsValid(Stats) && Stats->DeathState != EDeathState::NotDead;
	const AWeapon* Weapon = IsValid(Combat) ? Combat->CurrentWeapon.Get() : nullptr;
	const float Target = (!bDead && IsValid(Combat) && Combat->bAiming) ? 1.f : 0.f;

	// 第 10 步：侧瞄。关镜、死亡、侧瞄配件没了（切枪/卸配件）就退出侧瞄；退出和进入都瞬间完成
	if (bCantedAim)
	{
		UStaticMeshComponent* CantedMesh = nullptr;
		const UAttachmentData* CantedData = nullptr;
		const bool bHasCanted = IsValid(Weapon) && Weapon->GetCantedSight(CantedMesh, CantedData);
		if (Target <= 0.f || !bHasCanted)
		{
			SetCantedAim(false);
			AimAlpha = Target;
			AimAlignAlpha = Target;
			return;
		}
		AimAlpha = 1.f;
		AimAlignAlpha = 1.f;
		return;
	}

	// 开镜和关镜都用同一个时间；时间为 0 就瞬间到位
	const float AdsTime = IsValid(Weapon) ? Weapon->GetEffectiveAdsTime() : 0.2f;
	if (AdsTime <= KINDA_SMALL_NUMBER)
	{
		AimAlpha = Target;
	}
	else
	{
		AimAlpha = FMath::FInterpConstantTo(AimAlpha, Target, DeltaTime, 1.f / AdsTime);
	}
	// 给手臂和 FOV 用的是缓入缓出后的值，看起来不生硬
	AimAlignAlpha = FMath::InterpEaseInOut(0.f, 1.f, AimAlpha, 2.f);
}

void AShooterCharacter::UpdateAimFieldOfView(float DeltaTime)
{
	if (!bDriveAimFOVInCode || !IsLocallyControlled() || !IsValid(FirstPersonCamera)) return;

	// 开镜 FOV 目标：镜外镜头按瞄具 AimFOV（没覆盖就用枪的）。画中画的放大在镜片里另算。
	const AWeapon* Weapon = IsValid(Combat) ? Combat->CurrentWeapon.Get() : nullptr;
	float AimTargetFOV = DefaultFOV;
	if (IsValid(Combat) && IsValid(Weapon))
	{
		AimTargetFOV = Combat->GetCurrentAimFOV(DefaultFOV);
	}
	// 第 10 步：侧瞄用自己的 FOV（0 = 枪的 AimFeildOfView），瞬间切换
	if (bCantedAim && IsValid(Weapon))
	{
		UStaticMeshComponent* CantedMesh = nullptr;
		const UAttachmentData* CantedData = nullptr;
		Weapon->GetCantedSight(CantedMesh, CantedData);
		const float CantedFOV = (IsValid(CantedData) && CantedData->CantedAimFOV > 0.f) ? CantedData->CantedAimFOV : Weapon->AimFeildOfView;
		AimTargetFOV = CantedFOV > 0.f ? CantedFOV : DefaultFOV;
		SmoothedAimFOV = AimTargetFOV;
	}

	if (AimAlpha <= KINDA_SMALL_NUMBER)
	{
		// 完全腰射时直接对齐目标，下次开镜只按开镜进度混合
		SmoothedAimFOV = AimTargetFOV;
	}
	else if (IsValid(Combat) && Combat->bAiming)
	{
		// 开镜中切倍率档 / 换瞄具：平滑追过去。关镜过程中保持原值，让镜头从当前倍率退回默认
		SmoothedAimFOV = AimFOVInterpSpeed > 0.f
			? FMath::FInterpTo(SmoothedAimFOV, AimTargetFOV, DeltaTime, AimFOVInterpSpeed)
			: AimTargetFOV;
	}

	// 最终 FOV = 默认 FOV 和开镜 FOV 按开镜进度混合（死亡、关镜、切枪时开镜进度会回到 0）
	const float NewFOV = FMath::Lerp(DefaultFOV, SmoothedAimFOV, AimAlignAlpha);
	if (!FMath::IsNearlyEqual(FirstPersonCamera->FieldOfView, NewFOV, 0.001f))
	{
		FirstPersonCamera->SetFieldOfView(NewFOV);
	}
}

void AShooterCharacter::AddVisualRecoil(const FVector& KickLocation, const FRotator& KickRotation)
{
	if (!IsLocallyControlled() || !bEnableVisualRecoil) return;

	VisualRecoilLocTarget += KickLocation;
	VisualRecoilRotTarget += KickRotation;

	// 连发时别越叠越夸张：位移最多 8 厘米，旋转最多 10 度
	VisualRecoilLocTarget = VisualRecoilLocTarget.GetClampedToMaxSize(8.f);
	VisualRecoilRotTarget.Pitch = FMath::Clamp(VisualRecoilRotTarget.Pitch, -10.f, 10.f);
	VisualRecoilRotTarget.Yaw = FMath::Clamp(VisualRecoilRotTarget.Yaw, -10.f, 10.f);
	VisualRecoilRotTarget.Roll = FMath::Clamp(VisualRecoilRotTarget.Roll, -10.f, 10.f);
}

void AShooterCharacter::RemoveVisualRecoilFromMesh()
{
	if (!bVisualRecoilWritten || !IsValid(Mesh1P)) return;
	bVisualRecoilWritten = false;

	FVector BaseLoc = Mesh1P->GetRelativeLocation();
	FQuat BaseQuat = Mesh1P->GetRelativeRotation().Quaternion();

	// 只有 Mesh1P 还是我们上一帧写的值时才减回去。
	// 如果蓝图（比如开镜时间轴）这期间改过它，就以蓝图的新值为准，不再减
	if (BaseLoc.Equals(LastWrittenMeshLoc, 0.01f))
	{
		BaseLoc -= AppliedRecoilLocParent;
	}
	if (BaseQuat.Equals(LastWrittenMeshQuat, 1.e-4f))
	{
		BaseQuat = AppliedRecoilQuat.Inverse() * BaseQuat;
	}
	Mesh1P->SetRelativeLocationAndRotation(BaseLoc, BaseQuat);
}

void AShooterCharacter::ApplyVisualRecoilToMesh(float DeltaTime)
{
	if (!IsLocallyControlled() || !IsValid(Mesh1P) || !IsValid(FirstPersonCamera)) return;

	// 关掉时清空残留偏移（上一帧的偏移已在 RemoveVisualRecoilFromMesh 撤掉），之后不再碰 Mesh1P
	if (!bEnableVisualRecoil)
	{
		VisualRecoilLocTarget = FVector::ZeroVector;
		VisualRecoilRotTarget = FRotator::ZeroRotator;
		VisualRecoilLocCurrent = FVector::ZeroVector;
		VisualRecoilRotCurrent = FRotator::ZeroRotator;
		return;
	}

	const AWeapon* Weapon = IsValid(Combat) ? Combat->CurrentWeapon.Get() : nullptr;
	const float KickSpeed = IsValid(Weapon) ? Weapon->VisualRecoilKickSpeed : 30.f;
	const float RecoverySpeed = IsValid(Weapon) ? Weapon->VisualRecoilRecoverySpeed : 10.f;

	// 两段弹簧：目标慢慢回 0，当前值快速追目标 → 开枪瞬间“踢一下”，然后平滑回位
	VisualRecoilLocTarget = FMath::VInterpTo(VisualRecoilLocTarget, FVector::ZeroVector, DeltaTime, RecoverySpeed);
	VisualRecoilRotTarget = FMath::RInterpTo(VisualRecoilRotTarget, FRotator::ZeroRotator, DeltaTime, RecoverySpeed);
	VisualRecoilLocCurrent = FMath::VInterpTo(VisualRecoilLocCurrent, VisualRecoilLocTarget, DeltaTime, KickSpeed);
	VisualRecoilRotCurrent = FMath::RInterpTo(VisualRecoilRotCurrent, VisualRecoilRotTarget, DeltaTime, KickSpeed);

	if (VisualRecoilLocCurrent.IsNearlyZero(0.001f) && VisualRecoilRotCurrent.IsNearlyZero(0.001f)
		&& VisualRecoilLocTarget.IsNearlyZero(0.001f) && VisualRecoilRotTarget.IsNearlyZero(0.001f))
	{
		VisualRecoilLocCurrent = FVector::ZeroVector;
		VisualRecoilRotCurrent = FRotator::ZeroRotator;
		return;   // 没有后坐就不碰 Mesh1P
	}

	// 偏移是按镜头空间定义的（X 前、Z 上），换算到 Mesh1P 父组件的空间
	USceneComponent* Parent = Mesh1P->GetAttachParent();
	FVector LocParent = VisualRecoilLocCurrent;
	FQuat RotParent = VisualRecoilRotCurrent.Quaternion();
	if (IsValid(Parent) && Parent != FirstPersonCamera)
	{
		const FTransform CameraToParent = FirstPersonCamera->GetComponentTransform().GetRelativeTransform(Parent->GetComponentTransform());
		LocParent = CameraToParent.TransformVectorNoScale(VisualRecoilLocCurrent);
		const FQuat CamQ = CameraToParent.GetRotation();
		RotParent = CamQ * RotParent * CamQ.Inverse();
	}

	const FVector NewLoc = Mesh1P->GetRelativeLocation() + LocParent;
	const FQuat NewQuat = RotParent * Mesh1P->GetRelativeRotation().Quaternion();
	Mesh1P->SetRelativeLocationAndRotation(NewLoc, NewQuat);

	AppliedRecoilLocParent = LocParent;
	AppliedRecoilQuat = RotParent;
	LastWrittenMeshLoc = Mesh1P->GetRelativeLocation();
	LastWrittenMeshQuat = Mesh1P->GetRelativeRotation().Quaternion();
	bVisualRecoilWritten = true;
}

float AShooterCharacter::GetAimTimelineAlpha(float /*DeltaTime*/)
{
	return AimAlignAlpha;
}

void AShooterCharacter::UpdateAimSocketAlignment(float /*DeltaTime*/)
{
	if (!IsLocallyControlled() || !IsValid(Mesh1P) || !IsValid(FirstPersonCamera) || !IsValid(Combat))
	{
		return;
	}

	FVector* AdsLocationProperty = FindBlueprintStruct<FVector>(this, TEXT("AdsMesh1PLoc"));

	// C++ 驱动开镜：先算出开镜位置，再按开镜进度在腰射位置和它之间插值，写进 Mesh1P
	if (bDriveAdsInCode && bCantedAim)
	{
		// 第 10 步：侧瞄。旋转 + 位置都按侧瞄插槽算，瞬间到位
		FQuat CantedRotation;
		FVector CantedLocation;
		if (ComputeCantedMeshTransform(CantedRotation, CantedLocation))
		{
			Mesh1P->SetRelativeLocationAndRotation(CantedLocation, CantedRotation);
			return;
		}
	}

	if (bDriveAdsInCode)
	{
		// 第 8 步：腰射目标 = 角色默认位置 + 当前武器的腰射偏移。每帧读当前武器，切枪自动生效
		const AWeapon* HipWeapon = Combat->CurrentWeapon.Get();
		const FVector HipLocation = AuthoredHipMeshLocation + (IsValid(HipWeapon) ? HipWeapon->HipMesh1POffset : FVector::ZeroVector);
		const FRotator HipRotation = AuthoredHipMeshRotation + (IsValid(HipWeapon) ? HipWeapon->HipMesh1PRotationOffset : FRotator::ZeroRotator);
		// 先按开镜进度把旋转从腰射偏移过渡回默认旋转，再算开镜位置（完全开镜时旋转=默认，对准是精确的）
		Mesh1P->SetRelativeRotation(FQuat::Slerp(HipRotation.Quaternion(), AuthoredHipMeshRotation.Quaternion(), AimAlignAlpha));

		FVector AdsLocation = AdsLocationProperty ? *AdsLocationProperty : HipLocation;
		if (ComputeAdsMeshLocation(AdsLocation) && AdsLocationProperty)
		{
			*AdsLocationProperty = AdsLocation;   // 蓝图里还读这个变量的话，值也是对的
		}
		const FVector NewLocation = FMath::Lerp(HipLocation, AdsLocation, AimAlignAlpha);
		if (!NewLocation.ContainsNaN())
		{
			Mesh1P->SetRelativeLocation(NewLocation);
		}
		return;
	}

	// 旧方式：只算 AdsMesh1PLoc，蓝图时间轴去移动 Mesh1P
	FVector AdsLocation;
	if (AdsLocationProperty && ComputeAdsMeshLocation(AdsLocation))
	{
		*AdsLocationProperty = AdsLocation;
	}
}

bool AShooterCharacter::ComputeAdsMeshLocation(FVector& OutLocation)
{

	AWeapon* Weapon = Combat->CurrentWeapon;
	USkeletalMeshComponent* WeaponMesh = IsValid(Weapon) ? Weapon->GetMesh1P() : nullptr;
	if (!IsValid(WeaponMesh) || !WeaponMesh->IsAttachedTo(Mesh1P))
	{
		return false;
	}

	if (!Weapon->HasAimSocket())
	{
		if (Weapon->WeaponKind != EWeaponKind::Melee && MissingAimSocketWeapon.Get() != Weapon)
		{
			MissingAimSocketWeapon = Weapon;
			UE_LOG(LogTemp, Warning, TEXT("%s has no socket '%s'. ADS keeps the blueprint location."),
				*GetNameSafe(Weapon), *Weapon->AimSocketName.ToString());
		}
		return false;
	}

	USceneComponent* Parent = Mesh1P->GetAttachParent();
	if (!IsValid(Parent))
	{
		return false;
	}

	WeaponMesh->UpdateComponentToWorld();
	FVector SocketWorld = FVector::ZeroVector;
	if (!Weapon->GetAimSocketWorldLocation(SocketWorld))
	{
		return false;
	}

	// Mesh1P stays at its component rotation (yaw -90 on the camera). The blueprint
	// rotator variables are not applied, so measure the miss in camera space and
	// slide the current relative location by that parent-space delta.
	const FTransform CameraTransform = FirstPersonCamera->GetComponentTransform();
	const FVector SocketCamera = CameraTransform.InverseTransformPosition(SocketWorld);
	// 第 8 步：眼距。瞄具勾了覆盖就用瞄具的，否则用武器的 AdsEyeDistance；都为 0 时保持原距离（旧行为）
	float EyeDistance = Weapon->AdsEyeDistance;
	if (const UAttachmentData* EyeOptic = Weapon->GetOpticAttachment())
	{
		if (EyeOptic->bOverridesEyeDistance)
		{
			EyeDistance = EyeOptic->EyeDistance;
		}
	}
	// 第 8 步修正：眼距为 0 时，不能直接用当前瞄准点距离当目标（Mesh1P 每帧在动，会形成反馈漂移）。
	// 改用 Mesh1P 在腰射位置时瞄准点的距离：当前距离减去 Mesh1P 相对腰射位置沿镜头前方已挪过的量。
	// 这个值和 Mesh1P 当前位置无关，所以目标稳定、能收敛。
	float BaseEyeX = EyeDistance;
	if (BaseEyeX <= 0.f)
	{
		const FVector HipRelLocation = AuthoredHipMeshLocation + Weapon->HipMesh1POffset;
		const FVector MovedWorld = Parent->GetComponentTransform().TransformVector(Mesh1P->GetRelativeLocation() - HipRelLocation);
		const float MovedForward = CameraTransform.InverseTransformVector(MovedWorld).X;
		BaseEyeX = SocketCamera.X - MovedForward;
	}
	const FVector DesiredCamera(BaseEyeX, 0.f, 0.f);
	const FVector DesiredWorld = CameraTransform.TransformPosition(DesiredCamera);
	const FVector DeltaParent = Parent->GetComponentTransform().InverseTransformVector(DesiredWorld - SocketWorld);
	const FVector CenteredLocation = Mesh1P->GetRelativeLocation() + DeltaParent;
	if (CenteredLocation.ContainsNaN())
	{
		return false;
	}

	OutLocation = CenteredLocation;
	return true;
}




void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* ShooterInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	ShooterInputComponent->BindAction(CycleWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_CycleWeapon);
	ShooterInputComponent->BindAction(FireWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_FireWeapon_Pressed);
	ShooterInputComponent->BindAction(FireWeaponAction, ETriggerEvent::Completed, this, &ThisClass::Input_FireWeapon_Released);
	ShooterInputComponent->BindAction(AimWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_Aim_Pressed);
	ShooterInputComponent->BindAction(ReloadWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_ReloadWeapon);
	ShooterInputComponent->BindAction(EquipMainWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_EquipMainWeapon);
	ShooterInputComponent->BindAction(EquipSeconedaryWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_EquipSeconedaryWeapon);
	ShooterInputComponent->BindAction(EquipMeleeWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_EquipMeleeWeapon);
	ShooterInputComponent->BindAction(EquipPistolWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_EquipPistolWeapon);
	ShooterInputComponent->BindAction(HealAction, ETriggerEvent::Started, this, &ThisClass::Input_Heal);
	if (SwitchZoomAction)
	{
		ShooterInputComponent->BindAction(SwitchZoomAction, ETriggerEvent::Started, this, &ThisClass::Input_SwitchZoom);
	}
	if (SwitchFireModeAction)
	{
		ShooterInputComponent->BindAction(SwitchFireModeAction, ETriggerEvent::Started, this, &ThisClass::Input_SwitchFireMode);
	}
	if (VersionWeaponAction)
	{
		ShooterInputComponent->BindAction(VersionWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_VersionWeapon);
	}

}

void AShooterCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (IsValid(Combat))
	{
		Combat->SpawnInventory();
	}
}

void AShooterCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (IsValid(Combat))
	{
		Combat->InitializeWeaponWidget();
	}
}

FName AShooterCharacter::GetWeaponAttachPoint_Implementation(const FGameplayTag& WeaponType) const
{
	checkf(Combat, TEXT("Combat component is missing"));
	checkf(Combat->WeaponData, TEXT("No WeaponData Asset - Please Fill out BP_ShooterCharacter"));
	if (const FName* GripPoint = Combat->WeaponData->GripPoints.Find(WeaponType))
	{
		return *GripPoint;
	}

	UE_LOG(LogTemp, Warning, TEXT("No grip socket for weapon tag %s. Using RifleSocket."), *WeaponType.ToString());
	return TEXT("RifleSocket");
}

USkeletalMeshComponent* AShooterCharacter::GetMesh1P_Implementation() const
{
	return Mesh1P;
}

USkeletalMeshComponent* AShooterCharacter::GetMesh3P_Implementation() const
{
	return GetMesh();
}

void AShooterCharacter::WeaponReplicated_Implementation()
{
	if (!bWeaponFirstReplicated)
	{
		bWeaponFirstReplicated = true;
		OnWeaponFirstReplicated.Broadcast(Combat->CurrentWeapon);
	}
}

AWeapon* AShooterCharacter::GetCurrentWeapon_Implementation()
{
	return IsValid(Combat) ? Combat->CurrentWeapon : nullptr;
}

int32 AShooterCharacter::GetReserveAmmo_Implementation() const
{
	return IsValid(Combat) ? Combat->CurrentReserveAmmo : 0;
}

void AShooterCharacter::Notify_ReloadWeapon_Implementation()
{
	Combat->Notify_ReloadWeapon();
}

bool AShooterCharacter::DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator)
{
	if (!IsValid(Stats))return false;

	Stats->ChangeHealthByAmout(-DamageAmout, DamageInstigator);
	//calculate whether or not damage lethal

	const int32 MontageSelection = FMath::RandRange(0, HitReacts.Num() - 1);
	Multicast_HitReact(MontageSelection);

	return false;
}



void AShooterCharacter::Multicast_HitReact_Implementation(int32 MontageIndex)
{
	if (GetNetMode() != NM_DedicatedServer && !IsLocallyControlled())
	{
		if (HitReacts.IsValidIndex(MontageIndex))
		{
			GetMesh()->GetAnimInstance()->Montage_Play(HitReacts[MontageIndex]);
		}
	}
}

void AShooterCharacter::ConfigureWeaponHitCollision()
{
	// The capsule has no bone. Ignore it so the trace reaches the body mesh and Hit.BoneName is filled.
	GetCapsuleComponent()->SetCollisionResponseToChannel(FPSTraceChannels::ECC_Weapon, ECR_Ignore);

	USkeletalMeshComponent* BodyMesh = GetMesh();
	if (IsValid(BodyMesh))
	{
		BodyMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		BodyMesh->SetCollisionResponseToChannel(FPSTraceChannels::ECC_Weapon, ECR_Block);
	}

	if (IsValid(Mesh1P))
	{
		Mesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AShooterCharacter::Input_CycleWeapon()
{
	if (IsValid(Combat)) Combat->Initiate_CycleWeapon();
}

void AShooterCharacter::Input_SwitchZoom()
{
	if (!IsValid(Combat)) return;
	AWeapon* Weapon = Combat->CurrentWeapon;
	UStaticMeshComponent* CantedMesh = nullptr;
	const UAttachmentData* CantedData = nullptr;
	const bool bHasCanted = Combat->bAiming && IsValid(Weapon) && Weapon->GetCantedSight(CantedMesh, CantedData);
	if (!bHasCanted)
	{
		Combat->Initiate_CycleZoom();
		return;
	}

	// 第 12 步：循环 = 主镜各档 -> 侧瞄 -> 主镜第一档
	if (bCantedAim)
	{
		Weapon->CurrentZoomIndex = 0;
		SetCantedAim(false);
		Combat->OnZoomLevelChanged.Broadcast(Weapon->CurrentZoomIndex, Weapon->GetEffectiveAimFOV());
		return;
	}
	const int32 Count = Weapon->GetZoomLevelCount();
	if (Count >= 2 && Weapon->CurrentZoomIndex < Count - 1)
	{
		Combat->Initiate_CycleZoom();
		return;
	}
	SetCantedAim(true);
}

void AShooterCharacter::SetCantedAim(bool bNewCanted)
{
	if (bCantedAim == bNewCanted) return;
	bCantedAim = bNewCanted;
	// 切换是瞬间的：主镜和侧瞄之间不走开镜时间
	if (IsValid(Combat) && Combat->bAiming)
	{
		AimAlpha = 1.f;
		AimAlignAlpha = 1.f;
		if (!bCantedAim)
		{
			// 回主镜也是瞬间的：FOV 直接跳到主镜目标
			SmoothedAimFOV = Combat->GetCurrentAimFOV(DefaultFOV);
		}
	}
	OnCantedSightChanged.Broadcast(bCantedAim);
}

bool AShooterCharacter::ComputeCantedMeshTransform(FQuat& OutRelRotation, FVector& OutRelLocation)
{
	AWeapon* Weapon = IsValid(Combat) ? Combat->CurrentWeapon.Get() : nullptr;
	UStaticMeshComponent* CantedMesh = nullptr;
	const UAttachmentData* CantedData = nullptr;
	if (!IsValid(Weapon) || !Weapon->GetCantedSight(CantedMesh, CantedData)) return false;
	USceneComponent* Parent = Mesh1P->GetAttachParent();
	if (!IsValid(Parent)) return false;

	const FTransform CameraTransform = FirstPersonCamera->GetComponentTransform();

	// 1) 旋转：让侧瞄插槽的朝向和镜头一致（X 朝前、Z 朝上），枪就会按插槽的歪角整体转过去
	Mesh1P->UpdateComponentToWorld();
	CantedMesh->UpdateComponentToWorld();
	const FQuat SocketWorldQuat = CantedMesh->GetSocketQuaternion(CantedData->CantedSightSocketName);
	const FQuat MeshWorldQuat = Mesh1P->GetComponentQuat();
	const FQuat SocketRelToMesh = MeshWorldQuat.Inverse() * SocketWorldQuat;   // 插槽相对手臂的固定旋转
	const FQuat DesiredMeshWorld = CameraTransform.GetRotation() * SocketRelToMesh.Inverse();
	OutRelRotation = Parent->GetComponentQuat().Inverse() * DesiredMeshWorld;
	OutRelRotation.Normalize();

	// 2) 先把旋转写进去，再量插槽位置，按和主镜一样的方法把插槽挪到镜头正前方
	Mesh1P->SetRelativeRotation(OutRelRotation);
	Mesh1P->UpdateComponentToWorld();
	CantedMesh->UpdateComponentToWorld();
	const FVector SocketWorld = CantedMesh->GetSocketLocation(CantedData->CantedSightSocketName);
	const FVector SocketCamera = CameraTransform.InverseTransformPosition(SocketWorld);

	// 眼距：侧瞄自己的 > 枪的 AdsEyeDistance > 保持当前距离（只取前后距离，侧向偏移都归零，不会累加漂移）
	float EyeDistance = CantedData->CantedEyeDistance > 0.f ? CantedData->CantedEyeDistance : Weapon->AdsEyeDistance;
	if (EyeDistance <= 0.f)
	{
		EyeDistance = SocketCamera.X;
	}
	const FVector DesiredWorld = CameraTransform.TransformPosition(FVector(EyeDistance, 0.f, 0.f));
	const FVector DeltaParent = Parent->GetComponentTransform().InverseTransformVector(DesiredWorld - SocketWorld);
	OutRelLocation = Mesh1P->GetRelativeLocation() + DeltaParent;
	return !OutRelLocation.ContainsNaN() && !OutRelRotation.ContainsNaN();
}

void AShooterCharacter::Input_SwitchFireMode()
{
	if (IsValid(Combat)) Combat->Initiate_SwitchFireMode();
}

void AShooterCharacter::Input_ReloadWeapon()
{
	if (IsValid(Combat)) Combat->Initiate_ReloadWeapon();
}

void AShooterCharacter::Input_FireWeapon_Pressed()
{
	// 配件菜单打开时鼠标留给界面。Enhanced Input 仍会收到左键，这里不要再开枪。
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->bShowMouseCursor)
		{
			return;
		}
	}
	if (IsValid(Combat)) Combat->Initiate_FireWeapon_Pressed();
}

void AShooterCharacter::Input_FireWeapon_Released()
{
	if (IsValid(Combat)) Combat->Initiate_FireWeapon_Released();
}

void AShooterCharacter::Input_Aim_Pressed()
{
	if (!IsValid(Combat)) return;
	Combat->Initiate_Aim_Pressed();   // OnAim 现在由 HandleAimStateChanged 统一调用
}

void AShooterCharacter::EquipMainWeapon()
{
	if (IsValid(Combat)) Combat->Initiate_EquipMainWeapon();
}

void AShooterCharacter::EquipSeconedaryWeapon()
{
	if (IsValid(Combat)) Combat->Initiate_EquipSeconedaryWeapon();
}

void AShooterCharacter::EquipMeleeWeapon()
{
	if (IsValid(Combat)) Combat->Initiate_EquipMeleeWeapon();
}

void AShooterCharacter::EquipPistolWeapon()
{
	if (IsValid(Combat)) Combat->Initiate_EquipPistolWeapon();
}

void AShooterCharacter::VersionWeapon()
{
	if (IsValid(Combat)) Combat->Initiate_VersionWeapon();
}

bool AShooterCharacter::PlayHealMontage(USkeletalMeshComponent* HealMesh, bool bBindEnd)
{
	if (!IsValid(HealMesh) || !IsValid(HealMontage))
	{
		return false;
	}

	UAnimInstance* AnimInstance = HealMesh->GetAnimInstance();
	if (!IsValid(AnimInstance))
	{
		return false;
	}

	const float PlayedLength = AnimInstance->Montage_Play(HealMontage);
	if (PlayedLength <= 0.f)
	{
		return false;
	}

	HideEquippedWeaponForHeal();

	// Remote viewers bind this too, so their gun comes back when the 3P montage ends.
	if (bBindEnd || !IsLocallyControlled())
	{
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &AShooterCharacter::OnHealMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, HealMontage);
	}

	return true;
}

void AShooterCharacter::HideEquippedWeaponForHeal()
{
	if (!IsValid(Combat) || !IsValid(Combat->CurrentWeapon))
	{
		return;
	}

	HealHiddenWeapon = Combat->CurrentWeapon;
	HealHiddenWeapon->SetHiddenForHeal(true);
}

void AShooterCharacter::RestoreWeaponAfterHeal()
{
	AWeapon* HiddenWeapon = HealHiddenWeapon.Get();
	HealHiddenWeapon = nullptr;
	if (!IsValid(HiddenWeapon) || HiddenWeapon->IsHidden())
	{
		return;
	}

	HiddenWeapon->SetHiddenForHeal(false);
}

void AShooterCharacter::HealPlayer(float InHealAmout)
{
	if (!IsLocallyControlled() || bIsHealing || InHealAmout <= 0.f || !IsValid(Stats))
	{
		return;
	}

	if (Stats->DeathState != EDeathState::NotDead || Stats->Health >= Stats->MaxHealth)
	{
		return;
	}

	if (IsValid(Combat) && IsValid(Combat->CurrentWeapon))
	{
		const EWeaponStatus Status = Combat->CurrentWeapon->WeaponStatus;
		if (Status == EWeaponStatus::Reloading || Status == EWeaponStatus::Cycling || Status == EWeaponStatus::BoltCycling)
		{
			return;
		}
	}

	PendingHealAmout = InHealAmout;

	// The owner watches Mesh1P. Fall back to the body mesh when the arms instance cannot play it.
	const bool bPlayed = PlayHealMontage(Mesh1P, true) || PlayHealMontage(GetMesh(), true);
	if (!bPlayed)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s could not play HealMontage. Assign it on the character and use a slot the AnimBP evaluates."), *GetName());
		return;
	}

	bIsHealing = true;
	if (UWorld* World = GetWorld())
	{
		const float Timeout = HealMontage->GetPlayLength() + 2.f;
		World->GetTimerManager().SetTimer(HealLocalTimer, FTimerDelegate::CreateUObject(this, &AShooterCharacter::AbortLocalHeal), Timeout, false);
	}
	Server_BeginHeal(PendingHealAmout);
}

void AShooterCharacter::AbortLocalHeal()
{
	if (!bIsHealing)
	{
		return;
	}

	bIsHealing = false;
	PendingHealAmout = 0.f;
	RestoreWeaponAfterHeal();
	Server_FinishHeal(false);
}

void AShooterCharacter::Notify_HealPlayer()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	bIsHealing = false;
	PendingHealAmout = 0.f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealLocalTimer);
	}
	Server_FinishHeal(true);
}

void AShooterCharacter::OnHealMontageEnded(UAnimMontage* /*Montage*/, bool bInterrupted)
{
	RestoreWeaponAfterHeal();
	if (!IsLocallyControlled())
	{
		bIsHealing = false;
		return;
	}

	if (bInterrupted)
	{
		bIsHealing = false;
		PendingHealAmout = 0.f;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(HealLocalTimer);
		}
		Server_FinishHeal(false);
		return;
	}

	Notify_HealPlayer();
}

void AShooterCharacter::Server_BeginHeal_Implementation(float InHealAmout)
{
	if (bServerHealing || InHealAmout <= 0.f || HealAmout <= 0.f || !IsValid(Stats))
	{
		return;
	}

	if (Stats->DeathState != EDeathState::NotDead || Stats->Health >= Stats->MaxHealth)
	{
		return;
	}

	bServerHealing = true;
	ServerHealAmout = FMath::Min(InHealAmout, HealAmout);
	if (UWorld* World = GetWorld())
	{
		const float Timeout = (IsValid(HealMontage) ? HealMontage->GetPlayLength() : 0.f) + 2.f;
		World->GetTimerManager().SetTimer(HealServerTimer, FTimerDelegate::CreateUObject(this, &AShooterCharacter::AbortServerHeal), Timeout, false);
	}
	Multicast_PlayHealMontage();
}

void AShooterCharacter::AbortServerHeal()
{
	bServerHealing = false;
	ServerHealAmout = 0.f;
}

void AShooterCharacter::Server_FinishHeal_Implementation(bool bCompleted)
{
	if (!bServerHealing)
	{
		return;
	}

	bServerHealing = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealServerTimer);
	}
	if (!bCompleted || !IsValid(Stats))
	{
		return;
	}

	Stats->ChangeHealthByAmout(ServerHealAmout, this);
	ServerHealAmout = 0.f;
}

void AShooterCharacter::Multicast_PlayHealMontage_Implementation()
{
	if (IsLocallyControlled() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	PlayHealMontage(GetMesh(), false);
}

void AShooterCharacter::Input_EquipMainWeapon()
{
	EquipMainWeapon();
}

void AShooterCharacter::Input_EquipSeconedaryWeapon()
{
	EquipSeconedaryWeapon();
}

void AShooterCharacter::Input_EquipMeleeWeapon()
{
	EquipMeleeWeapon();
}

void AShooterCharacter::Input_EquipPistolWeapon()
{
	EquipPistolWeapon();
}

void AShooterCharacter::Input_Heal()
{
	HealPlayer(HealAmout);
}

void AShooterCharacter::Input_VersionWeapon()
{
	VersionWeapon();   // 第 9 步：检视武器
}

void AShooterCharacter::HandleAimStateChanged(bool bIsAiming)
{
	// 蓝图里的开镜动画/FOV 只在本地玩家身上播
	if (IsLocallyControlled())
	{
		OnAim(bIsAiming);
	}
	// 服务器和本地客户端都要改速度，两边一致才不会被拉回
	RefreshControllerMovementSpeed();
	RefreshLocomotionGait();
}

void AShooterCharacter::RefreshControllerMovementSpeed()
{
	// 模拟代理（别人电脑上的你）没有控制器，这里会是 nullptr，直接跳过
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetController()))
	{
		PC->RefreshMovementSpeed();
	}
}

void AShooterCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	RefreshControllerMovementSpeed();   // 下蹲会打断冲刺
	RefreshLocomotionGait();
}

void AShooterCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	RefreshControllerMovementSpeed();
	RefreshLocomotionGait();
}

bool AShooterCharacter::CanJumpInternal_Implementation() const
{
	// 客户端用同步过来的耐力做预测，服务器用真实耐力做最终判断
	return Super::CanJumpInternal_Implementation() && (!IsValid(Stats) || Stats->HasStaminaForJump());
}

void AShooterCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	// OnJumped 在客户端（预测）和服务器上都会调用，只在服务器扣，避免扣两次
	if (HasAuthority() && IsValid(Stats))
	{
		Stats->ChangeStaminaByAmout(-Stats->JumpStaminaCost);
	}
}

void AShooterCharacter::RefreshLocomotionGait()
{
	UpdateLocomotionGait(true);
}

void AShooterCharacter::UpdateLocomotionGait(bool bNotifyCombat)
{
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	GroundSpeed = Velocity.Size();

	const float MaxSpeed = IsValid(CMC)
		? (bIsCrouched ? CMC->MaxWalkSpeedCrouched : CMC->MaxWalkSpeed)
		: 0.f;

	// Want walk gait when allowed max speed is in the walk band (not Velocity/MaxWalkSpeed normalize).
	const bool bWantWalkGait = MaxSpeed > 0.f && MaxSpeed <= WalkGaitSpeedThreshold;
	// Only swap to Walk BS / use Walk reference speed if DA actually has Strafe_Walking.
	const bool bHasWalkBS = IsValid(Combat) && Combat->HasStrafeWalkingAnim();
	const bool bNewWalkGait = bWantWalkGait && bHasWalkBS;

	// PlayRate scales STEP FREQUENCY to match ground speed vs the authored sample speed
	// of the BlendSpace that is ACTUALLY playing (not the gait we wish we had).
	// Formula: LocomotionPlayRate = Clamp(GroundSpeed / RefSpeed, Min, Max)
	float RefSpeed;
	if (bIsCrouched)
	{
		RefSpeed = CrouchAnimReferenceSpeed; // Strafe_Crouching
	}
	else if (bNewWalkGait)
	{
		RefSpeed = WalkAnimReferenceSpeed; // Strafe_Walking
	}
	else
	{
		// Still on Jog Strafe_Standing (common when Strafe_Walking empty) — slow play rate = fewer steps
		RefSpeed = JogAnimReferenceSpeed;
	}
	RefSpeed = FMath::Max(RefSpeed, 1.f);

	if (GroundSpeed > 1.f)
	{
		LocomotionPlayRate = FMath::Clamp(GroundSpeed / RefSpeed, MinLocomotionPlayRate, MaxLocomotionPlayRate);
	}
	else
	{
		LocomotionPlayRate = 0.f; // idle: freeze locomotion BS feet (AnimBP should be in Idle state anyway)
	}

	const bool bGaitChanged = bNewWalkGait != bUseWalkGait;
	bUseWalkGait = bNewWalkGait;

	// 把步态写进 AnimInstance，蓝图可直接绑 Play Rate / BlendList（变量名需一致）
	auto PushToAnim = [this](USkeletalMeshComponent* AnimMesh)
	{
		if (!IsValid(AnimMesh)) return;
		UAnimInstance* Anim = AnimMesh->GetAnimInstance();
		if (!IsValid(Anim)) return;

		if (FBoolProperty* WalkProp = FindFProperty<FBoolProperty>(Anim->GetClass(), TEXT("bUseWalkGait")))
		{
			WalkProp->SetPropertyValue_InContainer(Anim, bUseWalkGait);
		}
		auto SetFloatProp = [Anim](const TCHAR* Name, float Value)
		{
			if (FFloatProperty* Prop = FindFProperty<FFloatProperty>(Anim->GetClass(), Name))
			{
				Prop->SetPropertyValue_InContainer(Anim, Value);
			}
		};
		// Primary name + common aliases so AnimBP Play Rate pin can use either
		SetFloatProp(TEXT("LocomotionPlayRate"), LocomotionPlayRate);
		SetFloatProp(TEXT("PlayRate"), LocomotionPlayRate);
		SetFloatProp(TEXT("BS_PlayRate"), LocomotionPlayRate);
		SetFloatProp(TEXT("GroundSpeed"), GroundSpeed);
		SetFloatProp(TEXT("Speed"), GroundSpeed);
	};
	PushToAnim(Mesh1P);
	PushToAnim(GetMesh());

	if (bNotifyCombat && bGaitChanged && IsValid(Combat))
	{
		Combat->RefreshLocomotionBlendSpaces(bUseWalkGait);
	}
}
