#include "ShooterGamePlayTags.h"

namespace ShooterTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_WeaponType_None, "Weapon.type.none", "No weapon type");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_WeaponType_Rifle, "Weapon.type.rifle", "Rifle weapon type");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_WeaponType_Pistol, "Weapon.type.pistol", "Pistol weapon type");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_WeaponType_Melee, "Weapon.type.melee", "Knife weapon type");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_WeaponType_AK, "Weapon.type.ak", "AK rifle — separate montage set from Weapon.type.rifle");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_WeaponType_ShotGun, "Weapon.type.shotgun", "ShotGunweapon type");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_WeaponType_Grenade, "Weapon.type.grenade", "Frag grenade — FirstPersonMontages keyed separately from guns");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_WeaponType_Smoke, "Weapon.type.smoke", "Smoke grenade");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_WeaponType_Molotov, "Weapon.type.molotov", "Molotov / incendiary");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_WeaponType_Flash, "Weapon.type.flash", "Flashbang");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_WeaponType_X24,"Weapon.type.x24","Pistol X24")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Attachment_Slot_Barrel, "Attachment.Slot.Barrel", "Attachment slot: Barrel");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Attachment_Slot_Handguard, "Attachment.Slot.Handguard", "Attachment slot: Handguard");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Attachment_Slot_Muzzle, "Attachment.Slot.Muzzle", "Attachment slot: Muzzle");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Attachment_Slot_Optic, "Attachment.Slot.Optic", "Attachment slot: Optic");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Attachment_Slot_Grip, "Attachment.Slot.Grip", "Attachment slot: Grip");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Attachment_Slot_Stock, "Attachment.Slot.Stock", "Attachment slot: Stock");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Attachment_Slot_Tactical, "Attachment.Slot.Tactical", "Attachment slot: Tactical");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Attachment_Slot_Magazine, "Attachment.Slot.Magazine", "Attachment slot: Magazine");
}