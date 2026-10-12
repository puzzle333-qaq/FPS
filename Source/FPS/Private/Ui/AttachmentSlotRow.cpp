#include "Ui/AttachmentSlotRow.h"

#include "Ui/AttachmentOptionEntry.h"
#include "Weapon/Weapon.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"

void UAttachmentSlotRow::Setup(AWeapon* Weapon, const FWeaponAttachmentSlot& SlotInfo)
{
	// 槽位名：取 Tag 最后一段，Attachment.SlotInfo.Barrel -> Barrel
	if (IsValid(Text_SlotName))
	{
		FString TagName = SlotInfo.SlotTag.ToString();
		int32 DotIndex;
		if (TagName.FindLastChar(TEXT('.'), DotIndex))
		{
			TagName = TagName.RightChop(DotIndex + 1);
		}
		Text_SlotName->SetText(FText::FromString(TagName));
	}

	if (!IsValid(EntryContainer) || !IsValid(Weapon) || !EntryClass) return;
	EntryContainer->ClearChildren();

	UAttachmentData* Equipped = Weapon->GetAttachmentInSlot(SlotInfo.SlotTag);

	// 非必需槽位：先放一个“卸下”按钮
	if (!SlotInfo.bRequired)
	{
		if (UAttachmentOptionEntry* Entry = CreateWidget<UAttachmentOptionEntry>(this, EntryClass))
		{
			Entry->Setup(nullptr, SlotInfo.SlotTag, Equipped == nullptr);
			EntryContainer->AddChild(Entry);
		}
	}

	// 白名单里的每个配件一个按钮（列出来的都是允许的，再问一次枪更保险）
	for (UAttachmentData* Data : Weapon->GetAllowedAttachmentsForSlot(SlotInfo.SlotTag))
	{
		if (!IsValid(Data)) continue;
		if (UAttachmentOptionEntry* Entry = CreateWidget<UAttachmentOptionEntry>(this, EntryClass))
		{
			Entry->Setup(Data, SlotInfo.SlotTag, Data == Equipped, Weapon->CanEquipAttachment(Data));
			EntryContainer->AddChild(Entry);
		}
	}
}
