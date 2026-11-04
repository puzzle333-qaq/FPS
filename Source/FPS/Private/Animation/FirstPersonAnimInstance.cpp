#include "Animation/FirstPersonAnimInstance.h"

#include "ShooterCharacter.h"
#include "ShooterPlayerController.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "WeaponData.h"
#include "ShooterGamePlayTags.h"
#include "UObject/UnrealType.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Weapon/AttachmentData.h"

namespace GripHoldAnim_Private
{
	static const FName NAME_DefaultSlot(TEXT("DefaultSlot"));
	static constexpr float GrabIdleMaxSpeedXY = 40.f;
}

namespace RarGripAimPose_Private
{
	static const TCHAR* PoseDir = TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Poses/");

	static bool IsRAR(const AWeapon* Weapon)
	{
		if (Weapon->GetClass()->GetPathName().Contains(TEXT("RealisticAssaultRifle"))) return true;
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Weapon);
		for (const USkeletalMeshComponent* M : Meshes)
		{
			if (M && M->GetSkeletalMeshAsset() && M->GetSkeletalMeshAsset()->GetPathName().Contains(TEXT("RealisticAssaultRifle"))) return true;
		}
		return false;
	}

	static UAnimSequence* LoadPose(const TCHAR* Name)
	{
		const FString Path = FString::Printf(TEXT("%s%s.%s"), PoseDir, Name, Name);
		return LoadObject<UAnimSequence>(nullptr, *Path);
	}

	// Discrete, no blending: Vertical / Angled / Angled_Canted. Null = no grip -> keep DA aim idle.
	static UAnimSequence* Resolve(const AWeapon* Weapon)
	{
		static TObjectPtr<UAnimSequence> Angled, Canted, Vertical;
		const UAttachmentData* Grip = Weapon->GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Grip);
		if (!IsValid(Grip)) return nullptr;
		const FString GripName = Grip->GetName();
		if (GripName.Contains(TEXT("Cant")))
		{
			if (!Canted) { Canted = LoadPose(TEXT("A_RAR_FP_PCH_AssaultRifle_Aim_Pose_Grip_Angled_Canted")); }
			return Canted;
		}
		if (Grip->GripHoldStyle == EGripHoldStyle::Grab || GripName.Contains(TEXT("Vert")))
		{
			if (!Vertical) { Vertical = LoadPose(TEXT("A_RAR_FP_PCH_AssaultRifle_Aim_Pose_Grip_Vertical")); }
			return Vertical;
		}
		if (!Angled) { Angled = LoadPose(TEXT("A_RAR_FP_PCH_AssaultRifle_Aim_Pose_Grip_Angled")); }
		return Angled;
	}
}

void UFirstPersonAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
}

void UFirstPersonAnimInstance::ForceShotgunRawPose1P()
{
	// ABP_FirstPerson AnimGraph is only: SequencePlayer -> BlendListByBool(bUseLeftHandHoldPose) -> Slot -> Root.
	// No AimOffset / FABRIK / LayeredBoneBlend / ControlRig / TwoBoneIK nodes exist on disk.
	// Event Graph (BlueprintUpdateAnimation) can re-copy CurrenrAnim from WeaponData and set
	// bUseLeftHandHoldPose AFTER NativeUpdateAnimation — starve again here (ThreadSafe runs after BP).
	bUseLeftHandHoldPose = false;
	LeftHandHoldPose = nullptr;
	if (FStructProperty* AnimProperty = FindFProperty<FStructProperty>(GetClass(), TEXT("CurrenrAnim")))
	{
		if (AnimProperty->Struct == FPlayerAnim::StaticStruct())
		{
			if (FPlayerAnim* Currenr = AnimProperty->ContainerPtrToValuePtr<FPlayerAnim>(this))
			{
				Currenr->AimOffset_Hip = nullptr;
				Currenr->AimOffset_Aim = nullptr;
				Currenr->Strafe_Standing = nullptr;
				Currenr->Strafe_Crouching = nullptr;
				Currenr->Strafe_Walking = nullptr;
				Currenr->LeftHandHoldPose_Cradle = nullptr;
				Currenr->LeftHandHoldPose_Grab = nullptr;
				Currenr->IdleAnim_Grab = nullptr;
				Currenr->AimIdleAnim_Grab = nullptr;
				if (IsValid(ActiveIdleAnim))
				{
					Currenr->IdleAnim = ActiveIdleAnim;
				}
			}
		}
	}
}

void UFirstPersonAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	AShooterCharacter* Shooter = Cast<AShooterCharacter>(TryGetPawnOwner());
	if (!IsValid(Shooter))
	{
		FABRIK_Alpha1P = 0.f;
		return;
	}

	FABRIK_Socket1P = Shooter->FABRIK_SocketTransform1P;
	FABRIK_Alpha1P = Shooter->FABRIK_Alpha1P;

	if (FBoolProperty* SprintProp = FindFProperty<FBoolProperty>(GetClass(), TEXT("bIsSprinting")))
	{
		bool bSprint = false;
		if (const AShooterPlayerController* PC = Cast<AShooterPlayerController>(Shooter->GetController()))
		{
			bSprint = PC->IsSprinting() && Shooter->GetVelocity().SizeSquared2D() > 100.f;
		}
		SprintProp->SetPropertyValue_InContainer(this, bSprint);
	}

	UCombatComponent* Combat = Shooter->FindComponentByClass<UCombatComponent>();
	if (!IsValid(Combat) || !IsValid(Combat->CurrentWeapon) || !IsValid(Combat->WeaponData))
	{
		return;
	}

	FPlayerAnim AnimCopy;
	if (const FPlayerAnim* WeaponAnim = Combat->WeaponData->FirstPersonAnims.Find(Combat->CurrentWeapon->WeaponType))
	{
		AnimCopy = *WeaponAnim;
	}
	Combat->CurrentWeapon->ApplyAuthoredArmAnims(AnimCopy);
	if (!IsValid(AnimCopy.IdleAnim))
	{
		return;
	}

	const EGripHoldStyle Style = Combat->CurrentWeapon->GetGripHoldStyle();
	GripHoldStyle = Style;

	UAnimSequence* DesiredIdle = AnimCopy.IdleAnim.Get();
	UAnimSequence* DesiredAimIdle = AnimCopy.AimIdleAnim.Get();
	if (Style == EGripHoldStyle::Grab)
	{
		if (IsValid(AnimCopy.IdleAnim_Grab) && AnimCopy.IdleAnim_Grab != AnimCopy.LeftHandHoldPose_Grab)
		{
			DesiredIdle = AnimCopy.IdleAnim_Grab.Get();
		}
		if (IsValid(AnimCopy.AimIdleAnim_Grab))
		{
			DesiredAimIdle = AnimCopy.AimIdleAnim_Grab.Get();
		}
		LeftHandHoldPose = AnimCopy.LeftHandHoldPose_Grab.Get();
	}
	else
	{
		UAnimSequence* CradlePose = AnimCopy.LeftHandHoldPose_Cradle.Get();
		LeftHandHoldPose = (CradlePose && CradlePose != AnimCopy.IdleAnim.Get()) ? CradlePose : nullptr;
	}
	if (UAnimSequence* KineGripPose = Combat->CurrentWeapon->GetGripPoseAnim())
	{
		LeftHandHoldPose = KineGripPose;
	}
	bUseLeftHandHoldPose = IsValid(LeftHandHoldPose);
	GripPoseWeight = FMath::Clamp(1.f - GetCurveValue(TEXT("Mask_Grip")), 0.f, 1.f);
	ActiveIdleAnim = DesiredIdle;

	if (RarGripAimPose_Private::IsRAR(Combat->CurrentWeapon))
	{
		if (UAnimSequence* RarAim = RarGripAimPose_Private::Resolve(Combat->CurrentWeapon))
		{
			DesiredAimIdle = RarAim;
		}
	}

	if (FStructProperty* AnimProperty = FindFProperty<FStructProperty>(GetClass(), TEXT("CurrenrAnim")))
	{
		if (AnimProperty->Struct == FPlayerAnim::StaticStruct())
		{
			if (FPlayerAnim* Currenr = AnimProperty->ContainerPtrToValuePtr<FPlayerAnim>(this))
			{
				Currenr->IdleAnim = DesiredIdle;
				if (IsValid(DesiredAimIdle))
				{
					Currenr->AimIdleAnim = DesiredAimIdle;
				}
				if (IsValid(AnimCopy.SprintAnim))
				{
					Currenr->SprintAnim = AnimCopy.SprintAnim.Get();
				}
				else if (!IsValid(Currenr->SprintAnim) && IsValid(DesiredIdle))
				{
					Currenr->SprintAnim = DesiredIdle;
				}
				// Mesh1P is on the camera. Look aim offset is Mesh3P only.
				Currenr->AimOffset_Hip = nullptr;
				Currenr->AimOffset_Aim = nullptr;
			}
		}
	}

	if (Combat->CurrentWeapon->WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_ShotGun))
	{
		ForceShotgunRawPose1P();
	}
}

void UFirstPersonAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// After BlueprintUpdateAnimation, so an Event Graph copy cannot put aim offset back on Mesh1P.
	if (FStructProperty* AnimProperty = FindFProperty<FStructProperty>(GetClass(), TEXT("CurrenrAnim")))
	{
		if (AnimProperty->Struct == FPlayerAnim::StaticStruct())
		{
			if (FPlayerAnim* Currenr = AnimProperty->ContainerPtrToValuePtr<FPlayerAnim>(this))
			{
				Currenr->AimOffset_Hip = nullptr;
				Currenr->AimOffset_Aim = nullptr;
			}
		}
	}

	// Runs AFTER BlueprintUpdateAnimation — defeats Event Graph restoring LeftHand / CurrenrAnim from DA.
	AShooterCharacter* Shooter = Cast<AShooterCharacter>(TryGetPawnOwner());
	if (!IsValid(Shooter))
	{
		return;
	}
	UCombatComponent* Combat = Shooter->FindComponentByClass<UCombatComponent>();
	if (!IsValid(Combat) || !IsValid(Combat->CurrentWeapon))
	{
		return;
	}
	if (!Combat->CurrentWeapon->WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_ShotGun))
	{
		return;
	}

	ForceShotgunRawPose1P();

	static bool bLoggedOnce = false;
	if (!bLoggedOnce)
	{
		bLoggedOnce = true;
		UE_LOG(LogTemp, Warning,
			TEXT("[ShotgunRaw1P] ThreadSafe starve OK. ABP nodes=SequencePlayer+BlendListByBool(bUseLeftHandHoldPose)+Slot only. Idle=%s bUseLH=%d"),
			*GetNameSafe(ActiveIdleAnim), bUseLeftHandHoldPose ? 1 : 0);
	}
}
