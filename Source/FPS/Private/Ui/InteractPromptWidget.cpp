#include "Ui/InteractPromptWidget.h"

#include "ShooterPlayerController.h"
#include "Ui/TacticalUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UInteractPromptWidget::RebuildWidget()
{
	PanelBrush = FTacticalUIStyle::MakePanelBrush(FTacticalUIStyle::Panel, 6.f, 1.f);
	return SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 120.f))
		[
			SAssignNew(Box, SBorder)
			.BorderImage(&PanelBrush)
			.Padding(FMargin(18.f, 10.f))
			.Visibility(EVisibility::Collapsed)
			[
				SAssignNew(Label, STextBlock)
				.Font(FTacticalUIStyle::MakeFont(20, true))
				.ColorAndOpacity(FTacticalUIStyle::Text)
				.Justification(ETextJustify::Center)
			]
		];
}

void UInteractPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	(void)InDeltaTime;
	if (!Label || !Box) return;
	FText Prompt;
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		Prompt = PC->GetInteractPromptText();
	}
	Label->SetText(Prompt);
	Box->SetVisibility(Prompt.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
}

void UInteractPromptWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Box.Reset();
	Label.Reset();
}
