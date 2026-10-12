#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AttachmentSlotRow.generated.h"

class UTextBlock;
class UPanelWidget;
class UAttachmentOptionEntry;
class AWeapon;
struct FWeaponAttachmentSlot;

// 一行 = 枪上的一个槽位：左边槽位名，右边一排可选配件按钮
UCLASS()
class FPS_API UAttachmentSlotRow : public UUserWidget
{
	GENERATED_BODY()
public:
	void Setup(AWeapon* Weapon, const FWeaponAttachmentSlot& SlotInfo);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_SlotName;

	// 放按钮的容器：Horizontal Box 或 Wrap Box 都行（都是 PanelWidget）
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> EntryContainer;

	// 每个按钮用哪个蓝图（填 WBP_AttachmentEntry）
	UPROPERTY(EditAnywhere, Category = "FPS|Attachment")
	TSubclassOf<UAttachmentOptionEntry> EntryClass;
};
