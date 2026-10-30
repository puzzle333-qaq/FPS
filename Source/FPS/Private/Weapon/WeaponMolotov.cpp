#include "Weapon/WeaponMolotov.h"
#include "Weapon/ThrownProjectile.h"
#include "ShooterGamePlayTags.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"

AWeaponMolotov::AWeaponMolotov()
{
	WeaponKind = EWeaponKind::Throwable;
	WeaponType = ShooterTags::Tag_WeaponType_Molotov;
	MagCapacity = 1;
	Ammo = 1;
	StartingCarriedAmmo = 0;
	ProjectileClass = AThrownMolotov::StaticClass();
	// No dedicated molotov mesh in project yet — reuse frag mesh so hand is not empty.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshFinder(
		TEXT("/Game/FPSTP/Content/Weapons/Grenades/GrenadeList/FragGrenade/SkeletalMesh/WithFuse/FragGrenade.FragGrenade"));
	if (MeshFinder.Succeeded())
	{
		Mesh1P->SetSkeletalMesh(MeshFinder.Object);
		Mesh3P->SetSkeletalMesh(MeshFinder.Object);
	}
}
