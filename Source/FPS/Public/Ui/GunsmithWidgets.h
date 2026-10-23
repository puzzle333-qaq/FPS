#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Loadout/GunsmithComponent.h"
#include "GunsmithWidgets.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UProgressBar;
class UGunsmithScreen;
class UAttachmentData;
class AWeapon;

// 改枪台小控件的 C++ 父类。UECANDY 的 WBP_S1_* 复制后改父类成这些，控件按名字自动绑定（全部可选）。

// 左侧槽位分类（WBP_S1_CategoryItem）
UCLASS(Abstract)
class FPS_API UGunsmithCategoryItem : public UUserWidget
{
	GENERATED_BODY()
public:
	void Setup(UGunsmithScreen* InScreen, FGameplayTag InSlot, UAttachmentData* Equipped, bool bSelected);

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Gunsmith")
	FGameplayTag SlotTag;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Gunsmith")
	TObjectPtr<UAttachmentData> EquippedAttachment;

	UFUNCTION(BlueprintPure, Category = "FPS|Gunsmith")
	static FText GetSlotDisplayName(FGameplayTag Tag);

	// 蓝图里可以做高亮动画
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Gunsmith")
	void OnItemSetup(bool bSelected);

	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith")
	void Select();

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Select;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SlotName;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Equipped;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_Icon;

	UPROPERTY(Transient) TObjectPtr<UGunsmithScreen> Screen;
};

// 配件列表一项（WBP_S1_AttachmentItem）。Attachment 为空 = “卸下”。
UCLASS(Abstract)
class FPS_API UGunsmithAttachmentItem : public UUserWidget
{
	GENERATED_BODY()
public:
	void Setup(UGunsmithScreen* InScreen, FGameplayTag InSlot, UAttachmentData* InAttachment, bool bEquipped);

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Gunsmith")
	FGameplayTag SlotTag;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Gunsmith")
	TObjectPtr<UAttachmentData> Attachment;

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Gunsmith")
	void OnItemSetup(bool bEquipped);

	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith")
	void Pick();

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION() void HandleHovered();
	UFUNCTION() void HandleUnhovered();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Select;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Name;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Description;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_Icon;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UWidget> Widget_EquippedMark;

	UPROPERTY(Transient) TObjectPtr<UGunsmithScreen> Screen;
};

// 一条属性（WBP_S1_Stats_Item）：两根条叠放，ProgressBar_Preview 在下（显示换后），ProgressBar_Current 在上。
UCLASS(Abstract)
class FPS_API UGunsmithStatItem : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith")
	void SetStat(const FGunsmithStatLine& Line);

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Gunsmith")
	FGunsmithStatLine Stat;

	// 变好/变差颜色
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Gunsmith")
	FLinearColor BetterColor = FLinearColor(0.2f, 0.85f, 0.3f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Gunsmith")
	FLinearColor WorseColor = FLinearColor(0.9f, 0.25f, 0.2f);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Gunsmith")
	void OnStatSet(float Delta);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Label;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Value;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> ProgressBar_Current;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> ProgressBar_Preview;
};

// 武器列表一项（WBP_GS_WeaponItem，父类选它）。点了就换预览枪
UCLASS(Abstract)
class FPS_API UGunsmithWeaponItem : public UUserWidget
{
	GENERATED_BODY()
public:
	void Setup(UGunsmithScreen* InScreen, TSubclassOf<AWeapon> InWeapon, bool bSelected);

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Gunsmith")
	TSubclassOf<AWeapon> WeaponClass;

	// BP_Weapon_PCC_C -> PCC
	UFUNCTION(BlueprintPure, Category = "FPS|Gunsmith")
	static FText GetWeaponDisplayName(TSubclassOf<AWeapon> InWeapon);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Gunsmith")
	void OnItemSetup(bool bSelected);

	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith")
	void Select();

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Select;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Name;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_Icon;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UWidget> Widget_SelectedMark;

	UPROPERTY(Transient) TObjectPtr<UGunsmithScreen> Screen;
};

// 预览图上的槽位标签（WBP_S1_PartName），点了就选这个槽
UCLASS(Abstract)
class FPS_API UGunsmithPartName : public UUserWidget
{
	GENERATED_BODY()
public:
	void Setup(UGunsmithScreen* InScreen, FGameplayTag InSlot, UAttachmentData* Equipped, bool bSelected);

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Gunsmith")
	FGameplayTag SlotTag;

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Gunsmith")
	void OnItemSetup(bool bSelected);

protected:
	virtual void NativeOnInitialized() override;
	UFUNCTION() void HandleClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Select;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(Transient) TObjectPtr<UGunsmithScreen> Screen;
};