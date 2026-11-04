#include "Ui/AttachmentSlotRow.h"

#include "Ui/AttachmentOptionEntry.h"
#include "Ui/GunsmithWidgets.h"
#include "Weapon/Weapon.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"

void UAttachmentSlotRow::Setup(AWeapon* Weapon, const FWeaponAttachmentSlot& SlotInfo)
{
	if (IsValid(Text_SlotName))
	{
		Text_SlotName->SetText(UGunsmithCategoryItem::GetSlotDisplayName(SlotInfo.SlotTag));
	}

	if (!IsValid(EntryContainer) || !IsValid(Weapon) || !EntryClass) return;
	EntryContainer->ClearChildren();

	UAttachmentData* Equipped = Weapon->GetAttachmentInSlot(SlotInfo.SlotTag);

	// 非必需槽位：先放一个“卸下”按钮
	if (!SlotInfo.bRequired)
	{
		if (UAttachmentOptionEntry* Entry = CreateWidget<UAttachmentOptionEntry>(this, EntryClass))
		{
			Entry->Setup(nullptr, SlotInfo.SlotTag, Equipped == nullptr, true, Weapon);
			EntryContainer->AddChild(Entry);
		}
	}

	// 白名单里的每个配件一个按钮（列出来的都是允许的，再问一次枪更保险）
	for (UAttachmentData* Data : Weapon->GetAllowedAttachmentsForSlot(SlotInfo.SlotTag))
	{
		if (!IsValid(Data)) continue;
		if (UAttachmentOptionEntry* Entry = CreateWidget<UAttachmentOptionEntry>(this, EntryClass))
		{
			Entry->Setup(Data, SlotInfo.SlotTag, Data == Equipped, Weapon->CanEquipAttachment(Data), Weapon);
			EntryContainer->AddChild(Entry);
		}
	}
}
