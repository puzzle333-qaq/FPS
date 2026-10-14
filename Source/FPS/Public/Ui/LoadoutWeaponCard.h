#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadoutWeaponCard.generated.h"

class AWeapon;
class UButton;
class UTextBlock;
class UImage;

DECLARE_MULTICAST_DELEGATE_OneParam(FLoadoutCardClicked, int32 /*CardIndex*/);

// 整备界面左侧的一张武器卡片。做 WBP 子类（WBP_LoadoutWeaponCard）自己设计；树为空时用代码默认样式。
UCLASS()
class FPS_API ULoadoutWeaponCard : public UUserWidget
{
	GENERATED_BODY()

public:
	// 由 LoadoutScreen 调用。Weapon 为空 = 空槽
	void SetupCard(int32 InIndex, AWeapon* InWeapon, const FText& InName);
	void SetSelected(bool bInSelected);

	FLoadoutCardClicked OnCardClicked;

	UFUNCTION(BlueprintPure, Category = "FPS|Loadout")
	AWeapon* GetWeapon() const { return Weapon.Get(); }

	UFUNCTION(BlueprintPure, Category = "FPS|Loadout")
	int32 GetCardIndex() const { return CardIndex; }

	UFUNCTION(BlueprintPure, Category = "FPS|Loadout")
	bool IsSelected() const { return bSelected; }

	UFUNCTION(BlueprintPure, Category = "FPS|Loadout")
	bool IsEmptySlot() const { return !Weapon.IsValid(); }

	// 状态变化时调用（选中 / 空槽 / 换了武器）。在 WBP 里实现它改颜色、边框等
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Loadout")
	void OnCardStateChanged(bool bIsSelected, bool bIsEmpty, AWeapon* InWeapon);

	// 空槽显示的文字
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Loadout")
	FText EmptyText = NSLOCTEXT("Loadout", "EmptySlot", "空");

protected:
	virtual void NativeOnInitialized() override;

	// 必需：整张卡片的点击按钮
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> Button;

	// 可选：武器名
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Name;

	// 可选：武器图标（AWeapon::WeaponIcon）。没有图标时隐藏
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Icon;

private:
	void BuildLayout();
	void NotifyState();

	UFUNCTION()
	void HandleClicked();

	TWeakObjectPtr<AWeapon> Weapon;
	int32 CardIndex = INDEX_NONE;
	bool bSelected = false;
};
