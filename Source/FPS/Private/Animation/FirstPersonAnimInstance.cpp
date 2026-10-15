#include "Animation/FirstPersonAnimInstance.h"

#include "ShooterCharacter.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "WeaponData.h"
#include "UObject/UnrealType.h"

namespace GripHoldAnim_Private
{
	static const FName NAME_DefaultSlot(TEXT("DefaultSlot"));
	static constexpr float GrabIdleMaxSpeedXY = 40.f;
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

	// Resolve idle the same way reload montages resolve Grab: live GetGripHoldStyle() + WeaponData.
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
		// IdleAnim_Grab equal to LeftHandHoldPose_Grab is a LeftHand-only layer pose (e.g. G2):
		// never use it as the full idle (its weapon_r/hand_r would twist the gun). The ABP
		// applies it via Layered blend per bone (upperarm_l) on top of the Cradle idle.
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

	// Keep CurrenrAnim.IdleAnim in sync for any Sequence Player that reads it.
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
			}
		}
	}

}
