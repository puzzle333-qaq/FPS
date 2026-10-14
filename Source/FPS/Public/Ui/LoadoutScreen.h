#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadoutScreen.generated.h"

class AWeapon;
class ALoadoutPreviewRig;
class UAttachmentSlotRow;
class ULoadoutWeaponCard;
class UCanvasPanel;
class UPanelWidget;
class UImage;
class UTextBlock;
class UButton;
class UWidget;

// 点一把枪后弹出的配件窗。拖标题栏可以挪位置。
// 可以做 WBP 子类自己设计：树里放好下面这些名字的控件即可；树为空时用代码默认布局。
UCLASS()
class FPS_API ULoadoutAttachmentWindow : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowForWeapon(AWeapon* InWeapon);
	void HideWindow();
	bool IsWindowShown() const;
	AWeapon* GetShownWeapon() const { return Weapon.Get(); }
	void SetWindowPosition(FVector2D Position);
	bool HasUserMoved() const { return bUserMoved; }

	// 每个配件槽一行用的控件。为空时加载 /Game/FPS/UI/WBP_AttachmentSlotRow
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Loadout")
	TSubclassOf<UAttachmentSlotRow> RowClass;

	// 行与行之间的上下间距（只在 SlotList 是 VerticalBox 时生效）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Loadout")
	FMargin RowPadding = FMargin(0.f, 4.f, 0.f, 4.f);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 必需：按住拖动窗口的标题栏（任意控件类型）
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> DragHandle;

	// 必需：放配件行的容器（推荐 VerticalBox / ScrollBox）
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPanelWidget> SlotList;

	// 可选：关闭按钮
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	// 可选：武器名
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_WeaponName;

	// 可选：没有配件槽时显示
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Empty;

private:
	void BuildLayout();
	void BindWeapon(AWeapon* InWeapon);
	void RebuildSlots();

	UFUNCTION()
	void HandleAttachmentsChanged(AWeapon* ChangedWeapon);

	UFUNCTION()
	void OnCloseClicked();

	TWeakObjectPtr<AWeapon> Weapon;
	bool bDragging = false;
	bool bUserMoved = false;
	FVector2D DragOffset = FVector2D::ZeroVector;
};

// Tab 打开的全屏整备界面。左侧三把枪，点开可拖动的配件窗。
// 可以做 WBP 子类（WBP_LoadoutScreen）自己设计；树为空时用代码默认布局。
UCLASS()
class FPS_API ULoadoutScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	ULoadoutScreen(const FObjectInitializer& ObjectInitializer);

	// 武器卡片控件类（WBP_LoadoutWeaponCard）。为空用 C++ 默认卡片
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Loadout")
	TSubclassOf<ULoadoutWeaponCard> CardClass;

	// 配件弹窗控件类（WBP_LoadoutAttachmentWindow）。为空用 C++ 默认弹窗
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Loadout")
	TSubclassOf<ULoadoutAttachmentWindow> AttachmentWindowClass;

	// 预览台类（可以做 BP 子类改渲染尺寸/正交宽度/行距）。为空用 C++ 默认
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Loadout")
	TSubclassOf<ALoadoutPreviewRig> PreviewRigClass;

	// 弹窗第一次出现的位置：X = 视口宽度 × 这个比例，Y = 像素
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Loadout")
	FVector2D WindowDefaultPosition = FVector2D(0.42f, 72.f);

	// 卡片数量（物品栏前 N 把）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Loadout", meta = (ClampMin = "1", ClampMax = "8"))
	int32 NumCards = 3;

	UFUNCTION(BlueprintPure, Category = "FPS|Loadout")
	ALoadoutPreviewRig* GetPreviewRig() const { return PreviewRig; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 必需：弹窗所在的画布（CanvasPanel，铺满全屏）。弹窗要靠 CanvasSlot 定位和拖动
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> PopupLayer;

	// 必需：放武器卡片的容器（VerticalBox / HorizontalBox / UniformGrid 等）
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPanelWidget> CardContainer;

	// 可选：三把枪的合成预览图（渲染目标）
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> PreviewImage;

	// 可选：标题 / 提示文字（代码不改它们的内容，WBP 里自己写）
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Hint;

private:
	void BuildLayout();
	void BuildCards();
	void RefreshWeapons();
	void RefreshCardStates();
	void OpenWindow(int32 Index);
	void HandleCardClicked(int32 Index);

	UFUNCTION()
	void HandleAttachmentsChanged(AWeapon* ChangedWeapon);

	UFUNCTION()
	void HandleWindowVisibilityChanged(ESlateVisibility InVisibility);

	UPROPERTY()
	TArray<TObjectPtr<ULoadoutWeaponCard>> Cards;

	UPROPERTY()
	TArray<TObjectPtr<AWeapon>> ShownWeapons;

	UPROPERTY()
	TObjectPtr<ALoadoutPreviewRig> PreviewRig;

	UPROPERTY()
	TObjectPtr<ULoadoutAttachmentWindow> AttachmentWindow;
};
