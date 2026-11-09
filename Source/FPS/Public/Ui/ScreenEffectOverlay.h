#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScreenEffectOverlay.generated.h"

// Full-screen, click-through overlay: flashbang whiteout and corner blood.
UCLASS()
class FPS_API UScreenEffectOverlay : public UUserWidget
{
	GENERATED_BODY()

public:
	UScreenEffectOverlay(const FObjectInitializer& ObjectInitializer);
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
