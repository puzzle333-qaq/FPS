#include "Ui/BodyInjuryHud.h"
#include "ShooterCharacter.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"

static FBodyInjuryQuad MakeQ(float X0, float Y0, float X1, float Y1)
{
	FBodyInjuryQuad Q; Q.TL = FVector2D(X0, Y0); Q.TR = FVector2D(X1, Y0); Q.BR = FVector2D(X1, Y1); Q.BL = FVector2D(X0, Y1); return Q;
}

UBodyInjuryHud::UBodyInjuryHud(const FObjectInitializer& OI) : Super(OI)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> Tex(TEXT("/Game/FPS/T_Body.T_Body"));
	if (Tex.Succeeded()) BodyTexture = Tex.Object;
	// Defaults tuned for T_Body (A-pose, 512x512, feet at bottom). Tweak in WBP Details.
	HeadZone  = { MakeQ(0.40f, 0.00f, 0.60f, 0.355f) };
	TorsoZone = { MakeQ(0.405f, 0.355f, 0.595f, 0.66f) };
	ArmZone   = { MakeQ(0.00f, 0.36f, 0.405f, 1.f), MakeQ(0.595f, 0.36f, 1.f, 1.f) };
	LegZone   = { MakeQ(0.405f, 0.66f, 0.595f, 1.f) };
}

void UBodyInjuryHud::SetZoneOverride(bool bHead, bool bTorso, bool bArm, bool bLeg)
{
	bOvHead = bHead; bOvTorso = bTorso; bOvArm = bArm; bOvLeg = bLeg; Invalidate(EInvalidateWidgetReason::Paint);
}

void UBodyInjuryHud::NativeTick(const FGeometry& G, float Dt)
{
	Super::NativeTick(G, Dt);
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 UBodyInjuryHud::DrawZone(const TArray<FBodyInjuryQuad>& Zone, const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FSlateBrush& Brush) const
{
	const FVector2D Size = G.GetLocalSize();
	for (const FBodyInjuryQuad& Q : Zone)
	{
		auto P = [&](const FVector2D& N) { return G.LocalToAbsolute(N * Size); };
		Out.PushClip(FSlateClippingZone(P(Q.TL), P(Q.TR), P(Q.BL), P(Q.BR)));
		FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(), &Brush, ESlateDrawEffect::None, InjuredColor);
		Out.PopClip();
	}
	return Layer;
}

int32 UBodyInjuryHud::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	Layer = Super::NativePaint(Args, G, Cull, Out, Layer, Style, bParentEnabled);
	if (!BodyTexture) return Layer;
	FSlateBrush Brush; Brush.SetResourceObject(BodyTexture); Brush.ImageSize = G.GetLocalSize();
	++Layer;
	FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(), &Brush, ESlateDrawEffect::None, HealthyColor);

	bool bH = bOvHead, bT = bOvTorso, bA = bOvArm, bL = bOvLeg;
	if (const AShooterCharacter* C = Cast<AShooterCharacter>(GetOwningPlayerPawn()))
	{
		bH |= C->IsHitZoneMarked(EHitZone::Head); bT |= C->IsHitZoneMarked(EHitZone::Torso);
		bA |= C->IsHitZoneMarked(EHitZone::Arm);  bL |= C->IsHitZoneMarked(EHitZone::Leg);
	}
	if (bPreviewAllInjured) { bH = bT = bA = bL = true; }
	++Layer;
	if (bH) DrawZone(HeadZone, G, Out, Layer, Brush);
	if (bT) DrawZone(TorsoZone, G, Out, Layer, Brush);
	if (bA) DrawZone(ArmZone, G, Out, Layer, Brush);
	if (bL) DrawZone(LegZone, G, Out, Layer, Brush);
	return Layer;
}