#include "Ui/TacticalUIStyle.h"

#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ContentWidget.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Styling/CoreStyle.h"

const FLinearColor FTacticalUIStyle::BgDim(0.01f, 0.012f, 0.016f, 0.92f);
const FLinearColor FTacticalUIStyle::Panel(0.06f, 0.07f, 0.09f, 0.94f);
const FLinearColor FTacticalUIStyle::PanelSoft(0.08f, 0.09f, 0.11f, 0.90f);
const FLinearColor FTacticalUIStyle::Row(0.10f, 0.11f, 0.13f, 0.88f);
const FLinearColor FTacticalUIStyle::Button(0.10f, 0.11f, 0.14f, 1.f);
const FLinearColor FTacticalUIStyle::ButtonHover(0.16f, 0.14f, 0.08f, 1.f);
const FLinearColor FTacticalUIStyle::ButtonPressed(0.22f, 0.18f, 0.08f, 1.f);
const FLinearColor FTacticalUIStyle::ButtonDisabled(0.06f, 0.06f, 0.07f, 0.55f);
const FLinearColor FTacticalUIStyle::Accent(0.85f, 0.68f, 0.22f, 1.f);
const FLinearColor FTacticalUIStyle::Outline(1.f, 1.f, 1.f, 0.08f);
const FLinearColor FTacticalUIStyle::Text(0.92f, 0.93f, 0.95f, 1.f);
const FLinearColor FTacticalUIStyle::TextMuted(0.62f, 0.64f, 0.68f, 1.f);
const FLinearColor FTacticalUIStyle::TextGold(1.f, 0.82f, 0.35f, 1.f);

FSlateBrush FTacticalUIStyle::MakePanelBrush(const FLinearColor& Tint, float Radius, float OutlineWidth, const FLinearColor* OutlineOverride)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(Tint);
	Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Brush.OutlineSettings.Width = OutlineWidth;
	Brush.OutlineSettings.Color = FSlateColor(OutlineOverride ? *OutlineOverride : Outline);
	return Brush;
}

FButtonStyle FTacticalUIStyle::MakeButtonStyle()
{
	FButtonStyle Style;
	auto Set = [](FSlateBrush& Brush, const FLinearColor& Tint, const FLinearColor& Line)
	{
		Brush = FTacticalUIStyle::MakePanelBrush(Tint, 4.f, 1.f, &Line);
	};
	Set(Style.Normal, Button, Outline);
	Set(Style.Hovered, ButtonHover, Accent);
	Set(Style.Pressed, ButtonPressed, Accent);
	Set(Style.Disabled, ButtonDisabled, Outline);
	Style.NormalPadding = FMargin(14.f, 10.f);
	Style.PressedPadding = FMargin(14.f, 11.f, 14.f, 9.f);
	return Style;
}

FSlateFontInfo FTacticalUIStyle::MakeFont(int32 Size, bool bBold)
{
	FSlateFontInfo Font;
	Font.FontObject = LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	Font.TypefaceFontName = bBold ? TEXT("Bold") : TEXT("Regular");
	Font.Size = Size;
	return Font;
}

void FTacticalUIStyle::StyleButton(UButton* ButtonWidget)
{
	if (!IsValid(ButtonWidget)) return;
	ButtonWidget->SetStyle(MakeButtonStyle());
	ButtonWidget->SetBackgroundColor(FLinearColor::White);
}

void FTacticalUIStyle::StyleText(UTextBlock* TextWidget)
{
	if (!IsValid(TextWidget)) return;

	const FString Name = TextWidget->GetName();
	const FString Lower = Name.ToLower();

	const bool bTitle = Name.Contains(TEXT("Title")) || Name.Contains(TEXT("Logo")) || Name.Contains(TEXT("Header"))
		|| Lower.Contains(TEXT("title")) || Lower.Contains(TEXT("logo"));
	const bool bAccent = Name.Contains(TEXT("RoomCode")) || Name.Contains(TEXT("Mode")) || Name.Contains(TEXT("Score"))
		|| Name.Contains(TEXT("ReadyLabel")) || Name.Contains(TEXT("MatchResult"))
		|| Lower.Contains(TEXT("roomcode")) || Lower.Contains(TEXT("mode"));
	const bool bMuted = Name.Contains(TEXT("Hint")) || Name.Contains(TEXT("Status")) || Name.Contains(TEXT("Message"))
		|| Name.Contains(TEXT("Ping")) || Name.Contains(TEXT("Empty")) || Name.Contains(TEXT("Sub"))
		|| Lower.Contains(TEXT("hint")) || Lower.Contains(TEXT("status")) || Lower.Contains(TEXT("message"));

	if (bTitle)
	{
		TextWidget->SetColorAndOpacity(FSlateColor(TextGold));
		TextWidget->SetFont(MakeFont(22, true));
	}
	else if (bAccent)
	{
		TextWidget->SetColorAndOpacity(FSlateColor(TextGold));
		TextWidget->SetFont(MakeFont(16, true));
	}
	else if (bMuted)
	{
		TextWidget->SetColorAndOpacity(FSlateColor(TextMuted));
		TextWidget->SetFont(MakeFont(13, false));
	}
	else
	{
		TextWidget->SetColorAndOpacity(FSlateColor(Text));
		FSlateFontInfo Font = TextWidget->GetFont();
		if (Font.Size < 12)
		{
			TextWidget->SetFont(MakeFont(14, false));
		}
		else
		{
			Font.FontObject = LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
			TextWidget->SetFont(Font);
		}
	}
}

void FTacticalUIStyle::StyleBorder(UBorder* Border)
{
	if (!IsValid(Border)) return;

	const FString Lower = Border->GetName().ToLower();
	FLinearColor Tint = Panel;
	FLinearColor Line = Outline;
	float Radius = 6.f;

	if (Lower.Contains(TEXT("dim")) || Lower.Contains(TEXT("overlay")) || Lower.Contains(TEXT("backdrop"))
		|| Lower.Contains(TEXT("scrim")) || Lower.Contains(TEXT("bg_full")))
	{
		Tint = BgDim;
		Radius = 0.f;
	}
	else if (Lower.Contains(TEXT("row")) || Lower.Contains(TEXT("entry")) || Lower.Contains(TEXT("item")))
	{
		Tint = Row;
		Radius = 4.f;
	}
	else if (Lower.Contains(TEXT("header")) || Lower.Contains(TEXT("topbar")) || Lower.Contains(TEXT("title")))
	{
		Tint = PanelSoft;
		Line = Accent;
		Radius = 4.f;
	}

	Border->SetBrush(MakePanelBrush(Tint, Radius, Radius > 0.f ? 1.f : 0.f, &Line));
	Border->SetBrushColor(FLinearColor::White);

	const FMargin Pad = Border->GetPadding();
	if (Pad.Left + Pad.Right + Pad.Top + Pad.Bottom < 8.f)
	{
		Border->SetPadding(FMargin(12.f, 10.f));
	}
}

void FTacticalUIStyle::StyleImage(UImage* Image)
{
	if (!IsValid(Image)) return;
	const FString Lower = Image->GetName().ToLower();
	if (!(Lower.Contains(TEXT("bg")) || Lower.Contains(TEXT("backdrop")) || Lower.Contains(TEXT("panel"))
		|| Lower.Contains(TEXT("frame")) || Lower.Contains(TEXT("dim")) || Lower.Contains(TEXT("overlay"))))
	{
		return;
	}
	FSlateBrush Brush = Image->GetBrush();
	Brush.TintColor = FSlateColor(Panel);
	if (Brush.DrawAs == ESlateBrushDrawType::NoDrawType || Brush.GetResourceObject() == nullptr)
	{
		Brush = MakePanelBrush(Panel, 6.f);
	}
	Image->SetBrush(Brush);
	Image->SetColorAndOpacity(FLinearColor::White);
}

void FTacticalUIStyle::StyleEditableTextBox(UEditableTextBox* Box)
{
	if (!IsValid(Box)) return;
	FEditableTextBoxStyle Style = Box->WidgetStyle;
	Style.BackgroundImageNormal = MakePanelBrush(Row, 4.f);
	Style.BackgroundImageHovered = MakePanelBrush(PanelSoft, 4.f, 1.f, &Accent);
	Style.BackgroundImageFocused = MakePanelBrush(PanelSoft, 4.f, 1.f, &Accent);
	Style.BackgroundImageReadOnly = MakePanelBrush(ButtonDisabled, 4.f);
	Style.ForegroundColor = FSlateColor(Text);
	Style.Padding = FMargin(10.f, 8.f);
	Box->WidgetStyle = Style;
	Box->SetForegroundColor(Text);
}

void FTacticalUIStyle::StyleEditableText(UEditableText* Edit)
{
	if (!IsValid(Edit)) return;
	FEditableTextStyle Style = Edit->WidgetStyle;
	Style.ColorAndOpacity = FSlateColor(Text);
	Edit->WidgetStyle = Style;
}

void FTacticalUIStyle::ApplyToWidget(UWidget* Widget)
{
	if (!IsValid(Widget)) return;

	if (UUserWidget* Nested = Cast<UUserWidget>(Widget))
	{
		ApplyToUserWidget(Nested);
		return;
	}

	if (UButton* Btn = Cast<UButton>(Widget))
	{
		StyleButton(Btn);
	}
	else if (UTextBlock* Txt = Cast<UTextBlock>(Widget))
	{
		StyleText(Txt);
	}
	else if (UBorder* Brd = Cast<UBorder>(Widget))
	{
		StyleBorder(Brd);
	}
	else if (UImage* Img = Cast<UImage>(Widget))
	{
		StyleImage(Img);
	}
	else if (UEditableTextBox* Box = Cast<UEditableTextBox>(Widget))
	{
		StyleEditableTextBox(Box);
	}
	else if (UEditableText* Edit = Cast<UEditableText>(Widget))
	{
		StyleEditableText(Edit);
	}

	if (UPanelWidget* PanelW = Cast<UPanelWidget>(Widget))
	{
		const int32 Num = PanelW->GetChildrenCount();
		for (int32 i = 0; i < Num; ++i)
		{
			ApplyToWidget(PanelW->GetChildAt(i));
		}
		return;
	}

	if (UContentWidget* Content = Cast<UContentWidget>(Widget))
	{
		ApplyToWidget(Content->GetContent());
	}
}

void FTacticalUIStyle::ApplyToUserWidget(UUserWidget* Root)
{
	if (!IsValid(Root)) return;
	ApplyToWidget(Root->GetRootWidget());
}
