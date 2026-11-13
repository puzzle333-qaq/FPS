#include "Ui/TutorialPromptWidget.h"

#include "ShooterPlayerController.h"
#include "Ui/OperationUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UTutorialPromptWidget::RebuildWidget()
{
	PanelBrush = FOperationUIStyle::MakePanelBrush(FOperationUIStyle::Panel, 6.f, 1.f);
	return SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.f, 96.f, 0.f, 0.f))
		[
			SAssignNew(Box, SBorder)
			.BorderImage(&PanelBrush)
			.Padding(FMargin(24.f, 16.f))
			.Visibility(EVisibility::Collapsed)
			[
				SAssignNew(Label, STextBlock)
				.Font(FOperationUIStyle::MakeFont(20))
				.ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))
				.ColorAndOpacity(FOperationUIStyle::Text)
				.Justification(ETextJustify::Center)
				.AutoWrapText(true)
				.WrappingPolicy(ETextWrappingPolicy::DefaultWrapping)
			]
		];
}

void UTutorialPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	(void)InDeltaTime;
	if (!Label || !Box) return;
	FText Prompt;
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		Prompt = PC->GetTutorialPromptText();
	}
	Label->SetText(Prompt);
	Box->SetVisibility(Prompt.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
}

void UTutorialPromptWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Box.Reset();
	Label.Reset();
}
