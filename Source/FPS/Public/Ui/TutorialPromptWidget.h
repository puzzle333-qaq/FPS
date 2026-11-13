#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TutorialPromptWidget.generated.h"

class SBorder;
class STextBlock;

/** Local-only tutorial hint. Filled by AShooterPlayerController::GetTutorialPromptText. */
UCLASS()
class FPS_API UTutorialPromptWidget : public UUserWidget
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
