#pragma once

#include "CoreMinimal.h"

class UWidget;
class UUserWidget;
class UButton;
class UTextBlock;
class UBorder;
class UImage;
class UEditableTextBox;
class UEditableText;

/**
 * Shared tactical FPS chrome (dark panels + muted gold) matching LoadoutScreen / BuyShop.
 * Runtime-only: walks widget trees so Blueprint bindings stay untouched.
 */
struct FPS_API FTacticalUIStyle
{
	static const FLinearColor BgDim;
	static const FLinearColor Panel;
	static const FLinearColor PanelSoft;
	static const FLinearColor Row;
	static const FLinearColor Button;
	static const FLinearColor ButtonHover;
	static const FLinearColor ButtonPressed;
	static const FLinearColor ButtonDisabled;
	static const FLinearColor Accent;
	static const FLinearColor Outline;
	static const FLinearColor Text;
	static const FLinearColor TextMuted;
	static const FLinearColor TextGold;

	static FSlateBrush MakePanelBrush(const FLinearColor& Tint, float Radius = 6.f, float OutlineWidth = 1.f, const FLinearColor* OutlineOverride = nullptr);
	static FButtonStyle MakeButtonStyle();
	static FSlateFontInfo MakeFont(int32 Size, bool bBold = false);

	static void ApplyToUserWidget(UUserWidget* Root);
	static void ApplyToWidget(UWidget* Widget);

	static void StyleButton(UButton* Button);
	static void StyleText(UTextBlock* Text);
	static void StyleBorder(UBorder* Border);
	static void StyleImage(UImage* Image);
	static void StyleEditableTextBox(UEditableTextBox* Box);
	static void StyleEditableText(UEditableText* Text);
};
