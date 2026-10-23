#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Loadout/GunsmithComponent.h"
#include "GunsmithScreen.generated.h"

class UButton;
class UImage;
class UGunsmithComponent;
class UAttachmentData;
class AWeapon;
class UPanelWidget;
class UCanvasPanel;
class UGunsmithCategoryItem;
class UGunsmithAttachmentItem;
class UGunsmithStatItem;
class UGunsmithPartName;
class UGunsmithWeaponItem;

USTRUCT(BlueprintType)
struct FGunsmithSlotScreenTag
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Gunsmith") FGameplayTag SlotTag;
	// 预览图上的 UV（0~1）
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Gunsmith") FVector2D UV = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Gunsmith") bool bVisible = false;
};

class UGunsmithScreen;

// 动态补出来的槽位按钮点击转发（WBP 里只有 6 个固定槽位按钮）。
UCLASS()
class FPS_API UGunsmithSlotButtonProxy : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TObjectPtr<UGunsmithScreen> Screen;

	FGameplayTag SlotTag;

	UFUNCTION()
	void HandleClicked();
};

// 没填 WeaponItemClass 时，C++ 自己造的武器按钮的点击转发
UCLASS()
class FPS_API UGunsmithWeaponButtonProxy : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TObjectPtr<UGunsmithScreen> Screen;

	UPROPERTY()
	TSubclassOf<AWeapon> WeaponClass;

	UFUNCTION()
	void HandleClicked();
};

// 改枪台界面的 C++ 父类（可选）。WBP_Gunsmith 选它当父类，控件按名字自动绑定，全部是可选的。
UCLASS()
class FPS_API UGunsmithScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FPS|Gunsmith")
	UGunsmithComponent* GetGunsmith() const;

	// 打开改枪台时调用：换预览枪并刷新贴图
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith")
	void ShowWeapon(TSubclassOf<AWeapon> WeaponClass);

	// 槽位按钮点了以后：记下当前槽位，并通知蓝图打开配件列表
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith")
	void SelectSlot(FGameplayTag SlotTag);

	UFUNCTION(BlueprintPure, Category = "FPS|Gunsmith")
	FGameplayTag GetSelectedSlot() const { return SelectedSlot; }

	// 配件列表里点了一项（传空 = 卸下当前槽）
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith")
	bool PickAttachment(UAttachmentData* Attachment);

	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith")
	bool Save();

	// 和保存同一套：配件写入配装，长枪变成进局第一把。WBP 可另放名为 Button_Equip 的按钮。
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith")
	bool Equip();

	// ---- 给蓝图实现的事件 ----
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Gunsmith")
	void OnBackRequested();

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Gunsmith")
	void OnSaved(bool bSuccess);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Gunsmith")
	void OnEquippedPrimary(TSubclassOf<AWeapon> WeaponClass, bool bSuccess);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Gunsmith")
	void OnSlotSelected(FGameplayTag SlotTag, const TArray<UAttachmentData*>& Allowed, UAttachmentData* Equipped);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Gunsmith")
	void OnPreviewUpdated();

	// ---- 预览视角（WBP 也可以自己调） ----
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith|View")
	void RotatePreview(float DeltaYaw, float DeltaPitch);

	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith|View")
	void ZoomPreview(float Delta);

	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith|View")
	bool FocusSlot(FGameplayTag SlotTag);

	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith|View")
	void ResetView();

	// 每个槽在预览图上的位置（给自己画标签用；放了 Canvas_PartNames 就不用管，C++ 自动摆）
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith|View")
	TArray<FGunsmithSlotScreenTag> GetSlotScreenPositions() const;

	// ---- 属性 ----
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith|Stats")
	TArray<FGunsmithStatLine> GetStatPreview(FGameplayTag SlotTag, UAttachmentData* Candidate) const;

	// 悬停配件时调用：属性条显示换上后的对比
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith|Stats")
	void PreviewStatsFor(FGameplayTag SlotTag, UAttachmentData* Candidate);

	// 回到当前配装的属性
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith|Stats")
	void RefreshStats();

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Gunsmith|Stats")
	void OnStatsUpdated(const TArray<FGunsmithStatLine>& Lines);

	// ---- 动态列表用的子控件类（填 UECANDY 改过父类的 WBP） ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith|Widgets")
	TSubclassOf<UGunsmithCategoryItem> CategoryItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith|Widgets")
	TSubclassOf<UGunsmithAttachmentItem> AttachmentItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith|Widgets")
	TSubclassOf<UGunsmithStatItem> StatItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith|Widgets")
	TSubclassOf<UGunsmithPartName> PartNameClass;

	// 武器列表一项。留空也行：C++ 在 Panel_Weapons 里自己造“按钮+文字”
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith|Widgets")
	TSubclassOf<UGunsmithWeaponItem> WeaponItemClass;

	// 改枪台可选的枪：GunsmithComponent.AvailableWeapons，再补上当前主长枪（去重）
	UFUNCTION(BlueprintPure, Category = "FPS|Gunsmith")
	TArray<TSubclassOf<AWeapon>> GetGunsmithWeapons() const;

	// 配件列表第一项放“卸下”（必选槽不放）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith|Widgets")
	bool bAddRemoveItem = true;

	// 选槽位时镜头自动对准
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith|View")
	bool bFocusOnSelect = true;

	// 拖拽：每像素转多少度
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith|View")
	float DragRotateSpeed = 0.4f;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// UECANDY 布局里的容器（名字对上就自动用，全部可选）
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> Panel_Categories;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> Panel_Attachments;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> Panel_Stats;
	// 盖在 Image_Preview 上、同样大小的 CanvasPanel，放槽位标签
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UCanvasPanel> Canvas_PartNames;
	// 武器列表容器（VerticalBox / HorizontalBox / ScrollBox / WrapBox 都行）
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> Panel_Weapons;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Back;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Save;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Equip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Preview;

	// WBP 里现成的槽位按钮。槽位多于 BuiltInSlotButtons 时，C++ 在同一个容器里补按钮。
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Slot5;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Slot6;

	UPROPERTY(EditAnywhere, Category = "FPS|Gunsmith")
	int32 BuiltInSlotButtons = 6;

private:
	UFUNCTION()
	void HandleBack();

	UFUNCTION()
	void HandleSave();

	UFUNCTION()
	void HandleEquip();

	UFUNCTION()
	void HandlePreviewChanged();

	FGameplayTag SelectedSlot;

	void RebuildExtraSlotButtons();
	void RebuildCategories();
	void RebuildAttachmentList();
	void ApplyStats(const TArray<FGunsmithStatLine>& Lines);
	void RebuildPartNames();
	void RebuildWeaponList();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGunsmithWeaponButtonProxy>> WeaponProxies;
	void UpdatePartNamePositions();
	bool IsOverPreview(const FPointerEvent& Event) const;

	bool bDragging = false;

	UPROPERTY(Transient) TArray<TObjectPtr<UGunsmithPartName>> PartNameWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> ExtraSlotButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGunsmithSlotButtonProxy>> ExtraSlotProxies;
};