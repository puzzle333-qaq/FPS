#include "Ui/AttachmentMenu.h"

#include "Ui/AttachmentSlotRow.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"

void UAttachmentMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// 订阅“切枪”：换枪后整张表要重建
	if (UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetOwningPlayerPawn()))
	{
		Combat->OnCurrentWeaponChanged.AddUniqueDynamic(this, &ThisClass::OnCurrentWeaponChanged);
		BoundCombat = Combat;
	}
	Refresh();
}

void UAttachmentMenu::NativeDestruct()
{
	// 关闭（RemoveFromParent）时解绑，避免控件不在屏幕上还收到通知
	if (BoundCombat.IsValid())
	{
		BoundCombat->OnCurrentWeaponChanged.RemoveDynamic(this, &ThisClass::OnCurrentWeaponChanged);
	}
	BindToWeapon(nullptr);
	BoundCombat.Reset();

	Super::NativeDestruct();
}

void UAttachmentMenu::BindToWeapon(AWeapon* Weapon)
{
	if (BoundWeapon.Get() == Weapon) return;

	if (BoundWeapon.IsValid())
	{
		BoundWeapon->OnAttachmentsChanged.RemoveDynamic(this, &ThisClass::OnAttachmentsChanged);
	}
	BoundWeapon = Weapon;
	if (IsValid(Weapon))
	{
		// 订阅“配件变化”：服务器装好、同步到客户端后会广播
		Weapon->OnAttachmentsChanged.AddUniqueDynamic(this, &ThisClass::OnAttachmentsChanged);
	}
}

void UAttachmentMenu::Refresh()
{
	if (!IsValid(SlotList)) return;
	SlotList->ClearChildren();

	UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetOwningPlayerPawn());
	AWeapon* Weapon = IsValid(Combat) ? Combat->CurrentWeapon.Get() : nullptr;
	BindToWeapon(Weapon);

	if (IsValid(Text_WeaponName))
	{
		Text_WeaponName->SetText(IsValid(Weapon) ? FText::FromString(Weapon->GetName()) : FText::GetEmpty());
	}

	if (!IsValid(Weapon) || !RowClass) return;

	for (const FWeaponAttachmentSlot& SlotInfo : Weapon->GetAttachmentSlots())
	{
		if (UAttachmentSlotRow* Row = CreateWidget<UAttachmentSlotRow>(this, RowClass))
		{
			Row->Setup(Weapon, SlotInfo);
			SlotList->AddChild(Row);
		}
	}
}

void UAttachmentMenu::OnAttachmentsChanged(AWeapon* Weapon)
{
	Refresh();
}

void UAttachmentMenu::OnCurrentWeaponChanged(AWeapon* NewWeapon)
{
	Refresh();
}
