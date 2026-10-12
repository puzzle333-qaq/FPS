#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadoutScreen.generated.h"

class AWeapon;
class ALoadoutPreviewRig;
class UAttachmentSlotRow;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UButton;
class UVerticalBox;
class UBorder;

// 点一把枪后弹出的配件窗。拖标题栏可以挪位置。
UCLASS()
class FPS_API ULoadoutAttachmentWindow : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowForWeapon(AWeapon* InWeapon);
	void SetWindowPosition(FVector2D Position);
	bool HasUserMoved() const { return bUserMoved; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void BuildLayout();
	void BindWeapon(AWeapon* InWeapon);
	void RebuildSlots();

	UFUNCTION()
	void HandleAttachmentsChanged(AWeapon* ChangedWeapon);

	UPROPERTY()
	TObjectPtr<UBorder> DragHandle;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_WeaponName;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text_Empty;

	UPROPERTY()
	TObjectPtr<UVerticalBox> SlotList;

	UPROPERTY()
	TSubclassOf<UAttachmentSlotRow> RowClass;

	TWeakObjectPtr<AWeapon> Weapon;
	bool bBuilt = false;
	bool bDragging = false;
	bool bUserMoved = false;
	FVector2D DragOffset = FVector2D::ZeroVector;
};

// Tab 打开的全屏整备界面。左侧三把枪，点开可拖动的配件窗。右侧留给背包，这里不做。
UCLASS()
class FPS_API ULoadoutScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	ULoadoutScreen(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildLayout();
	void RefreshWeapons();
	void OpenWindow(int32 Index);
	void SetCardLabel(int32 Index, const FText& Label, bool bEnabled);

	UFUNCTION()
	void OnClickedWeapon0();
	UFUNCTION()
	void OnClickedWeapon1();
	UFUNCTION()
	void OnClickedWeapon2();

	UFUNCTION()
	void HandleAttachmentsChanged(AWeapon* ChangedWeapon);

	UPROPERTY()
	TObjectPtr<UCanvasPanel> PopupLayer;

	UPROPERTY()
	TObjectPtr<UImage> PreviewImage;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> WeaponButtons;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> WeaponLabels;

	UPROPERTY()
	TArray<TObjectPtr<AWeapon>> ShownWeapons;

	UPROPERTY()
	TObjectPtr<ALoadoutPreviewRig> PreviewRig;

	UPROPERTY()
	TObjectPtr<ULoadoutAttachmentWindow> AttachmentWindow;

	bool bBuilt = false;
};
