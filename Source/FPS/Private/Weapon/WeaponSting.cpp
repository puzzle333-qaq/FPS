#include "Weapon/WeaponSting.h"
#include "Weapon/ThrownProjectile.h"
#include "ShooterGamePlayTags.h"

AWeaponSting::AWeaponSting()
{
	WeaponKind = EWeaponKind::Throwable;
	WeaponType = ShooterTags::Tag_WeaponType_Sting;
	MagCapacity = 1;
	Ammo = 1;
	StartingCarriedAmmo = 0;
	ProjectileClass = AThrownStinger::StaticClass();
}
