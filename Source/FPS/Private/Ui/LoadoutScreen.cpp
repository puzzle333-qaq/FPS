#include "Ui/LoadoutScreen.h"

#include "Ui/LoadoutPreviewRig.h"
#include "Ui/AttachmentSlotRow.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Engine/Font.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TimerManager.h"
#include "Blueprint/WidgetTree.h"
#include "InputCoreTypes.h"

namespace
{
	FSlateFontInfo LoadoutFont(int32 Size)
	{
		FSlateFontInfo Font;
		Font.FontObject = LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
		Font.TypefaceFontName = TEXT("Bold");
		Font.Size = Size;
		return Font;
	}

	void FillCanvas(UCanvasPanelSlot* PanelSlot, float MinX, float MinY, float MaxX, float MaxY, FMargin Offsets)
	{
		if (!IsValid(PanelSlot)) return;
		PanelSlot->SetAnchors(FAnchors(MinX, MinY, MaxX, MaxY));
		PanelSlot->SetOffsets(Offsets);
		PanelSlot->SetAlignment(FVector2D::ZeroVector);
	}

	FButtonStyle TransparentButtonStyle()
	{
		FButtonStyle Style;
		auto Clear = [](FSlateBrush& Brush, const FLinearColor& Tint)
		{
			Brush.DrawAs = Tint.A <= 0.f ? ESlateBrushDrawType::NoDrawType : ESlateBrushDrawType::Box;
			Brush.TintColor = FSlateColor(Tint);
		};
		Clear(Style.Normal, FLinearColor(0.f, 0.f, 0.f, 0.f));
		Clear(Style.Hovered, FLinearColor(1.f, 0.78f, 0.25f, 0.16f));
		Clear(Style.Pressed, FLinearColor(1.f, 0.78f, 0.25f, 0.30f));
		Clear(Style.Disabled, FLinearColor(0.f, 0.f, 0.f, 0.f));
		return Style;
	}
}

void ULoadoutAttachmentWindow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(false);
	BuildLayout();
}

void ULoadoutAttachmentWindow::NativeDestruct()
{
	BindWeapon(nullptr);
	Super::NativeDestruct();
}

void ULoadoutAttachmentWindow::BuildLayout()
{
	if (bBuilt || !WidgetTree) return;
	bBuilt = true;

	USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("WindowSize"));
	RootSize->SetMinDesiredWidth(420.f);
	WidgetTree->RootWidget = RootSize;

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
	FSlateBrush FrameBrush;
	FrameBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	FrameBrush.TintColor = FSlateColor(FLinearColor(0.07f, 0.08f, 0.1f, 0.96f));
	FrameBrush.OutlineSettings.CornerRadii = FVector4(8.f, 8.f, 8.f, 8.f);
	FrameBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	FrameBrush.OutlineSettings.Width = 1.5f;
	FrameBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.85f, 0.68f, 0.22f, 1.f));
	Frame->SetBrush(FrameBrush);
	Frame->SetPadding(FMargin(14.f, 10.f, 14.f, 14.f));
	RootSize->AddChild(Frame);

	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Body"));
	Frame->AddChild(Body);

	DragHandle = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DragHandle"));
	FSlateBrush HandleBrush;
	HandleBrush.DrawAs = ESlateBrushDrawType::Box;
	HandleBrush.TintColor = FSlateColor(FLinearColor(0.16f, 0.13f, 0.07f, 1.f));
	DragHandle->SetBrush(HandleBrush);
	DragHandle->SetPadding(FMargin(10.f, 8.f, 10.f, 8.f));
	if (UVerticalBoxSlot* HandleSlot = Body->AddChildToVerticalBox(DragHandle))
	{
		HandleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}

	Text_WeaponName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_WeaponName"));
	Text_WeaponName->SetFont(LoadoutFont(20));
	Text_WeaponName->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.82f, 0.35f, 1.f)));
	Text_WeaponName->SetText(NSLOCTEXT("Loadout", "Attachments", "配件"));
	DragHandle->AddChild(Text_WeaponName);

	Text_Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Empty"));
	Text_Empty->SetFont(LoadoutFont(14));
	Text_Empty->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f, 1.f)));
	Text_Empty->SetText(NSLOCTEXT("Loadout", "NoSlots", "这把枪没有配件槽"));
	Text_Empty->SetVisibility(ESlateVisibility::Collapsed);
	Body->AddChildToVerticalBox(Text_Empty);

	SlotList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SlotList"));
	Body->AddChildToVerticalBox(SlotList);

	RowClass = LoadClass<UAttachmentSlotRow>(nullptr, TEXT("/Game/FPS/UI/WBP_AttachmentSlotRow.WBP_AttachmentSlotRow_C"));
	if (!RowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Loadout attachment window could not load WBP_AttachmentSlotRow."));
	}
}

void ULoadoutAttachmentWindow::ShowForWeapon(AWeapon* InWeapon)
{
	SetVisibility(ESlateVisibility::Visible);
	BindWeapon(InWeapon);
	RebuildSlots();
}

void ULoadoutAttachmentWindow::SetWindowPosition(FVector2D Position)
{
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CanvasSlot->SetPosition(Position);
	}
}

void ULoadoutAttachmentWindow::BindWeapon(AWeapon* InWeapon)
{
	if (Weapon.Get() == InWeapon) return;

	if (Weapon.IsValid())
	{
		Weapon->OnAttachmentsChanged.RemoveDynamic(this, &ThisClass::HandleAttachmentsChanged);
	}
	Weapon = InWeapon;
	if (IsValid(InWeapon))
	{
		InWeapon->OnAttachmentsChanged.AddUniqueDynamic(this, &ThisClass::HandleAttachmentsChanged);
	}
}

void ULoadoutAttachmentWindow::HandleAttachmentsChanged(AWeapon* ChangedWeapon)
{
	if (ChangedWeapon != Weapon.Get()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// 点击发生在按钮里。同一帧拆掉按钮会把这次点击弄丢。
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsValid(this))
		{
			RebuildSlots();
		}
	}));
}

void ULoadoutAttachmentWindow::RebuildSlots()
{
	if (!IsValid(SlotList)) return;
	SlotList->ClearChildren();

	const bool bHasWeapon = Weapon.IsValid();
	if (IsValid(Text_WeaponName))
	{
		const FText Name = bHasWeapon ? Weapon->GetClass()->GetDisplayNameText() : FText::GetEmpty();
		Text_WeaponName->SetText(Name.IsEmpty() ? NSLOCTEXT("Loadout", "Attachments", "配件") : Name);
	}

	const TArray<FWeaponAttachmentSlot> Slots = bHasWeapon ? Weapon->GetAttachmentSlots() : TArray<FWeaponAttachmentSlot>();
	if (IsValid(Text_Empty))
	{
		Text_Empty->SetVisibility(Slots.Num() == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (!bHasWeapon || !RowClass) return;

	AWeapon* Target = Weapon.Get();
	for (const FWeaponAttachmentSlot& SlotInfo : Slots)
	{
		if (UAttachmentSlotRow* Row = CreateWidget<UAttachmentSlotRow>(this, RowClass))
		{
			Row->Setup(Target, SlotInfo);
			if (UVerticalBoxSlot* RowSlot = SlotList->AddChildToVerticalBox(Row))
			{
				RowSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 4.f));
			}
		}
	}
}

FReply ULoadoutAttachmentWindow::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !IsValid(DragHandle))
	{
		return FReply::Unhandled();
	}
	if (!DragHandle->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Unhandled();
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
	UWidget* Parent = CanvasSlot ? CanvasSlot->Parent : nullptr;
	if (!CanvasSlot || !IsValid(Parent))
	{
		return FReply::Unhandled();
	}

	const FVector2D LocalClick = Parent->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	DragOffset = LocalClick - CanvasSlot->GetPosition();
	bDragging = true;
	bUserMoved = true;
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply ULoadoutAttachmentWindow::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging)
	{
		return FReply::Unhandled();
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
	UWidget* Parent = CanvasSlot ? CanvasSlot->Parent : nullptr;
	if (!CanvasSlot || !IsValid(Parent))
	{
		return FReply::Unhandled();
	}

	const FVector2D Local = Parent->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	CanvasSlot->SetPosition(Local - DragOffset);
	return FReply::Handled();
}

FReply ULoadoutAttachmentWindow::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	bDragging = false;
	return FReply::Handled().ReleaseMouseCapture();
}

ULoadoutScreen::ULoadoutScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void ULoadoutScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
}

void ULoadoutScreen::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshWeapons();
}

void ULoadoutScreen::NativeDestruct()
{
	for (AWeapon* Weapon : ShownWeapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->OnAttachmentsChanged.RemoveDynamic(this, &ThisClass::HandleAttachmentsChanged);
		}
	}
	ShownWeapons.Reset();

	if (IsValid(PreviewRig))
	{
		PreviewRig->Destroy();
		PreviewRig = nullptr;
	}

	Super::NativeDestruct();
}

void ULoadoutScreen::BuildLayout()
{
	if (bBuilt || !WidgetTree) return;
	bBuilt = true;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	FSlateBrush DimBrush;
	DimBrush.DrawAs = ESlateBrushDrawType::Box;
	DimBrush.TintColor = FSlateColor(FLinearColor(0.01f, 0.012f, 0.016f, 0.90f));
	Dim->SetBrush(DimBrush);
	FillCanvas(Root->AddChildToCanvas(Dim), 0.f, 0.f, 1.f, 1.f, FMargin(0.f));

	UBorder* WeaponPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("WeaponPanel"));
	FSlateBrush PanelBrush;
	PanelBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	PanelBrush.TintColor = FSlateColor(FLinearColor(0.06f, 0.07f, 0.09f, 0.94f));
	PanelBrush.OutlineSettings.CornerRadii = FVector4(8.f, 8.f, 8.f, 8.f);
	PanelBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	PanelBrush.OutlineSettings.Width = 1.f;
	PanelBrush.OutlineSettings.Color = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.08f));
	WeaponPanel->SetBrush(PanelBrush);
	WeaponPanel->SetPadding(FMargin(16.f));
	WeaponPanel->SetHorizontalAlignment(HAlign_Fill);
	WeaponPanel->SetVerticalAlignment(VAlign_Fill);
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(WeaponPanel))
	{
		FillCanvas(PanelSlot, 0.f, 0.f, 0.40f, 1.f, FMargin(28.f, 28.f, 16.f, 28.f));
		PanelSlot->SetZOrder(1);
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WeaponColumn"));
	WeaponPanel->AddChild(Column);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Title->SetText(NSLOCTEXT("Loadout", "Weapons", "武器"));
	Title->SetFont(LoadoutFont(28));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetPadding(FMargin(4.f, 0.f, 0.f, 2.f));
	}

	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Hint"));
	Hint->SetText(NSLOCTEXT("Loadout", "CloseHint", "Tab 关闭"));
	Hint->SetFont(LoadoutFont(14));
	Hint->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.f)));
	if (UVerticalBoxSlot* HintSlot = Column->AddChildToVerticalBox(Hint))
	{
		HintSlot->SetPadding(FMargin(4.f, 0.f, 0.f, 12.f));
	}

	UOverlay* PreviewOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PreviewOverlay"));
	if (UVerticalBoxSlot* PreviewSlot = Column->AddChildToVerticalBox(PreviewOverlay))
	{
		PreviewSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		PreviewSlot->SetHorizontalAlignment(HAlign_Fill);
		PreviewSlot->SetVerticalAlignment(VAlign_Fill);
	}

	PreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PreviewImage"));
	PreviewImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* ImageSlot = PreviewOverlay->AddChildToOverlay(PreviewImage))
	{
		ImageSlot->SetHorizontalAlignment(HAlign_Fill);
		ImageSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* Hits = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WeaponHits"));
	if (UOverlaySlot* HitSlot = PreviewOverlay->AddChildToOverlay(Hits))
	{
		HitSlot->SetHorizontalAlignment(HAlign_Fill);
		HitSlot->SetVerticalAlignment(VAlign_Fill);
	}

	const FButtonStyle Style = TransparentButtonStyle();
	WeaponButtons.Reset();
	WeaponLabels.Reset();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("WeaponHit%d"), Index));
		Button->SetStyle(Style);
		if (UVerticalBoxSlot* ButtonSlot = Hits->AddChildToVerticalBox(Button))
		{
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			ButtonSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("WeaponLabel%d"), Index));
		Label->SetFont(LoadoutFont(16));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Label->SetJustification(ETextJustify::Center);
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlay* LabelOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass()))
		{
			Button->AddChild(LabelOverlay);
			if (UOverlaySlot* LabelSlot = LabelOverlay->AddChildToOverlay(Label))
			{
				LabelSlot->SetHorizontalAlignment(HAlign_Center);
				LabelSlot->SetVerticalAlignment(VAlign_Bottom);
				LabelSlot->SetPadding(FMargin(8.f, 8.f, 8.f, 10.f));
			}
		}

		WeaponButtons.Add(Button);
		WeaponLabels.Add(Label);
	}

	if (WeaponButtons.IsValidIndex(0))
	{
		WeaponButtons[0]->OnClicked.AddDynamic(this, &ThisClass::OnClickedWeapon0);
	}
	if (WeaponButtons.IsValidIndex(1))
	{
		WeaponButtons[1]->OnClicked.AddDynamic(this, &ThisClass::OnClickedWeapon1);
	}
	if (WeaponButtons.IsValidIndex(2))
	{
		WeaponButtons[2]->OnClicked.AddDynamic(this, &ThisClass::OnClickedWeapon2);
	}

	PopupLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PopupLayer"));
	PopupLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* PopupSlot = Root->AddChildToCanvas(PopupLayer))
	{
		FillCanvas(PopupSlot, 0.f, 0.f, 1.f, 1.f, FMargin(0.f));
		PopupSlot->SetZOrder(2);
	}
}

void ULoadoutScreen::SetCardLabel(int32 Index, const FText& Label, bool bEnabled)
{
	if (WeaponLabels.IsValidIndex(Index) && IsValid(WeaponLabels[Index]))
	{
		WeaponLabels[Index]->SetText(Label);
	}
	if (WeaponButtons.IsValidIndex(Index) && IsValid(WeaponButtons[Index]))
	{
		WeaponButtons[Index]->SetIsEnabled(bEnabled);
	}
}

void ULoadoutScreen::RefreshWeapons()
{
	for (AWeapon* Weapon : ShownWeapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->OnAttachmentsChanged.RemoveDynamic(this, &ThisClass::HandleAttachmentsChanged);
		}
	}

	ShownWeapons.Reset();
	ShownWeapons.SetNum(3);

	UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetOwningPlayerPawn());
	static const TArray<AWeapon*> EmptyInventory;
	const TArray<AWeapon*>& Inventory = IsValid(Combat) ? Combat->GetInventory() : EmptyInventory;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		AWeapon* Weapon = Inventory.IsValidIndex(Index) ? Inventory[Index] : nullptr;
		if (!IsValid(Weapon))
		{
			SetCardLabel(Index, NSLOCTEXT("Loadout", "EmptySlot", "空"), false);
			continue;
		}

		ShownWeapons[Index] = Weapon;
		Weapon->OnAttachmentsChanged.AddUniqueDynamic(this, &ThisClass::HandleAttachmentsChanged);
		FText Name = Weapon->GetClass()->GetDisplayNameText();
		if (Name.IsEmpty())
		{
			Name = FText::FromString(Weapon->GetClass()->GetName());
		}
		SetCardLabel(Index, Name, true);
	}

	UWorld* World = GetWorld();
	if (!IsValid(PreviewRig) && World)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags = RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PreviewRig = World->SpawnActor<ALoadoutPreviewRig>(FVector(0.f, 0.f, -100000.f), FRotator::ZeroRotator, Params);
	}

	if (IsValid(PreviewRig))
	{
		TArray<AWeapon*> Weapons;
		Weapons.Reserve(3);
		for (AWeapon* Weapon : ShownWeapons)
		{
			Weapons.Add(Weapon);
		}
		PreviewRig->ShowWeapons(Weapons);

		if (IsValid(PreviewImage) && IsValid(PreviewRig->GetRenderTarget()))
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(PreviewRig->GetRenderTarget());
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.ImageSize = FVector2D(512.f, 768.f);
			PreviewImage->SetBrush(Brush);
		}

		if (World)
		{
			TWeakObjectPtr<ALoadoutPreviewRig> Rig = PreviewRig;
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [Rig]()
			{
				if (Rig.IsValid())
				{
					Rig->Capture();
				}
			}));
		}
	}
}

void ULoadoutScreen::HandleAttachmentsChanged(AWeapon* ChangedWeapon)
{
	if (!IsValid(ChangedWeapon) || !ShownWeapons.Contains(ChangedWeapon) || !IsValid(PreviewRig)) return;

	TArray<AWeapon*> Weapons;
	for (AWeapon* Weapon : ShownWeapons)
	{
		Weapons.Add(Weapon);
	}
	PreviewRig->ShowWeapons(Weapons);
}

void ULoadoutScreen::OnClickedWeapon0() { OpenWindow(0); }
void ULoadoutScreen::OnClickedWeapon1() { OpenWindow(1); }
void ULoadoutScreen::OnClickedWeapon2() { OpenWindow(2); }

void ULoadoutScreen::OpenWindow(int32 Index)
{
	if (!ShownWeapons.IsValidIndex(Index) || !IsValid(ShownWeapons[Index]) || !IsValid(PopupLayer)) return;

	if (!IsValid(AttachmentWindow))
	{
		AttachmentWindow = CreateWidget<ULoadoutAttachmentWindow>(GetOwningPlayer(), ULoadoutAttachmentWindow::StaticClass());
		if (!IsValid(AttachmentWindow)) return;

		if (UCanvasPanelSlot* WindowSlot = PopupLayer->AddChildToCanvas(AttachmentWindow))
		{
			WindowSlot->SetAutoSize(true);
			WindowSlot->SetAnchors(FAnchors(0.f, 0.f));
			WindowSlot->SetAlignment(FVector2D::ZeroVector);
			WindowSlot->SetZOrder(5);
		}
	}

	if (!AttachmentWindow->HasUserMoved())
	{
		int32 ViewX = 1280;
		int32 ViewY = 720;
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->GetViewportSize(ViewX, ViewY);
		}
		AttachmentWindow->SetWindowPosition(FVector2D(ViewX * 0.42f, 72.f));
	}

	AttachmentWindow->ShowForWeapon(ShownWeapons[Index]);
}
