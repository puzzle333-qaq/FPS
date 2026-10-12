#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "AttachmentOptionEntry.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UAttachmentData;

// 一个配件按钮：点一下就把这个配件装到当前枪上
// Data 为空时表示“卸下”按钮（只给可以卸空的槽位用）
UCLASS()
class FPS_API UAttachmentOptionEntry : public UUserWidget
{
	GENERATED_BODY()
public:
	// 由 SlotRow 调用：告诉按钮显示哪个配件、属于哪个槽、现在是否已装上
	void Setup(UAttachmentData* InData, FGameplayTag InSlotTag, bool bInEquipped, bool bInAllowed = true);

protected:
	virtual void NativeOnInitialized() override;

	// 下面的名字必须和 UMG 里控件的名字完全一样
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Select;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Icon;

	// 已装备 / 未装备 时按钮的颜色
	UPROPERTY(EditAnywhere, Category = "FPS|Attachment")
	FLinearColor EquippedColor = FLinearColor(1.f, 0.75f, 0.1f);

	UPROPERTY(EditAnywhere, Category = "FPS|Attachment")
	FLinearColor NormalColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "FPS|Attachment")
	FText RemoveText = NSLOCTEXT("Attachment", "Remove", "无");

private:
	UFUNCTION()
	void OnSelectClicked();

	UPROPERTY()
	TObjectPtr<UAttachmentData> Data;

	FGameplayTag SlotTag;
	bool bEquipped = false;
};
