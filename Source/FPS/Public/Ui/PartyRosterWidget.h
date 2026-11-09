#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "PartyRosterWidget.generated.h"

class SButton;
class STextBlock;
class SVerticalBox;

/** Top-right room list and the invite button. Drawn with Slate so it cannot collapse to an empty box. */
UCLASS()
class FPS_API UPartyRosterWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	void RefreshRoster();
	FReply OnInviteClicked();
	FReply OnLeaveClicked();

	TSharedPtr<SVerticalBox> Rows;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<SButton> InviteButton;
	TSharedPtr<SButton> LeaveButton;
	FSlateBrush PanelBrush;
	FButtonStyle ButtonStyle;
	FString LastSignature;
	float RefreshWait = 0.f;
};
