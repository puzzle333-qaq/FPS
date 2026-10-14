#include "Ui/LoadoutScreen.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

#include "Ui/LoadoutPreviewRig.h"
#include "Ui/LoadoutWeaponCard.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/HorizontalBox.h"
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
	// WBP 子类：树里已经有控件（绑定好了），不再生成默认布局
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnCloseClicked);
	}
	if (IsValid(Text_Empty))
	{
		Text_Empty->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ULoadoutAttachmentWindow::NativeDestruct()
{
	BindWeapon(nullptr);
	Super::NativeDestruct();
}

void ULoadoutAttachmentWindow::BuildLayout()
{
	if (!WidgetTree) return;

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

	UBorder* Handle = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DragHandle"));
	FSlateBrush HandleBrush;
	HandleBrush.DrawAs = ESlateBrushDrawType::Box;
	HandleBrush.TintColor = FSlateColor(FLinearColor(0.16f, 0.13f, 0.07f, 1.f));
	Handle->SetBrush(HandleBrush);
	Handle->SetPadding(FMargin(10.f, 8.f, 10.f, 8.f));
	DragHandle = Handle;
	if (UVerticalBoxSlot* HandleSlot = Body->AddChildToVerticalBox(Handle))
	{
		HandleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}

	UHorizontalBox* HandleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HandleRow"));
	Handle->AddChild(HandleRow);

	Text_WeaponName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_WeaponName"));
	Text_WeaponName->SetFont(LoadoutFont(20));
	Text_WeaponName->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.82f, 0.35f, 1.f)));
	Text_WeaponName->SetText(NSLOCTEXT("Loadout", "Attachments", "配件"));
	if (UHorizontalBoxSlot* NameSlot = HandleRow->AddChildToHorizontalBox(Text_WeaponName))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Close button (X) on the title bar.
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	CloseButton->SetStyle(TransparentButtonStyle());
	UTextBlock* CloseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseText"));
	CloseText->SetText(FText::FromString(TEXT("X")));
	CloseText->SetFont(LoadoutFont(18));
	CloseText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.82f, 0.35f, 1.f)));
	CloseButton->AddChild(CloseText);
	if (UHorizontalBoxSlot* CloseSlot = HandleRow->AddChildToHorizontalBox(CloseButton))
	{
		CloseSlot->SetVerticalAlignment(VAlign_Center);
		CloseSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	}

	Text_Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Empty"));
	Text_Empty->SetFont(LoadoutFont(14));
	Text_Empty->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f, 1.f)));
	Text_Empty->SetText(NSLOCTEXT("Loadout", "NoSlots", "这把枪没有配件槽"));
	Body->AddChildToVerticalBox(Text_Empty);

	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SlotList"));
	Body->AddChildToVerticalBox(List);
	SlotList = List;
}

void ULoadoutAttachmentWindow::ShowForWeapon(AWeapon* InWeapon)
{
	SetVisibility(ESlateVisibility::Visible);
	BindWeapon(InWeapon);
	RebuildSlots();
}

void ULoadoutAttachmentWindow::HideWindow()
{
	bDragging = false;
	SetVisibility(ESlateVisibility::Collapsed);
	BindWeapon(nullptr);
}

bool ULoadoutAttachmentWindow::IsWindowShown() const
{
	return GetVisibility() != ESlateVisibility::Collapsed && GetVisibility() != ESlateVisibility::Hidden;
}

void ULoadoutAttachmentWindow::OnCloseClicked()
{
	HideWindow();
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
	if (!RowClass)
	{
		RowClass = LoadClass<UAttachmentSlotRow>(nullptr, TEXT("/Game/FPS/UI/WBP_AttachmentSlotRow.WBP_AttachmentSlotRow_C"));
		if (!RowClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("Loadout attachment window has no RowClass and could not load WBP_AttachmentSlotRow."));
		}
	}
	if (!bHasWeapon || !RowClass) return;

	AWeapon* Target = Weapon.Get();
	for (const FWeaponAttachmentSlot& SlotInfo : Slots)
	{
		if (UAttachmentSlotRow* Row = CreateWidget<UAttachmentSlotRow>(this, RowClass))
		{
			Row->Setup(Target, SlotInfo);
			UPanelSlot* RowSlot = SlotList->AddChild(Row);
			if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(RowSlot))
			{
				VSlot->SetPadding(RowPadding);
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
	// Let the close button receive its own click instead of starting a drag.
	if (IsValid(CloseButton) && CloseButton->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
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
	// WBP 子类：树里已有控件，用绑定的；纯 C++ 类：生成默认布局
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	if (!IsValid(PopupLayer) || !IsValid(CardContainer))
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadoutScreen %s is missing PopupLayer or CardContainer."), *GetName());
	}
	BuildCards();
}

void ULoadoutScreen::NativeConstruct()
{
	Super::NativeConstruct();
	// The screen is reused between Tab presses. The attachment popup must start closed every time.
	if (IsValid(AttachmentWindow))
	{
		AttachmentWindow->HideWindow();
	}
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
	if (!WidgetTree) return;

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

	Text_Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Title"));
	Text_Title->SetText(NSLOCTEXT("Loadout", "Weapons", "武器"));
	Text_Title->SetFont(LoadoutFont(28));
	Text_Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Text_Title))
	{
		TitleSlot->SetPadding(FMargin(4.f, 0.f, 0.f, 2.f));
	}

	Text_Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Hint"));
	Text_Hint->SetText(NSLOCTEXT("Loadout", "CloseHint", "Tab 关闭"));
	Text_Hint->SetFont(LoadoutFont(14));
	Text_Hint->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.f)));
	if (UVerticalBoxSlot* HintSlot = Column->AddChildToVerticalBox(Text_Hint))
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

	// Cards stack over the preview image: one transparent button per weapon row.
	UVerticalBox* Container = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CardContainer"));
	if (UOverlaySlot* HitSlot = PreviewOverlay->AddChildToOverlay(Container))
	{
		HitSlot->SetHorizontalAlignment(HAlign_Fill);
		HitSlot->SetVerticalAlignment(VAlign_Fill);
	}
	CardContainer = Container;

	PopupLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PopupLayer"));
	PopupLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* PopupSlot = Root->AddChildToCanvas(PopupLayer))
	{
		FillCanvas(PopupSlot, 0.f, 0.f, 1.f, 1.f, FMargin(0.f));
		PopupSlot->SetZOrder(2);
	}
}

void ULoadoutScreen::BuildCards()
{
	Cards.Reset();
	if (!IsValid(CardContainer)) return;
	CardContainer->ClearChildren();

	UClass* UseClass = CardClass ? CardClass.Get() : ULoadoutWeaponCard::StaticClass();
	for (int32 Index = 0; Index < NumCards; ++Index)
	{
		ULoadoutWeaponCard* Card = CreateWidget<ULoadoutWeaponCard>(this, UseClass);
		if (!IsValid(Card)) continue;

		UPanelSlot* CardSlot = CardContainer->AddChild(Card);
		if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(CardSlot))
		{
			VSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			VSlot->SetHorizontalAlignment(HAlign_Fill);
			VSlot->SetVerticalAlignment(VAlign_Fill);
		}
		else if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(CardSlot))
		{
			HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			HSlot->SetHorizontalAlignment(HAlign_Fill);
			HSlot->SetVerticalAlignment(VAlign_Fill);
		}

		Card->OnCardClicked.AddUObject(this, &ThisClass::HandleCardClicked);
		Cards.Add(Card);
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
	ShownWeapons.SetNum(NumCards);

	UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetOwningPlayerPawn());
	static const TArray<AWeapon*> EmptyInventory;
	const TArray<AWeapon*>& Inventory = IsValid(Combat) ? Combat->GetInventory() : EmptyInventory;
	for (int32 Index = 0; Index < NumCards; ++Index)
	{
		AWeapon* Weapon = Inventory.IsValidIndex(Index) ? Inventory[Index] : nullptr;
		FText Name;
		if (IsValid(Weapon))
		{
			ShownWeapons[Index] = Weapon;
			Weapon->OnAttachmentsChanged.AddUniqueDynamic(this, &ThisClass::HandleAttachmentsChanged);
			Name = Weapon->GetClass()->GetDisplayNameText();
			if (Name.IsEmpty())
			{
				Name = FText::FromString(Weapon->GetClass()->GetName());
			}
		}
		if (Cards.IsValidIndex(Index) && IsValid(Cards[Index]))
		{
			Cards[Index]->SetupCard(Index, IsValid(Weapon) ? Weapon : nullptr, Name);
		}
	}
	RefreshCardStates();

	UWorld* World = GetWorld();
	if (!IsValid(PreviewRig) && World)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags = RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UClass* RigClass = PreviewRigClass ? PreviewRigClass.Get() : ALoadoutPreviewRig::StaticClass();
		PreviewRig = World->SpawnActor<ALoadoutPreviewRig>(RigClass, FVector(0.f, 0.f, -100000.f), FRotator::ZeroRotator, Params);
	}

	if (IsValid(PreviewRig))
	{
		TArray<AWeapon*> Weapons;
		Weapons.Reserve(ShownWeapons.Num());
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
			Brush.ImageSize = FVector2D(PreviewRig->RenderTargetWidth, PreviewRig->RenderTargetHeight);
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

void ULoadoutScreen::RefreshCardStates()
{
	AWeapon* Open = (IsValid(AttachmentWindow) && AttachmentWindow->IsWindowShown()) ? AttachmentWindow->GetShownWeapon() : nullptr;
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		if (!IsValid(Cards[Index])) continue;
		const AWeapon* CardWeapon = ShownWeapons.IsValidIndex(Index) ? ShownWeapons[Index].Get() : nullptr;
		Cards[Index]->SetSelected(Open != nullptr && CardWeapon == Open);
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

void ULoadoutScreen::HandleWindowVisibilityChanged(ESlateVisibility InVisibility)
{
	RefreshCardStates();
}

void ULoadoutScreen::HandleCardClicked(int32 Index)
{
	OpenWindow(Index);
	RefreshCardStates();
}

void ULoadoutScreen::OpenWindow(int32 Index)
{
	if (!ShownWeapons.IsValidIndex(Index) || !IsValid(ShownWeapons[Index]) || !IsValid(PopupLayer)) return;

	// Clicking the same weapon again closes the popup.
	if (IsValid(AttachmentWindow) && AttachmentWindow->IsWindowShown() && AttachmentWindow->GetShownWeapon() == ShownWeapons[Index])
	{
		AttachmentWindow->HideWindow();
		return;
	}

	if (!IsValid(AttachmentWindow))
	{
		UClass* WindowClass = AttachmentWindowClass ? AttachmentWindowClass.Get() : ULoadoutAttachmentWindow::StaticClass();
		AttachmentWindow = CreateWidget<ULoadoutAttachmentWindow>(GetOwningPlayer(), WindowClass);
		if (!IsValid(AttachmentWindow)) return;
		AttachmentWindow->OnVisibilityChanged.AddUniqueDynamic(this, &ThisClass::HandleWindowVisibilityChanged);

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
		AttachmentWindow->SetWindowPosition(FVector2D(ViewX * WindowDefaultPosition.X, WindowDefaultPosition.Y));
	}

	AttachmentWindow->ShowForWeapon(ShownWeapons[Index]);
}
