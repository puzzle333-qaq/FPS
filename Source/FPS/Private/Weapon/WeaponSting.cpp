#include "Weapon/WeaponSting.h"
#include "ShooterGamePlayTags.h"

AWeaponSting::AWeaponSting()
{
	WeaponKind = EWeaponKind::Throwable;
	WeaponType = ShooterTags::Tag_WeaponType_Sting;
	MagCapacity = 1;
	Ammo = 1;
	StartingCarriedAmmo = 0;
}
