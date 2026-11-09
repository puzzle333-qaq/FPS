#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractPromptWidget.generated.h"

class SBorder;
class STextBlock;

/** Bottom-center "press F" line. The shooter overlay only has health and ammo. */
UCLASS()
class FPS_API UInteractPromptWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	TSharedPtr<SBorder> Box;
	TSharedPtr<STextBlock> Label;
	FSlateBrush PanelBrush;
};
