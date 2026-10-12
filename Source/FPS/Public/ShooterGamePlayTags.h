#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace ShooterTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_None);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Rifle);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Pistol);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Sniper);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WeaponType_Melee);

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