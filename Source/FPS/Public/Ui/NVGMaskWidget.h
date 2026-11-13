#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NVGMaskWidget.generated.h"

/** Black dual-tube housing. Hidden for quad NVG and when the tubes are off. */
UCLASS()
class FPS_API UNVGMaskWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UNVGMaskWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
