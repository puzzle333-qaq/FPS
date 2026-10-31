#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BodyInjuryHud.generated.h"

class UTexture2D;

/** One zone = list of quads (normalized 0..1 over the widget). Corners: TL, TR, BR, BL. */
USTRUCT(BlueprintType)
struct FBodyInjuryQuad
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D TL = FVector2D(0, 0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D TR = FVector2D(1, 0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D BR = FVector2D(1, 1);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D BL = FVector2D(0, 1);
};

/**
 * Paper-doll HUD: draws T_Body, then redraws T_Body clipped to injured zones in red.
 * Reads AShooterCharacter::IsHitZoneMarked (Head/Torso/Arm/Leg) every paint. No masks needed.
 */
UCLASS(Blueprintable)
class FPS_API UBodyInjuryHud : public UUserWidget
{
	GENERATED_BODY()
public:
	UBodyInjuryHud(const FObjectInitializer& OI);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BodyInjury") TObjectPtr<UTexture2D> BodyTexture;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BodyInjury") FLinearColor HealthyColor = FLinearColor(1, 1, 1, 0.85f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BodyInjury") FLinearColor InjuredColor = FLinearColor(0.9f, 0.05f, 0.05f, 1.f);
	/** Debug: force all zones red in designer/PIE to tune quads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BodyInjury") bool bPreviewAllInjured = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BodyInjury|Zones") TArray<FBodyInjuryQuad> HeadZone;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BodyInjury|Zones") TArray<FBodyInjuryQuad> TorsoZone;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BodyInjury|Zones") TArray<FBodyInjuryQuad> ArmZone;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BodyInjury|Zones") TArray<FBodyInjuryQuad> LegZone;

	/** Manual override from Blueprint (OR'ed with character flags). */
	UFUNCTION(BlueprintCallable, Category = "BodyInjury") void SetZoneOverride(bool bHead, bool bTorso, bool bArm, bool bLeg);

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual void NativeTick(const FGeometry& G, float Dt) override;
private:
	bool bOvHead = false, bOvTorso = false, bOvArm = false, bOvLeg = false;
	int32 DrawZone(const TArray<FBodyInjuryQuad>& Zone, const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FSlateBrush& Brush) const;
};