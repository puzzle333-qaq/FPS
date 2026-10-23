#include "Ui/GunsmithWidgets.h"

#include "Ui/GunsmithScreen.h"
#include "Weapon/AttachmentData.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Weapon/Weapon.h"

namespace
{
	void SetIcon(UImage* Image, const UAttachmentData* Data)
	{
		if (!Image) return;
		if (IsValid(Data) && IsValid(Data->Icon))
		{
			Image->SetBrushFromTexture(Data->Icon);
			Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	FText AttachmentName(const UAttachmentData* Data)
	{
		if (!IsValid(Data)) return NSLOCTEXT("Gunsmith", "None", "无");
		return Data->DisplayName.IsEmpty() ? FText::FromString(Data->GetName()) : Data->DisplayName;
	}
}

// ---------------- Category ----------------
FText UGunsmithCategoryItem::GetSlotDisplayName(FGameplayTag Tag)
{
	FString Name = Tag.ToString();
	Name.Split(TEXT("."), nullptr, &Name, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	static const TMap<FString, FString> Cn = {
		{TEXT("Barrel"), TEXT("枪管")}, {TEXT("Muzzle"), TEXT("枪口")}, {TEXT("Optic"), TEXT("瞄具")},
		{TEXT("Stock"), TEXT("枪托")}, {TEXT("Grip"), TEXT("握把")}, {TEXT("Magazine"), TEXT("弹匣")},
		{TEXT("Handguard"), TEXT("护木")}, {TEXT("Tactical"), TEXT("战术")}, {TEXT("Sight"), TEXT("机瞄")},
		{TEXT("Underbarrel"), TEXT("下挂")}, {TEXT("Laser"), TEXT("激光")} };
	if (const FString* Found = Cn.Find(Name)) return FText::FromString(*Found);
	return FText::FromString(Name);
}

void UGunsmithCategoryItem::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_Select) Button_Select->OnClicked.AddUniqueDynamic(this, &ThisClass::Select);
}

void UGunsmithCategoryItem::Setup(UGunsmithScreen* InScreen, FGameplayTag InSlot, UAttachmentData* Equipped, bool bSelected)
{
	Screen = InScreen;
	SlotTag = InSlot;
	EquippedAttachment = Equipped;
	if (Text_SlotName) Text_SlotName->SetText(GetSlotDisplayName(InSlot));
	if (Text_Equipped) Text_Equipped->SetText(AttachmentName(Equipped));
	SetIcon(Image_Icon, Equipped);
	OnItemSetup(bSelected);
}

void UGunsmithCategoryItem::Select()
{
	if (IsValid(Screen)) Screen->SelectSlot(SlotTag);
}

// ---------------- Attachment ----------------
void UGunsmithAttachmentItem::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_Select)
	{
		Button_Select->OnClicked.AddUniqueDynamic(this, &ThisClass::Pick);
		Button_Select->OnHovered.AddUniqueDynamic(this, &ThisClass::HandleHovered);
		Button_Select->OnUnhovered.AddUniqueDynamic(this, &ThisClass::HandleUnhovered);
	}
}

void UGunsmithAttachmentItem::Setup(UGunsmithScreen* InScreen, FGameplayTag InSlot, UAttachmentData* InAttachment, bool bEquipped)
{
	Screen = InScreen;
	SlotTag = InSlot;
	Attachment = InAttachment;
	if (Text_Name) Text_Name->SetText(IsValid(InAttachment) ? AttachmentName(InAttachment) : NSLOCTEXT("Gunsmith", "Remove", "卸下"));
	if (Text_Description) Text_Description->SetText(IsValid(InAttachment) ? InAttachment->Description : FText::GetEmpty());
	SetIcon(Image_Icon, InAttachment);
	if (Widget_EquippedMark) Widget_EquippedMark->SetVisibility(bEquipped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	OnItemSetup(bEquipped);
}

void UGunsmithAttachmentItem::Pick()
{
	if (!IsValid(Screen)) return;
	Screen->SelectSlot(SlotTag);
	Screen->PickAttachment(Attachment);
}

void UGunsmithAttachmentItem::HandleHovered()
{
	if (IsValid(Screen)) Screen->PreviewStatsFor(SlotTag, Attachment);
}

void UGunsmithAttachmentItem::HandleUnhovered()
{
	if (IsValid(Screen)) Screen->RefreshStats();
}

// ---------------- Stat ----------------
void UGunsmithStatItem::SetStat(const FGunsmithStatLine& Line)
{
	Stat = Line;
	const float Max = FMath::Max(Line.Max, 1.f);
	const float Delta = Line.Preview - Line.Current;
	if (Text_Label) Text_Label->SetText(Line.Label);
	if (Text_Value)
	{
		FString V = FString::Printf(TEXT("%d"), FMath::RoundToInt(Line.Current));
		if (!FMath::IsNearlyZero(Delta))
		{
			V += FString::Printf(TEXT("  %s%d"), Delta > 0 ? TEXT("+") : TEXT(""), FMath::RoundToInt(Delta));
		}
		Text_Value->SetText(FText::FromString(V));
		Text_Value->SetColorAndOpacity(FMath::IsNearlyZero(Delta) ? FSlateColor(FLinearColor::White) : FSlateColor(Delta > 0 ? BetterColor : WorseColor));
	}
	// 变好：底层预览条更长（绿）；变差：上层当前条缩到预览值，底层显示原值（红）
	const float Low = FMath::Min(Line.Current, Line.Preview) / Max;
	const float High = FMath::Max(Line.Current, Line.Preview) / Max;
	if (ProgressBar_Current) ProgressBar_Current->SetPercent(Low);
	if (ProgressBar_Preview)
	{
		ProgressBar_Preview->SetPercent(High);
		if (!FMath::IsNearlyZero(Delta)) ProgressBar_Preview->SetFillColorAndOpacity(Delta > 0 ? BetterColor : WorseColor);
	}
	OnStatSet(Delta);
}

// ---------------- PartName ----------------
void UGunsmithPartName::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_Select) Button_Select->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void UGunsmithPartName::Setup(UGunsmithScreen* InScreen, FGameplayTag InSlot, UAttachmentData* Equipped, bool bSelected)
{
	Screen = InScreen;
	SlotTag = InSlot;
	if (Text_Name)
	{
		const FText SlotText = UGunsmithCategoryItem::GetSlotDisplayName(InSlot);
		Text_Name->SetText(IsValid(Equipped) ? FText::Format(NSLOCTEXT("Gunsmith", "PartFmt", "{0}: {1}"), SlotText, AttachmentName(Equipped)) : SlotText);
	}
	OnItemSetup(bSelected);
}

void UGunsmithPartName::HandleClicked()
{
	if (IsValid(Screen)) Screen->SelectSlot(SlotTag);
}
// ---------------- Weapon ----------------
FText UGunsmithWeaponItem::GetWeaponDisplayName(TSubclassOf<AWeapon> InWeapon)
{
	if (!InWeapon) return NSLOCTEXT("Gunsmith", "None", "无");
	FString Name = InWeapon->GetName();
	Name.RemoveFromEnd(TEXT("_C"));
	Name.RemoveFromStart(TEXT("BP_Weapon_"));
	Name.RemoveFromStart(TEXT("BP_"));
	return FText::FromString(Name);
}

void UGunsmithWeaponItem::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_Select) Button_Select->OnClicked.AddUniqueDynamic(this, &ThisClass::Select);
}

void UGunsmithWeaponItem::Setup(UGunsmithScreen* InScreen, TSubclassOf<AWeapon> InWeapon, bool bSelected)
{
	Screen = InScreen;
	WeaponClass = InWeapon;
	if (Text_Name) Text_Name->SetText(GetWeaponDisplayName(InWeapon));
	if (Image_Icon)
	{
		const AWeapon* CDO = InWeapon ? InWeapon->GetDefaultObject<AWeapon>() : nullptr;
		if (CDO && IsValid(CDO->WeaponIcon))
		{
			Image_Icon->SetBrushFromTexture(CDO->WeaponIcon);
			Image_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_Icon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (Widget_SelectedMark) Widget_SelectedMark->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	OnItemSetup(bSelected);
}

void UGunsmithWeaponItem::Select()
{
	if (IsValid(Screen) && WeaponClass) Screen->ShowWeapon(WeaponClass);
}