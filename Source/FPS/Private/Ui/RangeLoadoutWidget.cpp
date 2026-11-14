#include "Ui/RangeLoadoutWidget.h"
#include "Character/PoliceAppearanceComponent.h"
#include "Weapon/WeaponSting.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "CombatComponent.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Input/Events.h"
#include "Modules/ModuleManager.h"
#include "Loadout/PouchInventoryComponent.h"
#include "Range/RangeLoadoutBench.h"
#include "ShooterCharacter.h"
#include "ShooterGamePlayTags.h"
#include "ShooterPlayerController.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "Weapon/AttachmentData.h"

namespace RangeDrawer
{
	FText SlotLabel(const FGameplayTag& Tag)
	{
		if (Tag.MatchesTagExact(ShooterTags::Tag_Attachment_Slot_Barrel)) return FText::FromString(TEXT("枪管"));
		if (Tag.MatchesTagExact(ShooterTags::Tag_Attachment_Slot_Handguard)) return FText::FromString(TEXT("护木"));
		if (Tag.MatchesTagExact(ShooterTags::Tag_Attachment_Slot_Muzzle)) return FText::FromString(TEXT("枪口"));
		if (Tag.MatchesTagExact(ShooterTags::Tag_Attachment_Slot_Optic)) return FText::FromString(TEXT("瞄具"));
		if (Tag.MatchesTagExact(ShooterTags::Tag_Attachment_Slot_Grip)) return FText::FromString(TEXT("握把"));
		if (Tag.MatchesTagExact(ShooterTags::Tag_Attachment_Slot_Stock)) return FText::FromString(TEXT("枪托"));
		if (Tag.MatchesTagExact(ShooterTags::Tag_Attachment_Slot_Tactical)) return FText::FromString(TEXT("战术"));
		if (Tag.MatchesTagExact(ShooterTags::Tag_Attachment_Slot_Laser)) return FText::FromString(TEXT("镭射／手电"));
		if (Tag.MatchesTagExact(ShooterTags::Tag_Attachment_Slot_Flashlight)) return FText::FromString(TEXT("手电"));
		if (Tag.MatchesTagExact(ShooterTags::Tag_Attachment_Slot_Magazine)) return FText::FromString(TEXT("弹匣"));
		FString Leaf = Tag.ToString();
		int32 Dot = INDEX_NONE;
		if (Leaf.FindLastChar(TCHAR('.'), Dot))
		{
			Leaf.RightChopInline(Dot + 1);
		}
		return FText::FromString(Leaf);
	}

	FText ClassLabel(UClass* Class)
	{
		if (!Class) return FText::GetEmpty();
		if (Class->GetPathName() == TEXT("/Game/FPS/Weapon/Throwable/BP_Weapon_Grenade.BP_Weapon_Grenade_C"))
		{
			return NSLOCTEXT("RangeBench", "FragGrenade", "破片手雷（M67）");
		}
		FString Name = Class->GetName();
		Name.RemoveFromEnd(TEXT("_C"));
		Name.RemoveFromStart(TEXT("BP_"));
		Name.RemoveFromStart(TEXT("Weapon_"));
		Name.ReplaceInline(TEXT("_"), TEXT(" "));
		return FText::FromString(Name);
	}

	FText AttachmentLabel(const UAttachmentData* Data)
	{
		if (!IsValid(Data)) return FText::GetEmpty();
		if (!Data->DisplayName.IsEmpty()) return Data->DisplayName;
		FString Name = Data->GetName();
		Name.RemoveFromStart(TEXT("DA_"));
		Name.ReplaceInline(TEXT("_"), TEXT(" "));
		return FText::FromString(Name);
	}

	bool HasSlot(const TArray<FWeaponAttachmentSlot>& Slots, const FGameplayTag& Tag)
	{
		return Slots.ContainsByPredicate([&Tag](const FWeaponAttachmentSlot& AttachSlot)
		{
			return AttachSlot.SlotTag.MatchesTagExact(Tag);
		});
	}

	TArray<FWeaponAttachmentSlot> BuildUiSlots(TSubclassOf<AWeapon> WeaponClass)
	{
		const AWeapon* CDO = WeaponClass ? WeaponClass->GetDefaultObject<AWeapon>() : nullptr;
		return CDO ? CDO->GetAttachmentSlots() : TArray<FWeaponAttachmentSlot>();
	}

	FGameplayTag UiEquipSlot(const TArray<FWeaponAttachmentSlot>& Slots, const UAttachmentData* Attachment)
	{
		if (!IsValid(Attachment)) return FGameplayTag();
		if ((Attachment->bProvidesLaser || Attachment->bProvidesFlashlight
			|| Attachment->SlotTag == ShooterTags::Tag_Attachment_Slot_Laser
			|| Attachment->SlotTag == ShooterTags::Tag_Attachment_Slot_Flashlight)
			&& HasSlot(Slots, ShooterTags::Tag_Attachment_Slot_Laser))
		{
			return ShooterTags::Tag_Attachment_Slot_Laser;
		}
		return Attachment->SlotTag;
	}

	const UAttachmentData* FindEquipped(const FWeaponPreset& Preset, const TArray<FWeaponAttachmentSlot>& Slots, const FGameplayTag& SlotTag)
	{
		for (const TObjectPtr<UAttachmentData>& Data : Preset.Attachments)
		{
			if (IsValid(Data) && UiEquipSlot(Slots, Data) == SlotTag)
			{
				return Data;
			}
		}
		return nullptr;
	}

	bool IsLocked(const FWeaponPreset& Preset, const FGameplayTag& SlotTag)
	{
		for (const TObjectPtr<UAttachmentData>& Data : Preset.Attachments)
		{
			if (IsValid(Data) && (Data->BlockedSlots.HasTagExact(SlotTag)
				|| (SlotTag == ShooterTags::Tag_Attachment_Slot_Laser &&
					(Data->BlockedSlots.HasTagExact(ShooterTags::Tag_Attachment_Slot_Tactical)
					|| Data->BlockedSlots.HasTagExact(ShooterTags::Tag_Attachment_Slot_Flashlight)))))
			{
				return true;
			}
		}
		return false;
	}

	const FWeaponAttachmentSlot* FindSlot(const TArray<FWeaponAttachmentSlot>& Slots, const FGameplayTag& SlotTag)
	{
		return Slots.FindByPredicate([&SlotTag](const FWeaponAttachmentSlot& AttachSlot)
		{
			return AttachSlot.SlotTag.MatchesTagExact(SlotTag);
		});
	}
}

void URangeLoadoutWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
 if(UsesNativePresentation())return;
	ResolveDrawerWidgets();
	BindDrawerButtons();
}

FReply URangeLoadoutWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::F)
	{
		CloseBench();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void URangeLoadoutWidget::Init(ARangeLoadoutBench* InBench)
{
	Bench = InBench;
	CollectCatalog();
	SeedFromPawn();
 RefreshProtectionPreview();
	bDirty = false;
	if (Primary.WeaponClass)
	{
		PushPreview(&Primary);
	}
}

void URangeLoadoutWidget::NotifyBenchReady()
{
	RebuildDrawer();
	OnBenchReady();
}

void URangeLoadoutWidget::CloseBench()
{
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		PC->LeaveRangeBench();
	}
}

void URangeLoadoutWidget::NativeDestruct()
{
 if(IsValid(ProtectionPreview))ProtectionPreview->DestroyComponent();
 ProtectionPreview=nullptr;Super::NativeDestruct();
}
void URangeLoadoutWidget::RefreshProtectionPreview()
{
 auto* Character=GetOwningPlayerPawn<AShooterCharacter>();
 if(!Character||!Character->PoliceAppearance||!Bench||!Bench->GetPreviewMesh())return;
 if(!ProtectionPreview)
 {
  ProtectionPreview=NewObject<UPoliceAppearanceComponent>(Bench);
  ProtectionPreview->SetIsReplicated(false);Bench->AddInstanceComponent(ProtectionPreview);ProtectionPreview->RegisterComponent();
  ProtectionPreview->InitializePreview(Bench->GetPreviewMesh(),Character->PoliceAppearance->FaceProtection,Character->GetBodyArmor());
 }
 else ProtectionPreview->SetPreviewProtection(Character->PoliceAppearance->FaceProtection,Character->GetBodyArmor());
}
void URangeLoadoutWidget::SelectFaceProtection(EPoliceFaceProtection Face)
{
 if(auto* Character=GetOwningPlayerPawn<AShooterCharacter>();Character&&Character->PoliceAppearance)
 {
  Character->PoliceAppearance->SetProtection(Face,Character->GetBodyArmor());
  RefreshProtectionPreview();QueueSelectionChanged();
 }
}
void URangeLoadoutWidget::SelectBodyArmor(EBodyArmorType Armor)
{
 if(auto* Character=GetOwningPlayerPawn<AShooterCharacter>();Character&&Character->PoliceAppearance)
 {
  Character->PoliceAppearance->SetProtection(Character->PoliceAppearance->FaceProtection,Armor);
  RefreshProtectionPreview();QueueSelectionChanged();
 }
}

FRangeWeaponChoice URangeLoadoutWidget::MakeWeaponChoice(TSubclassOf<AWeapon> WeaponClass, bool bSelected) const
{
	FRangeWeaponChoice Choice;
	Choice.WeaponClass = WeaponClass;
	Choice.DisplayName = RangeDrawer::ClassLabel(WeaponClass);
	Choice.bSelected = bSelected;
	if (const AWeapon* CDO = WeaponClass ? WeaponClass->GetDefaultObject<AWeapon>() : nullptr)
	{
		Choice.Icon = CDO->WeaponIcon;
	}
	return Choice;
}

TArray<FRangeWeaponChoice> URangeLoadoutWidget::GetPrimaryChoices() const
{
	TArray<FRangeWeaponChoice> Choices;
	Choices.Reserve(PrimaryClasses.Num());
	for (const TSubclassOf<AWeapon>& WeaponClass : PrimaryClasses)
	{
		Choices.Add(MakeWeaponChoice(WeaponClass, WeaponClass == Primary.WeaponClass));
	}
	return Choices;
}

TArray<FRangeWeaponChoice> URangeLoadoutWidget::GetSecondaryChoices() const
{
	TArray<FRangeWeaponChoice> Choices;
	Choices.Reserve(SecondaryClasses.Num());
	for (const TSubclassOf<AWeapon>& WeaponClass : SecondaryClasses)
	{
		Choices.Add(MakeWeaponChoice(WeaponClass, WeaponClass == Secondary.WeaponClass));
	}
	return Choices;
}

TArray<FRangeWeaponChoice> URangeLoadoutWidget::GetThrowableChoices() const
{
	TArray<FRangeWeaponChoice> Choices;
	Choices.Reserve(ThrowableClasses.Num());
	for (const TSubclassOf<AWeapon>& WeaponClass : ThrowableClasses)
	{
		const bool bSelected = Throwables.ContainsByPredicate([&WeaponClass](const FWeaponPreset& Preset)
		{
			return Preset.WeaponClass == WeaponClass;
		});
		Choices.Add(MakeWeaponChoice(WeaponClass, bSelected));
	}
	return Choices;
}

TArray<FRangeMagazineChoice> URangeLoadoutWidget::GetMagazineChoices() const
{
	TArray<FRangeMagazineChoice> Choices;
	const FWeaponPreset* Guns[] = { &Primary, &Secondary };
	for (const FWeaponPreset* Preset : Guns)
	{
		if (!Preset || !Preset->WeaponClass) continue;
		const TArray<FWeaponAttachmentSlot> Slots = RangeDrawer::BuildUiSlots(Preset->WeaponClass);
		const FWeaponAttachmentSlot* MagSlot = RangeDrawer::FindSlot(Slots, ShooterTags::Tag_Attachment_Slot_Magazine);
		if (!MagSlot) continue;
		const UAttachmentData* Equipped = RangeDrawer::FindEquipped(*Preset, Slots, MagSlot->SlotTag);
		const FText WeaponName = RangeDrawer::ClassLabel(Preset->WeaponClass);
		for (UAttachmentData* Magazine : MagSlot->AllowedAttachments)
		{
			if (!IsValid(Magazine)) continue;
			FRangeMagazineChoice Choice;
			Choice.WeaponClass = Preset->WeaponClass;
			Choice.WeaponName = WeaponName;
			Choice.Magazine = Magazine;
			Choice.DisplayName = FText::FromString(WeaponName.ToString() + TEXT("  ") + RangeDrawer::AttachmentLabel(Magazine).ToString());
			Choice.Icon = Magazine->Icon;
			Choice.bEquipped = Equipped == Magazine;
			Choices.Add(Choice);
		}
	}
	return Choices;
}

TArray<FRangeAttachmentSlotView> URangeLoadoutWidget::GetWeaponSlots(TSubclassOf<AWeapon> WeaponClass) const
{
	TArray<FRangeAttachmentSlotView> Views;
	if (!WeaponClass) return Views;

	const TArray<FWeaponAttachmentSlot> Slots = RangeDrawer::BuildUiSlots(WeaponClass);
	FWeaponPreset Scratch;
	const FWeaponPreset* Preset = FindPreset(WeaponClass);
	if (!Preset)
	{
		Scratch.WeaponClass = WeaponClass;
		SeedDefaults(Scratch);
		Preset = &Scratch;
	}

	for (const FWeaponAttachmentSlot& AttachSlot : Slots)
	{
		if (!AttachSlot.SlotTag.IsValid()) continue;
		const bool bLocked = RangeDrawer::IsLocked(*Preset, AttachSlot.SlotTag);
		const UAttachmentData* Equipped = RangeDrawer::FindEquipped(*Preset, Slots, AttachSlot.SlotTag);

		FRangeAttachmentSlotView View;
		View.SlotTag = AttachSlot.SlotTag;
		View.SlotName = RangeDrawer::SlotLabel(AttachSlot.SlotTag);
		View.bRequired = AttachSlot.bRequired;
		View.bLocked = bLocked;
		View.bCanClear = !bLocked && !AttachSlot.bRequired && Equipped != nullptr;
		if (Equipped)
		{
			View.EquippedName = RangeDrawer::AttachmentLabel(Equipped);
			View.EquippedIcon = Equipped->Icon;
		}
		else
		{
			View.EquippedName = FText::FromString(bLocked ? TEXT("锁定") : TEXT("空"));
		}

		if (!bLocked)
		{
			for (UAttachmentData* Attachment : AttachSlot.AllowedAttachments)
			{
				if (!IsValid(Attachment)) continue;
				FRangeAttachmentChoice Choice;
				Choice.Attachment = Attachment;
				Choice.DisplayName = RangeDrawer::AttachmentLabel(Attachment);
				Choice.Icon = Attachment->Icon;
				Choice.bEquipped = Equipped == Attachment;
				View.Choices.Add(Choice);
			}
		}
		Views.Add(View);
	}
	return Views;
}

bool URangeLoadoutWidget::SelectWeapon(FWeaponPreset& Held, TSubclassOf<AWeapon> WeaponClass, EWeaponKind ExpectedKind)
{
	if (!WeaponClass || IsHiddenWeapon(WeaponClass)) return false;
	const AWeapon* CDO = WeaponClass->GetDefaultObject<AWeapon>();
	if (!CDO || CDO->WeaponKind != ExpectedKind) return false;
	if (Held.WeaponClass != WeaponClass)
	{
		const int32 KeptMags = Held.WeaponClass ? Held.StackCount : FMath::Min(ExpectedKind == EWeaponKind::Pistol ? 3 : 5,FMath::Max(0,PouchLimit()-PouchUsed()));
		Held.WeaponClass = WeaponClass;
		SeedDefaults(Held);
		Held.StackCount = KeptMags;
		bDirty = true;
	}
	PushPreview(&Held);
	QueueSelectionChanged();
	return true;
}

bool URangeLoadoutWidget::SelectPrimary(TSubclassOf<AWeapon> WeaponClass)
{
	return SelectWeapon(Primary, WeaponClass, EWeaponKind::LongGun);
}

bool URangeLoadoutWidget::SelectSecondary(TSubclassOf<AWeapon> WeaponClass)
{
	return SelectWeapon(Secondary, WeaponClass, EWeaponKind::Pistol);
}

bool URangeLoadoutWidget::ToggleThrowable(TSubclassOf<AWeapon> WeaponClass)
{
	if (!WeaponClass || IsHiddenWeapon(WeaponClass)) return false;
	const AWeapon* CDO = WeaponClass->GetDefaultObject<AWeapon>();
	if (!CDO || CDO->WeaponKind != EWeaponKind::Throwable) return false;

	const int32 Index = Throwables.IndexOfByPredicate([&WeaponClass](const FWeaponPreset& Preset)
	{
		return Preset.WeaponClass == WeaponClass;
	});
	if (Index != INDEX_NONE)
	{
		Throwables.RemoveAt(Index);
		bDirty = true;
		if (Primary.WeaponClass) PushPreview(&Primary);
		else if (Secondary.WeaponClass) PushPreview(&Secondary);
		QueueSelectionChanged();
		return false;
	}
	if (Throwables.Num() >= GetMaxThrowables()) return false;

	FWeaponPreset Preset;
	Preset.WeaponClass = WeaponClass;
	Preset.StackCount = 1;
	SeedDefaults(Preset);
	Throwables.Add(Preset);
	bDirty = true;
	PushPreview(&Throwables.Last());
	QueueSelectionChanged();
	return true;
}

int32 URangeLoadoutWidget::PouchLimit() const
{
 if(const auto* C=GetOwningPlayerPawn<AShooterCharacter>())if(auto* P=C->GetPouch())return P->GetSlotCount();
	return UPouchInventoryComponent::SlotCount;
}

int32 URangeLoadoutWidget::PouchUsed() const
{
	int32 Used = 0;
	if (Primary.WeaponClass) Used += FMath::Max(0, Primary.StackCount);
	if (Secondary.WeaponClass) Used += FMath::Max(0, Secondary.StackCount);
	for (const FWeaponPreset& Preset : Throwables)
	{
		Used += FMath::Max(0, Preset.StackCount);
	}
	return Used;
}

int32 URangeLoadoutWidget::GetStackCount(TSubclassOf<AWeapon> WeaponClass) const
{
	const FWeaponPreset* Preset = FindPreset(WeaponClass);
	return Preset ? FMath::Max(0, Preset->StackCount) : 0;
}

bool URangeLoadoutWidget::AdjustMagCount(TSubclassOf<AWeapon> WeaponClass, int32 Delta)
{
	FWeaponPreset* Preset = nullptr;
	if (Primary.WeaponClass == WeaponClass) Preset = &Primary;
	else if (Secondary.WeaponClass == WeaponClass) Preset = &Secondary;
	if (!Preset || Delta == 0) return false;
	if (Delta > 0 && PouchUsed() >= PouchLimit()) return false;

	const int32 Next = FMath::Clamp(Preset->StackCount + Delta, 0, PouchLimit());
	if (Next == Preset->StackCount) return false;
	Preset->StackCount = Next;
	bDirty = true;
	QueueSelectionChanged();
	return true;
}

bool URangeLoadoutWidget::AdjustThrowableCount(TSubclassOf<AWeapon> WeaponClass, int32 Delta)
{
	if (!WeaponClass || IsHiddenWeapon(WeaponClass) || Delta == 0) return false;
	const AWeapon* CDO = WeaponClass->GetDefaultObject<AWeapon>();
	if (!CDO || CDO->WeaponKind != EWeaponKind::Throwable) return false;

	const int32 Index = Throwables.IndexOfByPredicate([&WeaponClass](const FWeaponPreset& Preset)
	{
		return Preset.WeaponClass == WeaponClass;
	});
	if (Delta > 0)
	{
		if (PouchUsed() >= PouchLimit()) return false;
		if (Index == INDEX_NONE)
		{
			if (Throwables.Num() >= GetMaxThrowables()) return false;
			FWeaponPreset Preset;
			Preset.WeaponClass = WeaponClass;
			Preset.StackCount = 1;
			SeedDefaults(Preset);
			Throwables.Add(Preset);
		}
		else
		{
			Throwables[Index].StackCount = FMath::Max(0, Throwables[Index].StackCount) + 1;
		}
	}
	else
	{
		if (Index == INDEX_NONE || Throwables[Index].StackCount <= 0) return false;
		Throwables[Index].StackCount -= 1;
		if (Throwables[Index].StackCount <= 0)
		{
			Throwables.RemoveAt(Index);
		}
	}
	bDirty = true;
	QueueSelectionChanged();
	return true;
}

bool URangeLoadoutWidget::EquipAttachment(TSubclassOf<AWeapon> WeaponClass, UAttachmentData* Attachment)
{
	FWeaponPreset* Preset = FindPreset(WeaponClass);
	if (!Preset || !IsValid(Attachment)) return false;

	const TArray<FWeaponAttachmentSlot> Slots = RangeDrawer::BuildUiSlots(WeaponClass);
	const FGameplayTag SlotTag = RangeDrawer::UiEquipSlot(Slots, Attachment);
	const FWeaponAttachmentSlot* AttachSlot = RangeDrawer::FindSlot(Slots, SlotTag);
	if (!AttachSlot || RangeDrawer::IsLocked(*Preset, SlotTag)) return false;
	if (!AttachSlot->AllowedAttachments.Contains(Attachment)) return false;

	SetSlotAttachment(*Preset, Attachment);
	bDirty = true;
	PushPreview(Preset);
	QueueSelectionChanged();
	return true;
}

bool URangeLoadoutWidget::ClearAttachmentSlot(TSubclassOf<AWeapon> WeaponClass, FGameplayTag SlotTag)
{
	FWeaponPreset* Preset = FindPreset(WeaponClass);
	if (!Preset || !SlotTag.IsValid()) return false;

	const TArray<FWeaponAttachmentSlot> Slots = RangeDrawer::BuildUiSlots(WeaponClass);
	const FWeaponAttachmentSlot* AttachSlot = RangeDrawer::FindSlot(Slots, SlotTag);
	if (!AttachSlot || AttachSlot->bRequired || RangeDrawer::IsLocked(*Preset, SlotTag)) return false;
	if (!RangeDrawer::FindEquipped(*Preset, Slots, SlotTag)) return false;

	RemoveSlotAttachment(*Preset, SlotTag);
	bDirty = true;
	PushPreview(Preset);
	QueueSelectionChanged();
	return true;
}

bool URangeLoadoutWidget::EquipMagazine(TSubclassOf<AWeapon> WeaponClass, UAttachmentData* Magazine)
{
	if (!IsValid(Magazine)) return false;
	const TArray<FWeaponAttachmentSlot> Slots = RangeDrawer::BuildUiSlots(WeaponClass);
	if (RangeDrawer::UiEquipSlot(Slots, Magazine) != ShooterTags::Tag_Attachment_Slot_Magazine) return false;
	return EquipAttachment(WeaponClass, Magazine);
}

void URangeLoadoutWidget::CollectCatalog()
{
	PrimaryClasses.Reset();
	SecondaryClasses.Reset();
	ThrowableClasses.Reset();

	TArray<UClass*> Found;
	FAssetRegistryModule& Assets = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> AssetList;
	Assets.Get().GetAssetsByPath(TEXT("/Game/FPS/Weapon"), AssetList, true);
	const FTopLevelAssetPath BlueprintPath(TEXT("/Script/Engine"), TEXT("Blueprint"));
	for (const FAssetData& Asset : AssetList)
	{
		if (Asset.AssetClassPath != BlueprintPath) continue;
		const FString ClassPath = Asset.GetObjectPathString() + TEXT("_C");
		UClass* Loaded = LoadClass<AWeapon>(nullptr, *ClassPath);
		if (!Loaded || Loaded->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated)) continue;
		const FString Name = Loaded->GetName();
		if (Name.Contains(TEXT("SKEL_")) || Name.Contains(TEXT("REINST_"))) continue;
		Found.AddUnique(Loaded);
	}

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(AWeapon::StaticClass()) || Class == AWeapon::StaticClass()) continue;
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated)) continue;
		if (!Class->IsNative()) continue;
		const FString Name = Class->GetName();
		if (Name.Contains(TEXT("SKEL_")) || Name.Contains(TEXT("REINST_"))) continue;
		Found.AddUnique(Class);
	}

	Found.RemoveAll([&Found](UClass* Class)
	{
		for (UClass* Other : Found)
		{
			if (Other != Class && Other->IsChildOf(Class))
			{
				return true;
			}
		}
		return false;
	});

	for (UClass* Class : Found)
	{
		const AWeapon* CDO = Class->GetDefaultObject<AWeapon>();
		if (!CDO || IsHiddenWeapon(Class)) continue;
		if (CDO->WeaponKind == EWeaponKind::LongGun) PrimaryClasses.Add(Class);
		else if (CDO->WeaponKind == EWeaponKind::Pistol) SecondaryClasses.Add(Class);
		else if (CDO->WeaponKind == EWeaponKind::Throwable) ThrowableClasses.Add(Class);
	}
	// Keep the working M67 available for comparison, even if an older bench hides it.
	if (UClass* Grenade = LoadClass<AWeapon>(nullptr, TEXT("/Game/FPS/Weapon/Throwable/BP_Weapon_Grenade.BP_Weapon_Grenade_C")))
	{
		ThrowableClasses.AddUnique(Grenade);
	}

	auto ByName = [](const TSubclassOf<AWeapon>& A, const TSubclassOf<AWeapon>& B)
	{
		return RangeDrawer::ClassLabel(A).ToString() < RangeDrawer::ClassLabel(B).ToString();
	};
	PrimaryClasses.Sort(ByName);
	SecondaryClasses.Sort(ByName);
	ThrowableClasses.AddUnique(AWeaponSting::StaticClass());
	ThrowableClasses.Sort(ByName);
}

void URangeLoadoutWidget::SeedFromPawn()
{
	Primary = FWeaponPreset();
	Secondary = FWeaponPreset();
	Throwables.Reset();

	const AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer());
	const APawn* OwnedPawn = PC ? PC->GetPawn() : nullptr;
	const UCombatComponent* Combat = OwnedPawn ? OwnedPawn->FindComponentByClass<UCombatComponent>() : nullptr;
	if (!Combat) return;

	const AShooterCharacter* Shooter = Cast<AShooterCharacter>(OwnedPawn);
	const UPouchInventoryComponent* Pouch = Shooter ? Shooter->GetPouch() : nullptr;
	auto CountPouch = [Pouch](EPouchSlotKind Kind, const FGameplayTag& MagType, TSubclassOf<AWeapon> ThrowableClass)
	{
		if (!Pouch) return 0;
		int32 Found = 0;
		for (int32 Index = 0; Index < Pouch->GetSlotCount(); ++Index)
		{
			const FPouchSlot Entry = Pouch->GetSlot(Index);
			if (Entry.Kind != Kind) continue;
			if (Kind == EPouchSlotKind::Magazine && MagType.IsValid() && Entry.MagWeaponType.MatchesTagExact(MagType))
			{
				++Found;
			}
			else if (Kind == EPouchSlotKind::Throwable && Entry.ThrowableClass == ThrowableClass)
			{
				++Found;
			}
		}
		return Found;
	};

	for (AWeapon* Weapon : Combat->GetInventory())
	{
		if (!IsValid(Weapon)) continue;
		FWeaponPreset Preset;
		Preset.WeaponClass = Weapon->GetClass();
		for (UAttachmentData* Data : Weapon->GetEquippedAttachments())
		{
			if (IsValid(Data)) Preset.Attachments.Add(Data);
		}
		if (Weapon->WeaponKind == EWeaponKind::LongGun && !Primary.WeaponClass)
		{
			Preset.StackCount = CountPouch(EPouchSlotKind::Magazine, Weapon->WeaponType, nullptr);
			Primary = Preset;
		}
		else if (Weapon->WeaponKind == EWeaponKind::Pistol && !Secondary.WeaponClass)
		{
			Preset.StackCount = CountPouch(EPouchSlotKind::Magazine, Weapon->WeaponType, nullptr);
			Secondary = Preset;
		}
		else if (Weapon->WeaponKind == EWeaponKind::Throwable && Throwables.Num() < GetMaxThrowables())
		{
			const TSubclassOf<AWeapon> ThrowableClass = Weapon->GetClass();
			if (Throwables.ContainsByPredicate([&ThrowableClass](const FWeaponPreset& Existing)
			{
				return Existing.WeaponClass == ThrowableClass;
			}))
			{
				continue;
			}
			Preset.StackCount = CountPouch(EPouchSlotKind::Throwable, FGameplayTag(), ThrowableClass);
			if (Preset.StackCount <= 0) Preset.StackCount = FMath::Max(1, Weapon->Ammo);
			Throwables.Add(Preset);
		}
	}
}

bool URangeLoadoutWidget::IsHiddenWeapon(TSubclassOf<AWeapon> WeaponClass) const
{
	if (!WeaponClass || !Bench) return false;
	if (WeaponClass->GetPathName() == TEXT("/Game/FPS/Weapon/Throwable/BP_Weapon_Grenade.BP_Weapon_Grenade_C")) return false;
	for (const TSubclassOf<AWeapon>& Hidden : Bench->HiddenWeapons)
	{
		if (Hidden && WeaponClass->IsChildOf(Hidden))
		{
			return true;
		}
	}
	return false;
}

void URangeLoadoutWidget::SeedDefaults(FWeaponPreset& Preset) const
{
	Preset.Attachments.Reset();
	for (const FWeaponAttachmentSlot& AttachSlot : RangeDrawer::BuildUiSlots(Preset.WeaponClass))
	{
		if (IsValid(AttachSlot.DefaultAttachment))
		{
			Preset.Attachments.Add(AttachSlot.DefaultAttachment);
		}
	}
}

FWeaponPreset* URangeLoadoutWidget::FindPreset(TSubclassOf<AWeapon> WeaponClass)
{
	if (Primary.WeaponClass == WeaponClass) return &Primary;
	if (Secondary.WeaponClass == WeaponClass) return &Secondary;
	for (FWeaponPreset& Preset : Throwables)
	{
		if (Preset.WeaponClass == WeaponClass) return &Preset;
	}
	return nullptr;
}

const FWeaponPreset* URangeLoadoutWidget::FindPreset(TSubclassOf<AWeapon> WeaponClass) const
{
	if (Primary.WeaponClass == WeaponClass) return &Primary;
	if (Secondary.WeaponClass == WeaponClass) return &Secondary;
	for (const FWeaponPreset& Preset : Throwables)
	{
		if (Preset.WeaponClass == WeaponClass) return &Preset;
	}
	return nullptr;
}

void URangeLoadoutWidget::SetSlotAttachment(FWeaponPreset& Preset, UAttachmentData* Attachment)
{
	if (!IsValid(Attachment) || !Preset.WeaponClass) return;
	const TArray<FWeaponAttachmentSlot> Slots = RangeDrawer::BuildUiSlots(Preset.WeaponClass);
	const FGameplayTag SlotTag = RangeDrawer::UiEquipSlot(Slots, Attachment);
	Preset.Attachments.RemoveAll([&Slots, SlotTag](const TObjectPtr<UAttachmentData>& Existing)
	{
		return !IsValid(Existing) || RangeDrawer::UiEquipSlot(Slots, Existing) == SlotTag;
	});
	Preset.Attachments.RemoveAll([Attachment, &Slots](const TObjectPtr<UAttachmentData>& Existing)
	{
		if (!IsValid(Existing)) return true;
		const FGameplayTag ExistingSlot = RangeDrawer::UiEquipSlot(Slots, Existing);
		return Attachment->BlockedSlots.HasTagExact(Existing->SlotTag) || Attachment->BlockedSlots.HasTagExact(ExistingSlot)
			|| (ExistingSlot == ShooterTags::Tag_Attachment_Slot_Laser &&
				(Attachment->BlockedSlots.HasTagExact(ShooterTags::Tag_Attachment_Slot_Tactical)
				|| Attachment->BlockedSlots.HasTagExact(ShooterTags::Tag_Attachment_Slot_Flashlight)));
	});
	Preset.Attachments.Add(Attachment);
}

void URangeLoadoutWidget::RemoveSlotAttachment(FWeaponPreset& Preset, const FGameplayTag& SlotTag)
{
	const TArray<FWeaponAttachmentSlot> Slots = RangeDrawer::BuildUiSlots(Preset.WeaponClass);
	Preset.Attachments.RemoveAll([&Slots, SlotTag](const TObjectPtr<UAttachmentData>& Existing)
	{
		return IsValid(Existing) && RangeDrawer::UiEquipSlot(Slots, Existing) == SlotTag;
	});
}

void URangeLoadoutWidget::PushPreview(const FWeaponPreset* Preset)
{
	AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer());
	if (!PC || !Bench || !Preset || !Preset->WeaponClass) return;
	TArray<UAttachmentData*> Attachments;
	for (const TObjectPtr<UAttachmentData>& Data : Preset->Attachments)
	{
		if (IsValid(Data)) Attachments.Add(Data.Get());
	}
	PC->UpdateRangeBenchPreview(Bench, Preset->WeaponClass, Attachments);
}

void URangeLoadoutWidget::QueueSelectionChanged()
{
	if (bSelectionNotifyQueued) return;
	UWorld* World = GetWorld();
	if (!World) return;
	bSelectionNotifyQueued = true;
	World->GetTimerManager().SetTimerForNextTick(this, &URangeLoadoutWidget::BroadcastSelectionChanged);
}

void URangeLoadoutWidget::BroadcastSelectionChanged()
{
	bSelectionNotifyQueued = false;
	RebuildDrawer();
	OnSelectionChanged();
}

void URangeRowClick::HandleClick()
{
	if (Owner)
	{
		Owner->ActivateRow(Kind, WeaponClass, Attachment, SlotTag);
	}
}

void URangeLoadoutWidget::ResolveDrawerWidgets()
{
	if (!WidgetTree) return;
	if (!BoxPrimary) BoxPrimary = WidgetTree->FindWidget<UVerticalBox>(TEXT("BoxPrimary"));
	if (!BoxSecondary) BoxSecondary = WidgetTree->FindWidget<UVerticalBox>(TEXT("BoxSecondary"));
	if (!BoxWarehouse) BoxWarehouse = WidgetTree->FindWidget<UVerticalBox>(TEXT("BoxWarehouse"));
	if (!BtnPrimary) BtnPrimary = WidgetTree->FindWidget<UButton>(TEXT("BtnPrimary"));
	if (!BtnSecondary) BtnSecondary = WidgetTree->FindWidget<UButton>(TEXT("BtnSecondary"));
	if (!BtnWarehouse) BtnWarehouse = WidgetTree->FindWidget<UButton>(TEXT("BtnWarehouse"));
	if (!LeaveButton)
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			UButton* Btn = Cast<UButton>(Widget);
			if (!Btn || LeaveButton) return;
			const FString Name = Btn->GetName();
			FString ContentText;
			TArray<UWidget*> Pending;
			if (UWidget* Content = Btn->GetContent()) Pending.Add(Content);
			while (Pending.Num() > 0)
			{
				UWidget* Next = Pending.Pop(EAllowShrinking::No);
				if (const UTextBlock* Text = Cast<UTextBlock>(Next))
				{
					ContentText = Text->GetText().ToString();
					break;
				}
				if (const UPanelWidget* Panel = Cast<UPanelWidget>(Next))
				{
					for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
					{
						Pending.Add(Panel->GetChildAt(ChildIndex));
					}
				}
			}
			if (Name.Contains(TEXT("Leave")) || Name.Contains(TEXT("离开")) || ContentText.Contains(TEXT("离开")))
			{
				LeaveButton = Btn;
			}
		});
	}
	if (!RowWidgetClass)
	{
		RowWidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/FPS/UI/WBP_RangeRow.WBP_RangeRow_C"));
	}
}

void URangeLoadoutWidget::BindDrawerButtons()
{
	if (bDrawerButtonsBound) return;
	if (BtnPrimary) BtnPrimary->OnClicked.AddDynamic(this, &URangeLoadoutWidget::HandlePrimaryHeader);
	if (BtnSecondary) BtnSecondary->OnClicked.AddDynamic(this, &URangeLoadoutWidget::HandleSecondaryHeader);
	if (BtnWarehouse) BtnWarehouse->OnClicked.AddDynamic(this, &URangeLoadoutWidget::HandleWarehouseHeader);
	if (LeaveButton) LeaveButton->OnClicked.AddDynamic(this, &URangeLoadoutWidget::HandleLeaveHeader);
	bDrawerButtonsBound = true;
}

void URangeLoadoutWidget::HandlePrimaryHeader() { ToggleDrawer(1); }
void URangeLoadoutWidget::HandleSecondaryHeader() { ToggleDrawer(2); }
void URangeLoadoutWidget::HandleWarehouseHeader() { ToggleDrawer(3); }
void URangeLoadoutWidget::HandleLeaveHeader() { CloseBench(); }

void URangeLoadoutWidget::ToggleDrawer(int32 Category)
{
	if (DrawerCategory == Category)
	{
		DrawerCategory = 0;
	}
	else
	{
		DrawerCategory = Category;
		DrawerSlot = FGameplayTag();
		if (Category == 1) DrawerWeapon = Primary.WeaponClass;
		else if (Category == 2) DrawerWeapon = Secondary.WeaponClass;
		else DrawerWeapon = nullptr;
	}
	QueueSelectionChanged();
}

void URangeLoadoutWidget::ActivateRow(uint8 Kind, TSubclassOf<AWeapon> WeaponClass, UAttachmentData* Attachment, const FGameplayTag& InSlotTag)
{
 if(Kind>=20&&Kind<=22){SelectFaceProtection(EPoliceFaceProtection(Kind-19));return;}
 if(Kind==30||Kind==31){SelectBodyArmor(Kind==30?EBodyArmorType::Light:EBodyArmorType::Heavy);return;}
	if (Kind == 0 || Kind == 1)
	{
		if (DrawerWeapon == WeaponClass) DrawerWeapon = nullptr;
		else DrawerWeapon = WeaponClass;
		DrawerSlot = FGameplayTag();
		const bool bAccepted = Kind == 0 ? SelectPrimary(WeaponClass) : SelectSecondary(WeaponClass);
		if (!bAccepted) QueueSelectionChanged();
		return;
	}
	if (Kind == 2)
	{
		return;
	}
	if (Kind == 7)
	{
		AdjustMagCount(WeaponClass, 1);
		return;
	}
	if (Kind == 8)
	{
		AdjustMagCount(WeaponClass, -1);
		return;
	}
	if (Kind == 9)
	{
		AdjustThrowableCount(WeaponClass, 1);
		return;
	}
	if (Kind == 10)
	{
		AdjustThrowableCount(WeaponClass, -1);
		return;
	}
	if (Kind == 3)
	{
		if (DrawerSlot.MatchesTagExact(InSlotTag)) DrawerSlot = FGameplayTag();
		else DrawerSlot = InSlotTag;
		QueueSelectionChanged();
		return;
	}
	if (Kind == 4)
	{
		EquipAttachment(WeaponClass, Attachment);
		return;
	}
	if (Kind == 5)
	{
		ClearAttachmentSlot(WeaponClass, InSlotTag);
		return;
	}
	if (Kind == 6)
	{
		EquipMagazine(WeaponClass, Attachment);
	}
}

void URangeLoadoutWidget::RebuildDrawer()
{
 if(UsesNativePresentation()){RefreshNativePresentation();return;}
	ResolveDrawerWidgets();
	if (!BoxPrimary || !BoxSecondary || !BoxWarehouse)
	{
		UE_LOG(LogTemp, Error, TEXT("[RangeBench] 找不到 BoxPrimary / BoxSecondary / BoxWarehouse。这三个 Vertical Box 的名字要对上，并勾 Is Variable。"));
		return;
	}
	RowClicks.Reset();
	BoxPrimary->ClearChildren();
	BoxSecondary->ClearChildren();
	BoxWarehouse->ClearChildren();

	auto Show = [](UWidget* Widget, bool bOpen)
	{
		if (Widget) Widget->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	};
	Show(BoxPrimary, DrawerCategory == 1);
	Show(BoxSecondary, DrawerCategory == 2);
	Show(BoxWarehouse, DrawerCategory == 3);

	if (DrawerCategory == 1) FillWeaponList(BoxPrimary, GetPrimaryChoices(), 0);
	else if (DrawerCategory == 2) FillWeaponList(BoxSecondary, GetSecondaryChoices(), 1);
	else if (DrawerCategory == 3) FillWarehouse();
	FitDrawerIcons();
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<URangeLoadoutWidget> WeakThis(this);
		FTimerHandle FitHandle;
		World->GetTimerManager().SetTimer(FitHandle, [WeakThis]()
		{
			if (URangeLoadoutWidget* Widget = WeakThis.Get())
			{
				Widget->FitDrawerIcons();
			}
		}, 0.f, false);
	}
}

void URangeLoadoutWidget::FitDrawerIcons()
{
	auto FitBox = [this](UVerticalBox* Box)
	{
		if (!Box) return;
		for (int32 Index = 0; Index < Box->GetChildrenCount(); ++Index)
		{
			if (UUserWidget* Row = Cast<UUserWidget>(Box->GetChildAt(Index)))
			{
				FitRowIcon(Row);
			}
		}
	};
	FitBox(BoxPrimary);
	FitBox(BoxSecondary);
	FitBox(BoxWarehouse);
}

void URangeLoadoutWidget::FillWeaponList(UVerticalBox* Target, const TArray<FRangeWeaponChoice>& Choices, uint8 Kind)
{
	for (const FRangeWeaponChoice& Choice : Choices)
	{
		AddDrawerRow(Target, 0.f, Choice.DisplayName, Choice.Icon, Choice.bSelected, false, Kind, Choice.WeaponClass, nullptr, FGameplayTag());
		if (Choice.bSelected && DrawerWeapon == Choice.WeaponClass)
		{
			FillAttachmentSlots(Target, Choice.WeaponClass);
		}
	}
}

void URangeLoadoutWidget::FillAttachmentSlots(UVerticalBox* Target, TSubclassOf<AWeapon> WeaponClass)
{
	for (const FRangeAttachmentSlotView& View : GetWeaponSlots(WeaponClass))
	{
		FString Line = View.EquippedName.IsEmpty()
			? View.SlotName.ToString()
			: View.SlotName.ToString() + TEXT("    ") + View.EquippedName.ToString();
		const bool bMagazineSlot = View.SlotTag.MatchesTagExact(ShooterTags::Tag_Attachment_Slot_Magazine);
		if (bMagazineSlot)
		{
			Line += FString::Printf(TEXT("    x%d"), GetStackCount(WeaponClass));
		}
		AddDrawerRow(Target, 16.f, FText::FromString(Line), View.EquippedIcon, false, View.bLocked, 3, WeaponClass, nullptr, View.SlotTag);
		if (bMagazineSlot)
		{
			AddCountRows(Target, 32.f, GetStackCount(WeaponClass), PouchUsed() < PouchLimit(), 7, 8, WeaponClass, TEXT("＋  增加一个弹匣"), TEXT("－  减少一个弹匣"));
		}
		if (View.bLocked || !DrawerSlot.MatchesTagExact(View.SlotTag)) continue;
		if (View.bCanClear)
		{
			AddDrawerRow(Target, 32.f, FText::FromString(TEXT("卸下")), nullptr, false, false, 5, WeaponClass, nullptr, View.SlotTag);
		}
		for (const FRangeAttachmentChoice& Choice : View.Choices)
		{
			AddDrawerRow(Target, 32.f, Choice.DisplayName, Choice.Icon, Choice.bEquipped, false, 4, WeaponClass, Choice.Attachment, View.SlotTag);
		}
	}
}

void URangeLoadoutWidget::AddCountRows(UVerticalBox* Target, float Indent, int32 Count, bool bCanAdd, uint8 PlusKind, uint8 MinusKind, TSubclassOf<AWeapon> WeaponClass, const FString& PlusLabel, const FString& MinusLabel)
{
	AddDrawerRow(Target, Indent, FText::FromString(FString::Printf(TEXT("数量  x%d"), Count)), nullptr, false, true, 255, WeaponClass, nullptr, FGameplayTag());
	if (bCanAdd)
	{
		AddDrawerRow(Target, Indent, FText::FromString(PlusLabel), nullptr, false, false, PlusKind, WeaponClass, nullptr, FGameplayTag());
	}
	if (Count > 0)
	{
		AddDrawerRow(Target, Indent, FText::FromString(MinusLabel), nullptr, false, false, MinusKind, WeaponClass, nullptr, FGameplayTag());
	}
}

void URangeLoadoutWidget::FillWarehouse()
{
 if(auto* Character=GetOwningPlayerPawn<AShooterCharacter>();Character&&Character->PoliceAppearance)
 {
  const auto Face=Character->PoliceAppearance->FaceProtection;
  AddDrawerRow(BoxWarehouse,0,FText::FromString(TEXT("角色防护")),nullptr,false,true,255,nullptr,nullptr,FGameplayTag());
  const TCHAR* Names[]={TEXT("闪光防护眼镜"),TEXT("闪光防护眼镜＋防弹面罩"),TEXT("防毒面具")};
  for(uint8 I=0;I<3;++I)AddDrawerRow(BoxWarehouse,0,FText::FromString(Names[I]),nullptr,uint8(Face)==I+1,false,20+I,nullptr,nullptr,FGameplayTag());
  AddDrawerRow(BoxWarehouse,0,FText::FromString(TEXT("轻甲")),nullptr,Character->GetBodyArmor()==EBodyArmorType::Light,false,30,nullptr,nullptr,FGameplayTag());
  AddDrawerRow(BoxWarehouse,0,FText::FromString(TEXT("重甲")),nullptr,Character->GetBodyArmor()==EBodyArmorType::Heavy,false,31,nullptr,nullptr,FGameplayTag());
 }
	AddDrawerRow(BoxWarehouse, 0.f, FText::FromString(FString::Printf(TEXT("背包  %d/%d"), PouchUsed(), PouchLimit())), nullptr, false, true, 255, nullptr, nullptr, FGameplayTag());
	AddDrawerRow(BoxWarehouse, 0.f, FText::FromString(TEXT("投掷物")), nullptr, false, true, 255, nullptr, nullptr, FGameplayTag());
	const int32 TypeCount = GetThrowableCount();
	for (const FRangeWeaponChoice& Choice : GetThrowableChoices())
	{
		const int32 Count = GetStackCount(Choice.WeaponClass);
		const FString Line = FString::Printf(TEXT("%s    x%d"), *Choice.DisplayName.ToString(), Count);
		const bool bCanAdd = PouchUsed() < PouchLimit() && (Count > 0 || TypeCount < GetMaxThrowables());
		AddDrawerRow(BoxWarehouse, 0.f, FText::FromString(Line), Choice.Icon, Count > 0, false, 2, Choice.WeaponClass, nullptr, FGameplayTag());
		AddCountRows(BoxWarehouse, 16.f, Count, bCanAdd, 9, 10, Choice.WeaponClass, TEXT("＋  增加一个"), TEXT("－  减少一个"));
	}
	AddDrawerRow(BoxWarehouse, 0.f, FText::FromString(TEXT("弹匣")), nullptr, false, true, 255, nullptr, nullptr, FGameplayTag());
	const FWeaponPreset* Guns[] = { &Primary, &Secondary };
	for (const FWeaponPreset* Preset : Guns)
	{
		if (!Preset || !Preset->WeaponClass) continue;
		const FString GunName = RangeDrawer::ClassLabel(Preset->WeaponClass).ToString();
		AddDrawerRow(BoxWarehouse, 0.f, FText::FromString(FString::Printf(TEXT("%s  备用弹匣"), *GunName)), nullptr, false, true, 255, Preset->WeaponClass, nullptr, FGameplayTag());
		AddCountRows(BoxWarehouse, 16.f, FMath::Max(0, Preset->StackCount), PouchUsed() < PouchLimit(), 7, 8, Preset->WeaponClass, TEXT("＋  增加一个弹匣"), TEXT("－  减少一个弹匣"));
	}
	for (const FRangeMagazineChoice& Choice : GetMagazineChoices())
	{
		AddDrawerRow(BoxWarehouse, 0.f, Choice.DisplayName, Choice.Icon, Choice.bEquipped, false, 6, Choice.WeaponClass, Choice.Magazine, FGameplayTag());
	}
}

void URangeLoadoutWidget::AddDrawerRow(UVerticalBox* Target, float Indent, const FText& Label, UTexture2D* Icon, bool bSelected, bool bLocked, uint8 Kind, TSubclassOf<AWeapon> WeaponClass, UAttachmentData* Attachment, const FGameplayTag& InSlotTag)
{
	if (!Target || !RowWidgetClass) return;
	UUserWidget* Row = CreateWidget<UUserWidget>(GetOwningPlayer(), RowWidgetClass);
	if (!Row) return;

	if (FTextProperty* Prop = FindFProperty<FTextProperty>(Row->GetClass(), TEXT("Label")))
	{
		Prop->SetPropertyValue_InContainer(Row, Label);
	}
	if (FTextProperty* Prop = FindFProperty<FTextProperty>(Row->GetClass(), TEXT("SubText")))
	{
		Prop->SetPropertyValue_InContainer(Row, FText::GetEmpty());
	}
	if (FObjectProperty* Prop = FindFProperty<FObjectProperty>(Row->GetClass(), TEXT("Icon")))
	{
		Prop->SetObjectPropertyValue_InContainer(Row, Icon);
	}
	if (FBoolProperty* Prop = FindFProperty<FBoolProperty>(Row->GetClass(), TEXT("bSelected")))
	{
		Prop->SetPropertyValue_InContainer(Row, bSelected);
	}
	if (FBoolProperty* Prop = FindFProperty<FBoolProperty>(Row->GetClass(), TEXT("bLocked")))
	{
		Prop->SetPropertyValue_InContainer(Row, bLocked);
	}
	if (FProperty* KindProp = Row->GetClass()->FindPropertyByName(TEXT("Kind")))
	{
		if (FByteProperty* ByteProp = CastField<FByteProperty>(KindProp))
		{
			ByteProp->SetPropertyValue_InContainer(Row, Kind);
		}
		else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(KindProp))
		{
			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(Row), static_cast<int64>(Kind));
		}
	}
	if (FClassProperty* Prop = FindFProperty<FClassProperty>(Row->GetClass(), TEXT("WeaponClass")))
	{
		Prop->SetObjectPropertyValue_InContainer(Row, WeaponClass.Get());
	}
	if (FObjectProperty* Prop = FindFProperty<FObjectProperty>(Row->GetClass(), TEXT("Attachment")))
	{
		Prop->SetObjectPropertyValue_InContainer(Row, Attachment);
	}
	if (FStructProperty* Prop = FindFProperty<FStructProperty>(Row->GetClass(), TEXT("SlotTag")))
	{
		if (Prop->Struct == FGameplayTag::StaticStruct())
		{
			*Prop->ContainerPtrToValuePtr<FGameplayTag>(Row) = InSlotTag;
		}
	}

	if (!bLocked && Kind != 255)
	{
		UButton* Btn = Row->WidgetTree ? Row->WidgetTree->FindWidget<UButton>(TEXT("Btn")) : nullptr;
		if (Btn)
		{
			URangeRowClick* Click = NewObject<URangeRowClick>(this);
			Click->Owner = this;
			Click->Kind = Kind;
			Click->WeaponClass = WeaponClass;
			Click->Attachment = Attachment;
			Click->SlotTag = InSlotTag;
			Btn->OnClicked.AddDynamic(Click, &URangeRowClick::HandleClick);
			RowClicks.Add(Click);
		}
	}

	if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(Target->AddChild(Row)))
	{
		RowSlot->SetPadding(FMargin(Indent, 3.f, 8.f, 3.f));
	}
	FitRowIcon(Row);
}

void URangeLoadoutWidget::FitRowIcon(UUserWidget* Row) const
{
	if (!Row || !Row->WidgetTree) return;
	const FVector2D IconSize(36.f, 36.f);
	if (USizeBox* IconBox = Cast<USizeBox>(Row->WidgetTree->FindWidget(TEXT("IconBox"))))
	{
		IconBox->SetWidthOverride(IconSize.X);
		IconBox->SetHeightOverride(IconSize.Y);
	}
	Row->WidgetTree->ForEachWidget([&IconSize](UWidget* Widget)
	{
		UImage* Image = Cast<UImage>(Widget);
		if (!Image) return;
		const bool bIcon = Image->GetName().Contains(TEXT("Icon"));
		if (!bIcon) return;
		Image->SetDesiredSizeOverride(IconSize);
		FSlateBrush Brush = Image->GetBrush();
		Brush.ImageSize = IconSize;
		Image->SetBrush(Brush);
	});
}

FLoadoutPreset URangeLoadoutWidget::BuildLoadout() const
{
	FLoadoutPreset Loadout;
	Loadout.Name = FText::FromString(TEXT("靶场配装"));
	Loadout.bReplaceThrowables = true;
 if(auto* C=GetOwningPlayerPawn<AShooterCharacter>()){Loadout.bIncludesProtection=true;Loadout.BodyArmor=C->GetBodyArmor();if(C->PoliceAppearance)Loadout.FaceProtection=C->PoliceAppearance->FaceProtection;}
	if (Primary.WeaponClass) Loadout.Weapons.Add(Primary);
	if (Secondary.WeaponClass) Loadout.Weapons.Add(Secondary);
	for (const FWeaponPreset& Preset : Throwables)
	{
		if (Preset.WeaponClass) Loadout.Weapons.Add(Preset);
	}
	return Loadout;
}

void URangeLoadoutWidget::CommitIfDirty()
{
	AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer());
	if (!PC) return;
	const FLoadoutPreset Loadout = BuildLoadout();
	if (Loadout.Weapons.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RangeBench] leave skipped: no weapons in the bench loadout."));
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[RangeBench] leave commits %d weapon(s), pouch %d/%d."), Loadout.Weapons.Num(), PouchUsed(), PouchLimit());
	PC->Server_SubmitLoadout(Loadout);
	bDirty = false;
}
