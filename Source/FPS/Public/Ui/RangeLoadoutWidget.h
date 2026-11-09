#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Loadout/LoadoutTypes.h"
#include "Weapon/Weapon.h"
#include "RangeLoadoutWidget.generated.h"

class ARangeLoadoutBench;
class AWeapon;
class UAttachmentData;
class UButton;
class URangeRowClick;
class UTexture2D;
class UVerticalBox;

// 主武器 / 副武器 / 投掷物列表里的一行。图标和名字给 WBP 用，展开收起由界面自己做。
USTRUCT(BlueprintType)
struct FRangeWeaponChoice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	TSubclassOf<AWeapon> WeaponClass;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	bool bSelected = false;
};

USTRUCT(BlueprintType)
struct FRangeAttachmentChoice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	TObjectPtr<UAttachmentData> Attachment;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	bool bEquipped = false;
};

// 一把枪的一个配件槽，以及这个槽现在能选的配件。锁定的槽 Choices 是空的。
USTRUCT(BlueprintType)
struct FRangeAttachmentSlotView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	FGameplayTag SlotTag;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	FText SlotName;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	FText EquippedName;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	TObjectPtr<UTexture2D> EquippedIcon;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	bool bRequired = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	bool bLocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	bool bCanClear = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	TArray<FRangeAttachmentChoice> Choices;
};

// 仓库里的一格弹匣。WeaponClass 是这颗弹匣要装到的那把枪（当前主武器或副武器）。
USTRUCT(BlueprintType)
struct FRangeMagazineChoice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	TSubclassOf<AWeapon> WeaponClass;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	FText WeaponName;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	TObjectPtr<UAttachmentData> Magazine;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|改枪台")
	bool bEquipped = false;
};

// 改枪台的数据层。不要直接用这个类当界面。
// 做一个 Widget Blueprint，父类选 RangeLoadoutWidget，把按钮接到下面这些函数上，
// 再把那个 WBP 填到改枪台的 Loadout Widget Class。
UCLASS(Blueprintable, meta = (DisplayName = "改枪台界面"))
class FPS_API URangeLoadoutWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Init(ARangeLoadoutBench* InBench);
	void CommitIfDirty();
	void NotifyBenchReady();
	void ActivateRow(uint8 Kind, TSubclassOf<AWeapon> WeaponClass, UAttachmentData* Attachment, const FGameplayTag& InSlotTag);

	UFUNCTION(BlueprintPure, Category = "FPS|改枪台", meta = (DisplayName = "主武器列表"))
	TArray<FRangeWeaponChoice> GetPrimaryChoices() const;

	UFUNCTION(BlueprintPure, Category = "FPS|改枪台", meta = (DisplayName = "副武器列表"))
	TArray<FRangeWeaponChoice> GetSecondaryChoices() const;

	UFUNCTION(BlueprintPure, Category = "FPS|改枪台", meta = (DisplayName = "投掷物列表"))
	TArray<FRangeWeaponChoice> GetThrowableChoices() const;

	UFUNCTION(BlueprintPure, Category = "FPS|改枪台", meta = (DisplayName = "弹匣列表"))
	TArray<FRangeMagazineChoice> GetMagazineChoices() const;

	UFUNCTION(BlueprintPure, Category = "FPS|改枪台", meta = (DisplayName = "配件槽"))
	TArray<FRangeAttachmentSlotView> GetWeaponSlots(TSubclassOf<AWeapon> WeaponClass) const;

	UFUNCTION(BlueprintPure, Category = "FPS|改枪台", meta = (DisplayName = "当前主武器"))
	TSubclassOf<AWeapon> GetSelectedPrimary() const { return Primary.WeaponClass; }

	UFUNCTION(BlueprintPure, Category = "FPS|改枪台", meta = (DisplayName = "当前副武器"))
	TSubclassOf<AWeapon> GetSelectedSecondary() const { return Secondary.WeaponClass; }

	UFUNCTION(BlueprintPure, Category = "FPS|改枪台", meta = (DisplayName = "投掷物上限"))
	int32 GetMaxThrowables() const { return 4; }

	UFUNCTION(BlueprintPure, Category = "FPS|改枪台", meta = (DisplayName = "当前投掷物数量"))
	int32 GetThrowableCount() const { return Throwables.Num(); }

	// 选中后会装上这把枪的默认配件，并刷新人物手上的预览。
	UFUNCTION(BlueprintCallable, Category = "FPS|改枪台", meta = (DisplayName = "选择主武器"))
	bool SelectPrimary(TSubclassOf<AWeapon> WeaponClass);

	UFUNCTION(BlueprintCallable, Category = "FPS|改枪台", meta = (DisplayName = "选择副武器"))
	bool SelectSecondary(TSubclassOf<AWeapon> WeaponClass);

	// 没带就放进仓库（最多 4 种），已经带着就拿走。返回值是现在还带不带。
	UFUNCTION(BlueprintCallable, Category = "FPS|改枪台", meta = (DisplayName = "切换投掷物"))
	bool ToggleThrowable(TSubclassOf<AWeapon> WeaponClass);

	// 步枪/手枪的备用弹匣。正数增加，负数减少，总数受背包格子限制。
	UFUNCTION(BlueprintCallable, Category = "FPS|改枪台", meta = (DisplayName = "调整弹匣数量"))
	bool AdjustMagCount(TSubclassOf<AWeapon> WeaponClass, int32 Delta);

	// 某一种投掷物的携带数量。减到 0 就从身上拿掉。
	UFUNCTION(BlueprintCallable, Category = "FPS|改枪台", meta = (DisplayName = "调整投掷物数量"))
	bool AdjustThrowableCount(TSubclassOf<AWeapon> WeaponClass, int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "FPS|改枪台", meta = (DisplayName = "装上配件"))
	bool EquipAttachment(TSubclassOf<AWeapon> WeaponClass, UAttachmentData* Attachment);

	UFUNCTION(BlueprintCallable, Category = "FPS|改枪台", meta = (DisplayName = "卸下配件"))
	bool ClearAttachmentSlot(TSubclassOf<AWeapon> WeaponClass, FGameplayTag SlotTag);

	UFUNCTION(BlueprintCallable, Category = "FPS|改枪台", meta = (DisplayName = "换弹匣"))
	bool EquipMagazine(TSubclassOf<AWeapon> WeaponClass, UAttachmentData* Magazine);

	// 关掉界面，并把当前主武器、副武器、弹匣数量、投掷物数量写回身上。F 和 Esc 也会走这里。
	UFUNCTION(BlueprintCallable, Category = "FPS|改枪台", meta = (DisplayName = "离开并装备"))
	void CloseBench();

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void HandlePrimaryHeader();

	UFUNCTION()
	void HandleSecondaryHeader();

	UFUNCTION()
	void HandleWarehouseHeader();

	UFUNCTION()
	void HandleLeaveHeader();

	// 界面已经挂上，列表可以建了。
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|改枪台", meta = (DisplayName = "数据准备好了"))
	void OnBenchReady();

	// 选枪、换配件之后的下一帧调用。在这里重刷你的列表。
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|改枪台", meta = (DisplayName = "选择变了"))
	void OnSelectionChanged();

private:
	void CollectCatalog();
	void SeedFromPawn();
	void SeedDefaults(FWeaponPreset& Preset) const;
	void PushPreview(const FWeaponPreset* Preset);
	void QueueSelectionChanged();
	void BroadcastSelectionChanged();
	FWeaponPreset* FindPreset(TSubclassOf<AWeapon> WeaponClass);
	const FWeaponPreset* FindPreset(TSubclassOf<AWeapon> WeaponClass) const;
	void SetSlotAttachment(FWeaponPreset& Preset, UAttachmentData* Attachment);
	void RemoveSlotAttachment(FWeaponPreset& Preset, const FGameplayTag& SlotTag);
	bool SelectWeapon(FWeaponPreset& Held, TSubclassOf<AWeapon> WeaponClass, EWeaponKind ExpectedKind);
	FLoadoutPreset BuildLoadout() const;
	FRangeWeaponChoice MakeWeaponChoice(TSubclassOf<AWeapon> WeaponClass, bool bSelected) const;
	bool IsHiddenWeapon(TSubclassOf<AWeapon> WeaponClass) const;
	void ResolveDrawerWidgets();
	void BindDrawerButtons();
	void RebuildDrawer();
	void ToggleDrawer(int32 Category);
	void FillWeaponList(UVerticalBox* Target, const TArray<FRangeWeaponChoice>& Choices, uint8 Kind);
	void FillAttachmentSlots(UVerticalBox* Target, TSubclassOf<AWeapon> WeaponClass);
	void FillWarehouse();
	void AddDrawerRow(UVerticalBox* Target, float Indent, const FText& Label, UTexture2D* Icon, bool bSelected, bool bLocked, uint8 Kind, TSubclassOf<AWeapon> WeaponClass, UAttachmentData* Attachment, const FGameplayTag& InSlotTag);
	void FitRowIcon(UUserWidget* Row) const;
	void FitDrawerIcons();
	int32 PouchLimit() const;
	int32 PouchUsed() const;
	int32 GetStackCount(TSubclassOf<AWeapon> WeaponClass) const;
	void AddCountRows(UVerticalBox* Target, float Indent, int32 Count, bool bCanAdd, uint8 PlusKind, uint8 MinusKind, TSubclassOf<AWeapon> WeaponClass, const FString& PlusLabel, const FString& MinusLabel);

	UPROPERTY()
	TObjectPtr<ARangeLoadoutBench> Bench;

	UPROPERTY()
	TArray<TSubclassOf<AWeapon>> PrimaryClasses;

	UPROPERTY()
	TArray<TSubclassOf<AWeapon>> SecondaryClasses;

	UPROPERTY()
	TArray<TSubclassOf<AWeapon>> ThrowableClasses;

	UPROPERTY()
	FWeaponPreset Primary;

	UPROPERTY()
	FWeaponPreset Secondary;

	UPROPERTY()
	TArray<FWeaponPreset> Throwables;

	UPROPERTY()
	TArray<TObjectPtr<URangeRowClick>> RowClicks;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> BoxPrimary;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> BoxSecondary;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> BoxWarehouse;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BtnPrimary;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BtnSecondary;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BtnWarehouse;

	UPROPERTY()
	TObjectPtr<UButton> LeaveButton;

	UPROPERTY()
	TSubclassOf<UUserWidget> RowWidgetClass;

	// 0 全关，1 主武器，2 副武器，3 仓库。展开哪一把、哪一个槽也记在这里。
	int32 DrawerCategory = 0;
	TSubclassOf<AWeapon> DrawerWeapon;
	FGameplayTag DrawerSlot;

	bool bDirty = false;
	bool bSelectionNotifyQueued = false;
	bool bDrawerButtonsBound = false;
};

// 每一行按钮的点击。行控件是蓝图，OnClicked 只能绑到 UFUNCTION。
UCLASS()
class FPS_API URangeRowClick : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<URangeLoadoutWidget> Owner;

	UPROPERTY()
	TSubclassOf<AWeapon> WeaponClass;

	UPROPERTY()
	TObjectPtr<UAttachmentData> Attachment;

	UPROPERTY()
	FGameplayTag SlotTag;

	uint8 Kind = 0;

	UFUNCTION()
	void HandleClick();
};
