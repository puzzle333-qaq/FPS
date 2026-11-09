#include "Ui/PlayerNameplateWidget.h"

#include "Blueprint/WidgetTree.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"
#include "ShooterCharacter.h"
#include "Ui/TacticalUIStyle.h"

void UPlayerNameplateWidget::SetSubject(AShooterCharacter* Subject)
{
	SubjectActor = Subject;
}

TSharedRef<SWidget> UPlayerNameplateWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		Initialize();
	}
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildPlate();
	}
	return Super::RebuildWidget();
}

void UPlayerNameplateWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UPlayerNameplateWidget::BuildPlate()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("NameRoot"));
	WidgetTree->RootWidget = Root;
	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameLabel"));
	NameText->SetFont(FTacticalUIStyle::MakeFont(14, true));
	NameText->SetColorAndOpacity(FTacticalUIStyle::Text);
	NameText->SetJustification(ETextJustify::Center);
	NameText->SetShadowOffset(FVector2D(1.f, 1.f));
	NameText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	if (UCanvasPanelSlot* LabelSlot = Root->AddChildToCanvas(NameText))
	{
		LabelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		LabelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		LabelSlot->SetAutoSize(true);
	}
}

void UPlayerNameplateWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	(void)InDeltaTime;
	if (!NameText) return;
	const AShooterCharacter* Body = SubjectActor.Get();
	const APlayerState* PS = Body ? Body->GetPlayerState() : nullptr;
	const FString Next = PS ? PS->GetPlayerName() : FString();
	if (Next == ShownName) return;
	ShownName = Next;
	NameText->SetText(FText::FromString(Next.IsEmpty() ? TEXT("…") : Next));
}
