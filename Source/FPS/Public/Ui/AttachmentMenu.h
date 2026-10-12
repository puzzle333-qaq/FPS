#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AttachmentMenu.generated.h"

class UVerticalBox;
class UTextBlock;
class UAttachmentSlotRow;
class UCombatComponent;
class AWeapon;

// 配件菜单根控件：按当前枪的槽位表，一行一个槽位
UCLASS()
class FPS_API UAttachmentMenu : public UUserWidget
{
	GENERATED_BODY()
public:
	// 重新生成所有行
	UFUNCTION(BlueprintCallable, Category = "FPS|Attachment")
	void Refresh();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> SlotList;

	// 可选：显示当前枪名
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_WeaponName;

	// 每行用哪个蓝图（填 WBP_AttachmentSlotRow）
	UPROPERTY(EditAnywhere, Category = "FPS|Attachment")
	TSubclassOf<UAttachmentSlotRow> RowClass;

private:
	UFUNCTION()
	void OnAttachmentsChanged(AWeapon* Weapon);

	UFUNCTION()
	void OnCurrentWeaponChanged(AWeapon* NewWeapon);

	void BindToWeapon(AWeapon* Weapon);

	// 用弱指针记住绑定过的对象，关闭时好解绑
	TWeakObjectPtr<UCombatComponent> BoundCombat;
	TWeakObjectPtr<AWeapon> BoundWeapon;
};
