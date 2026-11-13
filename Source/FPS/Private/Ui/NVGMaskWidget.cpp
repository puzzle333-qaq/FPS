#include "Ui/NVGMaskWidget.h"

#include "ShooterCharacter.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"

UNVGMaskWidget::UNVGMaskWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasScriptImplementedTick = true;
}

TSharedRef<SWidget> UNVGMaskWidget::RebuildWidget()
{
	return SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible);
}

void UNVGMaskWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	(void)InDeltaTime;
	const AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwningPlayerPawn());
	const bool bDual = IsValid(Shooter)
		&& Shooter->GetTacticalVisionMode() == ETacticalVisionMode::NVG
		&& Shooter->GetNVGHousing() == ENVGHousing::Dual;
	SetVisibility(bDual ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bDual)
	{
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

int32 UNVGMaskWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwningPlayerPawn());
	if (!IsValid(Shooter)
		|| Shooter->GetTacticalVisionMode() != ETacticalVisionMode::NVG
		|| Shooter->GetNVGHousing() != ENVGHousing::Dual)
	{
		return Layer;
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X < 8.f || Size.Y < 8.f) return Layer;
	const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	if (!White || !White->GetRenderingResource().IsValid()) return Layer;

	const FVector2D C1(Size.X * 0.355f, Size.Y * 0.50f);
	const FVector2D C2(Size.X * 0.645f, Size.Y * 0.50f);
	const float Radius = Size.Y * 0.34f;
	const float RadiusSq = Radius * Radius;
	const float Rim = FMath::Max(0.f, Radius - 7.f);
	const float RimSq = Rim * Rim;

	auto DistSq = [](const FVector2D& A, const FVector2D& B)
	{
		return FVector2D::DistSquared(A, B);
	};
	auto Outside = [&](const FVector2D& P)
	{
		return DistSq(P, C1) > RadiusSq && DistSq(P, C2) > RadiusSq;
	};
	auto OnRim = [&](const FVector2D& P)
	{
		const float D1 = DistSq(P, C1);
		const float D2 = DistSq(P, C2);
		const float D = FMath::Min(D1, D2);
		return D <= RadiusSq && D >= RimSq;
	};

	const FSlateRenderTransform& Xform = AllottedGeometry.GetAccumulatedRenderTransform();
	const FColor Black(0, 0, 0, 255);
	const FColor Lip(0, 14, 0, 255);
	TArray<FSlateVertex> Verts;
	TArray<SlateIndex> Indexes;
	Verts.Reserve(8000);
	Indexes.Reserve(12000);

	auto Emit = [&](const FVector2D& Min, const FVector2D& Max, const FColor& Color)
	{
		const int32 Base = Verts.Num();
		const FVector2f UV(0.f, 0.f);
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Xform, FVector2f(Min.X, Min.Y), UV, UV, Color));
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Xform, FVector2f(Max.X, Min.Y), UV, UV, Color));
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Xform, FVector2f(Max.X, Max.Y), UV, UV, Color));
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Xform, FVector2f(Min.X, Max.Y), UV, UV, Color));
		Indexes.Add(static_cast<SlateIndex>(Base));
		Indexes.Add(static_cast<SlateIndex>(Base + 1));
		Indexes.Add(static_cast<SlateIndex>(Base + 2));
		Indexes.Add(static_cast<SlateIndex>(Base));
		Indexes.Add(static_cast<SlateIndex>(Base + 2));
		Indexes.Add(static_cast<SlateIndex>(Base + 3));
	};

	auto PaintCell = [&](float X0, float Y0, float X1, float Y1)
	{
		const FVector2D P00(X0, Y0);
		const FVector2D P10(X1, Y0);
		const FVector2D P11(X1, Y1);
		const FVector2D P01(X0, Y1);
		const int32 OutCount = (Outside(P00) ? 1 : 0) + (Outside(P10) ? 1 : 0) + (Outside(P11) ? 1 : 0) + (Outside(P01) ? 1 : 0);
		const int32 RimCount = (OnRim(P00) ? 1 : 0) + (OnRim(P10) ? 1 : 0) + (OnRim(P11) ? 1 : 0) + (OnRim(P01) ? 1 : 0);
		if (OutCount == 4) Emit(P00, P11, Black);
		else if (RimCount == 4) Emit(P00, P11, Lip);
		else
		{
			const FVector2D Center((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f);
			if (Outside(Center)) Emit(P00, P11, Black);
			else if (OnRim(Center)) Emit(P00, P11, Lip);
		}
	};

	constexpr int32 Columns = 64;
	constexpr int32 Rows = 36;
	const float CellW = Size.X / Columns;
	const float CellH = Size.Y / Rows;
	for (int32 Y = 0; Y < Rows; ++Y)
	{
		for (int32 X = 0; X < Columns; ++X)
		{
			const float X0 = X * CellW;
			const float Y0 = Y * CellH;
			const float X1 = X0 + CellW;
			const float Y1 = Y0 + CellH;
			const int32 OutCount =
				(Outside(FVector2D(X0, Y0)) ? 1 : 0) + (Outside(FVector2D(X1, Y0)) ? 1 : 0)
				+ (Outside(FVector2D(X1, Y1)) ? 1 : 0) + (Outside(FVector2D(X0, Y1)) ? 1 : 0);
			if (OutCount == 4)
			{
				Emit(FVector2D(X0, Y0), FVector2D(X1, Y1), Black);
				continue;
			}
			if (OutCount == 0)
			{
				const int32 RimCount =
					(OnRim(FVector2D(X0, Y0)) ? 1 : 0) + (OnRim(FVector2D(X1, Y0)) ? 1 : 0)
					+ (OnRim(FVector2D(X1, Y1)) ? 1 : 0) + (OnRim(FVector2D(X0, Y1)) ? 1 : 0);
				if (RimCount == 4)
				{
					Emit(FVector2D(X0, Y0), FVector2D(X1, Y1), Lip);
				}
				continue;
			}
			constexpr int32 Sub = 8;
			const float SW = CellW / Sub;
			const float SH = CellH / Sub;
			for (int32 SY = 0; SY < Sub; ++SY)
			{
				for (int32 SX = 0; SX < Sub; ++SX)
				{
					PaintCell(X0 + SX * SW, Y0 + SY * SH, X0 + (SX + 1) * SW, Y0 + (SY + 1) * SH);
				}
			}
		}
	}

	if (Verts.Num() > 0)
	{
		FSlateDrawElement::MakeCustomVerts(
			OutDrawElements, Layer + 1, White->GetRenderingResource(), Verts, Indexes, nullptr, 0, 0);
	}
	(void)InWidgetStyle;
	(void)bParentEnabled;
	return Layer + 1;
}
