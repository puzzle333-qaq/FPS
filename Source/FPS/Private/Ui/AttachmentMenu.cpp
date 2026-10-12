#include "Ui/AttachmentMenu.h"

#include "Ui/AttachmentSlotRow.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

namespace
{
	// 配件变化发生在按钮的 OnClicked 里。当场 Refresh 会拆掉正在处理点击的控件。
	void ScheduleMenuRefresh(UAttachmentMenu* Menu)
	{
		if (!IsValid(Menu)) return;

		UWorld* World = Menu->GetWorld();
		if (!World) return;

		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Menu, [Menu]()
		{
			if (IsValid(Menu) && Menu->IsInViewport())
			{
				Menu->Refresh();
			}
		}));
	}
}

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
		// 用蓝图类的显示名。GetName() 是 BP_RIfle_C_0 这种实例名。
		Text_WeaponName->SetText(IsValid(Weapon) ? Weapon->GetClass()->GetDisplayNameText() : FText::GetEmpty());
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
	ScheduleMenuRefresh(this);
}

void UAttachmentMenu::OnCurrentWeaponChanged(AWeapon* NewWeapon)
{
	ScheduleMenuRefresh(this);
}
