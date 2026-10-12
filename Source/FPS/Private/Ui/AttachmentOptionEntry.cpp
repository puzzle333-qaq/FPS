#include "Ui/AttachmentOptionEntry.h"

#include "CombatComponent.h"
#include "Weapon/AttachmentData.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UAttachmentOptionEntry::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(Button_Select))
	{
		Button_Select->OnClicked.AddDynamic(this, &ThisClass::OnSelectClicked);
	}
}

void UAttachmentOptionEntry::Setup(UAttachmentData* InData, FGameplayTag InSlotTag, bool bInEquipped, bool bInAllowed)
{
	Data = InData;
	SlotTag = InSlotTag;
	bEquipped = bInEquipped;

	// 名字：配件没填 DisplayName 就用资产名
	if (IsValid(Text_Name))
	{
		FText Name = RemoveText;
		if (IsValid(Data))
		{
			Name = Data->DisplayName.IsEmpty() ? FText::FromString(Data->GetName()) : Data->DisplayName;
		}
		Text_Name->SetText(Name);
	}

	// 图标：有就显示，没有就隐藏
	if (IsValid(Image_Icon))
	{
		if (IsValid(Data) && IsValid(Data->Icon))
		{
			Image_Icon->SetBrushFromTexture(Data->Icon);
			Image_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_Icon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 高亮已装备的；不允许的变灰不可点
	if (IsValid(Button_Select))
	{
		Button_Select->SetBackgroundColor(bEquipped ? EquippedColor : NormalColor);
		Button_Select->SetIsEnabled(bInAllowed);
	}
}

void UAttachmentOptionEntry::OnSelectClicked()
{
	if (bEquipped) return; // 已经装着了，不用重复发请求

	UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetOwningPlayerPawn());
	if (!IsValid(Combat)) return;

	// 只发请求，真正装不装由服务器校验；装好后武器广播 OnAttachmentsChanged，菜单会自动刷新
	if (IsValid(Data))
	{
		Combat->Initiate_EquipAttachment(Data);
	}
	else
	{
		Combat->Initiate_RemoveAttachment(SlotTag);
	}
}
