#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "GunsmithScreen.generated.h"

class UButton;
class UImage;
class UGunsmithComponent;
class UAttachmentData;
class AWeapon;

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

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Back;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Save;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Equip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Preview;

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
};