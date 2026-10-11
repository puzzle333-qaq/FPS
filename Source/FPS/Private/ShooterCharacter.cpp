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
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include "FPSTraceChannels.h"
#include "UObject/UnrealType.h"
#include "ShooterPlayerController.h"

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

	CalculateTurnInPlaceParameters(DeltaTime);
	CalculateFABRIKSocket();
	UpdateAimSocketAlignment(DeltaTime);

	if (USniperScopeComponent* Scope = FindComponentByClass<USniperScopeComponent>())
	{
		Scope->UpdateView();
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


void AShooterCharacter::CalculateFABRIKSocket()
{
	if (!IsValid(Combat) || !IsValid(Combat->CurrentWeapon)) return;

	USkeletalMeshComponent* WeaponMesh = Combat->CurrentWeapon->GetMesh3P();
	if (!IsValid(WeaponMesh) || !WeaponMesh->DoesSocketExist(TEXT("FABRIK")))
	{
		FABRIK_SocketTransform = FTransform::Identity;
		return;
	}

	// A hidden third-person gun does not tick, so force a pose before reading the grip.
	WeaponMesh->RefreshBoneTransforms();

	const FTransform SocketWorld = WeaponMesh->GetSocketTransform(TEXT("FABRIK"), RTS_World);
	FVector OutLocation;
	FRotator OutRotation;
	GetMesh()->TransformToBoneSpace(
		TEXT("hand_r"),
		SocketWorld.GetLocation(),
		SocketWorld.Rotator(),
		OutLocation,
		OutRotation);
	FABRIK_SocketTransform.SetLocation(OutLocation);
	FABRIK_SocketTransform.SetRotation(OutRotation.Quaternion());
	FABRIK_SocketTransform.SetScale3D(FVector::OneVector);
}

void AShooterCharacter::ConfigureAimTickOrder()
{
	// The aim socket follows the arm bones, so read it after Mesh1P animates.
	if (IsValid(Mesh1P))
	{
		PrimaryActorTick.AddPrerequisite(Mesh1P, Mesh1P->PrimaryComponentTick);
	}
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
	if (!AdsLocationProperty)
	{
		return;
	}

	AWeapon* Weapon = Combat->CurrentWeapon;
	USkeletalMeshComponent* WeaponMesh = IsValid(Weapon) ? Weapon->GetMesh1P() : nullptr;
	if (!IsValid(WeaponMesh) || !WeaponMesh->IsAttachedTo(Mesh1P))
	{
		return;
	}

	if (!Weapon->HasAimSocket())
	{
		if (Weapon->WeaponKind != EWeaponKind::Melee && MissingAimSocketWeapon.Get() != Weapon)
		{
			MissingAimSocketWeapon = Weapon;
			UE_LOG(LogTemp, Warning, TEXT("%s has no socket '%s'. ADS keeps the blueprint location."),
				*GetNameSafe(Weapon), *Weapon->AimSocketName.ToString());
		}
		return;
	}

	USceneComponent* Parent = Mesh1P->GetAttachParent();
	if (!IsValid(Parent))
	{
		return;
	}

	WeaponMesh->UpdateComponentToWorld();
	FVector SocketWorld = FVector::ZeroVector;
	if (!Weapon->GetAimSocketWorldLocation(SocketWorld))
	{
		return;
	}

	// Mesh1P stays at its component rotation (yaw -90 on the camera). The blueprint
	// rotator variables are not applied, so measure the miss in camera space and
	// slide the current relative location by that parent-space delta.
	const FTransform CameraTransform = FirstPersonCamera->GetComponentTransform();
	const FVector SocketCamera = CameraTransform.InverseTransformPosition(SocketWorld);
	const FVector DesiredWorld = CameraTransform.TransformPosition(FVector(SocketCamera.X, 0.f, 0.f));
	const FVector DeltaParent = Parent->GetComponentTransform().InverseTransformVector(DesiredWorld - SocketWorld);
	const FVector CenteredLocation = Mesh1P->GetRelativeLocation() + DeltaParent;
	if (CenteredLocation.ContainsNaN())
	{
		return;
	}

	*AdsLocationProperty = CenteredLocation;
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

void AShooterCharacter::Input_ReloadWeapon()
{
	if (IsValid(Combat)) Combat->Initiate_ReloadWeapon();
}

void AShooterCharacter::Input_FireWeapon_Pressed()
{
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
}

void AShooterCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	RefreshControllerMovementSpeed();
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
