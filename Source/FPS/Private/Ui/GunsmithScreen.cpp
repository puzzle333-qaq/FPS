#include "Ui/GunsmithScreen.h"

#include "Loadout/GunsmithComponent.h"
#include "Weapon/Weapon.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Ui/GunsmithWidgets.h"
#include "ShooterGameInstance.h"
#include "Weapon/AttachmentData.h"

void UGunsmithSlotButtonProxy::HandleClicked()
{
	if (IsValid(Screen)) Screen->SelectSlot(SlotTag);
}

void UGunsmithWeaponButtonProxy::HandleClicked()
{
	if (IsValid(Screen) && WeaponClass) Screen->ShowWeapon(WeaponClass);
}

namespace
{
	// 这个槽被哪个已装配件锁住（短护木锁握把）
	UAttachmentData* FindSlotBlocker(const UGunsmithComponent* Gunsmith, const FGameplayTag& Tag)
	{
		const AWeapon* Weapon = Gunsmith ? Gunsmith->GetPreviewWeapon() : nullptr;
		return IsValid(Weapon) ? Weapon->GetSlotBlocker(Tag) : nullptr;
	}

	void ApplyLockState(UWidget* Widget, const UAttachmentData* Blocker)
	{
		if (!Widget) return;
		Widget->SetIsEnabled(Blocker == nullptr);
		Widget->SetToolTipText(Blocker
			? FText::Format(NSLOCTEXT("Gunsmith", "SlotLocked", "已被 {0} 锁定"),
				Blocker->DisplayName.IsEmpty() ? FText::FromString(Blocker->GetName()) : Blocker->DisplayName)
			: FText::GetEmpty());
	}
}

void UGunsmithScreen::RebuildExtraSlotButtons()
{
	for (UButton* B : ExtraSlotButtons)
	{
		if (IsValid(B)) B->RemoveFromParent();
	}
	ExtraSlotButtons.Reset();
	ExtraSlotProxies.Reset();

	UGunsmithComponent* Gunsmith = GetGunsmith();
	UButton* Template = Btn_Slot6 ? Btn_Slot6.Get() : Btn_Slot5.Get();
	UPanelWidget* Panel = Template ? Template->GetParent() : nullptr;
	if (!Gunsmith || !Panel || !WidgetTree) return;

	const TArray<FGameplayTag> Tags = Gunsmith->GetSlotTags();
	UCanvasPanelSlot* Slot6 = Btn_Slot6 ? Cast<UCanvasPanelSlot>(Btn_Slot6->Slot) : nullptr;
	UCanvasPanelSlot* Slot5 = Btn_Slot5 ? Cast<UCanvasPanelSlot>(Btn_Slot5->Slot) : nullptr;
	const FVector2D Step = (Slot5 && Slot6) ? Slot6->GetPosition() - Slot5->GetPosition() : FVector2D(0.f, 60.f);

	for (int32 Index = BuiltInSlotButtons; Index < Tags.Num(); ++Index)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetStyle(Template->GetStyle());
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FString Name = Tags[Index].ToString();
		Name.Split(TEXT("."), nullptr, &Name, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		Label->SetText(FText::FromString(Name));
		Button->AddChild(Label);
		UPanelSlot* NewSlot = Panel->AddChild(Button);
		if (UCanvasPanelSlot* Canvas = Cast<UCanvasPanelSlot>(NewSlot))
		{
			if (Slot6)
			{
				Canvas->SetSize(Slot6->GetSize());
				Canvas->SetAnchors(Slot6->GetAnchors());
				Canvas->SetAlignment(Slot6->GetAlignment());
				Canvas->SetPosition(Slot6->GetPosition() + Step * (Index - BuiltInSlotButtons + 1));
			}
		}

		UGunsmithSlotButtonProxy* Proxy = NewObject<UGunsmithSlotButtonProxy>(this);
		Proxy->Screen = this;
		Proxy->SlotTag = Tags[Index];
		Button->OnClicked.AddDynamic(Proxy, &UGunsmithSlotButtonProxy::HandleClicked);
		ExtraSlotButtons.Add(Button);
		ExtraSlotProxies.Add(Proxy);
	}
}

UGunsmithComponent* UGunsmithScreen::GetGunsmith() const
{
	const APlayerController* PC = GetOwningPlayer();
	return IsValid(PC) ? PC->FindComponentByClass<UGunsmithComponent>() : nullptr;
}

void UGunsmithScreen::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_Back) Button_Back->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBack);
	if (Button_Save) Button_Save->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSave);
	if (Button_Equip) Button_Equip->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEquip);
	if (UGunsmithComponent* Gunsmith = GetGunsmith())
	{
		Gunsmith->OnPreviewChanged.AddUniqueDynamic(this, &ThisClass::HandlePreviewChanged);
		// 蓝图没调 ShowWeapon 时自己挑一把：已装备的主长枪 -> 可选列表第一把
		if (!IsValid(Gunsmith->GetPreviewWeapon()))
		{
			TSubclassOf<AWeapon> WeaponClass;
			if (const UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>())
			{
				WeaponClass = GI->GetPrimaryLongGun();
			}
			if (!WeaponClass && Gunsmith->AvailableWeapons.Num() > 0)
			{
				WeaponClass = Gunsmith->AvailableWeapons[0];
			}
			if (WeaponClass)
			{
				Gunsmith->SetPreviewWeapon(WeaponClass); // 内部会广播 OnPreviewChanged
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[Gunsmith] %s: no weapon to preview (no primary long gun, AvailableWeapons empty)."), *GetName());
			}
		}
		else
		{
			Gunsmith->RefreshPreview();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Gunsmith] %s: owning PlayerController has no GunsmithComponent."), *GetName());
	}
	HandlePreviewChanged();
}

void UGunsmithScreen::NativeDestruct()
{
	if (UGunsmithComponent* Gunsmith = GetGunsmith())
	{
		Gunsmith->OnPreviewChanged.RemoveDynamic(this, &ThisClass::HandlePreviewChanged);
	}
	Super::NativeDestruct();
}

void UGunsmithScreen::ShowWeapon(TSubclassOf<AWeapon> WeaponClass)
{
	if (UGunsmithComponent* Gunsmith = GetGunsmith())
	{
		SelectedSlot = FGameplayTag(); // 换枪后槽位表不同，回到第一个槽
		Gunsmith->SetPreviewWeapon(WeaponClass);
	}
}

void UGunsmithScreen::SelectSlot(FGameplayTag SlotTag)
{
	SelectedSlot = SlotTag;
	UGunsmithComponent* Gunsmith = GetGunsmith();
	if (!Gunsmith) return;
	OnSlotSelected(SlotTag, Gunsmith->GetAllowedForSlot(SlotTag), Gunsmith->GetEquippedInSlot(SlotTag));
	if (bFocusOnSelect) Gunsmith->FocusSlot(SlotTag);
	RebuildCategories();
	RebuildAttachmentList();
	RebuildPartNames();
	RefreshStats();
}

bool UGunsmithScreen::PickAttachment(UAttachmentData* Attachment)
{
	UGunsmithComponent* Gunsmith = GetGunsmith();
	if (!Gunsmith) return false;
	return IsValid(Attachment) ? Gunsmith->EquipPreview(Attachment) : Gunsmith->RemovePreview(SelectedSlot);
}

bool UGunsmithScreen::Save()
{
	UGunsmithComponent* Gunsmith = GetGunsmith();
	const bool bOk = Gunsmith && Gunsmith->SaveCurrent();
	OnSaved(bOk);
	if (Gunsmith)
	{
		AWeapon* Preview = Gunsmith->GetPreviewWeapon();
		OnEquippedPrimary(IsValid(Preview) ? Preview->GetClass() : nullptr, bOk);
	}
	return bOk;
}

bool UGunsmithScreen::Equip()
{
	return Save();
}

void UGunsmithScreen::HandleBack()
{
	OnBackRequested();
}

void UGunsmithScreen::HandleSave()
{
	Save();
}

void UGunsmithScreen::HandleEquip()
{
	Equip();
}

void UGunsmithScreen::HandlePreviewChanged()
{
	UGunsmithComponent* Gunsmith = GetGunsmith();
	if (Image_Preview && Gunsmith)
	{
		if (UTextureRenderTarget2D* RT = Gunsmith->GetPreviewRenderTarget())
		{
			Image_Preview->SetBrushResourceObject(RT);
			Image_Preview->SetDesiredSizeOverride(FVector2D(RT->SizeX, RT->SizeY));
		}
	}
	RebuildExtraSlotButtons();
	RebuildWeaponList();
	if (Gunsmith && !Gunsmith->GetSlotTags().Contains(SelectedSlot))
	{
		SelectedSlot = FGameplayTag();
		const TArray<FGameplayTag> Tags = Gunsmith->GetSlotTags();
		if (Tags.Num() > 0) SelectedSlot = Tags[0];
	}
	RebuildCategories();
	RebuildAttachmentList();
	RebuildPartNames();
	RefreshStats();
	OnPreviewUpdated();
}
// ================= 动态列表 / 视角 / 属性 =================

void UGunsmithScreen::RebuildCategories()
{
	UGunsmithComponent* Gunsmith = GetGunsmith();
	if (!Panel_Categories || !CategoryItemClass || !Gunsmith) return;
	Panel_Categories->ClearChildren();
	for (const FGameplayTag& Tag : Gunsmith->GetSlotTags())
	{
		UGunsmithCategoryItem* Item = CreateWidget<UGunsmithCategoryItem>(this, CategoryItemClass);
		if (!Item) continue;
		Panel_Categories->AddChild(Item);
		Item->Setup(this, Tag, Gunsmith->GetEquippedInSlot(Tag), Tag.MatchesTagExact(SelectedSlot));
		ApplyLockState(Item, FindSlotBlocker(Gunsmith, Tag));
	}
}

TArray<TSubclassOf<AWeapon>> UGunsmithScreen::GetGunsmithWeapons() const
{
	TArray<TSubclassOf<AWeapon>> Result;
	if (const UGunsmithComponent* Gunsmith = GetGunsmith())
	{
		for (const TSubclassOf<AWeapon>& W : Gunsmith->AvailableWeapons)
		{
			if (W) Result.AddUnique(W);
		}
	}
	if (const UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>())
	{
		if (TSubclassOf<AWeapon> Primary = GI->GetPrimaryLongGun()) Result.AddUnique(Primary);
	}
	return Result;
}

void UGunsmithScreen::RebuildWeaponList()
{
	UGunsmithComponent* Gunsmith = GetGunsmith();
	if (!Panel_Weapons || !Gunsmith || !WidgetTree) return;
	Panel_Weapons->ClearChildren();
	WeaponProxies.Reset();
	const AWeapon* Preview = Gunsmith->GetPreviewWeapon();
	const UClass* Current = IsValid(Preview) ? Preview->GetClass() : nullptr;

	for (const TSubclassOf<AWeapon>& WeaponClass : GetGunsmithWeapons())
	{
		const bool bSelected = WeaponClass.Get() == Current;
		if (WeaponItemClass)
		{
			if (UGunsmithWeaponItem* Item = CreateWidget<UGunsmithWeaponItem>(this, WeaponItemClass))
			{
				Panel_Weapons->AddChild(Item);
				Item->Setup(this, WeaponClass, bSelected);
			}
			continue;
		}
		// 没填 WeaponItemClass：造一个“按钮 + 图标/名字”
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		const AWeapon* CDO = WeaponClass->GetDefaultObject<AWeapon>();
		if (CDO && IsValid(CDO->WeaponIcon))
		{
			UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Icon->SetBrushFromTexture(CDO->WeaponIcon);
			Icon->SetDesiredSizeOverride(FVector2D(160.f, 64.f));
			Button->AddChild(Icon);
		}
		else
		{
			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Label->SetText(UGunsmithWeaponItem::GetWeaponDisplayName(WeaponClass));
			Button->AddChild(Label);
		}
		Button->SetToolTipText(UGunsmithWeaponItem::GetWeaponDisplayName(WeaponClass));
		Button->SetRenderOpacity(bSelected ? 1.f : 0.6f);
		Panel_Weapons->AddChild(Button);
		UGunsmithWeaponButtonProxy* Proxy = NewObject<UGunsmithWeaponButtonProxy>(this);
		Proxy->Screen = this;
		Proxy->WeaponClass = WeaponClass;
		Button->OnClicked.AddDynamic(Proxy, &UGunsmithWeaponButtonProxy::HandleClicked);
		WeaponProxies.Add(Proxy);
	}
}

void UGunsmithScreen::RebuildAttachmentList()
{
	UGunsmithComponent* Gunsmith = GetGunsmith();
	if (!Panel_Attachments || !AttachmentItemClass || !Gunsmith) return;
	Panel_Attachments->ClearChildren();
	if (!SelectedSlot.IsValid()) return;

	UAttachmentData* Equipped = Gunsmith->GetEquippedInSlot(SelectedSlot);
	bool bRequired = false;
	for (const FWeaponAttachmentSlot& SlotDef : Gunsmith->GetSlots())
	{
		if (SlotDef.SlotTag.MatchesTagExact(SelectedSlot)) { bRequired = SlotDef.bRequired; break; }
	}

	auto AddItem = [this, Equipped](UAttachmentData* Data)
	{
		UGunsmithAttachmentItem* Item = CreateWidget<UGunsmithAttachmentItem>(this, AttachmentItemClass);
		if (!Item) return;
		Panel_Attachments->AddChild(Item);
		Item->Setup(this, SelectedSlot, Data, Data == Equipped);
		ApplyLockState(Item, FindSlotBlocker(GetGunsmith(), SelectedSlot));
	};
	if (bAddRemoveItem && !bRequired) AddItem(nullptr);
	for (UAttachmentData* Data : Gunsmith->GetAllowedForSlot(SelectedSlot)) AddItem(Data);
}

void UGunsmithScreen::ApplyStats(const TArray<FGunsmithStatLine>& Lines)
{
	if (Panel_Stats && StatItemClass)
	{
		// 数量不变就复用，避免悬停时闪
		if (Panel_Stats->GetChildrenCount() != Lines.Num())
		{
			Panel_Stats->ClearChildren();
			for (int32 i = 0; i < Lines.Num(); ++i)
			{
				if (UGunsmithStatItem* Item = CreateWidget<UGunsmithStatItem>(this, StatItemClass)) Panel_Stats->AddChild(Item);
			}
		}
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			if (UGunsmithStatItem* Item = Cast<UGunsmithStatItem>(Panel_Stats->GetChildAt(i))) Item->SetStat(Lines[i]);
		}
	}
	OnStatsUpdated(Lines);
}

TArray<FGunsmithStatLine> UGunsmithScreen::GetStatPreview(FGameplayTag SlotTag, UAttachmentData* Candidate) const
{
	const UGunsmithComponent* Gunsmith = GetGunsmith();
	return Gunsmith ? Gunsmith->GetStatPreview(SlotTag, Candidate) : TArray<FGunsmithStatLine>();
}

void UGunsmithScreen::PreviewStatsFor(FGameplayTag SlotTag, UAttachmentData* Candidate)
{
	ApplyStats(GetStatPreview(SlotTag, Candidate));
}

void UGunsmithScreen::RefreshStats()
{
	const UGunsmithComponent* Gunsmith = GetGunsmith();
	ApplyStats(Gunsmith ? Gunsmith->GetCurrentStats() : TArray<FGunsmithStatLine>());
}

void UGunsmithScreen::RebuildPartNames()
{
	UGunsmithComponent* Gunsmith = GetGunsmith();
	if (!Canvas_PartNames || !PartNameClass || !Gunsmith) return;
	Canvas_PartNames->ClearChildren();
	PartNameWidgets.Reset();
	for (const FGameplayTag& Tag : Gunsmith->GetSlotTags())
	{
		UGunsmithPartName* Item = CreateWidget<UGunsmithPartName>(this, PartNameClass);
		if (!Item) continue;
		if (UCanvasPanelSlot* CanvasSlot = Canvas_PartNames->AddChildToCanvas(Item))
		{
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));
		}
		Item->Setup(this, Tag, Gunsmith->GetEquippedInSlot(Tag), Tag.MatchesTagExact(SelectedSlot));
		ApplyLockState(Item, FindSlotBlocker(Gunsmith, Tag));
		PartNameWidgets.Add(Item);
	}
	UpdatePartNamePositions();
}

TArray<FGunsmithSlotScreenTag> UGunsmithScreen::GetSlotScreenPositions() const
{
	TArray<FGunsmithSlotScreenTag> Out;
	const UGunsmithComponent* Gunsmith = GetGunsmith();
	if (!Gunsmith) return Out;
	for (const FGameplayTag& Tag : Gunsmith->GetSlotTags())
	{
		FGunsmithSlotScreenTag Entry;
		Entry.SlotTag = Tag;
		Entry.bVisible = Gunsmith->GetSlotScreenUV(Tag, Entry.UV);
		Out.Add(Entry);
	}
	return Out;
}

void UGunsmithScreen::UpdatePartNamePositions()
{
	UGunsmithComponent* Gunsmith = GetGunsmith();
	if (!Canvas_PartNames || !Gunsmith) return;
	const FVector2D Size = Canvas_PartNames->GetCachedGeometry().GetLocalSize();
	if (Size.X <= 1.f || Size.Y <= 1.f) return;
	for (UGunsmithPartName* Item : PartNameWidgets)
	{
		if (!IsValid(Item)) continue;
		FVector2D UV;
		const bool bVisible = Gunsmith->GetSlotScreenUV(Item->SlotTag, UV);
		Item->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Item->Slot)) CanvasSlot->SetPosition(UV * Size);
	}
}

void UGunsmithScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// 贴图可能晚于 NativeConstruct 才创建：发现笔刷还没设就补上
	if (Image_Preview)
	{
		UGunsmithComponent* Gunsmith = GetGunsmith();
		UTextureRenderTarget2D* RT = Gunsmith ? Gunsmith->GetPreviewRenderTarget() : nullptr;
		if (RT && Image_Preview->GetBrush().GetResourceObject() != RT) HandlePreviewChanged();
	}
	UpdatePartNamePositions();
}

void UGunsmithScreen::RotatePreview(float DeltaYaw, float DeltaPitch)
{
	if (UGunsmithComponent* Gunsmith = GetGunsmith()) Gunsmith->RotatePreview(DeltaYaw, DeltaPitch);
}

void UGunsmithScreen::ZoomPreview(float Delta)
{
	if (UGunsmithComponent* Gunsmith = GetGunsmith()) Gunsmith->ZoomPreview(Delta);
}

bool UGunsmithScreen::FocusSlot(FGameplayTag SlotTag)
{
	UGunsmithComponent* Gunsmith = GetGunsmith();
	return Gunsmith && Gunsmith->FocusSlot(SlotTag);
}

void UGunsmithScreen::ResetView()
{
	if (UGunsmithComponent* Gunsmith = GetGunsmith()) Gunsmith->ResetPreviewView();
}

bool UGunsmithScreen::IsOverPreview(const FPointerEvent& Event) const
{
	return Image_Preview && Image_Preview->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition());
}

FReply UGunsmithScreen::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && IsOverPreview(InMouseEvent))
	{
		bDragging = true;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && IsOverPreview(InMouseEvent))
	{
		ResetView();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UGunsmithScreen::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UGunsmithScreen::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		const FVector2D Delta = InMouseEvent.GetCursorDelta();
		RotatePreview(Delta.X * DragRotateSpeed, -Delta.Y * DragRotateSpeed);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UGunsmithScreen::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsOverPreview(InMouseEvent))
	{
		ZoomPreview(InMouseEvent.GetWheelDelta());
		return FReply::Handled();
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}