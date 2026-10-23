#include "Animation/FirstPersonAnimInstance.h"

#include "ShooterCharacter.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "WeaponData.h"
#include "ShooterGamePlayTags.h"
#include "UObject/UnrealType.h"

namespace GripHoldAnim_Private
{
	static const FName NAME_DefaultSlot(TEXT("DefaultSlot"));
	static constexpr float GrabIdleMaxSpeedXY = 40.f;
}

void UFirstPersonAnimInstance::ForceShotgunRawPose1P()
{
	// ABP_FirstPerson AnimGraph is only: SequencePlayer -> BlendListByBool(bUseLeftHandHoldPose) -> Slot -> Root.
	// No AimOffset / FABRIK / LayeredBoneBlend / ControlRig / TwoBoneIK nodes exist on disk.
	// Event Graph (BlueprintUpdateAnimation) can re-copy CurrenrAnim from WeaponData and set
	// bUseLeftHandHoldPose AFTER NativeUpdateAnimation — starve again here (ThreadSafe runs after BP).
	bUseLeftHandHoldPose = false;
	LeftHandHoldPose = nullptr;
	FABRIK_Alpha1P = 0.f;

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
	FABRIK_Alpha1P = 0.f; // 1P FABRIK stays off (shotgun and all)

	UCombatComponent* Combat = Shooter->FindComponentByClass<UCombatComponent>();
	if (!IsValid(Combat) || !IsValid(Combat->CurrentWeapon) || !IsValid(Combat->WeaponData))
	{
		return;
	}

	const FPlayerAnim* WeaponAnim = Combat->WeaponData->FirstPersonAnims.Find(Combat->CurrentWeapon->WeaponType);
	if (!WeaponAnim)
	{
		return;
	}

	const EGripHoldStyle Style = Combat->CurrentWeapon->GetGripHoldStyle();
	GripHoldStyle = Style;

	UAnimSequence* DesiredIdle = WeaponAnim->IdleAnim.Get();
	UAnimSequence* DesiredAimIdle = WeaponAnim->AimIdleAnim.Get();
	if (Style == EGripHoldStyle::Grab)
	{
		if (IsValid(WeaponAnim->IdleAnim_Grab) && WeaponAnim->IdleAnim_Grab != WeaponAnim->LeftHandHoldPose_Grab)
		{
			DesiredIdle = WeaponAnim->IdleAnim_Grab.Get();
		}
		if (IsValid(WeaponAnim->AimIdleAnim_Grab))
		{
			DesiredAimIdle = WeaponAnim->AimIdleAnim_Grab.Get();
		}
		LeftHandHoldPose = WeaponAnim->LeftHandHoldPose_Grab.Get();
	}
	else
	{
		UAnimSequence* CradlePose = WeaponAnim->LeftHandHoldPose_Cradle.Get();
		LeftHandHoldPose = (CradlePose && CradlePose != WeaponAnim->IdleAnim.Get()) ? CradlePose : nullptr;
	}
	bUseLeftHandHoldPose = IsValid(LeftHandHoldPose);
	ActiveIdleAnim = DesiredIdle;

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
				if (IsValid(WeaponAnim->SprintAnim))
				{
					Currenr->SprintAnim = WeaponAnim->SprintAnim.Get();
				}
				else if (!IsValid(Currenr->SprintAnim))
				{
					const FString TypeStr = Combat->CurrentWeapon->WeaponType.ToString();
					if (TypeStr.Equals(TEXT("Weapon.type.rar"), ESearchCase::IgnoreCase))
					{
						static const TCHAR* RarSprintPath = TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Movement/A_RAR_FP_PCH_AssaultRifle_Sprint_F_Loop.A_RAR_FP_PCH_AssaultRifle_Sprint_F_Loop");
						Currenr->SprintAnim = LoadObject<UAnimSequence>(nullptr, RarSprintPath);
					}
					if (!IsValid(Currenr->SprintAnim) && IsValid(DesiredIdle))
					{
						Currenr->SprintAnim = DesiredIdle;
					}
				}
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
