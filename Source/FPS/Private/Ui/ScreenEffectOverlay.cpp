#include "Ui/ScreenEffectOverlay.h"

#include "ShooterCharacter.h"
#include "Ui/OperationUIStyle.h"
#include "Styling/CoreStyle.h"

UScreenEffectOverlay::UScreenEffectOverlay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasScriptImplementedTick = true;
}

void UScreenEffectOverlay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UScreenEffectOverlay::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

int32 UScreenEffectOverlay::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled) + 1;
	const AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwningPlayerPawn());
	if (!IsValid(Shooter)) return Layer;

	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	if (!Brush) return Layer;

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X < 2.f || Size.Y < 2.f) return Layer;

	const float Blood = Shooter->GetHitBloodAlpha();
	if (Blood > 0.01f)
	{
		// Bands fade out before the middle, so the sight picture stays clear. Corners stack and read redder.
		const float Reach = FMath::Min(Size.X, Size.Y) * 0.30f;
		const int32 Steps = 10;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const float T0 = static_cast<float>(Step) / Steps;
			const float T1 = static_cast<float>(Step + 1) / Steps;
			const float Fade = FMath::Pow(1.f - T0, 2.6f);
			if (Fade < 0.03f) continue;

			const FLinearColor Color=FOperationUIStyle::Warning.CopyWithNewOpacity(Blood*.9f*Fade);
			const FLinearColor Tint = Color * InWidgetStyle.GetColorAndOpacityTint();
			const float Y0 = T0 * Reach;
			const float Band = (T1 - T0) * Reach;
			auto Box = [&](const FVector2D& Pos, const FVector2D& BoxSize)
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					Layer,
					AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(Pos)),
					Brush,
					ESlateDrawEffect::None,
					Tint);
			};
			Box(FVector2D(0.f, Y0), FVector2D(Size.X, Band));
			Box(FVector2D(0.f, Size.Y - Y0 - Band), FVector2D(Size.X, Band));
			Box(FVector2D(Y0, 0.f), FVector2D(Band, Size.Y));
			Box(FVector2D(Size.X - Y0 - Band, 0.f), FVector2D(Band, Size.Y));
		}
	}

	const float White = Shooter->GetFlashWhiteAlpha();
	if (White > 0.01f)
	{
		const float Alpha = FMath::Pow(FMath::Clamp(White, 0.f, 1.f), 0.5f);
		const FLinearColor Color = FLinearColor(1.f, 1.f, 1.f, Alpha) * InWidgetStyle.GetColorAndOpacityTint();
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			Layer + 1,
			AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2D::ZeroVector)),
			Brush,
			ESlateDrawEffect::None,
			Color);
	}

	return Layer + 1;
}
