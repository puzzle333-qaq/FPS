#include "Ui/InteractPromptWidget.h"

#include "ShooterPlayerController.h"
#include "Ui/OperationUIStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UInteractPromptWidget::RebuildWidget()
{
	PanelBrush = FOperationUIStyle::MakePanelBrush(FLinearColor::FromSRGBColor(FColor(26,28,30)), 2.f, 1.f);
	return SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 96.f, 0.f, 0.f))
		[
			SAssignNew(Box, SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
			.Padding(0)
			.Visibility(EVisibility::Collapsed)
			[
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(6,2))
                 [SNew(STextBlock).Text(FText::FromString(TEXT("F"))).Font(FOperationUIStyle::MakeFont(13,true)).ColorAndOpacity(FOperationUIStyle::Text)
                  .ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))]]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8,0,0,0))
                [SAssignNew(Label, STextBlock).Font(FOperationUIStyle::MakeFont(16))
                 .ColorAndOpacity(FOperationUIStyle::Text).ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))]
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
 FString Action=Prompt.ToString();
 for(const TCHAR* Prefix:{TEXT("按 F："),TEXT("按 F "),TEXT("F "),TEXT("[F] ")})
  if(Action.StartsWith(Prefix)){Action.RightChopInline(FCString::Strlen(Prefix));break;}
 Action=Action.Replace(TEXT("Close door"),TEXT("关门")).Replace(TEXT("Kick door"),TEXT("踹门")).Replace(TEXT("Push door"),TEXT("推门"));
 Label->SetText(FText::FromString(Action));
	Box->SetVisibility(Prompt.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
}

void UInteractPromptWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Box.Reset();
	Label.Reset();
}
