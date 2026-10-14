#include "Ui/LoadoutWeaponCard.h"

#include "Weapon/Weapon.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"

namespace
{
	FSlateFontInfo CardFont(int32 Size)
	{
		FSlateFontInfo Font;
		Font.FontObject = LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
		Font.TypefaceFontName = TEXT("Bold");
		Font.Size = Size;
		return Font;
	}

	FButtonStyle CardButtonStyle()
	{
		FButtonStyle Style;
		auto Clear = [](FSlateBrush& Brush, const FLinearColor& Tint)
		{
			Brush.DrawAs = Tint.A <= 0.f ? ESlateBrushDrawType::NoDrawType : ESlateBrushDrawType::Box;
			Brush.TintColor = FSlateColor(Tint);
		};
		Clear(Style.Normal, FLinearColor(0.f, 0.f, 0.f, 0.f));
		Clear(Style.Hovered, FLinearColor(1.f, 0.78f, 0.25f, 0.16f));
		Clear(Style.Pressed, FLinearColor(1.f, 0.78f, 0.25f, 0.30f));
		Clear(Style.Disabled, FLinearColor(0.f, 0.f, 0.f, 0.f));
		return Style;
	}
}

void ULoadoutWeaponCard::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// WBP 子类：用设计好的控件；纯 C++：生成默认的透明按钮 + 底部文字
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	if (IsValid(Button))
	{
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
	}
}

void ULoadoutWeaponCard::BuildLayout()
{
	if (!WidgetTree) return;

	Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button"));
	Button->SetStyle(CardButtonStyle());
	WidgetTree->RootWidget = Button;

	UOverlay* LabelOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("LabelOverlay"));
	Button->AddChild(LabelOverlay);

	Text_Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Name"));
	Text_Name->SetFont(CardFont(16));
	Text_Name->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Text_Name->SetJustification(ETextJustify::Center);
	Text_Name->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* LabelSlot = LabelOverlay->AddChildToOverlay(Text_Name))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Bottom);
		LabelSlot->SetPadding(FMargin(8.f, 8.f, 8.f, 10.f));
	}
}

void ULoadoutWeaponCard::SetupCard(int32 InIndex, AWeapon* InWeapon, const FText& InName)
{
	CardIndex = InIndex;
	Weapon = InWeapon;
	const bool bEmpty = !IsValid(InWeapon);

	if (IsValid(Text_Name))
	{
		Text_Name->SetText(bEmpty ? EmptyText : InName);
	}
	if (IsValid(Image_Icon))
	{
		UTexture2D* Icon = bEmpty ? nullptr : InWeapon->WeaponIcon.Get();
		if (Icon)
		{
			Image_Icon->SetBrushFromTexture(Icon);
			Image_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_Icon->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	if (IsValid(Button))
	{
		Button->SetIsEnabled(!bEmpty);
	}
	if (bEmpty)
	{
		bSelected = false;
	}
	NotifyState();
}

void ULoadoutWeaponCard::SetSelected(bool bInSelected)
{
	if (bSelected == bInSelected) return;
	bSelected = bInSelected;
	NotifyState();
}

void ULoadoutWeaponCard::NotifyState()
{
	OnCardStateChanged(bSelected, !Weapon.IsValid(), Weapon.Get());
}

void ULoadoutWeaponCard::HandleClicked()
{
	OnCardClicked.Broadcast(CardIndex);
}
