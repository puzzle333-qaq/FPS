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
 if(Weapon && Weapon->WeaponKind==EWeaponKind::LongGun)return true;
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

namespace GripBodyPose_Private
{
	static UAnimSequence* Load(const TCHAR* Path)
	{
		return LoadObject<UAnimSequence>(nullptr, Path);
	}

	static bool UsesRarArms(const USkeletalMeshComponent* Arms, const AWeapon* Weapon)
	{
		const USkeletalMesh* Mesh = IsValid(Arms) ? Arms->GetSkeletalMeshAsset() : nullptr;
		if (Weapon && Weapon->WeaponKind==EWeaponKind::LongGun) return true;
		if (IsValid(Mesh))
		{
			const FString Path = Mesh->GetPathName();
			if (Path.Contains(TEXT("InfimaGames")) || Path.Contains(TEXT("SKEL_IG"))) return true;
			if (Path.Contains(TEXT("Manny")) || Path.Contains(TEXT("FPSArms"))) return false;
		}
		return RarGripAimPose_Private::IsRAR(Weapon);
	}

	// Full-body idles. The 1P graph has no left-hand IK, so the hold has to be the sequence itself.
	static UAnimSequence* Resolve(const USkeletalMeshComponent* Arms, const AWeapon* Weapon, EGripHoldStyle Style)
	{
		if (Style == EGripHoldStyle::None) return nullptr;
		static TObjectPtr<UAnimSequence> RarAngled, RarVertical, KinAngled, KinVertical;
		if (UsesRarArms(Arms, Weapon))
		{
			if (Style == EGripHoldStyle::Grab)
			{
				if (!RarVertical) RarVertical = Load(TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Poses/A_RAR_FP_PCH_AssaultRifle_Idle_Pose_Grip_Vertical.A_RAR_FP_PCH_AssaultRifle_Idle_Pose_Grip_Vertical"));
				return RarVertical;
			}
			if (!RarAngled) RarAngled = Load(TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Poses/A_RAR_FP_PCH_AssaultRifle_Idle_Pose_Grip_Angled.A_RAR_FP_PCH_AssaultRifle_Idle_Pose_Grip_Angled"));
			return RarAngled;
		}
		if (Style == EGripHoldStyle::Grab)
		{
			if (!KinVertical) KinVertical = Load(TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/General/Character/A_FP_VerticalGrip.A_FP_VerticalGrip"));
			return KinVertical;
		}
		if (!KinAngled) KinAngled = Load(TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/TR15/Character/A_FP_TR15_Idle_Pose_Grip.A_FP_TR15_Idle_Pose_Grip"));
		return KinAngled;
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
	bRestoreFlashIdle1P = false;

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

 // Inspection owns the support hand; never pin its animated motion to the foregrip.
 if(Combat->IsInspecting()) FABRIK_Alpha1P=0.f;
	FPlayerAnim AnimCopy;
	if (const FPlayerAnim* WeaponAnim = Combat->WeaponData->FirstPersonAnims.Find(Combat->ResolveHoldPoseTag(Combat->CurrentWeapon)))
	{
		AnimCopy = *WeaponAnim;
	}
	Combat->CurrentWeapon->ApplyAuthoredArmAnims(AnimCopy);
	if (!IsValid(AnimCopy.IdleAnim))
	{
		return;
	}

	const EGripHoldStyle Style = Combat->CurrentWeapon->GetGripHoldStyle();
	if (GripHoldStyle != Style)
	{
		UE_LOG(LogTemp, Log, TEXT("[GripHold] %s style %d -> %d"),
			*GetNameSafe(Combat->CurrentWeapon), (int32)GripHoldStyle, (int32)Style);
	}
	GripHoldStyle = Style;

	UAnimSequence* DesiredIdle = AnimCopy.IdleAnim.Get();
	UAnimSequence* DesiredAimIdle = AnimCopy.AimIdleAnim.Get();
	// Handguard keeps the weapon idle. Angled and vertical replace it after the AnimBP copy.
	LeftHandHoldPose = nullptr;
	bUseLeftHandHoldPose = false;
	GripBodyPose = GripBodyPose_Private::Resolve(GetSkelMeshComponent(), Combat->CurrentWeapon, Style);
	GripAimPose = DesiredAimIdle;
	GripPoseWeight = FMath::Clamp(1.f - GetCurveValue(TEXT("Mask_Grip")), 0.f, 1.f);
 FABRIK_Alpha1P *= GripPoseWeight;
	ActiveIdleAnim = DesiredIdle;
	bRestoreFlashIdle1P = Combat->CurrentWeapon->WeaponKind == EWeaponKind::Throwable
		&& Combat->CurrentWeapon->WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Flash);

	if (RarGripAimPose_Private::IsRAR(Combat->CurrentWeapon))
	{
		if (UAnimSequence* RarAim = RarGripAimPose_Private::Resolve(Combat->CurrentWeapon))
		{
			DesiredAimIdle = RarAim;
			GripAimPose = RarAim;
		}
	}
	else if (IsValid(GripBodyPose))
	{
		GripAimPose = GripBodyPose;
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
	ApplyResolvedGripPose();
}

void UFirstPersonAnimInstance::ApplyResolvedGripPose()
{
	if (bRestoreFlashIdle1P || !IsValid(ActiveIdleAnim))
	{
		return;
	}
	if (GripPoseWeight <= 0.01f)
	{
		return;
	}
	FStructProperty* AnimProperty = FindFProperty<FStructProperty>(GetClass(), TEXT("CurrenrAnim"));
	if (!AnimProperty || AnimProperty->Struct != FPlayerAnim::StaticStruct())
	{
		return;
	}
	FPlayerAnim* Currenr = AnimProperty->ContainerPtrToValuePtr<FPlayerAnim>(this);
	if (!Currenr)
	{
		return;
	}
	Currenr->IdleAnim = IsValid(GripBodyPose) ? GripBodyPose.Get() : ActiveIdleAnim.Get();
	Currenr->AimIdleAnim = IsValid(GripAimPose) ? GripAimPose.Get() : ActiveIdleAnim.Get();
	bUseLeftHandHoldPose = false;
	LeftHandHoldPose = nullptr;
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
				// BlueprintUpdateAnimation runs after NativeUpdateAnimation and can replace
				// CurrenrAnim with an empty direct Flash-tag lookup. Restore the resolved
				// Grenade/BP-authored idle immediately before the AnimGraph consumes it.
				if (bRestoreFlashIdle1P && IsValid(ActiveIdleAnim))
				{
					Currenr->IdleAnim = ActiveIdleAnim;
					if (!IsValid(Currenr->AimIdleAnim)) Currenr->AimIdleAnim = ActiveIdleAnim;
					if (!IsValid(Currenr->SprintAnim)) Currenr->SprintAnim = ActiveIdleAnim;
					LeftHandHoldPose = nullptr;
					bUseLeftHandHoldPose = false;
				}
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
	if (Combat->CurrentWeapon->WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_ShotGun))
	{
		ForceShotgunRawPose1P();
	}
	// Event Graph copies the weapon-data idle after NativeUpdateAnimation. Put the grip pose back.
	ApplyResolvedGripPose();

	if (!Combat->CurrentWeapon->WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_ShotGun))
	{
		return;
	}

	static bool bLoggedOnce = false;
	if (!bLoggedOnce)
	{
		bLoggedOnce = true;
		UE_LOG(LogTemp, Warning,
			TEXT("[ShotgunRaw1P] ThreadSafe starve OK. ABP nodes=SequencePlayer+BlendListByBool(bUseLeftHandHoldPose)+Slot only. Idle=%s bUseLH=%d"),
			*GetNameSafe(ActiveIdleAnim), bUseLeftHandHoldPose ? 1 : 0);
	}
}
