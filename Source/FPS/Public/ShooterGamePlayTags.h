#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace ShooterTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_None);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Rifle);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Pistol);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_AK);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_ShotGun);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Grenade);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Smoke);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Molotov);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Flash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_X24);
	// DEFINITIVE_FPS_KIT (skip Glock18)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_AR10);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_MCX);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_HXR05);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Colt1911);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Deagle);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_DoubleBarrel);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Winchester);

	// ---- 配件槽位类型（原生标签）。更多类型可在 Project Settings -> GameplayTags 里加 Attachment.Slot.xxx ----
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_Attachment_Slot_Barrel);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_Attachment_Slot_Handguard);   // 护木
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_Attachment_Slot_Muzzle);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_Attachment_Slot_Optic);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_Attachment_Slot_Grip);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_Attachment_Slot_Stock);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_Attachment_Slot_Tactical);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_Attachment_Slot_Magazine);
}