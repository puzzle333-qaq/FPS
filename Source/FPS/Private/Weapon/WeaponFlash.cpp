#include "Weapon/WeaponFlash.h"
#include "Weapon/ThrownProjectile.h"
#include "ShooterGamePlayTags.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"

AWeaponFlash::AWeaponFlash()
{
	WeaponKind = EWeaponKind::Throwable;
	WeaponType = ShooterTags::Tag_WeaponType_Flash;
	MagCapacity = 1;
	Ammo = 1;
	StartingCarriedAmmo = 0;
	ProjectileClass = AThrownFlash::StaticClass();
	// Match the working M67 arms placement, including the native fallback used by the pouch.
	HipMesh1POffset = FVector(-6.f, 0.f, 0.f);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshFinder(
		TEXT("/Game/FPSTP/Content/Weapons/Grenades/GrenadeList/FlashGrenade/SkeletalMesh/SK_FlashGrenade.SK_FlashGrenade"));
	if (MeshFinder.Succeeded())
	{
		Mesh1P->SetSkeletalMesh(MeshFinder.Object);
		Mesh3P->SetSkeletalMesh(MeshFinder.Object);
	}
}
