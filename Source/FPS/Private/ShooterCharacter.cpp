// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterCharacter.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Ui/PlayerNameplateWidget.h"
#include "Loadout/PouchInventoryComponent.h"
#include "Ui/HudBodyMarks.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "Game/ShooterGameState.h"
#include "Engine/DecalActor.h"
#include "Components/DecalComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "WeaponData.h"
#include "Weapon/Weapon.h"
#include "Weapon/WeaponObstacle.h"
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
#include "Net/UnrealNetwork.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Engine/LocalPlayer.h"
#include "ShooterGameModeBase.h"
#include "Game/ShooterPlayerState.h"
#include "Interface/HostageProtectInterface.h"
#include "CombatComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/EngineTypes.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/Package.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Range/TargetDummy.h"

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

	bool IsBanditsSkelMesh(const USkeletalMesh* Mesh)
	{
		if (!IsValid(Mesh))
		{
			return false;
		}
		const FString Path = Mesh->GetPathName();
		return Path.Contains(TEXT("Bandit")) || Path.Contains(TEXT("Arms_Gloves"));
	}

	USkeletalMesh* LoadCanonicalArms1PSource()
	{
		static const TCHAR* Paths[] = {
			TEXT("/Game/FirstPersonBPIronbelly/Arms/SK_FPSArms.SK_FPSArms"),
			TEXT("/Game/FirstPersonRifleAnimations/Demo/FirstPersonArms/Character/Mesh/SK_Mannequin_Arms.SK_Mannequin_Arms"),
		};
		for (const TCHAR* Path : Paths)
		{
			if (USkeletalMesh* Loaded = LoadObject<USkeletalMesh>(nullptr, Path))
			{
				if (!IsBanditsSkelMesh(Loaded))
				{
					return Loaded;
				}
			}
		}
		return nullptr;
	}

	USkeletalMesh* LoadCanonicalMesh3PSource()
	{
		static const TCHAR* Paths[] = {
			TEXT("/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"),
			TEXT("/Game/FPS_Controller/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"),
			TEXT("/Game/FPS_Controller/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny"),
			TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny"),
		};
		USkeletalMesh* Fallback = nullptr;
		for (const TCHAR* Path : Paths)
		{
			if (USkeletalMesh* Loaded = LoadObject<USkeletalMesh>(nullptr, Path))
			{
				if (IsBanditsSkelMesh(Loaded))
				{
					continue;
				}
				if (Loaded->FindSocket(TEXT("RifleSocketak")) || Loaded->FindSocket(TEXT("SOCKET_Weapon")))
				{
					return Loaded;
				}
				if (!Fallback)
				{
					Fallback = Loaded;
				}
			}
		}
		return Fallback;
	}
}



AShooterCharacter::AShooterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	SlideAnim = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/FreeAnimationLibrary/Animations/Slide/anim_Slide_L.anim_Slide_L")));

	// 默认脚步 Cue（FPS_Controller）。BP 可覆盖；Wood/Glass 暂无 Default。
	FootstepSounds.Default = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/FPS_Controller/Audio/Footsteps/SC_Footsteps_Generic_Stereo.SC_Footsteps_Generic_Stereo")));
	FootstepSounds.Concrete = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/FPS_Controller/Audio/Footsteps/SC_Footsteps_Tile.SC_Footsteps_Tile")));
	FootstepSounds.Metal = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/FPS_Controller/Audio/Footsteps/SC_Footsteps_Metal.SC_Footsteps_Metal")));
	FootstepSounds.Wood = FootstepSounds.Default;
	FootstepSounds.Glass = FootstepSounds.Default;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->TargetArmLength = 0.f;
	SpringArm->bEnableCameraLag = false;   // 第一人称位置延迟会让高速移动/滑铲时手臂和镜头"拖在身后"
	SpringArm->CameraLagSpeed = 15.f;

	SlideDuration = 0.75f;
	SlideSpeedBoost = 200.f;
	SlideEndSpeed = 350.f;
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->AirControl = 0.4f;
		CMC->JumpZVelocity = 0.f; // TacticalTrim: jump disabled
		CMC->GravityScale = 1.5f;
		CMC->BrakingDecelerationFalling = 200.f;
		CMC->MaxAcceleration = 3000.f;
		CMC->BrakingDecelerationWalking = 1800.f;
		CMC->GroundFriction = 8.f;
		CMC->bUseSeparateBrakingFriction = true;
		CMC->BrakingFriction = 6.f;
		CMC->FallingLateralFriction = 0.f;
	}
	// TacticalTrim: locomotion play rate may slow (<1) but never speed up above 1.
	MaxLocomotionPlayRate = 1.0f;
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

	// FP sleeve cloth (LeaderPose follower of Mesh1P). Bandits has no dedicated sleeve SKM — use jacket/bomber.
	Mesh1P_Sleeves = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P_Sleeves"));
	Mesh1P_Sleeves->SetupAttachment(Mesh1P);
	Mesh1P_Sleeves->bOnlyOwnerSee = true;
	Mesh1P_Sleeves->bOwnerNoSee = false;
	Mesh1P_Sleeves->bCastDynamicShadow = false;
	Mesh1P_Sleeves->bReceivesDecals = false;
	Mesh1P_Sleeves->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh1P_Sleeves->SetGenerateOverlapEvents(false);
	Mesh1P_Sleeves->SetVisibility(false);
	Mesh1P_Sleeves->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh1P_Sleeves->PrimaryComponentTick.TickGroup = TG_PrePhysics;

	//BodyMeshB
	GetMesh()->bOnlyOwnerSee = false;
	GetMesh()->bOwnerNoSee = true;
	GetMesh()->bReceivesDecals = false;

	// CharGen modular 3P parts (LeaderPose followers). Hidden until Police look.
	Mesh3P_Head = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh3P_Head"));
	Mesh3P_Head->SetupAttachment(GetMesh());
	Mesh3P_Head->bOnlyOwnerSee = false;
	Mesh3P_Head->bOwnerNoSee = true;
	Mesh3P_Head->bReceivesDecals = false;
	Mesh3P_Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh3P_Head->SetVisibility(false);

	Mesh3P_BodyUp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh3P_BodyUp"));
	Mesh3P_BodyUp->SetupAttachment(GetMesh());
	Mesh3P_BodyUp->bOnlyOwnerSee = false;
	Mesh3P_BodyUp->bOwnerNoSee = true;
	Mesh3P_BodyUp->bReceivesDecals = false;
	Mesh3P_BodyUp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh3P_BodyUp->SetVisibility(false);

	Mesh3P_Pants = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh3P_Pants"));
	Mesh3P_Pants->SetupAttachment(GetMesh());
	Mesh3P_Pants->bOnlyOwnerSee = false;
	Mesh3P_Pants->bOwnerNoSee = true;
	Mesh3P_Pants->bReceivesDecals = false;
	Mesh3P_Pants->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh3P_Pants->SetVisibility(false);

	Mesh3P_Jacket = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh3P_Jacket"));
	Mesh3P_Jacket->SetupAttachment(GetMesh());
	Mesh3P_Jacket->bOnlyOwnerSee = false;
	Mesh3P_Jacket->bOwnerNoSee = true;
	Mesh3P_Jacket->bReceivesDecals = false;
	Mesh3P_Jacket->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh3P_Jacket->SetVisibility(false);

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
	Combat->SetIsReplicated(true);

	Pouch = CreateDefaultSubobject<UPouchInventoryComponent>(TEXT("Pouch"));
	Pouch->SetIsReplicated(true);

	Stats = CreateDefaultSubobject<UStatsComponent>("Stats");
	Stats->SetIsReplicated(true);

	Nameplate = CreateDefaultSubobject<UWidgetComponent>(TEXT("PlayerNameplate"));
	Nameplate->SetupAttachment(GetCapsuleComponent());
	Nameplate->SetRelativeLocation(FVector(0.f, 0.f, 104.f));
	Nameplate->SetWidgetSpace(EWidgetSpace::Screen);
	Nameplate->SetWidgetClass(UPlayerNameplateWidget::StaticClass());
	Nameplate->SetDrawSize(FVector2D(220.f, 32.f));
	Nameplate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Nameplate->SetGenerateOverlapEvents(false);
	Nameplate->SetOwnerNoSee(true);
	Nameplate->SetCastShadow(false);

	DefaultFOV = 90.f;
	TurningStatus = ETurningInPlace::NotTurning;

	bWeaponFirstReplicated = false;

	ConfigureWeaponHitCollision();

	HealAmout = 30.f;
}

void AShooterCharacter::BeginPlay()
{
	// 倍镜画中画（配件瞄具 OpticType = Magnified 时用，PSR 走这条）
	if (GetNetMode() != NM_DedicatedServer && !FindComponentByClass<UOpticScopeComponent>())
	{
		UOpticScopeComponent* OpticScope = NewObject<UOpticScopeComponent>(this, TEXT("OpticScope"));
		OpticScope->RegisterComponent();
	}

	Super::BeginPlay();
	RefreshNameplate();

	Stats->OnDeathStarted.AddDynamic(this, &ThisClass::OnDeathStarted);

	// A Blueprint collision override is applied after the constructor. Set the weapon channel again here.
	ConfigureWeaponHitCollision();

	FirstPersonCamera->SetFieldOfView(DefaultFOV);
	CameraBaseLocation = FirstPersonCamera->GetRelativeLocation();
	CameraBaseRotation = FirstPersonCamera->GetRelativeRotation();
	bCameraBaseCaptured = true;

	if (IsValid(SpringArm))
	{
		SpringArm->bEnableCameraLag = false;   // 蓝图里勾了也强制关（蹲起高度改由 CrouchCameraOffsetZ 平滑）
		SpringArmBaseLocation = SpringArm->GetRelativeLocation();
		bSpringArmBaseCaptured = true;
	}
	if (UCharacterMovementComponent* ProneCMC = GetCharacterMovement())
	{
		DefaultCrouchedHalfHeight = ProneCMC->GetCrouchedHalfHeight();
	}

	StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);

	if (IsValid(Mesh1P))
	{
		AuthoredHipMeshLocation = Mesh1P->GetRelativeLocation();
		AuthoredHipMeshRotation = Mesh1P->GetRelativeRotation();
	}

	EnsureFactionLookDefaults();
	// Spawn default: Bandits police look (3P Preset_13 + matching 1P arms/sleeves). BP may still serialize Default.
	// Future loadout UI / PickFaction can call SetFactionLookLocal to change this.
	if (FactionLook == ECharacterLookFaction::Default)
	{
		FactionLook = ECharacterLookFaction::Police;
	}
	if (!bCapturedDefaultMesh3P && IsValid(GetMesh()))
	{
		USkeletalMesh* CurBody = GetMesh()->GetSkeletalMeshAsset();
		DefaultMesh3PAnimClass = GetMesh()->GetAnimClass();
		DefaultMesh3PAsset = IsBanditsSkelMesh(CurBody) ? LoadCanonicalMesh3PSource() : CurBody;
		if (!IsValid(DefaultMesh3PAsset))
		{
			DefaultMesh3PAsset = CurBody;
		}
		bCapturedDefaultMesh3P = true;
	}
	ApplyFactionLookMeshes();
	VisionBatterySeconds = VisionBatteryMaxSeconds;
	if (WatchCheckMontage.IsNull())
	{
		WatchCheckMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Actions/AM_RAR_FP_PCH_AssaultRifle_WatchCheck.AM_RAR_FP_PCH_AssaultRifle_WatchCheck")));
	}
	if (BloodFootprintDecal.IsNull())
	{
		BloodFootprintDecal = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/DEFINITIVE_FPS_KIT/NPC/Zombies/Decal_Particles/M_blood_decal_Mat.M_blood_decal_Mat")));
	}
	if (BloodHitDecal.IsNull())
	{
		BloodHitDecal = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/DEFINITIVE_FPS_KIT/NPC/Zombies/Decal_Particles/M_blood_decal_Mat.M_blood_decal_Mat")));
	}
	if (BloodHitNiagara.IsNull())
	{
		BloodHitNiagara = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/DEFINITIVE_FPS_KIT/Niagara_VFX/Niagara/Impact/Impact_Blood.Impact_Blood")));
	}
	// Always write custom depth so Thermal can silhouette others when a local player enables it.
	if (USkeletalMeshComponent* M3 = GetMesh())
	{
		M3->SetRenderCustomDepth(true);
		M3->SetCustomDepthStencilValue(1);
	}

	ConfigureAimTickOrder();

	if (IsValid(Combat))
	{
		// 不管是按右键还是切枪自动关镜，都走这一个入口
		Combat->OnAimStateChanged.AddUObject(this, &ThisClass::HandleAimStateChanged);
	}

	AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetController());
	if (IsValid(PC))
	{
		PC->bPawnAlive = true;
	}
}

void AShooterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 枪是独立 Actor，且 Mesh1P 是根组件、挂在角色手臂上。
	// 不能放到 BeginDestroy：那时已经在 GC 里，再 Destroy 会在 ~AWeapon 读到坏指针。
	if (IsValid(Combat))
	{
		Combat->DestroyInventory();
	}

	Super::EndPlay(EndPlayReason);
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
	// 被观战的远端角色：用复制的瞄准方向（RemoteViewPitch）驱动镜头，上下看也跟着
	if (bFirstPersonViewedLocally && !IsLocallyControlled() && IsValid(SpringArm))
	{
		SpringArm->SetWorldRotation(GetFixedAimRotation());
	}
	UpdateLocomotionGait(true);
	UpdateFootsteps(DeltaTime);

	CalculateTurnInPlaceParameters(DeltaTime);
	CalculateFABRIKSocket(DeltaTime);
	if (bWeaponAttachDebug)
	{
		DrawWeaponAttachDebug();
	}
	UpdateAimAlpha(DeltaTime);             // 开镜进度先推进，下面对准和 FOV 都用它
	RemoveVisualRecoilFromMesh();          // 开镜对准要按没有后坐的位置算
	UpdateAimSocketAlignment(DeltaTime);
	UpdateWeaponObstacle(DeltaTime);       // 枪管顶墙：抬枪/收枪，拦开镜
	UpdateMuzzleDip(DeltaTime);            // 近距离瞄队友/人质：枪口下压
	UpdateWeaponSway(DeltaTime);           // 转视角拖拽 / 走路晃 / 落地下沉（和枪身后坐一起写进 Mesh1P）
	ApplyVisualRecoilToMesh(DeltaTime);    // 算完再把枪身后坐叠回去
	UpdateBandage(DeltaTime);              // 长按包扎进度
	UpdateTacticalVision(DeltaTime);       // NVG / Thermal PP + battery
	UpdateAimFieldOfView(DeltaTime);
	UpdateAltPeek();
	UpdateLean(DeltaTime);
	UpdateMomentumCarry(DeltaTime);

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

	// Shotgun 1P: AO_Yaw feeds ABP AimOffset - zero it so arms stay on authored pose.
	if (IsValid(Combat) && IsValid(Combat->CurrentWeapon)
		&& Combat->CurrentWeapon->WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_ShotGun))
	{
		AO_Yaw = 0.f;
		InterpAO_Yaw = 0.f;
	}
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
		FABRIK_Alpha = 0.f;
		FABRIK_SocketTransform = FTransform::Identity;
		FABRIK_SocketTransform1P = FTransform::Identity;
		return;
	}

	// Melee: both hands follow knife anims only
	if (Combat->CurrentWeapon->WeaponKind == EWeaponKind::Melee)
	{
		FABRIK_Alpha1P = 0.f;
		FABRIK_Alpha = 0.f;
		FABRIK_SocketTransform = FTransform::Identity;
		FABRIK_SocketTransform1P = FTransform::Identity;
		return;
	}

	// Third-person: only authored left-hand sockets (grip / FABRIK / LeftHandSocket).
	// Do not IK to Barrel/Muzzle approximations — those looked like broken FABRIK on Bandits.
	FTransform SocketWorld;
	bool bHaveFabrik = Combat->CurrentWeapon->GetFabrikWorldTransform(false, SocketWorld);
	float WantAlpha = 0.f;
	if (bHaveFabrik)
	{
		WriteHandSpace(GetMesh(), SocketWorld, FABRIK_SocketTransform);
		WantAlpha = bProneCrawling ? 0.f : 1.f;
	}
	else
	{
		FABRIK_SocketTransform = FTransform::Identity;
		WantAlpha = 0.f;
	}

	if (bDisableLeftHandIKDuringReload
		&& Combat->CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading)
	{
		WantAlpha = 0.f;
	}
	if (!DisableLeftHandIKCurveName.IsNone())
	{
		if (USkeletalMeshComponent* Body = GetMesh())
		{
			if (UAnimInstance* Anim = Body->GetAnimInstance())
			{
				const float DisableCurve = Anim->GetCurveValue(DisableLeftHandIKCurveName);
				if (DisableCurve > 0.01f)
				{
					WantAlpha = 0.f;
				}
			}
		}
	}

	const float Blend = FMath::Clamp(LeftHandIKBlendSpeed * DeltaTime, 0.f, 1.f);
	FABRIK_Alpha = FMath::Lerp(FABRIK_Alpha, WantAlpha, Blend);

	// First-person: snap the left hand only when a foregrip is mounted.
	// Pack idle poses often already place both hands; FABRIK without a grip can fight that pose.
	FTransform SocketWorld1P;
	float WantAlpha1P = 0.f;
	const bool bWantGripSnap1P =
		Combat->CurrentWeapon->GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Grip) != nullptr;
	if (bWantGripSnap1P && Combat->CurrentWeapon->GetFabrikWorldTransform(true, SocketWorld1P) && IsValid(Mesh1P))
	{
		FABRIK_SocketTransform1P = SocketWorld1P.GetRelativeTransform(Mesh1P->GetComponentTransform());
		WantAlpha1P = 1.f;
	}
	else
	{
		FABRIK_SocketTransform1P = FTransform::Identity;
		WantAlpha1P = 0.f;
	}

	if (bDisableLeftHandIKDuringReload
		&& Combat->CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading)
	{
		WantAlpha1P = 0.f;
	}
	if (!DisableLeftHandIKCurveName.IsNone() && IsValid(Mesh1P))
	{
		if (UAnimInstance* Anim1P = Mesh1P->GetAnimInstance())
		{
			const float DisableCurve = Anim1P->GetCurveValue(DisableLeftHandIKCurveName);
			if (DisableCurve > 0.01f)
			{
				WantAlpha1P = 0.f;
			}
			const float MaskGrip = Anim1P->GetCurveValue(TEXT("Mask_Grip"));
			if (MaskGrip > 0.01f)
			{
				WantAlpha1P *= FMath::Clamp(1.f - MaskGrip, 0.f, 1.f);
			}
		}
	}

	FABRIK_Alpha1P = FMath::Lerp(FABRIK_Alpha1P, WantAlpha1P, Blend);
}

void AShooterCharacter::DrawWeaponAttachDebug() const
{
	UWorld* World = GetWorld();
	AWeapon* Weapon = IsValid(Combat) ? Combat->CurrentWeapon.Get() : nullptr;
	USkeletalMeshComponent* Arms = Mesh1P;
	if (!World)
	{
		return;
	}

	auto DrawAxis = [World](const FTransform& Xf, float Size, const FColor& Color)
	{
		const FVector Loc = Xf.GetLocation();
		DrawDebugCoordinateSystem(World, Loc, Xf.Rotator(), Size, false, 0.f, 255);
		DrawDebugSphere(World, Loc, 1.6f, 8, Color, false, 0.f, 255);
	};

	FString Line = TEXT("[WeaponDebug]\n");
	if (!IsValid(Weapon) || !IsValid(Arms))
	{
		Line += TEXT("no weapon or Mesh1P");
		if (GEngine) GEngine->AddOnScreenDebugMessage(0x57415431, 0.f, FColor::Red, Line);
		return;
	}

	USkeletalMeshComponent* Gun = Weapon->GetMesh1P();
	const FName Sock = IsValid(Gun) ? Gun->GetAttachSocketName() : NAME_None;
	const USceneComponent* Parent = IsValid(Gun) ? Gun->GetAttachParent() : nullptr;
	Line += FString::Printf(TEXT("gun=%s type=%s custom1P=%d\n"),
		*GetNameSafe(Weapon), *Weapon->WeaponType.ToString(), Weapon->UsesCustomMesh1PAttach() ? 1 : 0);
	Line += FString::Printf(TEXT("1P attach sock='%s' parent=%s\n"),
		*Sock.ToString(), *GetNameSafe(Parent ? Parent->GetOwner() : nullptr));
	Line += FString::Printf(TEXT("arms mesh=%s\n"), *GetNameSafe(Arms->GetSkeletalMeshAsset()));
	Line += FString::Printf(TEXT("idle=%s aim=%s\n"),
		*GetNameSafe(Weapon->ArmsIdleSequence), *GetNameSafe(Weapon->ArmsAimIdleSequence));
	Line += FString::Printf(TEXT("FABRIK a1P=%.2f a3P=%.2f grip=%s\n"),
		FABRIK_Alpha1P, FABRIK_Alpha, *GetNameSafe(Weapon->GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Grip)));

	if (IsValid(Gun))
	{
		DrawAxis(Gun->GetComponentTransform(), 8.f, FColor::Yellow);
		static const FName Marks[] = {
			TEXT("GripSocket"), TEXT("AimPoint"), TEXT("MuzzleSocket"), TEXT("FABRIK")
		};
		for (const FName Mark : Marks)
		{
			if (Gun->DoesSocketExist(Mark))
			{
				DrawAxis(Gun->GetSocketTransform(Mark), 6.f, FColor::Cyan);
			}
		}
	}

	if (!Sock.IsNone() && Arms->DoesSocketExist(Sock))
	{
		DrawAxis(Arms->GetSocketTransform(Sock), 10.f, FColor::Green);
	}
	else if (!Sock.IsNone() && Arms->GetBoneIndex(Sock) != INDEX_NONE)
	{
		DrawAxis(Arms->GetBoneTransform(Arms->GetBoneIndex(Sock)), 10.f, FColor::Green);
	}

	FTransform GripWorld;
	if (Weapon->GetFabrikWorldTransform(true, GripWorld))
	{
		DrawAxis(GripWorld, 7.f, FColor::Magenta);
		Line += TEXT("FABRIK target OK (magenta)\n");
	}
	else
	{
		Line += TEXT("FABRIK target MISSING\n");
	}

	Line += TEXT("Yel=gun origin  Grn=arm attach  Cyan=gun sockets  Mag=hand IK");
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(0x57415431, 0.f, FColor::White, Line);
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

	// 第 13 步：这把枪上次停在侧瞄，开镜时直接回到侧瞄（侧瞄没了就留在主镜）
	if (Target > 0.f && IsValid(Weapon) && Weapon->bRememberCanted)
	{
		UStaticMeshComponent* CantedMesh = nullptr;
		const UAttachmentData* CantedData = nullptr;
		if (Weapon->GetCantedSight(CantedMesh, CantedData))
		{
			SetCantedAim(true);
			return;
		}
	}

	// 开镜和关镜都用同一个时间；时间为 0 就瞬间到位
	float AdsTime = IsValid(Weapon) ? Weapon->GetEffectiveAdsTime() : 0.2f;
	// 用 bIsSliding 作目标，平滑追过去--不要跟 SlideAlpha 绑死（滑铲结束/开镜中途倍率突变会让枪卡一下）
	float DesiredAdsScale = bIsSliding ? SlideAdsTimeMultiplier : 1.f;
	if (bArmInjured) DesiredAdsScale *= ArmInjuryAdsTimeMultiplier;
	if (BodyArmor == EBodyArmorType::Heavy) DesiredAdsScale *= HeavyArmorAdsTimeMultiplier;
	AdsTimeScaleCurrent = FMath::FInterpTo(AdsTimeScaleCurrent, DesiredAdsScale, DeltaTime, 6.f);
	AdsTime *= AdsTimeScaleCurrent;
	if (AdsTime <= KINDA_SMALL_NUMBER)
	{
		AimAlpha = Target;
	}
	else
	{
		AimAlpha = FMath::FInterpConstantTo(AimAlpha, Target, DeltaTime, 1.f / AdsTime);
	}
	// 给手臂和 FOV 用的是先快后慢（EaseOut）的值：开镜前段更跟手，到位时不生硬
	AimAlignAlpha = FMath::InterpEaseOut(0.f, 1.f, AimAlpha, 2.f);
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

void AShooterCharacter::AddVisualRecoil(const FVector& /*KickLocation*/, const FRotator& /*KickRotation*/)
{
	// [NoMeshKick][RecoilFeel2] gun-model Loc/Rot kick permanently disabled (push-back looked wrong).
	VisualRecoilLocTarget = FVector::ZeroVector;
	VisualRecoilRotTarget = FRotator::ZeroRotator;
	VisualRecoilLocCurrent = FVector::ZeroVector;
	VisualRecoilRotCurrent = FRotator::ZeroRotator;
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

	// 关掉时清空残留偏移（上一帧的偏移已在 RemoveVisualRecoilFromMesh 撤掉）
	if (!bEnableVisualRecoil)
	{
		VisualRecoilLocTarget = FVector::ZeroVector;
		VisualRecoilRotTarget = FRotator::ZeroRotator;
		VisualRecoilLocCurrent = FVector::ZeroVector;
		VisualRecoilRotCurrent = FRotator::ZeroRotator;
	}
	else
	{
		const AWeapon* Weapon = IsValid(Combat) ? Combat->CurrentWeapon.Get() : nullptr;
		const float KickSpeed = IsValid(Weapon) ? Weapon->VisualRecoilKickSpeed : 50.f;
		const float RecoverySpeed = IsValid(Weapon) ? Weapon->VisualRecoilRecoverySpeed : 16.f;

		// 两段弹簧：目标慢慢回 0，当前值快速追目标 → 开枪瞬间"踢一下"，然后平滑回位
		VisualRecoilLocTarget = FMath::VInterpTo(VisualRecoilLocTarget, FVector::ZeroVector, DeltaTime, RecoverySpeed);
		VisualRecoilRotTarget = FMath::RInterpTo(VisualRecoilRotTarget, FRotator::ZeroRotator, DeltaTime, RecoverySpeed);
		VisualRecoilLocCurrent = FMath::VInterpTo(VisualRecoilLocCurrent, VisualRecoilLocTarget, DeltaTime, KickSpeed);
		VisualRecoilRotCurrent = FMath::RInterpTo(VisualRecoilRotCurrent, VisualRecoilRotTarget, DeltaTime, KickSpeed);

		if (VisualRecoilLocCurrent.IsNearlyZero(0.001f) && VisualRecoilRotCurrent.IsNearlyZero(0.001f)
			&& VisualRecoilLocTarget.IsNearlyZero(0.001f) && VisualRecoilRotTarget.IsNearlyZero(0.001f))
		{
			VisualRecoilLocCurrent = FVector::ZeroVector;
			VisualRecoilRotCurrent = FRotator::ZeroRotator;
		}
	}

	// 后坐 + 晃动 + 枪管顶墙抬枪（都是镜头空间）
	// [RecoilFeel2] exclude VisualRecoilLoc/Rot — only sway + barrel-block remain on Mesh1P
	const FQuat OtherRotation = (SwayRotCurrent + MuzzleDipRotCurrent + FRotator(SwayDipZ * 0.4f, 0.f, 0.f)).Quaternion();
	const FQuat BlockRotation = WeaponBlockRotCurrent.Quaternion();
	const FQuat TotalRotation = OtherRotation * BlockRotation;
	const FTransform& CameraTransform = FirstPersonCamera->GetComponentTransform();
	const FVector MeshOrigin = CameraTransform.InverseTransformPosition(Mesh1P->GetComponentLocation());
	const FVector Grip = Mesh1P->DoesSocketExist(TEXT("hand_r"))
		? CameraTransform.InverseTransformPosition(Mesh1P->GetSocketLocation(TEXT("hand_r")))
		: FVector(20.f, 10.f, -15.f);
	const FVector GripPivotCorrection = WeaponObstacle::PivotCorrection(MeshOrigin, Grip, BlockRotation);
	const FVector TotalLoc = SwayLocCurrent + OtherRotation.RotateVector(WeaponBlockLocCurrent + GripPivotCorrection)
		+ FVector(0.f, 0.f, SwayDipZ - MuzzleDipAlpha * 2.f);
	if (TotalLoc.IsNearlyZero(0.001f) && TotalRotation.Equals(FQuat::Identity, 1.e-5f))
	{
		return;   // 没有偏移就不碰 Mesh1P
	}

	// 偏移是按镜头空间定义的（X 前、Z 上），换算到 Mesh1P 父组件的空间
	USceneComponent* Parent = Mesh1P->GetAttachParent();
	FVector LocParent = TotalLoc;
	FQuat RotParent = TotalRotation;
	if (IsValid(Parent) && Parent != FirstPersonCamera)
	{
		const FTransform CameraToParent = FirstPersonCamera->GetComponentTransform().GetRelativeTransform(Parent->GetComponentTransform());
		LocParent = CameraToParent.TransformVectorNoScale(TotalLoc);
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

void AShooterCharacter::UpdateWeaponSway(float DeltaTime)
{
	if (!IsLocallyControlled() || DeltaTime <= KINDA_SMALL_NUMBER) return;

	const UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (IsValid(CMC) && CMC->IsFalling())
	{
		LastFallingVelocityZ = GetVelocity().Z;
	}

	const bool bDead = IsValid(Stats) && Stats->DeathState != EDeathState::NotDead;
	if (!bEnableWeaponSway || bDead || bCantedAim)
	{
		SwayLocCurrent = FVector::ZeroVector;
		SwayRotCurrent = FRotator::ZeroRotator;
		bHasLastSwayControlRot = false;
		LandingDipTarget = LandingDipCurrent = 0.f;
		SwayDipZ = 0.f;
		return;
	}

	// 开镜时晃动减小；手臂受伤时晃动加大
	float AimScale = FMath::Lerp(1.f, SwayAimMultiplier, FMath::Clamp(AimAlignAlpha, 0.f, 1.f));
	if (bArmInjured) AimScale *= ArmInjurySwayMultiplier;

	// 1) 转视角拖拽：视角角速度（度/秒）→ 枪反向偏一点
	const FRotator ControlRot = GetControlRotation();
	float YawRate = 0.f, PitchRate = 0.f;
	if (bHasLastSwayControlRot)
	{
		const FRotator Delta = (ControlRot - LastSwayControlRot).GetNormalized();
		YawRate = Delta.Yaw / DeltaTime;
		PitchRate = Delta.Pitch / DeltaTime;
	}
	LastSwayControlRot = ControlRot;
	bHasLastSwayControlRot = true;

	FRotator TargetRot;
	TargetRot.Yaw = FMath::Clamp(-YawRate * SwayLookRotationScale, -SwayMaxRotation, SwayMaxRotation);
	TargetRot.Pitch = FMath::Clamp(-PitchRate * SwayLookRotationScale, -SwayMaxRotation, SwayMaxRotation);
	TargetRot.Roll = FMath::Clamp(-YawRate * SwayLookRotationScale * 0.6f, -SwayMaxRotation, SwayMaxRotation);
	FVector TargetLoc(0.f,
		FMath::Clamp(-YawRate * SwayLookLocationScale, -SwayMaxLocation, SwayMaxLocation),
		FMath::Clamp(-PitchRate * SwayLookLocationScale, -SwayMaxLocation, SwayMaxLocation));

	// 2) 走路晃动：地面移动时按速度推进相位，竖直走 2 倍频（左右各一步）
	const bool bGrounded = IsValid(CMC) && CMC->IsMovingOnGround() && !bIsSliding;
	const float SpeedRatio = bGrounded ? FMath::Clamp(GetVelocity().Size2D() / FMath::Max(1.f, BobReferenceSpeed), 0.f, 1.8f) : 0.f;
	BobWeight = FMath::FInterpTo(BobWeight, SpeedRatio, DeltaTime, 6.f);
	BobPhase = FMath::Fmod(BobPhase + DeltaTime * BobFrequency * FMath::Max(0.6f, SpeedRatio), 2.f * PI);
	TargetLoc.Y += FMath::Sin(BobPhase) * BobHorizontalAmount * BobWeight;
	TargetLoc.Z += FMath::Sin(BobPhase * 2.f) * BobVerticalAmount * BobWeight;
	TargetRot.Roll += FMath::Sin(BobPhase) * 0.4f * BobWeight;
	// 瘸腿第一人称：镜头一高一低 + 侧倾（第三人称瘸腿动画以后再说）
	if (bLegInjured && BobWeight > 0.05f)
	{
		TargetLoc.Z += FMath::Sin(BobPhase) * LimpBobAmount * BobWeight;
		TargetRot.Roll += FMath::Sin(BobPhase) * 1.5f * BobWeight;
		TargetRot.Pitch += FMath::Cos(BobPhase) * 0.8f * BobWeight;
	}

	// 3) 落地下沉：目标慢慢回 0，当前值快速追目标
	LandingDipTarget = FMath::FInterpTo(LandingDipTarget, 0.f, DeltaTime, LandingDipRecoverySpeed);
	LandingDipCurrent = FMath::FInterpTo(LandingDipCurrent, LandingDipTarget, DeltaTime, 25.f);

	TargetLoc *= AimScale;
	TargetRot *= AimScale;
	SwayLocCurrent = FMath::VInterpTo(SwayLocCurrent, TargetLoc, DeltaTime, SwaySmoothSpeed);
	SwayRotCurrent = FMath::RInterpTo(SwayRotCurrent, TargetRot, DeltaTime, SwaySmoothSpeed);
	// 下沉不走上面的平滑（自己有两段弹簧），在 ApplyVisualRecoilToMesh 里单独叠加；开镜时减到 40%
	SwayDipZ = LandingDipCurrent * FMath::Lerp(1.f, 0.4f, FMath::Clamp(AimAlignAlpha, 0.f, 1.f));
}

void AShooterCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (!IsLocallyControlled()) return;
	const float FallSpeed = FMath::Max(FMath::Abs(LastFallingVelocityZ), FMath::Abs(GetVelocity().Z));
	LastFallingVelocityZ = 0.f;
	if (FallSpeed < 200.f) return;   // 小台阶不下沉
	LandingDipTarget = -FMath::Min(FallSpeed / 1000.f * LandingDipAmount, LandingDipMax);
	// Brief landing slowdown (tactical)
	if (const UWorld* World = GetWorld())
	{
		LandingSlowUntil = World->GetTimeSeconds() + LandingSlowDuration;
		RefreshControllerMovementSpeed();
	}
}

float AShooterCharacter::GetLandingSpeedMultiplier() const
{
	const UWorld* World = GetWorld();
	if (!World || LandingSlowUntil < 0.f) return 1.f;
	return World->GetTimeSeconds() < LandingSlowUntil ? LandingSlowMultiplier : 1.f;
}

float AShooterCharacter::GetInjurySpeedMultiplier() const
{
	return bLegInjured ? LegInjurySpeedMultiplier : 1.f;
}

float AShooterCharacter::GetBandageSpeedMultiplier() const
{
	return bIsBandaging ? BandageSpeedMultiplier : 1.f;
}

float AShooterCharacter::GetArmorSpeedMultiplier() const
{
	return BodyArmor == EBodyArmorType::Heavy ? HeavyArmorMoveSpeedMultiplier : 1.f;
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

	EnsureLeanInput(ShooterInputComponent);
	EnsureFireModeInput(ShooterInputComponent);
	ShooterInputComponent->BindAction(LeanLeftAction, ETriggerEvent::Started, this, &ThisClass::Input_LeanLeft_Started);
	ShooterInputComponent->BindAction(LeanLeftAction, ETriggerEvent::Completed, this, &ThisClass::Input_LeanLeft_Completed);
	ShooterInputComponent->BindAction(LeanRightAction, ETriggerEvent::Started, this, &ThisClass::Input_LeanRight_Started);
	ShooterInputComponent->BindAction(LeanRightAction, ETriggerEvent::Completed, this, &ThisClass::Input_LeanRight_Completed);
	ShooterInputComponent->BindAction(FireWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_FireWeapon_Pressed);
	ShooterInputComponent->BindAction(FireWeaponAction, ETriggerEvent::Completed, this, &ThisClass::Input_FireWeapon_Released);
	ShooterInputComponent->BindAction(AimWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_Aim_Pressed);
	ShooterInputComponent->BindAction(AimWeaponAction, ETriggerEvent::Completed, this, &ThisClass::Input_Aim_Released);
	ShooterInputComponent->BindAction(ReloadWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_ReloadWeapon_Started);
	ShooterInputComponent->BindAction(ReloadWeaponAction, ETriggerEvent::Completed, this, &ThisClass::Input_ReloadWeapon_Completed);
	ShooterInputComponent->BindAction(EquipMainWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_EquipMainWeapon);
	ShooterInputComponent->BindAction(EquipSeconedaryWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_EquipSeconedaryWeapon);
	ShooterInputComponent->BindAction(EquipMeleeWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_EquipMeleeWeapon);
	ShooterInputComponent->BindAction(EquipPistolWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_EquipPistolWeapon);
	// HealAction: TAP = old heal/hotkey5; HOLD while injured = bandage (clears arm/leg).
	if (HealAction)
	{
		ShooterInputComponent->BindAction(HealAction, ETriggerEvent::Started, this, &ThisClass::Input_Bandage_Started);
		ShooterInputComponent->BindAction(HealAction, ETriggerEvent::Completed, this, &ThisClass::Input_Bandage_Completed);
		ShooterInputComponent->BindAction(HealAction, ETriggerEvent::Canceled, this, &ThisClass::Input_Bandage_Completed);
	}
	if (SwitchZoomAction)
	{
		ShooterInputComponent->BindAction(SwitchZoomAction, ETriggerEvent::Started, this, &ThisClass::Input_SwitchZoom);
	}
	if (SwitchFireModeAction)
	{
		ShooterInputComponent->BindAction(SwitchFireModeAction, ETriggerEvent::Started, this, &ThisClass::Input_SwitchFireMode);
	}
	// Free inspect removed: mag check is hold-R (see Input_ReloadWeapon_*).
	// VersionWeaponAction intentionally unbound.

}

void AShooterCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	ClearHitZoneMarks();
	// Swap to Police/Bandits meshes + copy Ironbelly/Manny grip sockets BEFORE guns attach.
	if (FactionLook == ECharacterLookFaction::Default)
	{
		FactionLook = ECharacterLookFaction::Police;
	}
	ApplyFactionLookMeshes();
	RefreshNameplate();
	if (IsValid(Combat))
	{
		Combat->SpawnInventory();
	}
}

bool AShooterCharacter::IsLocallyControlled() const
{
	return Super::IsLocallyControlled();
}

void AShooterCharacter::RefreshNameplate()
{
	if (!IsValid(Nameplate) || GetNetMode() == NM_DedicatedServer) return;
	Nameplate->InitWidget();
	if (UPlayerNameplateWidget* Plate = Cast<UPlayerNameplateWidget>(Nameplate->GetWidget()))
	{
		Plate->SetSubject(this);
	}
}

void AShooterCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	RefreshNameplate();

	if (IsValid(Combat))
	{
		Combat->InitializeWeaponWidget();
	}
}

FName AShooterCharacter::GetWeaponAttachPoint_Implementation(const FGameplayTag& WeaponType) const
{
	checkf(Combat, TEXT("Combat component is missing"));
	checkf(Combat->WeaponData, TEXT("No WeaponData Asset - Please Fill out BP_ShooterCharacter"));
	const FGameplayTag GripTag = WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Flash)
		? ShooterTags::Tag_WeaponType_Grenade
		: WeaponType;
	if (const FName* GripPoint = Combat->WeaponData->GripPoints.Find(GripTag))
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

void AShooterCharacter::Notify_ThrowWeapon_Implementation()
{
	if (IsValid(Combat)) Combat->Notify_ThrowWeapon();
}


bool AShooterCharacter::DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator)
{
	if (!IsValid(Stats))return false;
	if (Stats->DeathState != EDeathState::NotDead) return false;

	// 服务器：问 GameMode 这次伤害算不算（队友伤害 / 比赛已结束）
	AController* InstigatorController = nullptr;
	if (const APawn* InstigatorPawn = Cast<APawn>(DamageInstigator))
	{
		InstigatorController = InstigatorPawn->GetController();
	}
	AShooterGameModeBase* ShooterGM = GetWorld() ? GetWorld()->GetAuthGameMode<AShooterGameModeBase>() : nullptr;
	if (ShooterGM && !ShooterGM->CanDamage(InstigatorController, GetController()))
	{
		return false;
	}

	Stats->ChangeHealthByAmout(-DamageAmout, DamageInstigator);
	Multicast_NotifyBloodyFeet();
	if (ShooterGM) ShooterGM->NotifyDamage(InstigatorController, GetController());
	if (ShooterGM && Stats->DeathState != EDeathState::NotDead)
	{
		ShooterGM->OnPlayerKilled(InstigatorController, GetController());
	}
	//calculate whether or not damage lethal

	const bool bSelfDamage = DamageInstigator == this;
	const bool bHasSource = IsValid(DamageInstigator) && !bSelfDamage;

	// Directional hit react: front = within +-45 deg of facing, etc.
	EHitReactDirection Direction = EHitReactDirection::Front;
	if (bHasSource)
	{
		Direction = ComputeHitReactDirection(DamageInstigator->GetActorLocation());
	}
	const TArray<TObjectPtr<UAnimMontage>>& Pool = GetHitReactPool(Direction);
	if (Pool.Num() > 0)
	{
		Multicast_HitReact(Direction, FMath::RandRange(0, Pool.Num() - 1));
	}

	if (bHasSource)
	{
		// Victim: damage direction ring
		if (IsPlayerControlled())
		{
			Client_ShowDamageIndicator(DamageInstigator->GetActorLocation());
		}
		// Shooter: hit confirm marks（致死 = 红色击杀 X + 击杀音；爆头单独标记）
		if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(DamageInstigator))
		{
			if (Shooter->IsPlayerControlled())
			{
				const bool bHeadshot = Shooter->bPendingHeadshotHit;
				if (Stats->DeathState != EDeathState::NotDead)
				{
					Shooter->Client_ConfirmKill(bHeadshot);
				}
				else
				{
					Shooter->Client_ConfirmHit(bHeadshot);
				}
			}
		}
	}

	return false;
}

EHitReactDirection AShooterCharacter::ComputeHitReactDirection(const FVector& SourceLocation) const
{
	FVector ToSource = SourceLocation - GetActorLocation();
	ToSource.Z = 0.f;
	if (!ToSource.Normalize()) return EHitReactDirection::Front;

	FVector Forward = GetActorForwardVector();
	Forward.Z = 0.f;
	Forward.Normalize();

	const float Dot = FVector::DotProduct(Forward, ToSource);
	const float CrossZ = Forward.X * ToSource.Y - Forward.Y * ToSource.X; // > 0 = source on the right
	const float Cos45 = 0.70710678f;
	if (Dot >= Cos45) return EHitReactDirection::Front;
	if (Dot <= -Cos45) return EHitReactDirection::Back;
	return CrossZ > 0.f ? EHitReactDirection::Right : EHitReactDirection::Left;
}

const TArray<TObjectPtr<UAnimMontage>>& AShooterCharacter::GetHitReactPool(EHitReactDirection Direction) const
{
	const TArray<TObjectPtr<UAnimMontage>>* Pool = &HitReacts;
	switch (Direction)
	{
	case EHitReactDirection::Front: Pool = &HitReactsFront; break;
	case EHitReactDirection::Back:  Pool = &HitReactsBack;  break;
	case EHitReactDirection::Left:  Pool = &HitReactsLeft;  break;
	case EHitReactDirection::Right: Pool = &HitReactsRight; break;
	default: break;
	}
	return Pool->Num() > 0 ? *Pool : HitReacts;
}

void AShooterCharacter::Client_ShowDamageIndicator_Implementation(FVector_NetQuantize InstigatorLocation)
{
	(void)InstigatorLocation;
	AddHitBlood();
}

void AShooterCharacter::Client_ConfirmHit_Implementation(bool bHeadshot)
{
	OnHitConfirmed.Broadcast();
	OnHitConfirmedDetailed.Broadcast(bHeadshot, false);
}

void AShooterCharacter::Client_ConfirmKill_Implementation(bool bHeadshot)
{
	OnHitConfirmed.Broadcast();
	OnHitConfirmedDetailed.Broadcast(bHeadshot, true);
}

void AShooterCharacter::Multicast_HitReact_Implementation(EHitReactDirection Direction, int32 MontageIndex)
{
	if (GetNetMode() != NM_DedicatedServer && !IsLocallyControlled())
	{
		const TArray<TObjectPtr<UAnimMontage>>& Pool = GetHitReactPool(Direction);
		if (Pool.IsValidIndex(MontageIndex) && Pool[MontageIndex] && GetMesh()->GetAnimInstance())
		{
			GetMesh()->GetAnimInstance()->Montage_Play(Pool[MontageIndex]);
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

void AShooterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShooterCharacter, LeanTarget, COND_SkipOwner);
	DOREPLIFETIME(AShooterCharacter, bIsProne);
	DOREPLIFETIME(AShooterCharacter, bIsSliding);
	DOREPLIFETIME(AShooterCharacter, bArmInjured);
	DOREPLIFETIME(AShooterCharacter, bLegInjured);
	DOREPLIFETIME(AShooterCharacter, BodyArmor);
	DOREPLIFETIME(AShooterCharacter, FactionLook);
	DOREPLIFETIME(AShooterCharacter, bHasRadar);
	DOREPLIFETIME(AShooterCharacter, bHasFlashGoggles);
}

// ===== 趴下 / 趴行 (Prone) =====
void AShooterCharacter::EnsureProneInput()
{
	if (ProneAction) return;

	// 没配资产：运行时建 IA + 小 IMC（Z 键），同 Lean 的做法
	if (!ProneRuntimeIMC)
	{
		ProneRuntimeIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Prone_Runtime"));
	}
	ProneAction = NewObject<UInputAction>(this, TEXT("IA_Prone_Runtime"));
	ProneAction->ValueType = EInputActionValueType::Boolean;
	ProneRuntimeIMC->MapKey(ProneAction, EKeys::Z);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (Subsystem->HasMappingContext(ProneRuntimeIMC))
				{
					Subsystem->RemoveMappingContext(ProneRuntimeIMC);
				}
				Subsystem->AddMappingContext(ProneRuntimeIMC, 1);
			}
		}
	}
}

void AShooterCharacter::Input_Prone()
{
	if (!IsLocallyControlled()) return;
	RequestSetProne(!bIsProne, false);
}

bool AShooterCharacter::CanEnterProne() const
{
	const UCharacterMovementComponent* CMC = GetCharacterMovement();
	return IsValid(CMC) && CMC->IsMovingOnGround() && !bIsHealing;
}

bool AShooterCharacter::CanExitProne(bool bToCrouch) const
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!IsValid(Capsule) || !IsValid(CMC) || !GetWorld()) return false;

	const ACharacter* DefaultChar = GetClass()->GetDefaultObject<ACharacter>();
	const float StandHalf = DefaultChar && DefaultChar->GetCapsuleComponent()
		? DefaultChar->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()
		: 88.f;
	const float CrouchHalf = DefaultCrouchedHalfHeight > 0.f ? DefaultCrouchedHalfHeight : CMC->GetCrouchedHalfHeight();
	const float Scale = Capsule->GetShapeScale();
	const float TargetHalf = (bToCrouch ? CrouchHalf : StandHalf) * Scale;
	const float CurrentHalf = Capsule->GetScaledCapsuleHalfHeight();
	if (TargetHalf <= CurrentHalf) return true;

	// Keep the feet where they are: the taller capsule's centre rises by the height difference.
	const FVector TestLocation = GetActorLocation() + FVector(0.f, 0.f, TargetHalf - CurrentHalf + 1.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ProneStandUp), false, this);
	FCollisionResponseParams ResponseParams;
	CMC->InitCollisionParams(Params, ResponseParams);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() - 1.f, TargetHalf);
	return !GetWorld()->OverlapBlockingTestByChannel(TestLocation, FQuat::Identity, Capsule->GetCollisionObjectType(), Shape, Params, ResponseParams);
}

void AShooterCharacter::RequestSetProne(bool bNewProne, bool bExitToCrouch)
{
	(void)bNewProne;
	(void)bExitToCrouch;
}

void AShooterCharacter::Server_SetProne_Implementation(bool bNewProne, bool bExitToCrouch)
{
	if (bNewProne == bIsProne) return;
	const bool bAllowed = bNewProne ? CanEnterProne() : CanExitProne(bExitToCrouch);
	if (!bAllowed)
	{
		Client_ProneRejected(bIsProne);
		return;
	}
	ApplyProneState(bNewProne, bExitToCrouch);
}

void AShooterCharacter::Client_ProneRejected_Implementation(bool bServerProne)
{
	if (bServerProne != bIsProne)
	{
		ApplyProneState(bServerProne, false);
	}
}

void AShooterCharacter::OnRep_IsProne(bool bOldProne)
{
	// The owner already applied its prediction; only react if the server value really differs.
	const bool bNew = bIsProne;
	bIsProne = bOldProne;
	if (bNew != bOldProne)
	{
		ApplyProneState(bNew, false);
	}
	else
	{
		bIsProne = bNew;
	}
}

void AShooterCharacter::ApplyProneState(bool bNewProne, bool bExitToCrouch)
{
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!IsValid(CMC)) return;

	if (DefaultCrouchedHalfHeight <= 0.f)
	{
		DefaultCrouchedHalfHeight = CMC->GetCrouchedHalfHeight();
	}

	const bool bSimProxy = GetLocalRole() == ROLE_SimulatedProxy;
	const bool bWasProne = bIsProne;
	bIsProne = bNewProne;

	if (bNewProne)
	{
		// Already crouched (normal crouch height): release it so the next crouch uses the prone height.
		if (bIsCrouched && !bWasProne)
		{
			CMC->UnCrouch(bSimProxy);
		}
		CMC->SetCrouchedHalfHeight(ProneCapsuleHalfHeight);
		CMC->bWantsToCrouch = true;
		if (bSimProxy && bIsCrouched)
		{
			CMC->Crouch(true);
		}
	}
	else
	{
		CMC->SetCrouchedHalfHeight(DefaultCrouchedHalfHeight);
		if (bIsCrouched)
		{
			CMC->UnCrouch(bSimProxy);
		}
		CMC->bWantsToCrouch = bExitToCrouch;
		if (bSimProxy && bIsCrouched)
		{
			// Replicated bIsCrouched still true (prone -> crouch): re-crouch at normal height.
			CMC->Crouch(true);
		}
	}

	if (bNewProne)
	{
		LeanInputDir = 0;
		bLeanLeftHeld = bLeanRightHeld = false;
	}

	ApplyProneWeaponVisibility();
	RefreshControllerMovementSpeed();   // prone cancels sprint and switches to crawl speed
	RefreshLocomotionGait();
}

void AShooterCharacter::ApplyProneWeaponVisibility()
{
	if (!bHideWeapon3PWhileProne || !IsValid(Combat) || !IsValid(Combat->CurrentWeapon)) return;
	if (USkeletalMeshComponent* WeaponMesh3P = Combat->CurrentWeapon->GetMesh3P())
	{
		WeaponMesh3P->SetHiddenInGame(bIsProne);
	}
}

void AShooterCharacter::UpdateProne(float DeltaTime)
{
	bProneCrawling = bIsProne && GetVelocity().Size2D() > ProneCrawlSpeedThreshold;

	const float Target = bIsProne ? 1.f : 0.f;
	ProneAlpha = ProneCameraInterpSpeed > 0.f ? FMath::FInterpTo(ProneAlpha, Target, DeltaTime, ProneCameraInterpSpeed) : Target;
	if (FMath::IsNearlyEqual(ProneAlpha, Target, 0.001f)) ProneAlpha = Target;

	if (IsLocallyControlled() && bSpringArmBaseCaptured && IsValid(SpringArm))
	{
		CrouchCameraOffsetZ = CrouchCameraInterpSpeed > 0.f ? FMath::FInterpTo(CrouchCameraOffsetZ, 0.f, DeltaTime, CrouchCameraInterpSpeed) : 0.f;
		if (FMath::Abs(CrouchCameraOffsetZ) < 0.01f) CrouchCameraOffsetZ = 0.f;
		// 滑铲镜头下沉会把挂在相机上的 Mesh1P 一起带下来，开镜时手臂更容易挡视野；开镜进度起来淡出下沉
		const float SlideDip = SlideCameraZOffset * SlideAlpha * (1.f - FMath::Clamp(AimAlignAlpha, 0.f, 1.f));
		const FVector Desired = SpringArmBaseLocation + FVector(0.f, 0.f, ProneCameraZOffset * ProneAlpha + SlideDip + CrouchCameraOffsetZ);
		if (!SpringArm->GetRelativeLocation().Equals(Desired, 0.01f))
		{
			SpringArm->SetRelativeLocation(Desired);
		}
	}
}

// ===== 滑铲 (Slide) =====
bool AShooterCharacter::CanStartSlide(float SpeedScale) const
{
	if (!bEnableSlide) return false; // tactical mode: slide fully gated
	const UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!IsValid(CMC) || bIsSliding || bIsProne || bIsHealing) return false;
	if (!CMC->IsMovingOnGround()) return false;
	return GetVelocity().Size2D() > SlideMinSpeed * SpeedScale;
}

bool AShooterCharacter::TryStartSlide()
{
	return false;
}

void AShooterCharacter::CancelSlideToStand()
{
	if (!bIsSliding) return;
	bSlideEndCrouch = false;
	EndSlideLocal();
}

void AShooterCharacter::EndSlideLocal()
{
	ApplySlideState(false);
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		// Owner drives crouch via its move flags; if blocked overhead CMC stays crouched until clear.
		CMC->bWantsToCrouch = bSlideEndCrouch;
	}
	if (!HasAuthority())
	{
		Server_SetSliding(false);
	}
}

void AShooterCharacter::Server_SetSliding_Implementation(bool bNewSliding)
{
	if (bNewSliding == bIsSliding) return;
	if (bNewSliding && !CanStartSlide(0.75f))   // small tolerance for client/server speed drift
	{
		Client_SlideRejected();
		return;
	}
	ApplySlideState(bNewSliding);
}

void AShooterCharacter::Client_SlideRejected_Implementation()
{
	if (bIsSliding)
	{
		ApplySlideState(false);   // stays crouched (bWantsToCrouch already true)
	}
}

void AShooterCharacter::OnRep_IsSliding(bool bOldSliding)
{
	const bool bNew = bIsSliding;
	bIsSliding = bOldSliding;
	if (bNew != bOldSliding)
	{
		ApplySlideState(bNew);
	}
	else
	{
		bIsSliding = bNew;
	}
}


float AShooterCharacter::GetSlideAdsSettleRemaining() const
{
	if (!bSlideAdsDelayEnabled || !bIsSliding) return 0.f;
	const float Delay = (SlideToAdsDelay > KINDA_SMALL_NUMBER)
		? SlideToAdsDelay
		: (0.5f * FMath::Max(0.1f, SlideDuration));
	return FMath::Max(0.f, Delay - SlideElapsed);
}

void AShooterCharacter::ApplySlideState(bool bNewSliding)
{
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!IsValid(CMC)) return;
	const bool bWas = bIsSliding;
	bIsSliding = bNewSliding;
	if (bWas == bNewSliding) return;

	const bool bSimulates = IsLocallyControlled() || HasAuthority();
	if (bNewSliding)
	{
		SlideElapsed = 0.f;
		if (!bSlideSavedMovement)
		{
			SavedGroundFriction = CMC->GroundFriction;
			SavedBrakingDecelWalking = CMC->BrakingDecelerationWalking;
			bSlideSavedMovement = true;
		}
		// No input acceleration while sliding (controller skips move input) -> CMC brakes linearly by this.
		FrictionRecoverElapsed = -1.f;
		CarriedSpeed = 0.f;
		CMC->GroundFriction = 0.f;
		// 滑铲无移动输入时 CMC 走刹车分支；开了 bUseSeparateBrakingFriction 会用 BrakingFriction 而不是 GroundFriction。
		// 若不置 0，BrakingFriction=6 会一帧刹停，速度立刻 < SlideEndSpeed，滑铲当场结束。
		CMC->BrakingFriction = 0.f;
		CMC->BrakingDecelerationWalking = SlideDecelEarly;
		CMC->bWantsToCrouch = true;   // low capsule = normal crouch height
		if (bSimulates)
		{
			FVector Vel2D = CMC->Velocity;
			Vel2D.Z = 0.f;
			const FVector Dir = Vel2D.GetSafeNormal();
			CMC->Velocity += Dir * SlideSpeedBoost;
		}
		LeanInputDir = 0;
		bLeanLeftHeld = bLeanRightHeld = false;
	}
	else if (bSlideSavedMovement)
	{
		// 摩擦力不瞬间恢复：UpdateMomentumCarry 里 SlideFrictionRecoverTime 秒内插值回去
		CMC->BrakingDecelerationWalking = SavedBrakingDecelWalking;
		FrictionRecoverElapsed = 0.f;
		// 保留滑铲出口速度（站起/滑铲跳不掉速），落地后再衰减
		CarriedSpeed = CMC->Velocity.Size2D();
	}

	PlaySlideAnim(bNewSliding);
	RefreshControllerMovementSpeed();   // slide cancels sprint, raises the crouched speed cap
	RefreshLocomotionGait();
}

void AShooterCharacter::PlaySlideAnim(bool bPlay)
{
	if (GetNetMode() == NM_DedicatedServer) return;

	UAnimInstance* Anim3P = IsValid(GetMesh()) ? GetMesh()->GetAnimInstance() : nullptr;
	UAnimInstance* Anim1P = IsValid(Mesh1P) ? Mesh1P->GetAnimInstance() : nullptr;

	if (bPlay)
	{
		if (Anim3P)
		{
			if (SlideMontage)
			{
				Anim3P->Montage_Play(SlideMontage);
				ActiveSlideMontage = SlideMontage;
			}
			else if (UAnimSequenceBase* Seq = SlideAnim.LoadSynchronous())
			{
				const float Rate = FMath::Max(0.1f, Seq->GetPlayLength() / SlideDuration);
				ActiveSlideMontage = Anim3P->PlaySlotAnimationAsDynamicMontage(Seq, SlideSlotName, 0.15f, 0.25f, Rate, 1);
			}
		}
		if (Anim1P && SlideMontage1P && IsLocallyControlled())
		{
			Anim1P->Montage_Play(SlideMontage1P);
		}
	}
	else
	{
		if (Anim3P && IsValid(ActiveSlideMontage))
		{
			Anim3P->Montage_Stop(0.25f, ActiveSlideMontage);
		}
		ActiveSlideMontage = nullptr;
		if (Anim1P && SlideMontage1P)
		{
			Anim1P->Montage_Stop(0.2f, SlideMontage1P);
		}
	}
}

void AShooterCharacter::UpdateMomentumCarry(float DeltaTime)
{
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!IsValid(CMC)) return;

	if (FrictionRecoverElapsed >= 0.f && bSlideSavedMovement && !bIsSliding)
	{
		FrictionRecoverElapsed += DeltaTime;
		const float A = SlideFrictionRecoverTime > 0.f ? FMath::Clamp(FrictionRecoverElapsed / SlideFrictionRecoverTime, 0.f, 1.f) : 1.f;
		CMC->GroundFriction = FMath::Lerp(0.f, SavedGroundFriction, A);
		// 与 GroundFriction 同步恢复（COD 默认 BrakingFriction=6）
		CMC->BrakingFriction = FMath::Lerp(0.f, 6.f, A);
		if (A >= 1.f)
		{
			CMC->GroundFriction = SavedGroundFriction;
			CMC->BrakingFriction = 6.f;
			bSlideSavedMovement = false;
			FrictionRecoverElapsed = -1.f;
		}
	}

	if (CarriedSpeed <= 0.f) return;
	if (bIsSliding) { CarriedSpeed = 0.f; return; }
	// 空中保持；落地后按 MomentumCarryDecayTime 线性衰减到当前速度上限以下
	if (!CMC->IsFalling())
	{
		const float Rate = FMath::Max(100.f, CarriedSpeed / FMath::Max(0.05f, MomentumCarryDecayTime));
		CarriedSpeed = FMath::Max(0.f, CarriedSpeed - Rate * DeltaTime);
	}
	RefreshControllerMovementSpeed();
}

void AShooterCharacter::OnStartCrouchCameraFix(float ScaledHalfHeightAdjust)
{
	// 胶囊体变矮整体下移 → 镜头会瞬间掉一截。先反向补回，再在 UpdateProne 里平滑回 0
	if (IsLocallyControlled()) CrouchCameraOffsetZ += ScaledHalfHeightAdjust;
}

void AShooterCharacter::UpdateSlide(float DeltaTime)
{
	const float Target = bIsSliding ? 1.f : 0.f;
	SlideAlpha = FMath::FInterpTo(SlideAlpha, Target, DeltaTime, 10.f);
	if (FMath::IsNearlyEqual(SlideAlpha, Target, 0.001f)) SlideAlpha = Target;

	if (!bIsSliding) return;
	SlideElapsed += DeltaTime;

	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (IsValid(CMC) && (IsLocallyControlled() || HasAuthority()))
	{
		CMC->BrakingDecelerationWalking = SlideElapsed < SlideDecelSwitchTime ? SlideDecelEarly : SlideDecelLate;
		// 视角转向：速度方向向视角 Yaw 靠拢（限角速度，且只在 100° 以内转）
		if (SlideSteerRate > 0.f && CMC->IsMovingOnGround())
		{
			FVector V = CMC->Velocity; const float Z = V.Z; V.Z = 0.f;
			const float Speed = V.Size();
			if (Speed > 1.f)
			{
				const float VelYaw = V.Rotation().Yaw;
				const float Delta = FMath::FindDeltaAngleDegrees(VelYaw, GetControlRotation().Yaw);
				if (FMath::Abs(Delta) < 100.f)
				{
					const float Step = FMath::Clamp(Delta, -SlideSteerRate * DeltaTime, SlideSteerRate * DeltaTime);
					const FVector NewDir = FRotator(0.f, VelYaw + Step, 0.f).Vector();
					CMC->Velocity = NewDir * Speed + FVector(0.f, 0.f, Z);
				}
			}
		}
	}
	const bool bDone = SlideElapsed >= SlideDuration
		|| GetVelocity().Size2D() < SlideEndSpeed
		|| (IsValid(CMC) && !CMC->IsMovingOnGround());

	if (IsLocallyControlled())
	{
		if (bDone) EndSlideLocal();
	}
	else if (HasAuthority() && SlideElapsed >= SlideDuration + 0.5f)
	{
		ApplySlideState(false);   // safety: owner's end RPC never arrived
	}
}

// ===== 左右探头 =====
void AShooterCharacter::EnsureFireModeInput(UEnhancedInputComponent* InputComp)
{
	(void)InputComp;
	if (!IsLocallyControlled()) return;
	if (!SwitchFireModeAction)
	{
		SwitchFireModeAction = NewObject<UInputAction>(this, TEXT("IA_SwitchFireMode_Runtime"));
		SwitchFireModeAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!FireModeRuntimeIMC)
	{
		FireModeRuntimeIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_FireMode_Runtime"));
		FireModeRuntimeIMC->MapKey(SwitchFireModeAction, EKeys::X);
	}
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (!Subsystem->HasMappingContext(FireModeRuntimeIMC))
				{
					Subsystem->AddMappingContext(FireModeRuntimeIMC, 2);
				}
			}
		}
	}
}

void AShooterCharacter::UpdateAltPeek()
{
	const bool bWasPeek = bAltPeekCrouch;
	bAltPeekCrouch = false;
	AltPeekDir = 0;
	if (!IsLocallyControlled() || bIsProne || bIsSliding) 
	{
		// fall through to release crouch if a peek was active
	}
	else if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetController()))
	{
		const bool bAlt = PC->IsInputKeyDown(EKeys::LeftAlt) || PC->IsInputKeyDown(EKeys::RightAlt);
		if (PC->bPawnAlive && !PC->IsControlMovementLocked())
		{
			const bool bA = PC->IsInputKeyDown(EKeys::A);
			const bool bD = PC->IsInputKeyDown(EKeys::D);
			if (bAlt && (bA || bD))
			{
				bAltPeekCrouch = true;
				if (bA && !bD) AltPeekDir = -1;
				else if (bD && !bA) AltPeekDir = 1;
				PC->StopSprinting();
			}
		}
	}

	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!IsValid(CMC) || !IsLocallyControlled()) return;
	if (bAltPeekCrouch)
	{
		CMC->bWantsToCrouch = true;
		ConsumeMovementInputVector();
		FVector Vel = CMC->Velocity;
		Vel.X = 0.f;
		Vel.Y = 0.f;
		CMC->Velocity = Vel;
	}
	else if (bWasPeek && !bIsProne && !bIsSliding)
	{
		const AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetController());
		if (!PC || !PC->IsCrouchHeld())
		{
			CMC->bWantsToCrouch = false;
		}
	}
}

void AShooterCharacter::EnsureLeanInput(UEnhancedInputComponent* InputComp)
{
	const bool bNeedLeft = !LeanLeftAction;
	const bool bNeedRight = !LeanRightAction;
	if (!bNeedLeft && !bNeedRight) return;

	// 没配资产：运行时建 IA + 一个小 IMC（Q 左 / E 右）
	if (!LeanRuntimeIMC)
	{
		LeanRuntimeIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Lean_Runtime"));
	}
	if (bNeedLeft)
	{
		LeanLeftAction = NewObject<UInputAction>(this, TEXT("IA_LeanLeft_Runtime"));
		LeanLeftAction->ValueType = EInputActionValueType::Boolean;
		LeanRuntimeIMC->MapKey(LeanLeftAction, EKeys::Q);
	}
	if (bNeedRight)
	{
		LeanRightAction = NewObject<UInputAction>(this, TEXT("IA_LeanRight_Runtime"));
		LeanRightAction->ValueType = EInputActionValueType::Boolean;
		LeanRuntimeIMC->MapKey(LeanRightAction, EKeys::E);
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (Subsystem->HasMappingContext(LeanRuntimeIMC))
				{
					Subsystem->RemoveMappingContext(LeanRuntimeIMC);
				}
				Subsystem->AddMappingContext(LeanRuntimeIMC, 1);
			}
		}
	}
}

void AShooterCharacter::Input_LeanLeft_Started()    { HandleLeanInput(-1, true); }
void AShooterCharacter::Input_LeanLeft_Completed()  { HandleLeanInput(-1, false); }
void AShooterCharacter::Input_LeanRight_Started()   { HandleLeanInput(1, true); }
void AShooterCharacter::Input_LeanRight_Completed() { HandleLeanInput(1, false); }

void AShooterCharacter::HandleLeanInput(int32 Dir, bool bPressed)
{
	if (bLeanToggleMode)
	{
		if (bPressed)
		{
			LeanInputDir = (LeanInputDir == Dir) ? 0 : Dir;
		}
		return;
	}

	if (Dir < 0) bLeanLeftHeld = bPressed; else bLeanRightHeld = bPressed;
	if (bPressed)
	{
		LeanInputDir = Dir;   // 后按的优先
	}
	else
	{
		LeanInputDir = bLeanLeftHeld ? -1 : (bLeanRightHeld ? 1 : 0);
	}
}

bool AShooterCharacter::IsLeanBlocked() const
{
	if (const AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetController()))
	{
		if (PC->IsSprinting() && GetVelocity().Size2D() > 10.f) return true;
	}
	return bIsProne || bIsSliding;   // no leaning while lying down / sliding
}

void AShooterCharacter::Server_SetLeanTarget_Implementation(float NewTarget)
{
	LeanTarget = FMath::Clamp(NewTarget, -1.f, 1.f);
}

void AShooterCharacter::UpdateLean(float DeltaTime)
{
	if (IsLocallyControlled())
	{
		float Desired = 0.f;
		const bool bWideStep = bAltPeekCrouch && AltPeekDir != 0 && !IsLeanBlocked();
		if (bWideStep)
		{
			Desired = static_cast<float>(AltPeekDir);
			LeanDistanceScale = FMath::Max(1.f, AltPeekLeanScale);
		}
		else if (!IsLeanBlocked())
		{
			Desired = static_cast<float>(LeanInputDir);
			if (LeanInputDir != 0)
			{
				LeanDistanceScale = 1.f;
			}
		}
		if (!bWideStep && LeanInputDir == 0 && FMath::Abs(LeanAlpha) < 0.02f)
		{
			LeanDistanceScale = 1.f;
		}
		if (!FMath::IsNearlyEqual(Desired, LeanTarget))
		{
			LeanTarget = Desired;
			if (!HasAuthority())
			{
				Server_SetLeanTarget(Desired);
			}
		}
	}

	LeanAlpha = FMath::FInterpTo(LeanAlpha, LeanTarget, DeltaTime, LeanInterpSpeed);
	if (FMath::Abs(LeanAlpha) < KINDA_SMALL_NUMBER && LeanTarget == 0.f) LeanAlpha = 0.f;

	if (!IsLocallyControlled() || !IsValid(FirstPersonCamera) || !bCameraBaseCaptured) return;
	// 死亡视角时相机已脱离并改为绝对坐标，这里再写相对位置会把相机拉到世界原点
	if (FirstPersonCamera->GetAttachParent() == nullptr || FirstPersonCamera->IsUsingAbsoluteLocation()) return;

	// 防穿墙：从相机原位往探头位置扫一个球，撞墙就缩短横移
	float OffsetScale = 1.f;
	if (FMath::Abs(LeanAlpha) > KINDA_SMALL_NUMBER)
	{
		const USceneComponent* Parent = FirstPersonCamera->GetAttachParent();
		const FTransform ParentXf = IsValid(Parent) ? FirstPersonCamera->GetAttachParent()->GetSocketTransform(FirstPersonCamera->GetAttachSocketName()) : GetActorTransform();
		const FVector Start = ParentXf.TransformPosition(CameraBaseLocation);
		const FVector End = ParentXf.TransformPosition(CameraBaseLocation + FVector(0.f, LeanAlpha * LeanCameraOffset * LeanDistanceScale, 0.f));
		FCollisionQueryParams Params(SCENE_QUERY_STAT(LeanProbe), false, this);
		if (IsValid(Combat) && IsValid(Combat->CurrentWeapon)) Params.AddIgnoredActor(Combat->CurrentWeapon);
		FHitResult Hit;
		if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(LeanWallProbeRadius), Params))
		{
			OffsetScale = Hit.Time;
		}
	}

	FirstPersonCamera->SetRelativeLocation(CameraBaseLocation + FVector(0.f, LeanAlpha * LeanCameraOffset * LeanDistanceScale * OffsetScale, 0.f));
	// 侧倾手感保留：开镜时收到 SlideCameraRollAds，不整段清零
	const float SlideRoll = SlideAlpha * FMath::Lerp(SlideCameraRoll, SlideCameraRollAds, FMath::Clamp(AimAlignAlpha, 0.f, 1.f));
	FirstPersonCamera->SetRelativeRotation(CameraBaseRotation + FRotator(0.f, 0.f, LeanAlpha * LeanRollDegrees + SlideRoll));
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
		Weapon->bRememberCanted = false;
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
	Weapon->bRememberCanted = true;
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
	// 第 13 步：进侧瞄自动开激光（服务器设成开，不是切换）。退出侧瞄不关，保持玩家当前状态
	if (bCantedAim && IsValid(Combat))
	{
		Combat->Initiate_SetLaserOn(true);
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
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsInputKeyDown(EKeys::X)) return;
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (Now - LastFireModeSwitchTime < 0.05) return;
	LastFireModeSwitchTime = Now;
	if (IsValid(Combat)) Combat->Initiate_SwitchFireMode();
}

void AShooterCharacter::Input_ReloadWeapon()
{
	Input_ReloadWeapon_Started();
}

void AShooterCharacter::ClearReloadGestureTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadHoldTimer);
		World->GetTimerManager().ClearTimer(ReloadDoubleClickTimer);
	}
}

void AShooterCharacter::Input_ReloadWeapon_Started()
{
	// Second press inside the double-click window -> mark double (tactical on release).
	if (bReloadAwaitingDouble)
	{
		bReloadAwaitingDouble = false;
		bReloadDoubleClick = true;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ReloadDoubleClickTimer);
		}
	}
	else
	{
		bReloadDoubleClick = false;
	}

	bReloadKeyHeld = true;
	bReloadHoldTriggered = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadHoldTimer);
		World->GetTimerManager().SetTimer(
			ReloadHoldTimer, this, &AShooterCharacter::OnReloadHoldThreshold,
			FMath::Max(0.05f, ReloadHoldThreshold), false);
	}
}

void AShooterCharacter::OnReloadHoldThreshold()
{
	if (!bReloadKeyHeld || bReloadHoldTriggered) return;
	bReloadHoldTriggered = true;
	bReloadAwaitingDouble = false;
	bReloadDoubleClick = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadDoubleClickTimer);
	}
	// HOLD R = mag check (no free inspect key)
	if (IsValid(Combat)) Combat->Initiate_VersionWeapon();
}

void AShooterCharacter::OnReloadSingleClickTimeout()
{
	if (bReloadKeyHeld || bReloadHoldTriggered || bReloadDoubleClick) return;
	bReloadAwaitingDouble = false;
	// SINGLE R = reload (keep partial mag / Empty montage)
	if (IsValid(Combat)) Combat->Initiate_ReloadWeapon(false);
}

void AShooterCharacter::Input_ReloadWeapon_Completed()
{
	bReloadKeyHeld = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadHoldTimer);
	}
	if (bReloadHoldTriggered)
	{
		bReloadHoldTriggered = false;
		return; // hold already fired mag check
	}
	if (bReloadDoubleClick)
	{
		bReloadDoubleClick = false;
		bReloadAwaitingDouble = false;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ReloadDoubleClickTimer);
		}
		// DOUBLE R = tactical reload (drop mag / Tactical montage)
		if (IsValid(Combat)) Combat->Initiate_ReloadWeapon(true);
		return;
	}
	// Wait briefly for a possible second tap before committing single reload.
	bReloadAwaitingDouble = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadDoubleClickTimer);
		World->GetTimerManager().SetTimer(
			ReloadDoubleClickTimer, this, &AShooterCharacter::OnReloadSingleClickTimeout,
			FMath::Max(0.05f, ReloadDoubleClickWindow), false);
	}
	else if (IsValid(Combat))
	{
		Combat->Initiate_ReloadWeapon(false);
	}
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

void AShooterCharacter::Input_Aim_Released()
{
	if (IsValid(Combat)) Combat->Initiate_Aim_Released();
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
	if (IsValid(Combat) && !Combat->AllowsHeal()) return;
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
		if (Status == EWeaponStatus::Reloading || Status == EWeaponStatus::Cycling)
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
	if (IsValid(Combat)) Combat->Initiate_Hotkey(1);
}

void AShooterCharacter::Input_EquipSeconedaryWeapon()
{
	if (IsValid(Combat)) Combat->Initiate_Hotkey(2);
}

void AShooterCharacter::Input_EquipMeleeWeapon()
{
	if (IsValid(Combat)) Combat->Initiate_Hotkey(3);
}

void AShooterCharacter::Input_EquipPistolWeapon()
{
	if (IsValid(Combat)) Combat->Initiate_Hotkey(4);
}

void AShooterCharacter::Input_Heal()
{
	if (IsValid(Combat) && Combat->Initiate_Hotkey(5))
	{
		return;
	}
	HealPlayer(HealAmout);
}

void AShooterCharacter::Input_VersionWeapon()
{
	// Free inspect disabled. Mag check is hold-R.
}

void AShooterCharacter::OnDeathStarted()
{
	ClearHitZoneMarks();
	if (GetNetMode() != NM_DedicatedServer)
	{
		DeathEffect();
		if (IsLocallyControlled())
		{
			StartDeathView();
		}
		
		if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetController()); IsValid(PC))
		{
			DisableInput(PC);
			if (PC->IsLocalController())
			{
				PC->bPawnAlive = false;
			}
		}
	}
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(FPSTraceChannels::ECC_Weapon, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(FPSTraceChannels::ECC_Weapon, ECR_Ignore);
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
	OnStartCrouchCameraFix(ScaledHalfHeightAdjust);
	RefreshControllerMovementSpeed();   // 下蹲会打断冲刺
	RefreshLocomotionGait();
}

void AShooterCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	OnStartCrouchCameraFix(-ScaledHalfHeightAdjust);
	RefreshControllerMovementSpeed();
	RefreshLocomotionGait();
}

bool AShooterCharacter::CanJumpInternal_Implementation() const
{
	// TacticalTrim: CanJump denied — jump disabled.
	return false;
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
	if (bIsProne)
	{
		RefSpeed = ProneAnimReferenceSpeed; // Prone crawl BlendSpace
	}
	else if (bIsCrouched)
	{
		RefSpeed = CrouchAnimReferenceSpeed; // Strafe_Crouching
	}
	else if (bNewWalkGait)
	{
		RefSpeed = WalkAnimReferenceSpeed; // Strafe_Walking
	}
	else
	{
		// Still on Jog Strafe_Standing (common when Strafe_Walking empty) - slow play rate = fewer steps
		RefSpeed = JogAnimReferenceSpeed;
	}
	RefSpeed = FMath::Max(RefSpeed, 1.f);

	if (GroundSpeed > 1.f)
	{
		// TacticalTrim: clamp play rate max to 1.0 (allow slow-down only).
		LocomotionPlayRate = FMath::Clamp(GroundSpeed / RefSpeed, MinLocomotionPlayRate, FMath::Min(MaxLocomotionPlayRate, 1.0f));
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

void AShooterCharacter::StartDeathView()
{
	// 第一人称手臂和枪藏起来，自己能看到自己的身体
	if (IsValid(Mesh1P))
	{
		Mesh1P->SetVisibility(false, true);
	}
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetOwnerNoSee(false);
	}
	if (IsValid(Combat))
	{
		for (AWeapon* Weapon : Combat->GetInventory())
		{
			if (!IsValid(Weapon)) continue;
			if (USkeletalMeshComponent* W1P = Weapon->GetMesh1P()) W1P->SetVisibility(false, true);
			if (USkeletalMeshComponent* W3P = Weapon->GetMesh3P()) W3P->SetOwnerNoSee(false);
		}
	}

	// 镜头脱离角色（不跟布娃娃，避免抖动），平滑落到地面附近并仰望天空，然后固定
	if (IsValid(FirstPersonCamera))
	{
		FirstPersonCamera->bUsePawnControlRotation = false;
		FirstPersonCamera->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		FirstPersonCamera->SetUsingAbsoluteLocation(true);
		FirstPersonCamera->SetUsingAbsoluteRotation(true);

		const FVector StartLoc = FirstPersonCamera->GetComponentLocation();
		const FRotator StartRot = FirstPersonCamera->GetComponentRotation();
		FVector EndLoc = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DeathCam), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, StartLoc, StartLoc - FVector(0.f, 0.f, 1000.f), ECC_Visibility, Params))
		{
			EndLoc = Hit.ImpactPoint;
		}
		EndLoc.Z += DeathCamGroundHeight;
		const FRotator EndRot(89.9f, StartRot.Yaw, DeathCamRoll);

		const float MoveTime = DeathCamMoveTime;
		const double StartTime = GetWorld()->GetTimeSeconds();
		TSharedRef<FTimerHandle> MoveHandle = MakeShared<FTimerHandle>();
		TWeakObjectPtr<UCameraComponent> WeakCam = FirstPersonCamera.Get();
		TWeakObjectPtr<UWorld> WeakWorld = GetWorld();
		GetWorldTimerManager().SetTimer(*MoveHandle, FTimerDelegate::CreateWeakLambda(this, [WeakCam, WeakWorld, MoveHandle, StartLoc, EndLoc, StartRot, EndRot, MoveTime, StartTime]()
		{
			if (!WeakCam.IsValid() || !WeakWorld.IsValid()) return;
			const float Alpha = FMath::Clamp(float((WeakWorld->GetTimeSeconds() - StartTime) / MoveTime), 0.f, 1.f);
			const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);
			WeakCam->SetWorldLocationAndRotation(FMath::Lerp(StartLoc, EndLoc, Eased),
				FQuat::Slerp(StartRot.Quaternion(), EndRot.Quaternion(), Eased));
			if (Alpha >= 1.f)
			{
				WeakWorld->GetTimerManager().ClearTimer(*MoveHandle);
			}
		}), 0.01f, true);
	}

	OnDeathView();

	// 眨眼几次再黑屏（相机淡入淡出）
	APlayerController* PC = Cast<APlayerController>(GetController());
	APlayerCameraManager* PCM = IsValid(PC) ? PC->PlayerCameraManager.Get() : nullptr;
	if (!IsValid(PCM)) return;

	const float Half = DeathBlinkTime * 0.5f;
	TWeakObjectPtr<APlayerCameraManager> WeakPCM = PCM;
	for (int32 i = 0; i < DeathBlinkCount; ++i)
	{
		const float Start = i * DeathBlinkTime;
		FTimerHandle CloseHandle, OpenHandle;
		GetWorldTimerManager().SetTimer(CloseHandle, FTimerDelegate::CreateWeakLambda(this, [WeakPCM, Half]()
		{
			if (WeakPCM.IsValid()) WeakPCM->StartCameraFade(0.f, 1.f, Half, FLinearColor::Black, false, true);
		}), FMath::Max(Start, 0.001f), false);
		GetWorldTimerManager().SetTimer(OpenHandle, FTimerDelegate::CreateWeakLambda(this, [WeakPCM, Half]()
		{
			if (WeakPCM.IsValid()) WeakPCM->StartCameraFade(1.f, 0.f, Half, FLinearColor::Black, false, true);
		}), Start + Half, false);
	}

	FTimerHandle FadeHandle;
	const float FadeAt = DeathBlinkCount * DeathBlinkTime + DeathFadeDelay;
	const float FadeTime = DeathFadeDuration;
	GetWorldTimerManager().SetTimer(FadeHandle, FTimerDelegate::CreateWeakLambda(this, [WeakPCM, FadeTime]()
	{
		// 停在全黑，直到复活时控制器把淡出清掉
		if (WeakPCM.IsValid()) WeakPCM->StartCameraFade(0.f, 1.f, FadeTime, FLinearColor::Black, false, true);
	}), FMath::Max(FadeAt, 0.001f), false);
}

void AShooterCharacter::SetFirstPersonViewedLocally(bool bViewed)
{
	if (bFirstPersonViewedLocally == bViewed) return;
	bFirstPersonViewedLocally = bViewed;
	if (IsLocallyControlled()) return;

	// 1P 手臂 OnlyOwnerSee、3P 身体 OwnerNoSee：视角目标是这个角色时引擎自动切换。这里处理枪和镜头。
	if (IsValid(SpringArm))
	{
		SpringArm->bUsePawnControlRotation = !bViewed;
		if (bViewed) SpringArm->SetWorldRotation(GetFixedAimRotation());
	}
	if (IsValid(Mesh1P))
	{
		Mesh1P->VisibilityBasedAnimTickOption = bViewed ? EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones
			: EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	}
	if (IsValid(Combat))
	{
		for (AWeapon* Weapon : Combat->GetInventory())
		{
			if (IsValid(Weapon)) Weapon->RefreshViewMeshVisibility();
		}
	}
}


void AShooterCharacter::SetDebugThirdPerson(bool bThirdPerson)
{
	if (!IsLocallyControlled())
	{
		return;
	}
	if (bDebugThirdPerson == bThirdPerson)
	{
		return;
	}
	bDebugThirdPerson = bThirdPerson;

	if (IsValid(SpringArm))
	{
		if (!bSavedSpringArmLength)
		{
			SavedSpringArmLength = SpringArm->TargetArmLength;
			bSavedSpringArmLength = true;
		}
		SpringArm->TargetArmLength = bDebugThirdPerson ? DebugThirdPersonArmLength : SavedSpringArmLength;
		SpringArm->bDoCollisionTest = bDebugThirdPerson;
		SpringArm->bUsePawnControlRotation = true;
	}

	// 1P arms: hide in third person
	if (IsValid(Mesh1P))
	{
		Mesh1P->SetOwnerNoSee(bDebugThirdPerson);
		Mesh1P->SetHiddenInGame(bDebugThirdPerson);
		if (IsValid(Mesh1P_Sleeves)) Mesh1P_Sleeves->SetHiddenInGame(bDebugThirdPerson);
	}

	auto SetBodyOwnerSee = [bThirdPerson](USkeletalMeshComponent* Comp)
	{
		if (!IsValid(Comp)) return;
		Comp->SetOwnerNoSee(!bThirdPerson); // FP: owner no see body; 3P: owner sees body
	};
	SetBodyOwnerSee(GetMesh());
	SetBodyOwnerSee(Mesh3P_Head.Get());
	SetBodyOwnerSee(Mesh3P_BodyUp.Get());
	SetBodyOwnerSee(Mesh3P_Pants.Get());
	SetBodyOwnerSee(Mesh3P_Jacket.Get());

	if (IsValid(Combat))
	{
		for (AWeapon* Weapon : Combat->GetInventory())
		{
			if (!IsValid(Weapon)) continue;
			if (USkeletalMeshComponent* W1 = Weapon->GetMesh1P())
			{
				W1->SetOwnerNoSee(bDebugThirdPerson);
				W1->SetHiddenInGame(bDebugThirdPerson);
			}
			if (USkeletalMeshComponent* W3 = Weapon->GetMesh3P())
			{
				W3->SetOwnerNoSee(!bDebugThirdPerson);
			}
			// Refresh then re-apply debug ownership so FP/3P swap sticks for local view.
			Weapon->RefreshViewMeshVisibility();
			if (USkeletalMeshComponent* W1b = Weapon->GetMesh1P())
			{
				W1b->SetOwnerNoSee(bDebugThirdPerson);
				W1b->SetHiddenInGame(bDebugThirdPerson);
			}
			if (USkeletalMeshComponent* W3b = Weapon->GetMesh3P())
			{
				W3b->SetOwnerNoSee(!bDebugThirdPerson);
			}
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
			bDebugThirdPerson ? TEXT("DebugThirdPerson ON（看自己 3P）") : TEXT("DebugThirdPerson OFF（回第一人称）"));
	}
	UE_LOG(LogTemp, Log, TEXT("[Debug3P] %s"), bDebugThirdPerson ? TEXT("ON") : TEXT("OFF"));
}

void AShooterCharacter::ToggleDebugThirdPerson()
{
	SetDebugThirdPerson(!bDebugThirdPerson);
}

void AShooterCharacter::SetDefaultFieldOfView(float InFOV)
{
	DefaultFOV = FMath::Clamp(InFOV, 60.f, 120.f);
	if (IsValid(FirstPersonCamera) && !(IsValid(Combat) && Combat->bAiming))
	{
		FirstPersonCamera->SetFieldOfView(DefaultFOV);
	}
}

// ===== 脚步 =====
void AShooterCharacter::UpdateFootsteps(float DeltaTime)
{
	if (!bEnableFootsteps || DeltaTime <= 0.f)
	{
		return;
	}
	// 专用服务器不播；死亡不播
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (IsValid(Stats) && Stats->DeathState != EDeathState::NotDead)
	{
		FootstepDistanceAccum = 0.f;
		return;
	}

	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!IsValid(CMC) || !CMC->IsMovingOnGround() || CMC->IsFalling() || bIsSliding)
	{
		FootstepDistanceAccum = 0.f;
		return;
	}

	const float Speed2D = GetVelocity().Size2D();
	if (Speed2D < FootstepMinSpeed)
	{
		FootstepDistanceAccum = 0.f;
		return;
	}

	bool bSprinting = false;
	if (const AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetController()))
	{
		bSprinting = PC->IsSprinting();
	}

	float StepDist = FootstepDistanceRun;
	float Volume = FootstepVolumeRun;
	if (bIsProne)
	{
		StepDist = FootstepDistanceProne;
		Volume = FootstepVolumeProne;
	}
	else if (bIsCrouched)
	{
		StepDist = FootstepDistanceCrouch;
		Volume = FootstepVolumeCrouch;
	}
	else if (bSprinting)
	{
		StepDist = FootstepDistanceSprint;
		Volume = FootstepVolumeSprint;
	}
	else if (bUseWalkGait)
	{
		StepDist = FootstepDistanceWalk;
		Volume = FootstepVolumeWalk;
	}

	FootstepDistanceAccum += Speed2D * DeltaTime;
	if (FootstepDistanceAccum < StepDist)
	{
		return;
	}
	FootstepDistanceAccum = 0.f;
	bFootstepLeft = !bFootstepLeft;

	const EPhysicalSurface Surface = TraceFootstepSurface();
	USoundBase* Sound = ResolveFootstepSound(Surface);
	PlayFootstepAtFeet(Sound, Volume);

	// Bloody footprints (local): leave fading decals while bloodied from recent hits.
	if (BloodyFootstepsRemaining > 0 && GetNetMode() != NM_DedicatedServer)
	{
		SpawnBloodFootprint();
		--BloodyFootstepsRemaining;
	}
}

EPhysicalSurface AShooterCharacter::TraceFootstepSurface() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return SurfaceType_Default;
	}

	const UCapsuleComponent* Cap = GetCapsuleComponent();
	const float Half = IsValid(Cap) ? Cap->GetScaledCapsuleHalfHeight() : 90.f;
	const FVector Start = GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, Half + 40.f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FootstepSurface), false, this);
	Params.bReturnPhysicalMaterial = true;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return SurfaceType_Default;
	}
	if (Hit.PhysMaterial.IsValid())
	{
		return Hit.PhysMaterial->SurfaceType.GetValue();
	}
	return SurfaceType_Default;
}

USoundBase* AShooterCharacter::ResolveFootstepSound(EPhysicalSurface Surface) const
{
	auto LoadSoft = [](const TSoftObjectPtr<USoundBase>& Soft) -> USoundBase*
	{
		if (Soft.IsNull()) return nullptr;
		if (USoundBase* Loaded = Soft.Get()) return Loaded;
		return Soft.LoadSynchronous();
	};

	USoundBase* Pick = nullptr;
	switch (Surface)
	{
	case SurfaceType2: // Concrete
		Pick = LoadSoft(FootstepSounds.Concrete);
		break;
	case SurfaceType3: // Glass
		Pick = LoadSoft(FootstepSounds.Glass);
		break;
	case SurfaceType4: // Wood
		Pick = LoadSoft(FootstepSounds.Wood);
		break;
	case SurfaceType5: // Metal
		Pick = LoadSoft(FootstepSounds.Metal);
		break;
	default:
		break;
	}
	if (!Pick)
	{
		Pick = LoadSoft(FootstepSounds.Default);
	}
	return Pick;
}

void AShooterCharacter::PlayFootstepAtFeet(USoundBase* Sound, float Volume)
{
	if (!IsValid(Sound) || Volume <= 0.f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	const UCapsuleComponent* Cap = GetCapsuleComponent();
	const float Half = IsValid(Cap) ? Cap->GetScaledCapsuleHalfHeight() : 90.f;
	// 左右脚轻微侧移，避免所有脚步叠在一点
	const float Side = bFootstepLeft ? -12.f : 12.f;
	const FVector Loc = GetActorLocation() + GetActorRightVector() * Side - FVector(0.f, 0.f, Half - 2.f);

	USoundAttenuation* Atten = nullptr;
	if (!FootstepAttenuation.IsNull())
	{
		Atten = FootstepAttenuation.Get();
		if (!Atten) Atten = FootstepAttenuation.LoadSynchronous();
	}

	UGameplayStatics::PlaySoundAtLocation(World, Sound, Loc, Volume, 1.f, 0.f, Atten);
}


// ===== Barrel obstacle / limb injury / bandage (tactical) =====

WeaponObstacle::FProbeResult AShooterCharacter::ProbeWeaponObstacle() const
{
	const AWeapon* Weapon = IsValid(Combat) ? Combat->CurrentWeapon.Get() : nullptr;
	if (!IsValid(Weapon) || !GetWorld() || !IsValid(FirstPersonCamera)) return {};
	if (Weapon->WeaponKind != EWeaponKind::LongGun && Weapon->WeaponKind != EWeaponKind::Pistol) return {};
	float Length = Weapon->WeaponLength;
	if (Weapon->WeaponKind == EWeaponKind::Pistol && Length >= 60.f) Length = 22.f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponBarrelObstacle), false, this);
	Params.AddIgnoredActor(Weapon);
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors, true, true);
	Params.AddIgnoredActors(AttachedActors);
	// Remote first-person camera rotation is not driven every server frame. Use
	// authoritative aim rotation there instead of trusting a client fire-state flag.
	const FQuat ViewRotation = IsLocallyControlled()
		? FirstPersonCamera->GetComponentQuat() : GetBaseAimRotation().Quaternion();
	return WeaponObstacle::Probe(*GetWorld(), FirstPersonCamera->GetComponentLocation(), ViewRotation, Length, Params);
}

bool AShooterCharacter::IsWeaponFireBlocked() const
{
	const AWeapon* Weapon = IsValid(Combat) ? Combat->CurrentWeapon.Get() : nullptr;
	if (!IsValid(Weapon) || (Weapon->WeaponKind != EWeaponKind::LongGun && Weapon->WeaponKind != EWeaponKind::Pistol)) return false;
	// Block from the first visible lift until the pose has nearly settled, including
	// between character ticks. The server independently probes current geometry.
	return WeaponBlockAlpha > 0.02f || ProbeWeaponObstacle().Alpha > 0.02f;
}

void AShooterCharacter::UpdateWeaponObstacle(float DeltaTime)
{
	if (!IsLocallyControlled() || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const WeaponObstacle::FProbeResult Probe = ProbeWeaponObstacle();
	const float TargetAlpha = Probe.Alpha;
	if (bDebugWeaponObstacle && GetWorld() && !Probe.Starts[0].Equals(Probe.Ends[0]))
	{
		const FColor Color = TargetAlpha > 0.f ? FColor::Orange : FColor::Green;
		for (int32 Index = 0; Index < 2; ++Index)
		{
			DrawDebugLine(GetWorld(), Probe.Starts[Index], Probe.Ends[Index], Color, false, -1.f, 0, 1.2f);
			DrawDebugSphere(GetWorld(), Probe.Ends[Index], 4.f, 8, Color, false, -1.f);
		}
		if (TargetAlpha > 0.f) DrawDebugPoint(GetWorld(), Probe.Hit.ImpactPoint, 8.f, FColor::Red, false, -1.f);
	}

	// One exponential blend: consistent response across frame rates without stacking
	// three interpolation delays on alpha, location and rotation.
	const float Blend = WeaponBlockInterpSpeed > 0.f ? 1.f - FMath::Exp(-WeaponBlockInterpSpeed * DeltaTime) : 1.f;
	WeaponBlockAlpha = FMath::Lerp(WeaponBlockAlpha, TargetAlpha, Blend);
	WeaponBlockLocCurrent = FVector(-WeaponBlockRetract * WeaponBlockAlpha, 0.f, 0.f);
	const float RaisedPoseAlpha = FMath::Clamp(WeaponBlockAlpha / 0.65f, 0.f, 1.f);
	WeaponBlockRotCurrent = FRotator(WeaponBlockRaisePitch * RaisedPoseAlpha, 0.f, 0.f);

	// Auto drop ADS when jammed.
	if (IsWeaponBlocked() && IsValid(Combat) && Combat->bAiming)
	{
		Combat->Initiate_Aim_Released();
	}
}


bool AShooterCharacter::IsProtectedAimTarget(const AActor* HitActor) const
{
	if (!IsValid(HitActor) || HitActor == this) return false;

	// Future hostages: interface and/or Actor tag "Hostage".
	if (HitActor->ActorHasTag(FName(TEXT("Hostage")))) return true;
	if (HitActor->GetClass()->ImplementsInterface(UHostageProtectInterface::StaticClass()))
	{
		return true;
	}

	const AShooterCharacter* Other = Cast<AShooterCharacter>(HitActor);
	if (!Other)
	{
		Other = Cast<AShooterCharacter>(HitActor->GetOwner());
	}
	if (!Other || Other == this) return false;

	const AShooterPlayerState* MyPS = GetPlayerState<AShooterPlayerState>();
	const AShooterPlayerState* TheirPS = Other->GetPlayerState<AShooterPlayerState>();
	if (!MyPS || !TheirPS || !MyPS->HasTeam() || !TheirPS->HasTeam()) return false;
	return MyPS->GetTeamId() == TheirPS->GetTeamId();
}

void AShooterCharacter::UpdateMuzzleDip(float DeltaTime)
{
	if (!IsLocallyControlled() || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	float TargetAlpha = 0.f;
	const bool bAds = IsValid(Combat) && Combat->bAiming;
	const AWeapon* Weapon = IsValid(Combat) ? Combat->CurrentWeapon.Get() : nullptr;
	const bool bGunOut = IsValid(Weapon)
		&& Weapon->WeaponKind != EWeaponKind::Melee
		&& Weapon->WeaponKind != EWeaponKind::Throwable
		&& Weapon->WeaponKind != EWeaponKind::Bomb;

	if (!bAds && bGunOut && IsValid(FirstPersonCamera) && GetWorld() && MuzzleDipRange > 1.f)
	{
		const FVector Start = FirstPersonCamera->GetComponentLocation();
		const FVector Dir = FirstPersonCamera->GetForwardVector();
		const FVector End = Start + Dir * MuzzleDipRange;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(MuzzleDipProtect), false, this);
		Params.AddIgnoredActor(this);
		if (IsValid(Weapon)) Params.AddIgnoredActor(Weapon);

		FCollisionResponseParams ResponseParams;
		ResponseParams.CollisionResponse.SetAllChannels(ECR_Ignore);
		ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Block);
		ResponseParams.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);
		ResponseParams.CollisionResponse.SetResponse(ECC_WorldDynamic, ECR_Block);
		ResponseParams.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Block);

		FHitResult Hit;
		const bool bHit = GetWorld()->LineTraceSingleByChannel(
			Hit, Start, End, FPSTraceChannels::ECC_Weapon, Params, ResponseParams);

		if (bHit && IsProtectedAimTarget(Hit.GetActor()))
		{
			// Soft ramp within range; strongest when close.
			const float Dist = Hit.Distance;
			TargetAlpha = FMath::Clamp(1.f - (Dist / MuzzleDipRange), 0.15f, 1.f);
		}
	}

	MuzzleDipAlpha = FMath::FInterpTo(MuzzleDipAlpha, TargetAlpha, DeltaTime, MuzzleDipInterpSpeed);
	const FRotator TargetRot(-MuzzleDipPitch * MuzzleDipAlpha, 0.f, 0.f);
	MuzzleDipRotCurrent = FMath::RInterpTo(MuzzleDipRotCurrent, TargetRot, DeltaTime, MuzzleDipInterpSpeed);
	if (MuzzleDipAlpha < 0.01f && TargetAlpha <= 0.f)
	{
		MuzzleDipAlpha = 0.f;
		MuzzleDipRotCurrent = FRotator::ZeroRotator;
	}
}

float AShooterCharacter::ScaleDamageByArmor(float Damage, EHitZone Zone, bool bPistolOrShotgun) const
{
	if (BodyArmor == EBodyArmorType::None || Zone != EHitZone::Torso)
	{
		return Damage;
	}
	float Mul = 1.f;
	if (BodyArmor == EBodyArmorType::Light)
	{
		Mul = LightArmorTorsoMultiplier;
	}
	else // Heavy
	{
		Mul = bPistolOrShotgun ? HeavyArmorPistolShotgunTorsoMultiplier : HeavyArmorTorsoMultiplier;
	}
	return Damage * Mul;
}

void AShooterCharacter::SetBodyArmorLocal(EBodyArmorType NewArmor)
{
	if (HasAuthority())
	{
		BodyArmor = NewArmor;
		OnRep_BodyArmor();
	}
	else
	{
		Server_SetBodyArmor(NewArmor);
	}
}

void AShooterCharacter::Server_SetBodyArmor_Implementation(EBodyArmorType NewArmor)
{
	BodyArmor = NewArmor;
	OnRep_BodyArmor();
}

void AShooterCharacter::OnRep_BodyArmor()
{
	UE_LOG(LogTemp, Log, TEXT("[Armor] %s BodyArmor=%d"), *GetNameSafe(this), static_cast<int32>(BodyArmor));
}

void AShooterCharacter::EnsureFactionLookDefaults()
{
	// Prefer Bandits over any leftover CharGen terrorist soft ref from BP/CDO.
	const FSoftObjectPath BanditMesh(TEXT("/Game/Bandits/Mesh/Preset/SKM_Bandit_Preset_05.SKM_Bandit_Preset_05"));
	const FString CriminalPath = CriminalMesh3P.ToSoftObjectPath().ToString();
	if (CriminalMesh3P.IsNull() || CriminalPath.Contains(TEXT("CharGen")) || CriminalPath.Contains(TEXT("SK_Terrorist")))
	{
		CriminalMesh3P = TSoftObjectPtr<USkeletalMesh>(BanditMesh);
	}
	// Police: Bandits Preset_13 (beige gorka, black gloves) - same UE5 Epic skeleton as Preset_05.
	const FSoftObjectPath PoliceBanditMesh(TEXT("/Game/Bandits/Mesh/Preset/SKM_Bandit_Preset_13.SKM_Bandit_Preset_13"));
	const FString PolicePath = PoliceMesh3P.ToSoftObjectPath().ToString();
	if (PoliceMesh3P.IsNull() || PolicePath.Contains(TEXT("CharGen")) || PolicePath.Contains(TEXT("Preset_02")))
	{
		PoliceMesh3P = TSoftObjectPtr<USkeletalMesh>(PoliceBanditMesh);
	}
	if (PoliceHeadMesh3P.IsNull())
	{
		PoliceHeadMesh3P = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/CharGen/CharGen_Base/Meshes/SK_CTHead_2.SK_CTHead_2")));
	}
	if (PoliceBodyDetMesh3P.IsNull())
	{
		PoliceBodyDetMesh3P = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/CharGen/CharGen_Base/Meshes/SK_Body_det.SK_Body_det")));
	}
	if (PoliceBodyUpMesh3P.IsNull())
	{
		PoliceBodyUpMesh3P = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/CharGen/CharGen_Base/Meshes/SK_Body_Up.SK_Body_Up")));
	}
	if (PolicePantsMesh3P.IsNull())
	{
		PolicePantsMesh3P = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/CharGen/CharGen_Base/Meshes/SK_Body_Pants.SK_Body_Pants")));
	}
	if (PoliceJacketMesh3P.IsNull())
	{
		PoliceJacketMesh3P = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/CharGen/CharGen_Base/Meshes/SK_Jacket_Main.SK_Jacket_Main")));
	}
	if (PoliceJacketMaterial.IsNull())
	{
		PoliceJacketMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/CharGen/CharGen_Base/Materials/MI_Jacket_Black.MI_Jacket_Black")));
	}
	if (CharGenAnimBP3P.IsNull())
	{
		CharGenAnimBP3P = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/CharGen/CharGen_Base/Animations/TPS_Char_AnimBP.TPS_Char_AnimBP_C")));
	}
	if (CriminalArmsMesh1P.IsNull())
	{
		CriminalArmsMesh1P = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
			TEXT("/Game/Bandits/Mesh/Arms/Arms_Gloves/SKM_Arms_Gloves_Black.SKM_Arms_Gloves_Black")));
	}
	// Police arms forced to match Preset_13 (uses M_Arms_Gloves_Black)
	{
		PoliceArmsMesh1P = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
			TEXT("/Game/Bandits/Mesh/Arms/Arms_Gloves/SKM_Arms_Gloves_Black.SKM_Arms_Gloves_Black")));
	}
	if (CriminalSleeveMesh1P.IsNull())
	{
		// Preset_05 outer cloth = Bomber (brown MI). Same SKM_Bandit_Skeleton as FP arms.
		CriminalSleeveMesh1P = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
			TEXT("/Game/Bandits/Mesh/Cloth/Bomber/SKM_Bomber_Bege.SKM_Bomber_Bege")));
	}
	if (CriminalSleeveMaterial.IsNull())
	{
		CriminalSleeveMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
			TEXT("/Game/Bandits/Materials/Cloth/Bomber/M_Bomber_Brown.M_Bomber_Brown")));
	}
	if (PoliceSleeveMesh1P.IsNull())
	{
		// Preset_13 outer cloth = Gorka beige.
		PoliceSleeveMesh1P = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
			TEXT("/Game/Bandits/Mesh/Cloth/Jacket_Gorka/SKM_Jacket_Gorka_Bege.SKM_Jacket_Gorka_Bege")));
	}
}

USkeletalMesh* AShooterCharacter::LoadSoftMesh(const TSoftObjectPtr<USkeletalMesh>& Soft) const
{
	if (Soft.IsNull()) return nullptr;
	if (USkeletalMesh* Loaded = Soft.Get()) return Loaded;
	return Soft.LoadSynchronous();
}

UMaterialInterface* AShooterCharacter::LoadSoftMaterial(const TSoftObjectPtr<UMaterialInterface>& Soft) const
{
	if (Soft.IsNull()) return nullptr;
	if (UMaterialInterface* Loaded = Soft.Get()) return Loaded;
	return Soft.LoadSynchronous();
}

void AShooterCharacter::SetModularLookVisible(bool bVisible)
{
	const auto SetOne = [bVisible](USkeletalMeshComponent* Comp)
	{
		if (!IsValid(Comp)) return;
		Comp->SetVisibility(bVisible, true);
		Comp->SetHiddenInGame(!bVisible);
	};
	SetOne(Mesh3P_Head);
	SetOne(Mesh3P_BodyUp);
	SetOne(Mesh3P_Pants);
	SetOne(Mesh3P_Jacket);
}

bool AShooterCharacter::IsUsingBanditsFirstPersonArms() const
{
	if (!bUseBanditsArms1P)
	{
		return false;
	}
	if (FactionLook == ECharacterLookFaction::Police || FactionLook == ECharacterLookFaction::Criminal)
	{
		return true;
	}
	return IsValid(Mesh1P) && IsBanditsSkelMesh(Mesh1P->GetSkeletalMeshAsset());
}

void AShooterCharacter::SetFactionLookLocal(ECharacterLookFaction NewLook)
{
	if (HasAuthority())
	{
		FactionLook = NewLook;
		OnRep_FactionLook();
	}
	else
	{
		Server_SetFactionLook(NewLook);
	}
}

void AShooterCharacter::Server_SetFactionLook_Implementation(ECharacterLookFaction NewLook)
{
	FactionLook = NewLook;
	OnRep_FactionLook();
}

void AShooterCharacter::OnRep_FactionLook()
{
	ApplyFactionLookMeshes();
	UE_LOG(LogTemp, Log, TEXT("[Look] %s FactionLook=%d"), *GetNameSafe(this), static_cast<int32>(FactionLook));
}

void AShooterCharacter::ApplyFactionLookMeshes()
{
	EnsureFactionLookDefaults();

	USkeletalMeshComponent* Body = GetMesh();
	if (!IsValid(Body)) return;

	// BanditsArms1P: swap FP arms + copy weapon sockets from Ironbelly (no pose retarget).
	ApplyFactionArms1P();
	if (!bCapturedDefaultMesh3P)
	{
		USkeletalMesh* CurBody = Body->GetSkeletalMeshAsset();
		DefaultMesh3PAnimClass = Body->GetAnimClass();
		DefaultMesh3PAsset = IsBanditsSkelMesh(CurBody) ? LoadCanonicalMesh3PSource() : CurBody;
		if (!IsValid(DefaultMesh3PAsset))
		{
			DefaultMesh3PAsset = CurBody;
		}
		bCapturedDefaultMesh3P = true;
	}

	// Prefer project ABP_ThirdPerson for locomotion. CharGen TPS_Char_AnimBP is authored for
	// BP_CharGenCharacter only - on ShooterCharacter it stays in reference pose (T-pose).
	// UE4 CharGen skeleton <-> project Manny: link CompatibleSkeletons so ABP can drive the mesh.
	auto EnsureSkeletonCompat = [](USkeleton* A, USkeleton* B)
	{
		if (!IsValid(A) || !IsValid(B) || A == B) return;
#if WITH_EDITOR
		// AddCompatibleSkeleton is editor-only in UE5.8; PIE has WITH_EDITOR.
		A->AddCompatibleSkeleton(B);
		B->AddCompatibleSkeleton(A);
#endif
	};

	auto SetBodyMeshKeepAnim = [this, Body, &EnsureSkeletonCompat](USkeletalMesh* MeshAsset, bool /*bUseCharGenAnim*/)
	{
		if (!IsValid(Body) || !IsValid(MeshAsset)) return;
		if (!bCapturedDefaultMesh3P)
		{
			USkeletalMesh* CurBody = Body->GetSkeletalMeshAsset();
			DefaultMesh3PAnimClass = Body->GetAnimClass();
			DefaultMesh3PAsset = IsBanditsSkelMesh(CurBody) ? LoadCanonicalMesh3PSource() : CurBody;
			if (!IsValid(DefaultMesh3PAsset))
			{
				DefaultMesh3PAsset = CurBody;
			}
			bCapturedDefaultMesh3P = true;
		}

		USkeleton* MeshSkel = MeshAsset->GetSkeleton();
		USkeleton* DefaultSkel = IsValid(DefaultMesh3PAsset) ? DefaultMesh3PAsset->GetSkeleton() : nullptr;
		EnsureSkeletonCompat(MeshSkel, DefaultSkel);
		// Link Bandits skeleton <-> project Mannequin skeletons (load meshes, then GetSkeleton).
		static const TCHAR* ExtraMeshPaths[] = {
			TEXT("/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"),
			TEXT("/Game/FPS_Controller/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny"),
			TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"),
			TEXT("/Game/FPS_Controller/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"),
			TEXT("/Game/Bandits/Mesh/Preset/SKM_Bandit_Preset_05.SKM_Bandit_Preset_05"),
			TEXT("/Game/Bandits/Mesh/Preset/SKM_Bandit_Preset_13.SKM_Bandit_Preset_13"),
		};
		for (const TCHAR* Path : ExtraMeshPaths)
		{
			if (USkeletalMesh* ExtraMesh = LoadObject<USkeletalMesh>(nullptr, Path))
			{
				USkeleton* Extra = ExtraMesh->GetSkeleton();
				EnsureSkeletonCompat(MeshSkel, Extra);
				EnsureSkeletonCompat(DefaultSkel, Extra);
			}
		}
		if (USkeleton* BanditSkel = LoadObject<USkeleton>(nullptr, TEXT("/Game/Bandits/Mesh/SKM_Bandit_Skeleton.SKM_Bandit_Skeleton")))
		{
			EnsureSkeletonCompat(MeshSkel, BanditSkel);
			EnsureSkeletonCompat(DefaultSkel, BanditSkel);
		}

		Body->SetSkeletalMesh(MeshAsset, /*bReinitPose=*/true);
		Body->SetRelativeScale3D(FVector::OneVector);
		Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

		UClass* WantAnim = DefaultMesh3PAnimClass.Get();
		if (!WantAnim)
		{
			WantAnim = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/FPS/Character/ABP_ThirdPerson.ABP_ThirdPerson_C"));
			if (WantAnim)
			{
				DefaultMesh3PAnimClass = WantAnim;
			}
		}

		// Always re-assign after mesh swap so AnimInstance is recreated on the new skeleton.
		if (WantAnim)
		{
			Body->SetAnimInstanceClass(WantAnim);
			if (UAnimInstance* AnimInst = Body->GetAnimInstance())
			{
				AnimInst->InitializeAnimation(true);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Look] No DefaultMesh3P AnimBP - body will T-pose"));
		}

		UE_LOG(LogTemp, Log, TEXT("[Look] Body mesh=%s skel=%s anim=%s (project ABP, not CharGen)"),
			*GetNameSafe(MeshAsset),
			*GetNameSafe(MeshSkel),
			*GetNameSafe(Body->GetAnimClass()));
	};

	auto SetupFollower = [Body](USkeletalMeshComponent* Comp, USkeletalMesh* MeshAsset)
	{
		if (!IsValid(Comp)) return;
		if (!IsValid(MeshAsset))
		{
			Comp->SetLeaderPoseComponent(nullptr);
			Comp->SetSkeletalMesh(nullptr);
			Comp->SetVisibility(false);
			Comp->SetHiddenInGame(true);
			return;
		}
		Comp->SetSkeletalMesh(MeshAsset);
		Comp->SetLeaderPoseComponent(Body);
		Comp->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		Comp->SetRelativeScale3D(FVector::OneVector);
		Comp->bOnlyOwnerSee = Body->bOnlyOwnerSee;
		Comp->bOwnerNoSee = Body->bOwnerNoSee;
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	};

	auto ClearFollowers = [&]()
	{
		SetupFollower(Mesh3P_Head, nullptr);
		SetupFollower(Mesh3P_BodyUp, nullptr);
		SetupFollower(Mesh3P_Pants, nullptr);
		SetupFollower(Mesh3P_Jacket, nullptr);
	};

	switch (FactionLook)
	{
	case ECharacterLookFaction::Criminal:
	{
		ClearFollowers();
		SetModularLookVisible(false);
		if (USkeletalMesh* Criminal = LoadSoftMesh(CriminalMesh3P))
		{
			EnsureMesh3PGripSockets(Criminal, DefaultMesh3PAsset.Get(), Body);
			SetBodyMeshKeepAnim(Criminal, true);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Look] Criminal mesh failed to load"));
		}
		break;
	}
		case ECharacterLookFaction::Police:
	{
		// Prefer single Bandits preset (UE5 Epic skeleton, same pipeline as Criminal).
		// CharGen modular parts remain available if PoliceMesh3P fails to load.
		ClearFollowers();
		SetModularLookVisible(false);
		if (USkeletalMesh* PoliceBody = LoadSoftMesh(PoliceMesh3P))
		{
			EnsureMesh3PGripSockets(PoliceBody, DefaultMesh3PAsset.Get(), Body);
			SetBodyMeshKeepAnim(PoliceBody, true);
			break;
		}
		UE_LOG(LogTemp, Warning, TEXT("[Look] PoliceMesh3P failed; falling back to CharGen modular"));
		USkeletalMesh* BodyDet = LoadSoftMesh(PoliceBodyDetMesh3P);
		USkeletalMesh* BodyUp = LoadSoftMesh(PoliceBodyUpMesh3P);
		USkeletalMesh* Head = LoadSoftMesh(PoliceHeadMesh3P);
		USkeletalMesh* Pants = LoadSoftMesh(PolicePantsMesh3P);
		USkeletalMesh* Jacket = LoadSoftMesh(PoliceJacketMesh3P);
		if (BodyDet)
		{
			SetBodyMeshKeepAnim(BodyDet, true);
		}
		else if (BodyUp)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Look] Police Body_det missing; falling back to Body_Up (no hands)"));
			SetBodyMeshKeepAnim(BodyUp, true);
			BodyUp = nullptr;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Look] Police body meshes failed to load"));
		}
		SetupFollower(Mesh3P_Head, Head);
		SetupFollower(Mesh3P_BodyUp, BodyUp);
		SetupFollower(Mesh3P_Pants, Pants);
		SetupFollower(Mesh3P_Jacket, Jacket);
		if (IsValid(Mesh3P_Jacket))
		{
			if (UMaterialInterface* JacketMat = LoadSoftMaterial(PoliceJacketMaterial))
			{
				Mesh3P_Jacket->SetMaterial(0, JacketMat);
			}
		}
		SetModularLookVisible(true);
		break;
	}
	default:
	{
		ClearFollowers();
		SetModularLookVisible(false);
		if (IsValid(DefaultMesh3PAsset))
		{
			SetBodyMeshKeepAnim(DefaultMesh3PAsset, false);
		}
		break;
	}
	}

	ConfigureWeaponHitCollision();

	if (IsValid(Combat))
	{
		Combat->RefreshAnimsAfterMesh3PSwap();
		// Bandits body now has copied grip sockets; force equipped gun back onto weapon_r / RifleSocket*.
		auto ReattachEquipped = [](UCombatComponent* C)
		{
			if (!IsValid(C)) return;
			if (AWeapon* W = C->CurrentWeapon.Get())
			{
				W->AttachToOwingPawn();
			}
		};
		ReattachEquipped(Combat);
		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<AShooterCharacter> WeakThis(this);
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis]()
			{
				AShooterCharacter* Self = WeakThis.Get();
				if (!IsValid(Self)) return;
				UCombatComponent* C = Self->GetCombatComponent();
				if (!IsValid(C)) return;
				AWeapon* W = C->CurrentWeapon.Get();
				if (IsValid(W))
				{
					W->AttachToOwingPawn();
				}
				UE_LOG(LogTemp, Log, TEXT("[Look] Post-swap reattach CurrentWeapon=%s Mesh3P=%s"),
					*GetNameSafe(W), *GetNameSafe(Self->GetMesh() ? Self->GetMesh()->GetSkeletalMeshAsset() : nullptr));
			}));
		}
		// Re-apply debug 3P visibility after mesh/anim reinits.
		if (bDebugThirdPerson && IsLocallyControlled())
		{
			const bool bOn = bDebugThirdPerson;
			bDebugThirdPerson = false;
			SetDebugThirdPerson(bOn);
		}
	}
}

void AShooterCharacter::Auth_ApplyHitZoneInjury(EHitZone Zone)
{
	if (!HasAuthority()) return;
	if (IsValid(Stats) && Stats->DeathState != EDeathState::NotDead) return;

	Client_FlashHitZone(Zone);

	bool bChanged = false;
	if (Zone == EHitZone::Arm && !bArmInjured)
	{
		bArmInjured = true;
		bChanged = true;
		OnRep_ArmInjured();
	}
	else if (Zone == EHitZone::Leg && !bLegInjured)
	{
		bLegInjured = true;
		bChanged = true;
		OnRep_LegInjured();
	}
	if (bChanged)
	{
		ForceNetUpdate();
		RefreshControllerMovementSpeed();
	}
}

void AShooterCharacter::Client_FlashHitZone_Implementation(EHitZone Zone)
{
	switch (Zone)
	{
	case EHitZone::Head: bHeadHitMarked = true; break;
	case EHitZone::Torso: bTorsoHitMarked = true; break;
	case EHitZone::Arm: break;
	case EHitZone::Leg: break;
	default: break;
	}
	UE_LOG(LogTemp, Warning, TEXT("[BodyInjury] FlashHitZone %d on %s local=%d"), (int32)Zone, *GetName(), IsLocallyControlled() ? 1 : 0);
	// Remote clients don't get the bone side. NoteZone keeps a side the host already set.
	HudBodyMarks::NoteZone(this, Zone);
}

bool AShooterCharacter::IsHitZoneMarked(EHitZone Zone) const
{
	switch (Zone)
	{
	case EHitZone::Head: return bHeadHitMarked;
	case EHitZone::Torso: return bTorsoHitMarked;
	case EHitZone::Arm: return bArmInjured;
	case EHitZone::Leg: return bLegInjured;
	default: return false;
	}
}

void AShooterCharacter::ClearHitZoneMarks()
{
	bHeadHitMarked = false;
	bTorsoHitMarked = false;
	HudBodyMarks::Clear(this);
}

void AShooterCharacter::OnRep_ArmInjured()
{
	RefreshControllerMovementSpeed();
	if (!bArmInjured)
	{
		HudBodyMarks::ClearArms(this);
	}
}

void AShooterCharacter::OnRep_LegInjured()
{
	RefreshControllerMovementSpeed();
	if (!bLegInjured)
	{
		HudBodyMarks::ClearLegs(this);
	}
	// Leg injury cancels sprint on owning controller.
	if (bLegInjured)
	{
		if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetController()))
		{
			PC->StopSprinting();
		}
	}
}

void AShooterCharacter::Input_Bandage_Started()
{
	if (!IsLocallyControlled()) return;
	if (bArmInjured || bLegInjured)
	{
		BeginBandage();
		return;
	}
	// Not injured: keep old HealAction behavior (hotkey 5 / HP heal).
	Input_Heal();
}

void AShooterCharacter::Input_Bandage_Completed()
{
	if (!IsLocallyControlled()) return;
	if (!bBandageHoldInput && !bIsBandaging) return;
	bBandageHoldInput = false;
	if (bIsBandaging && BandageHoldElapsed < BandageHoldDuration - 0.05f)
	{
		AbortBandage(false);
	}
}

void AShooterCharacter::BeginBandage()
{
	if (!IsLocallyControlled() || bIsBandaging || bIsHealing) return;
	if (!bArmInjured && !bLegInjured) return;
	if (IsValid(Stats) && Stats->DeathState != EDeathState::NotDead) return;
	if (IsValid(Combat) && IsValid(Combat->CurrentWeapon))
	{
		const EWeaponStatus Status = Combat->CurrentWeapon->WeaponStatus;
		if (Status == EWeaponStatus::Reloading || Status == EWeaponStatus::Cycling) return;
	}

	bBandageHoldInput = true;
	bIsBandaging = true;
	BandageHoldElapsed = 0.f;
	if (IsValid(Combat) && Combat->bAiming)
	{
		Combat->Initiate_Aim_Released();
	}
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetController()))
	{
		PC->StopSprinting();
	}
	Server_BeginBandage();
	RefreshControllerMovementSpeed();
}

void AShooterCharacter::AbortBandage(bool bCompleted)
{
	const bool bWas = bIsBandaging;
	bBandageHoldInput = false;
	bIsBandaging = false;
	BandageHoldElapsed = 0.f;
	if (bWas && IsLocallyControlled())
	{
		Server_FinishBandage(bCompleted);
	}
	RefreshControllerMovementSpeed();
}

void AShooterCharacter::UpdateBandage(float DeltaTime)
{
	if (!IsLocallyControlled() || !bIsBandaging) return;

	if (!bBandageHoldInput || (!bArmInjured && !bLegInjured)
		|| (IsValid(Stats) && Stats->DeathState != EDeathState::NotDead))
	{
		AbortBandage(false);
		return;
	}

	BandageHoldElapsed += DeltaTime;
	if (BandageHoldElapsed >= BandageHoldDuration)
	{
		AbortBandage(true);
	}
}

void AShooterCharacter::Server_BeginBandage_Implementation()
{
	if (IsValid(Stats) && Stats->DeathState != EDeathState::NotDead) return;
	if (!bArmInjured && !bLegInjured) return;
	bIsBandaging = true;
	RefreshControllerMovementSpeed();
}

void AShooterCharacter::Server_FinishBandage_Implementation(bool bCompleted)
{
	bIsBandaging = false;
	if (bCompleted && (bArmInjured || bLegInjured))
	{
		bArmInjured = false;
		bLegInjured = false;
		OnRep_ArmInjured();
		OnRep_LegInjured();
		ForceNetUpdate();
	}
	RefreshControllerMovementSpeed();
}



// ---------- Tactical vision (NVG / Thermal) ----------
float AShooterCharacter::GetVisionBatteryNormalized() const
{
	return VisionBatteryMaxSeconds > KINDA_SMALL_NUMBER
		? FMath::Clamp(VisionBatterySeconds / VisionBatteryMaxSeconds, 0.f, 1.f)
		: 0.f;
}

void AShooterCharacter::RefillVisionBattery(float Amount)
{
	if (Amount < 0.f)
	{
		VisionBatterySeconds = VisionBatteryMaxSeconds;
	}
	else
	{
		VisionBatterySeconds = FMath::Clamp(VisionBatterySeconds + Amount, 0.f, VisionBatteryMaxSeconds);
	}
}

void AShooterCharacter::AddVisionFlashWash(float Amount)
{
	VisionFlashWash = FMath::Clamp(VisionFlashWash + FMath::Max(0.f, Amount), 0.f, 4.f);
	if (IsLocallyControlled())
	{
		ReceiveFlashbang(FMath::Clamp(Amount / 2.5f, 0.f, 1.f));
		ApplyTacticalVisionEffects();
	}
}

void AShooterCharacter::ReceiveFlashbang(float Strength)
{
	Strength = FMath::Clamp(Strength, 0.f, 1.f);
	if (Strength <= 0.f) return;
	if (bHasFlashGoggles)
	{
		FlashGhost = FMath::Max(FlashGhost, Strength);
	}
	else
	{
		FlashWhite = FMath::Max(FlashWhite, Strength);
	}
}

void AShooterCharacter::Client_ReceiveFlashbang_Implementation(float Strength)
{
	ReceiveFlashbang(Strength);
}

void AShooterCharacter::SetHasFlashGoggles(bool bEnable)
{
	bHasFlashGoggles = bEnable;
	if (!HasAuthority())
	{
		Server_SetHasFlashGoggles(bEnable);
	}
}

void AShooterCharacter::Server_SetHasFlashGoggles_Implementation(bool bEnable)
{
	bHasFlashGoggles = bEnable;
}

float AShooterCharacter::GetHitBloodAlpha() const
{
	return FMath::Clamp(FMath::Max(HitBloodFlash, HitBloodTail), 0.f, 1.f);
}

void AShooterCharacter::AddHitBlood()
{
	HitBloodFlash = 1.f;
	HitBloodTail = FMath::Max(HitBloodTail, 0.62f);
}

void AShooterCharacter::UpdateScreenEffects(float DeltaTime)
{
	if (FlashWhite > 0.f)
	{
		const float Rate = FlashWhite > 0.9f ? 0.18f : 0.48f;
		FlashWhite = FMath::Max(0.f, FlashWhite - DeltaTime * Rate);
	}
	if (FlashGhost > 0.f)
	{
		FlashGhost = FMath::Max(0.f, FlashGhost - DeltaTime * 0.32f);
	}
	HitBloodFlash = FMath::Max(0.f, HitBloodFlash - DeltaTime * 3.4f);
	HitBloodTail = FMath::Max(0.f, HitBloodTail - DeltaTime * 0.5f);
	ApplyFlashGhostPostProcess();
}

void AShooterCharacter::ApplyFlashGhostPostProcess()
{
	if (!IsLocallyControlled() || !IsValid(FirstPersonCamera)) return;

	FPostProcessSettings& PPS = FirstPersonCamera->PostProcessSettings;
	if (FlashGhost > 0.02f)
	{
		PPS.bOverride_SceneFringeIntensity = true;
		PPS.SceneFringeIntensity = FlashGhost * 6.5f;
		PPS.bOverride_ChromaticAberrationStartOffset = true;
		PPS.ChromaticAberrationStartOffset = FMath::Lerp(0.35f, 0.f, FlashGhost);
		if (TacticalVisionMode == ETacticalVisionMode::Off)
		{
			PPS.bOverride_BloomIntensity = true;
			PPS.BloomIntensity = 0.35f + FlashGhost * 1.4f;
			PPS.bOverride_ColorContrast = true;
			const float Contrast = 1.f - FlashGhost * 0.22f;
			PPS.ColorContrast = FVector4(Contrast, Contrast, Contrast, 1.f);
		}
		bGhostPostProcess = true;
		return;
	}

	if (!bGhostPostProcess) return;
	PPS.bOverride_SceneFringeIntensity = false;
	PPS.SceneFringeIntensity = 0.f;
	PPS.bOverride_ChromaticAberrationStartOffset = false;
	if (TacticalVisionMode == ETacticalVisionMode::Off)
	{
		PPS.bOverride_BloomIntensity = false;
		PPS.bOverride_ColorContrast = false;
	}
	bGhostPostProcess = false;
}

void AShooterCharacter::CycleTacticalVisionMode()
{
	const uint8 Next = (static_cast<uint8>(TacticalVisionMode) + 1) % 3;
	SetTacticalVisionMode(static_cast<ETacticalVisionMode>(Next));
}

void AShooterCharacter::ToggleNVG()
{
	if (!IsLocallyControlled())
	{
		return;
	}
	// Alt+K: NVG on/off only (Thermal stays console CycleVision / Vision 2).
	if (TacticalVisionMode == ETacticalVisionMode::NVG)
	{
		SetTacticalVisionMode(ETacticalVisionMode::Off);
	}
	else
	{
		SetTacticalVisionMode(ETacticalVisionMode::NVG);
	}
}


void AShooterCharacter::SetTacticalVisionMode(ETacticalVisionMode NewMode)
{
	if (!IsLocallyControlled())
	{
		return;
	}
	if (NewMode != ETacticalVisionMode::Off && VisionBatterySeconds <= 0.f)
	{
		NewMode = ETacticalVisionMode::Off;
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(99201, 2.f, FColor::Orange, TEXT("Vision battery empty"));
		}
	}
	if (TacticalVisionMode == NewMode)
	{
		ApplyTacticalVisionEffects();
		return;
	}
	TacticalVisionMode = NewMode;
	ApplyTacticalVisionEffects();
	if (GEngine)
	{
		const TCHAR* Name =
			TacticalVisionMode == ETacticalVisionMode::NVG ? TEXT("NVG") :
			TacticalVisionMode == ETacticalVisionMode::Thermal ? TEXT("Thermal") : TEXT("Off");
		GEngine->AddOnScreenDebugMessage(99202, 2.5f, FColor::Cyan,
			FString::Printf(TEXT("Vision -> %s (battery %.0f%%)"), Name, GetVisionBatteryNormalized() * 100.f));
	}
	UE_LOG(LogTemp, Log, TEXT("[Vision] mode=%d battery=%.1f"), static_cast<int32>(TacticalVisionMode), VisionBatterySeconds);
}

void AShooterCharacter::ClearThermalOverlays()
{
	for (const TWeakObjectPtr<USkeletalMeshComponent>& WeakMesh : ThermalOverlayMeshes)
	{
		if (USkeletalMeshComponent* MeshComp = WeakMesh.Get())
		{
			MeshComp->SetOverlayMaterial(nullptr);
		}
	}
	ThermalOverlayMeshes.Reset();
}

void AShooterCharacter::RefreshThermalOverlays()
{
	ClearThermalOverlays();
	if (TacticalVisionMode != ETacticalVisionMode::Thermal || !IsLocallyControlled() || !GetWorld())
	{
		return;
	}

	if (!CachedThermalOverlayMat)
	{
		CachedThermalOverlayMat = LoadSoftMaterial(ThermalBodyOverlayMaterial);
		if (!CachedThermalOverlayMat)
		{
			CachedThermalOverlayMat = LoadObject<UMaterialInterface>(nullptr,
				TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
		}
	}
	if (!CachedThermalOverlayMat)
	{
		return;
	}

	const FVector MyLoc = GetActorLocation();
	const float RangeSq = FMath::Square(ThermalOverlayRangeCm);
	for (TActorIterator<AShooterCharacter> It(GetWorld()); It; ++It)
	{
		AShooterCharacter* Other = *It;
		if (!IsValid(Other) || Other == this)
		{
			continue;
		}
		if (FVector::DistSquared(MyLoc, Other->GetActorLocation()) > RangeSq)
		{
			continue;
		}
		auto Mark = [this](USkeletalMeshComponent* Comp)
		{
			if (!IsValid(Comp)) return;
			Comp->SetRenderCustomDepth(true);
			Comp->SetCustomDepthStencilValue(1);
			Comp->SetOverlayMaterial(CachedThermalOverlayMat);
			ThermalOverlayMeshes.Add(Comp);
		};
		Mark(Other->GetMesh());
		// Modular police parts (if present)
		Mark(Other->Mesh3P_Head);
		Mark(Other->Mesh3P_BodyUp);
		Mark(Other->Mesh3P_Pants);
		Mark(Other->Mesh3P_Jacket);
	}

	// Range dummies also light up under thermal (handy in PIE without a second pawn).
	for (TActorIterator<ATargetDummy> DummyIt(GetWorld()); DummyIt; ++DummyIt)
	{
		ATargetDummy* Dummy = *DummyIt;
		if (!IsValid(Dummy) || !IsValid(Dummy->Mesh))
		{
			continue;
		}
		if (FVector::DistSquared(MyLoc, Dummy->GetActorLocation()) > RangeSq)
		{
			continue;
		}
		Dummy->Mesh->SetRenderCustomDepth(true);
		Dummy->Mesh->SetCustomDepthStencilValue(1);
		Dummy->Mesh->SetOverlayMaterial(CachedThermalOverlayMat);
		ThermalOverlayMeshes.Add(Dummy->Mesh);
	}
}

void AShooterCharacter::ClearTacticalVisionEffects()
{
	ClearThermalOverlays();
	if (!IsValid(FirstPersonCamera))
	{
		return;
	}
	if (ActiveVisionBlendable.IsValid())
	{
		FirstPersonCamera->RemoveBlendable(ActiveVisionBlendable.Get());
		ActiveVisionBlendable.Reset();
	}
	FPostProcessSettings& PPS = FirstPersonCamera->PostProcessSettings;
	PPS.bOverride_SceneColorTint = false;
	PPS.bOverride_ColorSaturation = false;
	PPS.bOverride_ColorContrast = false;
	PPS.bOverride_ColorGamma = false;
	PPS.bOverride_AutoExposureBias = false;
	PPS.bOverride_BloomIntensity = false;
	PPS.bOverride_VignetteIntensity = false;
	PPS.bOverride_FilmGrainIntensity = false;
	// NVGFix: clear exposure-range / shadow-lift overrides
	PPS.bOverride_AutoExposureMethod = false;
	PPS.bOverride_AutoExposureMinBrightness = false;
	PPS.bOverride_AutoExposureMaxBrightness = false;
	PPS.bOverride_ColorGammaShadows = false;
	PPS.bOverride_ColorGainShadows = false;
	PPS.bOverride_ColorOffset = false;
	FirstPersonCamera->PostProcessBlendWeight = 1.f;
	AppliedVisionMode = ETacticalVisionMode::Off;
}

void AShooterCharacter::ApplyTacticalVisionEffects()
{
	if (!IsLocallyControlled() || !IsValid(FirstPersonCamera))
	{
		return;
	}

	const bool bModeChanged = (AppliedVisionMode != TacticalVisionMode);
	if (bModeChanged)
	{
		ClearTacticalVisionEffects();
		AppliedVisionMode = TacticalVisionMode;
	}

	if (TacticalVisionMode == ETacticalVisionMode::Off)
	{
		return;
	}

	FPostProcessSettings& PPS = FirstPersonCamera->PostProcessSettings;
	const float Wash = VisionFlashWash;
	const float Batt = GetVisionBatteryNormalized();

	if (TacticalVisionMode == ETacticalVisionMode::NVG)
	{
		// NVGTube: classic tube look - grayscale -> green phosphor, dark vignette edges.
		PPS.bOverride_ColorSaturation = true;
		PPS.ColorSaturation = FVector4(0.05f, 0.05f, 0.05f, 1.f);
		PPS.bOverride_SceneColorTint = true;
		PPS.SceneColorTint = FLinearColor(0.35f, 1.0f, 0.40f);
		PPS.bOverride_ColorContrast = true;
		PPS.ColorContrast = FVector4(1.12f, 1.12f, 1.12f, 1.f);
		PPS.bOverride_AutoExposureMethod = true;
		PPS.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		PPS.bOverride_AutoExposureMinBrightness = true;
		PPS.AutoExposureMinBrightness = -1.5f;
		PPS.bOverride_AutoExposureMaxBrightness = true;
		PPS.AutoExposureMaxBrightness = 1.5f;
		PPS.bOverride_AutoExposureBias = true;
		PPS.AutoExposureBias = (1.3f + 0.4f * Batt) + Wash * 2.f;
		PPS.bOverride_ColorGammaShadows = true;
		PPS.ColorGammaShadows = FVector4(1.2f, 1.2f, 1.2f, 1.f);
		PPS.bOverride_ColorGainShadows = true;
		PPS.ColorGainShadows = FVector4(1.35f, 1.35f, 1.35f, 1.f);
		PPS.bOverride_ColorOffset = true;
		PPS.ColorOffset = FVector4(0.0f, 0.004f, 0.0f, 0.f);
		PPS.bOverride_BloomIntensity = true;
		PPS.BloomIntensity = 1.2f + Wash * 8.f;
		PPS.bOverride_VignetteIntensity = true;
		PPS.VignetteIntensity = 0.95f;
		PPS.bOverride_FilmGrainIntensity = true;
		PPS.FilmGrainIntensity = 0.3f + (1.f - Batt) * 0.2f;

		if (bModeChanged)
		{
			if (UMaterialInterface* NVGMat = LoadSoftMaterial(NVGPostProcessMaterial))
			{
				FirstPersonCamera->AddOrUpdateBlendable(NVGMat, 1.f);
				ActiveVisionBlendable = NVGMat;
			}
		}
	}
	else if (TacticalVisionMode == ETacticalVisionMode::Thermal)
	{
		PPS.bOverride_SceneColorTint = true;
		PPS.SceneColorTint = FLinearColor(0.12f, 0.22f, 0.55f);
		PPS.bOverride_ColorSaturation = true;
		PPS.ColorSaturation = FVector4(0.15f, 0.2f, 0.35f, 1.f);
		PPS.bOverride_ColorContrast = true;
		PPS.ColorContrast = FVector4(1.55f, 1.45f, 1.35f, 1.f);
		PPS.bOverride_ColorGamma = true;
		PPS.ColorGamma = FVector4(0.85f, 0.9f, 1.1f, 1.f);
		PPS.bOverride_AutoExposureBias = true;
		PPS.AutoExposureBias = 0.35f + Wash * 3.f;
		PPS.bOverride_BloomIntensity = true;
		PPS.BloomIntensity = 0.6f + Wash * 4.f;
		PPS.bOverride_VignetteIntensity = true;
		PPS.VignetteIntensity = 0.4f;
		PPS.bOverride_FilmGrainIntensity = true;
		PPS.FilmGrainIntensity = 0.2f;

		if (bModeChanged)
		{
			UMaterialInterface* ThermalMat = LoadSoftMaterial(ThermalPostProcessMaterial);
			if (!ThermalMat)
			{
				ThermalMat = LoadObject<UMaterialInterface>(nullptr,
					TEXT("/Engine/BufferVisualization/CustomDepth.CustomDepth"));
			}
			if (ThermalMat)
			{
				FirstPersonCamera->AddOrUpdateBlendable(ThermalMat, 0.85f);
				ActiveVisionBlendable = ThermalMat;
			}
			RefreshThermalOverlays();
		}
	}
}

void AShooterCharacter::UpdateTacticalVision(float DeltaTime)
{
	if (!IsLocallyControlled())
	{
		return;
	}

	if (VisionFlashWash > 0.f)
	{
		VisionFlashWash = FMath::Max(0.f, VisionFlashWash - DeltaTime * 1.25f);
	}

	if (TacticalVisionMode != ETacticalVisionMode::Off)
	{
		VisionBatterySeconds = FMath::Max(0.f, VisionBatterySeconds - VisionBatteryDrainPerSecond * DeltaTime);
		if (VisionBatterySeconds <= 0.f)
		{
			TacticalVisionMode = ETacticalVisionMode::Off;
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(99201, 2.5f, FColor::Orange, TEXT("Vision battery depleted -> Off"));
			}
		}
	}

	ApplyTacticalVisionEffects();

	if (TacticalVisionMode == ETacticalVisionMode::Thermal)
	{
		VisionOverlayRefreshAcc += DeltaTime;
		if (VisionOverlayRefreshAcc >= 0.35f)
		{
			VisionOverlayRefreshAcc = 0.f;
			RefreshThermalOverlays();
		}
	}
	else
	{
		VisionOverlayRefreshAcc = 0.f;
	}

	UpdateScreenEffects(DeltaTime);
}


// ===== Radar / Watch / Blood polish =====

void AShooterCharacter::SetHasRadar(bool bEnable)
{
	if (HasAuthority())
	{
		bHasRadar = bEnable;
		OnRep_HasRadar();
	}
	else
	{
		Server_SetHasRadar(bEnable);
	}
}

void AShooterCharacter::Server_SetHasRadar_Implementation(bool bEnable)
{
	bHasRadar = bEnable;
	OnRep_HasRadar();
}

void AShooterCharacter::OnRep_HasRadar()
{
	UE_LOG(LogTemp, Log, TEXT("[Radar] %s HasRadar=%s"), *GetNameSafe(this), bHasRadar ? TEXT("1") : TEXT("0"));
}

void AShooterCharacter::SetDebugWeaponObstacle(bool bEnable)
{
	bDebugWeaponObstacle = bEnable;
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow,
			FString::Printf(TEXT("DebugBarrel -> %s"), bEnable ? TEXT("ON") : TEXT("OFF")));
	}
}

void AShooterCharacter::CheckWatch()
{
	if (!IsLocallyControlled()) return;

	UAnimMontage* Montage = nullptr;
	if (!WatchCheckMontage.IsNull())
	{
		Montage = WatchCheckMontage.LoadSynchronous();
	}
	if (!Montage)
	{
		Montage = LoadObject<UAnimMontage>(nullptr,
			TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Actions/AM_RAR_FP_PCH_AssaultRifle_WatchCheck.AM_RAR_FP_PCH_AssaultRifle_WatchCheck"));
	}
	if (IsValid(Montage) && IsValid(Mesh1P))
	{
		if (UAnimInstance* Anim = Mesh1P->GetAnimInstance())
		{
			Anim->Montage_Play(Montage, 1.f);
		}
	}

	float Remaining = 0.f;
	FString PhaseNote;
	if (UWorld* World = GetWorld())
	{
		if (AShooterGameState* GS = World->GetGameState<AShooterGameState>())
		{
			Remaining = GS->GetRemainingTime();
		}
	}

	const int32 TotalSec = FMath::Max(0, FMath::CeilToInt(Remaining));
	const int32 Mins = TotalSec / 60;
	const int32 Secs = TotalSec % 60;
	const FString Msg = (Remaining <= 0.f)
		? TEXT("手表：无倒计时（非对局或未开始）")
		: FString::Printf(TEXT("手表：剩余 %d:%02d"), Mins, Secs);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(88221001, WatchTimeDisplaySeconds, FColor::Cyan, Msg);
	}
	UE_LOG(LogTemp, Log, TEXT("[Watch] %s"), *Msg);
}

void AShooterCharacter::Multicast_NotifyBloodyFeet_Implementation()
{
	MarkBloodyFeet();
}



void AShooterCharacter::PlayBloodHitFX(FVector ImpactPoint, FVector ImpactNormal)
{
	SpawnBloodImpactFX(this, ImpactPoint, ImpactNormal, this);
}

void AShooterCharacter::SpawnBloodImpactFX(UObject* WorldContext, FVector ImpactPoint, FVector ImpactNormal, AActor* IgnoreActor)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Resolve optional soft refs from the hit character if present.
	UNiagaraSystem* Niagara = nullptr;
	UMaterialInterface* DecalMat = nullptr;
	float DecalSize = 48.f;
	float DecalLife = 22.f;
	if (const AShooterCharacter* AsChar = Cast<AShooterCharacter>(IgnoreActor))
	{
		if (!AsChar->BloodHitNiagara.IsNull())
		{
			Niagara = AsChar->BloodHitNiagara.LoadSynchronous();
		}
		if (!AsChar->BloodHitDecal.IsNull())
		{
			DecalMat = AsChar->BloodHitDecal.LoadSynchronous();
		}
		DecalSize = AsChar->BloodHitDecalSize;
		DecalLife = AsChar->BloodHitDecalLifeSpan;
	}
	if (!Niagara)
	{
		Niagara = LoadObject<UNiagaraSystem>(nullptr,
			TEXT("/Game/DEFINITIVE_FPS_KIT/Niagara_VFX/Niagara/Impact/Impact_Blood.Impact_Blood"));
	}
	if (!Niagara)
	{
		Niagara = LoadObject<UNiagaraSystem>(nullptr,
			TEXT("/Game/DEFINITIVE_FPS_KIT/Niagara_VFX/Niagara/Blood/Blood_Splat.Blood_Splat"));
	}
	if (!DecalMat)
	{
		DecalMat = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/DEFINITIVE_FPS_KIT/NPC/Zombies/Decal_Particles/M_blood_decal_Mat.M_blood_decal_Mat"));
	}

	const FVector N = ImpactNormal.GetSafeNormal().IsNearlyZero() ? FVector::UpVector : ImpactNormal.GetSafeNormal();

	if (Niagara)
	{
		const FRotator SprayRot = FRotationMatrix::MakeFromX(N).Rotator();
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, Niagara, ImpactPoint + N * 2.f, SprayRot, FVector(1.f), true, true, ENCPoolMethod::AutoRelease);
	}

	if (!DecalMat)
	{
		return;
	}

	auto SpawnBloodDecal = [&](const FVector& Loc, const FVector& Normal, float SizeScale)
	{
		const FVector Nn = Normal.GetSafeNormal().IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
		const FRotator DecalRot = FRotationMatrix::MakeFromZX(Nn, FVector::ForwardVector).Rotator();
		const float S = DecalSize * SizeScale;
		UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
			World, DecalMat, FVector(S * 0.35f, S, S), Loc + Nn * 1.5f, DecalRot, DecalLife);
		if (IsValid(Decal))
		{
			Decal->SetFadeScreenSize(0.001f);
			Decal->SetFadeOut(DecalLife * 0.55f, DecalLife * 0.45f, false);
		}
	};

	// Small splat on the hit surface (body / wall behind).
	SpawnBloodDecal(ImpactPoint, N, 0.55f);

	// Larger stain on the floor under the hit.
	FHitResult FloorHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BloodHitFloor), false, IgnoreActor);
	const FVector FloorStart = ImpactPoint + FVector(0.f, 0.f, 8.f);
	const FVector FloorEnd = ImpactPoint - FVector(0.f, 0.f, 280.f);
	bool bFloor = World->LineTraceSingleByChannel(FloorHit, FloorStart, FloorEnd, ECC_Visibility, Params);
	if (!bFloor)
	{
		FCollisionObjectQueryParams Obj;
		Obj.AddObjectTypesToQuery(ECC_WorldStatic);
		Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
		bFloor = World->LineTraceSingleByObjectType(FloorHit, FloorStart, FloorEnd, Obj, Params);
	}
	if (bFloor)
	{
		SpawnBloodDecal(FloorHit.ImpactPoint, FloorHit.ImpactNormal, 1.f);
	}
}
void AShooterCharacter::MarkBloodyFeet(int32 ExtraSteps)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	const int32 Add = ExtraSteps > 0 ? ExtraSteps : BloodyFootstepsOnHit;
	BloodyFootstepsRemaining = FMath::Max(BloodyFootstepsRemaining, Add);
}

void AShooterCharacter::SpawnBloodFootprint()
{
	UWorld* World = GetWorld();
	if (!World || GetNetMode() == NM_DedicatedServer) return;

	UMaterialInterface* Mat = nullptr;
	if (!BloodFootprintDecal.IsNull())
	{
		Mat = BloodFootprintDecal.LoadSynchronous();
	}
	if (!Mat)
	{
		Mat = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/DEFINITIVE_FPS_KIT/NPC/Zombies/Decal_Particles/M_blood_decal_Mat.M_blood_decal_Mat"));
	}
	if (!Mat) return;

	const UCapsuleComponent* Cap = GetCapsuleComponent();
	const float Half = IsValid(Cap) ? Cap->GetScaledCapsuleHalfHeight() : 90.f;
	const FVector Start = GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, Half + 60.f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BloodFootprint), false, this);
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		// Floor often blocks WorldStatic better than Visibility.
		FCollisionObjectQueryParams Obj;
		Obj.AddObjectTypesToQuery(ECC_WorldStatic);
		Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
		if (!World->LineTraceSingleByObjectType(Hit, Start, End, Obj, Params))
		{
			return;
		}
	}

	const float Side = bFootstepLeft ? -10.f : 10.f;
	const FVector Right = GetActorRightVector();
	const FVector Loc = Hit.ImpactPoint + Hit.ImpactNormal * 1.5f + Right * Side;

	const FRotator DecalRot = FRotationMatrix::MakeFromZX(Hit.ImpactNormal, GetActorForwardVector()).Rotator();
	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
		World, Mat, FVector(BloodFootprintSize * 0.35f, BloodFootprintSize, BloodFootprintSize),
		Loc, DecalRot, BloodFootprintLifeSpan);
	if (IsValid(Decal))
	{
		Decal->SetFadeScreenSize(0.001f);
		Decal->SetFadeOut(BloodFootprintLifeSpan * 0.55f, BloodFootprintLifeSpan * 0.45f, false);
	}
}


namespace
{
	// Session-only grip copies for the Bandits faction meshes.
	// An authored socket keeps its parent bone, so RAR SOCKET_Weapon stays on ik_hand_gun
	// and follows the arm anim. A mesh socket is removed when the target has that bone,
	// so it cannot freeze an animated bone (weapon_r). Orphan bones (Simple weapon_r,
	// parent none) are not captured. SOCKET_Weapon is never aliased from weapon_r.
	struct FGripSocketCopy
	{
		FName Name;
		FName BoneName;
		FVector RelLoc = FVector::ZeroVector;
		FRotator RelRot = FRotator::ZeroRotator;
		FVector RelScale = FVector::OneVector;
		bool bWrite = false;
		bool bUseBone = false;
		const TCHAR* Kind = TEXT("authored");
	};

	static const FName GGripSocketNames[] = {
		TEXT("weapon_r"), TEXT("WeaponSocket"), TEXT("weapon_mount"), TEXT("SOCKET_Weapon"),
		TEXT("RifleSocket"), TEXT("RifleSocketak"), TEXT("RifleSocketMCX"), TEXT("RifleSocketPCC"),
		TEXT("PistolSocket"), TEXT("PistolSocketDeagle"), TEXT("PistolSocketX24"),
		TEXT("GrenadeSocket"), TEXT("HandGrip_R")
	};

	USkeletalMeshSocket* FindSourceGripSocket(USkeletalMesh* Mesh, FName Name)
	{
		if (!IsValid(Mesh) || Name.IsNone())
		{
			return nullptr;
		}
		// Same lookup the gun uses: a mesh socket overrides a skeleton socket.
		return Mesh->FindSocket(Name);
	}

	int32 RemoveMeshOnlyGripSocket(USkeletalMesh* Mesh, FName Name)
	{
		if (!IsValid(Mesh) || Name.IsNone())
		{
			return 0;
		}
		TArray<TObjectPtr<USkeletalMeshSocket>>& Sockets = Mesh->GetMeshOnlySocketList();
		return Sockets.RemoveAll([Mesh, Name](const TObjectPtr<USkeletalMeshSocket>& Socket)
		{
			return IsValid(Socket) && Socket->SocketName == Name && Socket->GetOuter() == Mesh;
		});
	}

	bool IsUsableSourceBone(const FReferenceSkeleton& Ref, int32 BoneIndex, FName& OutParent, FTransform& OutLocal)
	{
		if (!Ref.IsValidIndex(BoneIndex) || BoneIndex == 0)
		{
			return false;
		}
		const FMeshBoneInfo& Info = Ref.GetRefBoneInfo()[BoneIndex];
		if (!Ref.IsValidIndex(Info.ParentIndex))
		{
			return false;
		}
		OutParent = Ref.GetBoneName(Info.ParentIndex);
		OutLocal = Ref.GetRefBonePose()[BoneIndex];
		return !OutParent.IsNone();
	}

	FGripSocketCopy* FindGripCopy(TArray<FGripSocketCopy>& Copies, FName Name)
	{
		return Copies.FindByPredicate([Name](const FGripSocketCopy& Entry) { return Entry.Name == Name; });
	}

	void CaptureGripSockets(
		USkeletalMesh* Target,
		USkeletalMesh* Source,
		USkeletalMeshComponent* LiveComp,
		bool bBakeMissingBones,
		TArray<FGripSocketCopy>& OutCopies)
	{
		if (!IsValid(Target) || !IsValid(Source) || Target == Source)
		{
			return;
		}

		const FReferenceSkeleton& SourceRef = Source->GetRefSkeleton();
		const FReferenceSkeleton& TargetRef = Target->GetRefSkeleton();

		const bool bLiveReady = bBakeMissingBones
			&& IsValid(LiveComp)
			&& LiveComp->IsRegistered()
			&& LiveComp->GetSkeletalMeshAsset() == Source;
		TArray<FTransform> LivePose;
		if (bLiveReady)
		{
			LiveComp->RefreshBoneTransforms();
			LivePose = LiveComp->GetBoneSpaceTransforms();
		}

		for (const FName Name : GGripSocketNames)
		{
			if (FGripSocketCopy* Existing = FindGripCopy(OutCopies, Name))
			{
				if (Existing->bWrite || Existing->bUseBone)
				{
					continue;
				}
			}

			FGripSocketCopy Entry;
			Entry.Name = Name;

			if (USkeletalMeshSocket* SrcSocket = FindSourceGripSocket(Source, Name))
			{
				const FName ParentBone = SrcSocket->BoneName;
				if (!ParentBone.IsNone() && TargetRef.FindBoneIndex(ParentBone) != INDEX_NONE)
				{
					const int32 TargetBone = TargetRef.FindBoneIndex(Name);
					FName IgnoredParent;
					FTransform IgnoredLocal;
					if (TargetBone != INDEX_NONE && IsUsableSourceBone(TargetRef, TargetBone, IgnoredParent, IgnoredLocal))
					{
						Entry.bUseBone = true;
					}
					else
					{
						Entry.bWrite = true;
						Entry.BoneName = ParentBone;
						Entry.RelLoc = SrcSocket->RelativeLocation;
						Entry.RelRot = SrcSocket->RelativeRotation;
						Entry.RelScale = SrcSocket->RelativeScale.IsNearlyZero() ? FVector::OneVector : SrcSocket->RelativeScale;
						Entry.Kind = TEXT("authored");
					}
				}
			}

			if (!Entry.bWrite && !Entry.bUseBone && bBakeMissingBones)
			{
				const int32 SourceBone = SourceRef.FindBoneIndex(Name);
				FName ParentBone;
				FTransform Local = FTransform::Identity;
				if (IsUsableSourceBone(SourceRef, SourceBone, ParentBone, Local)
					&& TargetRef.FindBoneIndex(ParentBone) != INDEX_NONE)
				{
					const int32 TargetBone = TargetRef.FindBoneIndex(Name);
					FName TargetParent;
					FTransform TargetLocal;
					if (TargetBone != INDEX_NONE && IsUsableSourceBone(TargetRef, TargetBone, TargetParent, TargetLocal))
					{
						Entry.bUseBone = true;
					}
					else
					{
						const int32 CompBone = bLiveReady ? LiveComp->GetBoneIndex(Name) : INDEX_NONE;
						if (bLiveReady && CompBone == SourceBone && LivePose.IsValidIndex(CompBone))
						{
							const FTransform& LiveLocal = LivePose[CompBone];
							const float LocDelta = FVector::Dist(LiveLocal.GetLocation(), Local.GetLocation());
							const float RotDeltaDeg = FMath::RadiansToDegrees(
								LiveLocal.GetRotation().AngularDistance(Local.GetRotation()));
							if (LocDelta > 0.5f || RotDeltaDeg > 2.f)
							{
								Local = LiveLocal;
								Entry.Kind = TEXT("live");
							}
							else
							{
								Entry.Kind = TEXT("ref");
							}
						}
						else
						{
							Entry.Kind = TEXT("ref");
						}

						Entry.bWrite = true;
						Entry.BoneName = ParentBone;
						Entry.RelLoc = Local.GetLocation();
						Entry.RelRot = Local.Rotator();
						Entry.RelScale = Local.GetScale3D().IsNearlyZero() ? FVector::OneVector : Local.GetScale3D();
					}
				}
			}

			if (!Entry.bWrite && !Entry.bUseBone)
			{
				continue;
			}

			if (FGripSocketCopy* Existing = FindGripCopy(OutCopies, Name))
			{
				*Existing = Entry;
			}
			else
			{
				OutCopies.Add(Entry);
			}
		}
	}

	void AliasSocketWeapon(TArray<FGripSocketCopy>& Copies)
	{
		if (FGripSocketCopy* Existing = FindGripCopy(Copies, TEXT("SOCKET_Weapon")))
		{
			if (Existing->bWrite || Existing->bUseBone)
			{
				return;
			}
		}

		// RAR 3P on Manny has no SOCKET_Weapon and falls back to RifleSocketak.
		// Never take weapon_r: on Simple that bone is an orphan at the mesh root.
		static const FName Sources[] = {
			TEXT("RifleSocketak"), TEXT("RifleSocket"), TEXT("WeaponSocket"), TEXT("weapon_mount")
		};
		for (const FName SourceName : Sources)
		{
			FGripSocketCopy* Src = FindGripCopy(Copies, SourceName);
			if (!Src || !Src->bWrite)
			{
				continue;
			}
			FGripSocketCopy Alias = *Src;
			Alias.Name = TEXT("SOCKET_Weapon");
			Alias.Kind = TEXT("alias");
			if (FGripSocketCopy* Existing = FindGripCopy(Copies, TEXT("SOCKET_Weapon")))
			{
				*Existing = Alias;
			}
			else
			{
				Copies.Add(Alias);
			}
			return;
		}
	}

	void ApplyGripSocketCopies(USkeletalMesh* Target, const TArray<FGripSocketCopy>& Copies, const TCHAR* Label, const TCHAR* SourceName)
	{
		if (!IsValid(Target))
		{
			return;
		}

		UPackage* Package = Target->GetOutermost();
		const bool bDirtyBefore = Package && Package->IsDirty();

		int32 Written = 0;
		int32 Dropped = 0;
		for (const FGripSocketCopy& Entry : Copies)
		{
			if (Entry.bUseBone)
			{
				const int32 Removed = RemoveMeshOnlyGripSocket(Target, Entry.Name);
				if (Removed > 0)
				{
					Dropped += Removed;
					UE_LOG(LogTemp, Log, TEXT("[Look] socket %s drop mesh override on %s (use bone)"),
						*Entry.Name.ToString(), *GetNameSafe(Target));
				}
				continue;
			}
			if (!Entry.bWrite || Entry.BoneName.IsNone())
			{
				continue;
			}
			if (Target->GetRefSkeleton().FindBoneIndex(Entry.BoneName) == INDEX_NONE)
			{
				continue;
			}

			TArray<TObjectPtr<USkeletalMeshSocket>>& Sockets = Target->GetMeshOnlySocketList();
			USkeletalMeshSocket* Keep = nullptr;
			for (int32 Index = Sockets.Num() - 1; Index >= 0; --Index)
			{
				USkeletalMeshSocket* Socket = Sockets[Index];
				if (!IsValid(Socket) || Socket->SocketName != Entry.Name || Socket->GetOuter() != Target)
				{
					continue;
				}
				if (!Keep)
				{
					Keep = Socket;
				}
				else
				{
					Sockets.RemoveAt(Index);
				}
			}
			if (!Keep)
			{
				Keep = NewObject<USkeletalMeshSocket>(Target);
				Keep->SocketName = Entry.Name;
				Sockets.Add(Keep);
			}
			Keep->BoneName = Entry.BoneName;
			Keep->RelativeLocation = Entry.RelLoc;
			Keep->RelativeRotation = Entry.RelRot;
			Keep->RelativeScale = Entry.RelScale.IsNearlyZero() ? FVector::OneVector : Entry.RelScale;
			++Written;
			UE_LOG(LogTemp, Log, TEXT("[Look] socket %s -> %s loc=(%.2f,%.2f,%.2f) rot=(p%.1f,y%.1f,r%.1f) %s on %s"),
				*Entry.Name.ToString(), *Entry.BoneName.ToString(),
				Entry.RelLoc.X, Entry.RelLoc.Y, Entry.RelLoc.Z,
				Entry.RelRot.Pitch, Entry.RelRot.Yaw, Entry.RelRot.Roll,
				Entry.Kind, *GetNameSafe(Target));
		}

		// A stale runtime weapon_r / SOCKET_Weapon must not beat RifleSocketak in ResolveAttachPoint.
		auto DropIfUnwritten = [&](FName Name)
		{
			const FGripSocketCopy* Entry = nullptr;
			for (const FGripSocketCopy& Copy : Copies)
			{
				if (Copy.Name == Name)
				{
					Entry = &Copy;
					break;
				}
			}
			if (Entry && (Entry->bWrite || Entry->bUseBone))
			{
				return;
			}
			const int32 Removed = RemoveMeshOnlyGripSocket(Target, Name);
			if (Removed > 0)
			{
				Dropped += Removed;
				UE_LOG(LogTemp, Log, TEXT("[Look] socket %s drop stale mesh socket on %s"),
					*Name.ToString(), *GetNameSafe(Target));
			}
		};
		DropIfUnwritten(TEXT("weapon_r"));
		DropIfUnwritten(TEXT("SOCKET_Weapon"));

		Target->RebuildSocketMap();

		if (Package && Package->IsDirty() && !bDirtyBefore)
		{
			Package->ClearDirtyFlag();
		}

		UE_LOG(LogTemp, Log, TEXT("[Look] %s sockets upsert=%d drop=%d on %s (src=%s)"),
			Label, Written, Dropped, *GetNameSafe(Target), SourceName);
	}
}

void AShooterCharacter::EnsureMesh3PGripSockets(USkeletalMesh* BodyMesh, USkeletalMesh* SourceMesh, USkeletalMeshComponent* LiveSourceComp)
{
	if (!IsValid(BodyMesh))
	{
		return;
	}

	USkeletalMesh* MannyMesh = SourceMesh;
	const bool bSourceOk = IsValid(MannyMesh)
		&& MannyMesh != BodyMesh
		&& !IsBanditsSkelMesh(MannyMesh);
	if (!bSourceOk)
	{
		MannyMesh = LoadCanonicalMesh3PSource();
	}

	auto HasRifleSocketAk = [](USkeletalMesh* GripMesh) -> bool
	{
		return FindSourceGripSocket(GripMesh, TEXT("RifleSocketak")) != nullptr;
	};
	if (!HasRifleSocketAk(MannyMesh))
	{
		static const TCHAR* MannyPaths[] = {
			TEXT("/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"),
			TEXT("/Game/FPS_Controller/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"),
			TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny"),
			TEXT("/Game/FPS_Controller/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny"),
			TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"),
		};
		for (const TCHAR* Path : MannyPaths)
		{
			if (USkeletalMesh* Loaded = LoadObject<USkeletalMesh>(nullptr, Path))
			{
				if (HasRifleSocketAk(Loaded))
				{
					MannyMesh = Loaded;
					break;
				}
				if (!IsValid(MannyMesh))
				{
					MannyMesh = Loaded;
				}
			}
		}
	}

	TArray<FGripSocketCopy> Copies;
	// 3P does not bake weapon_r. RAR uses the RifleSocketak alias; AK requests RifleSocketak.
	CaptureGripSockets(BodyMesh, MannyMesh, LiveSourceComp, false, Copies);
	AliasSocketWeapon(Copies);
	ApplyGripSocketCopies(BodyMesh, Copies, TEXT("Mesh3P"), *GetNameSafe(MannyMesh));
}

void AShooterCharacter::EnsureArms1PGripSockets(USkeletalMesh* ArmsMesh, USkeletalMesh* SourceMesh)
{
	if (!IsValid(Mesh1P) || !IsValid(ArmsMesh))
	{
		return;
	}

	USkeletalMesh* ArmsSource = SourceMesh;
	const bool bSourceOk = IsValid(ArmsSource)
		&& ArmsSource != ArmsMesh
		&& !IsBanditsSkelMesh(ArmsSource);
	if (!bSourceOk)
	{
		ArmsSource = LoadCanonicalArms1PSource();
	}

	TArray<FGripSocketCopy> Copies;
	// Old Mesh1P is still the mannequin arms here. Bake weapon_r only when the new mesh lacks that bone.
	CaptureGripSockets(ArmsMesh, ArmsSource, Mesh1P, true, Copies);
	if (FindGripCopy(Copies, TEXT("SOCKET_Weapon")) == nullptr)
	{
		if (USkeletalMesh* MannequinArms = LoadObject<USkeletalMesh>(nullptr,
			TEXT("/Game/FirstPersonRifleAnimations/Demo/FirstPersonArms/Character/Mesh/SK_Mannequin_Arms.SK_Mannequin_Arms")))
		{
			if (MannequinArms != ArmsSource)
			{
				CaptureGripSockets(ArmsMesh, MannequinArms, Mesh1P, true, Copies);
			}
		}
	}
	AliasSocketWeapon(Copies);
	ApplyGripSocketCopies(ArmsMesh, Copies, TEXT("Arms1P"), *GetNameSafe(ArmsSource));
	EnsureAnimatedGunAttachSockets(ArmsMesh);
}

void AShooterCharacter::EnsureAnimatedGunAttachSockets(USkeletalMesh* ArmsMesh)
{
	// NEVER promote ik_hand_gun (or VB pivots) to a same-named mesh socket.
	// AttachToComponent prefers sockets over bones; a socket parented to ik_hand_root
	// freezes the gun at rest pose while the animated bone (and RAR SOCKET_Weapon on
	// that bone) keep moving — KINEMATION guns then look untunable.
	// Strip any mesh-only shadow sockets left from older sessions.
	if (!IsValid(ArmsMesh))
	{
		return;
	}

	TArray<TObjectPtr<USkeletalMeshSocket>>& Sockets = ArmsMesh->GetMeshOnlySocketList();
	const int32 Removed = Sockets.RemoveAll([ArmsMesh](const TObjectPtr<USkeletalMeshSocket>& Socket)
	{
		if (!IsValid(Socket) || Socket->GetOuter() != ArmsMesh)
		{
			return false;
		}
		const FName N = Socket->SocketName;
		if (N != FName(TEXT("ik_hand_gun"))
			&& N != FName(TEXT("VB ik_hand_gun"))
			&& N != FName(TEXT("VB ik_hand_gun_pivot")))
		{
			return false;
		}
		UE_LOG(LogTemp, Warning,
			TEXT("[GunAttach] removed shadow socket '%s' on %s (was parent=%s) — attach must use the animated bone"),
			*N.ToString(), *GetNameSafe(ArmsMesh), *Socket->BoneName.ToString());
		return true;
	});
	if (Removed > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GunAttach] stripped %d shadow gun-attach socket(s) from %s"),
			Removed, *GetNameSafe(ArmsMesh));
	}
}

void AShooterCharacter::ApplyFactionArms1P()
{
	if (!bUseBanditsArms1P || !IsValid(Mesh1P))
	{
		return;
	}

	// Always finish the Bandits/arms swap; AttachToOwingPawn runs again at the end of this function.
	// (An older early-out for UsesCustomMesh1PAttach skipped the swap and left guns on a bad attach target.)

	if (!bCapturedDefaultArms1P)
	{
		USkeletalMesh* CurArms = Mesh1P->GetSkeletalMeshAsset();
		DefaultArmsMesh1PAsset = IsBanditsSkelMesh(CurArms) ? LoadCanonicalArms1PSource() : CurArms;
		if (!IsValid(DefaultArmsMesh1PAsset))
		{
			DefaultArmsMesh1PAsset = CurArms;
		}
		bCapturedDefaultArms1P = true;
	}

	EnsureFactionLookDefaults();

	USkeletalMesh* WantMesh = nullptr;
	switch (FactionLook)
	{
	case ECharacterLookFaction::Criminal:
		WantMesh = LoadSoftMesh(CriminalArmsMesh1P);
		break;
	case ECharacterLookFaction::Police:
		WantMesh = LoadSoftMesh(PoliceArmsMesh1P);
		break;
	default:
		WantMesh = DefaultArmsMesh1PAsset.Get();
		break;
	}

	if (!IsValid(WantMesh))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Look] Arms1P mesh missing for FactionLook=%d"), static_cast<int32>(FactionLook));
		return;
	}

	USkeletalMesh* SourceForSockets = DefaultArmsMesh1PAsset.Get();
	if (!IsValid(SourceForSockets) || IsBanditsSkelMesh(SourceForSockets))
	{
		SourceForSockets = LoadCanonicalArms1PSource();
	}

	const bool bBanditsTarget = (FactionLook == ECharacterLookFaction::Criminal || FactionLook == ECharacterLookFaction::Police);
	if (bBanditsTarget)
	{
		EnsureArms1PGripSockets(WantMesh, SourceForSockets);
	}
	else
	{
		EnsureAnimatedGunAttachSockets(WantMesh);
	}

	USkeleton* WantSkel = WantMesh->GetSkeleton();
	USkeleton* CurSkel = Mesh1P->GetSkeletalMeshAsset() ? Mesh1P->GetSkeletalMeshAsset()->GetSkeleton() : nullptr;
#if WITH_EDITOR
	if (IsValid(WantSkel) && IsValid(CurSkel) && WantSkel != CurSkel)
	{
		WantSkel->AddCompatibleSkeleton(CurSkel);
		CurSkel->AddCompatibleSkeleton(WantSkel);
	}
	if (IsValid(WantSkel) && IsValid(SourceForSockets) && SourceForSockets->GetSkeleton())
	{
		WantSkel->AddCompatibleSkeleton(SourceForSockets->GetSkeleton());
		SourceForSockets->GetSkeleton()->AddCompatibleSkeleton(WantSkel);
	}
#endif

	const bool bAlready = (Mesh1P->GetSkeletalMeshAsset() == WantMesh);
	// Mesh1P BP carries OverrideMaterials from Ironbelly arms (slot 0 = skin/sleeve). Gloves slot was not
	// overridden so it looked right while forearms kept the old material. Clear overrides for Bandits arms.
	if (bBanditsTarget && Mesh1P->OverrideMaterials.Num() > 0)
	{
		Mesh1P->EmptyOverrideMaterials();
	}
	if (!bAlready)
	{
		const TSubclassOf<UAnimInstance> KeepAnim = Mesh1P->GetAnimClass();
		Mesh1P->SetSkeletalMesh(WantMesh, true);
		if (KeepAnim)
		{
			Mesh1P->SetAnimInstanceClass(KeepAnim);
			if (UAnimInstance* AnimInst = Mesh1P->GetAnimInstance())
			{
				AnimInst->InitializeAnimation(true);
			}
		}
	}

	if (IsValid(Combat))
	{
		Combat->RecacheDefaultFirstPersonArms();
		if (AWeapon* W = Combat->CurrentWeapon.Get())
		{
			W->AttachToOwingPawn();
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[Look] Mesh1P -> %s FactionLook=%d already=%d"),
		*GetNameSafe(WantMesh), static_cast<int32>(FactionLook), bAlready ? 1 : 0);

	ApplyFactionSleeves1P();
}
void AShooterCharacter::ApplyFactionSleeves1P()
{
	if (!IsValid(Mesh1P_Sleeves) || !IsValid(Mesh1P))
	{
		return;
	}

	auto ClearSleeves = [this]()
	{
		Mesh1P_Sleeves->SetLeaderPoseComponent(nullptr);
		Mesh1P_Sleeves->SetSkeletalMesh(nullptr);
		Mesh1P_Sleeves->EmptyOverrideMaterials();
		Mesh1P_Sleeves->SetVisibility(false);
	};

	if (!bUseBanditsArms1P || !bUseBanditsSleeves1P)
	{
		ClearSleeves();
		return;
	}

	EnsureFactionLookDefaults();

	USkeletalMesh* WantSleeve = nullptr;
	UMaterialInterface* WantMat = nullptr;
	switch (FactionLook)
	{
	case ECharacterLookFaction::Criminal:
		WantSleeve = LoadSoftMesh(CriminalSleeveMesh1P);
		WantMat = LoadSoftMaterial(CriminalSleeveMaterial);
		break;
	case ECharacterLookFaction::Police:
		WantSleeve = LoadSoftMesh(PoliceSleeveMesh1P);
		break;
	default:
		ClearSleeves();
		return;
	}

	if (!IsValid(WantSleeve))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Look] Sleeves1P mesh missing FactionLook=%d"), static_cast<int32>(FactionLook));
		ClearSleeves();
		return;
	}

	const bool bAlready = (Mesh1P_Sleeves->GetSkeletalMeshAsset() == WantSleeve);
	if (!bAlready)
	{
		Mesh1P_Sleeves->SetLeaderPoseComponent(nullptr);
		Mesh1P_Sleeves->SetSkeletalMesh(WantSleeve, true);
	}

	Mesh1P_Sleeves->SetLeaderPoseComponent(Mesh1P);
	Mesh1P_Sleeves->EmptyOverrideMaterials();
	if (IsValid(WantMat))
	{
		// Bomber brown etc. — apply to all slots so sleeve color matches 3P preset.
		const int32 NumMats = Mesh1P_Sleeves->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			Mesh1P_Sleeves->SetMaterial(i, WantMat);
		}
	}

	// Hide legs/head so FP mainly shows jacket sleeves (torso may still peek — Bandits has no sleeve-only mesh).
	static const FName HideBones[] = {
		TEXT("thigh_l"), TEXT("thigh_r"),
		TEXT("calf_l"), TEXT("calf_r"),
		TEXT("foot_l"), TEXT("foot_r"),
		TEXT("ball_l"), TEXT("ball_r"),
		TEXT("neck_01"), TEXT("neck_02"),
		TEXT("head"),
		TEXT("ik_foot_l"), TEXT("ik_foot_r"),
		TEXT("ik_foot_root"),
	};
	for (const FName BoneName : HideBones)
	{
		if (Mesh1P_Sleeves->GetBoneIndex(BoneName) != INDEX_NONE)
		{
			Mesh1P_Sleeves->HideBoneByName(BoneName, EPhysBodyOp::PBO_None);
		}
	}

	Mesh1P_Sleeves->bOnlyOwnerSee = true;
	Mesh1P_Sleeves->SetVisibility(Mesh1P->IsVisible());
	Mesh1P_Sleeves->SetHiddenInGame(Mesh1P->bHiddenInGame);

	UE_LOG(LogTemp, Warning, TEXT("[Look] Sleeves1P -> %s mat=%s FactionLook=%d already=%d leader=%s"),
		*GetNameSafe(WantSleeve), *GetNameSafe(WantMat), static_cast<int32>(FactionLook),
		bAlready ? 1 : 0, *GetNameSafe(Mesh1P));
}

