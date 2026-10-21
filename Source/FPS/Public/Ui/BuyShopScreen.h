#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Ui/BuyShopTypes.h"
#include "BuyShopScreen.generated.h"

class UButton;
class UTextBlock;

// 爆破购买商店 C++ 父类。WBP_BuyShop 选它，Button_Close 可选按名绑定。
UCLASS()
class FPS_API UBuyShopScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FPS|Buy")
	void CloseShop();

	// 购买目录第 Index 项（走控制器 Server RPC）
	UFUNCTION(BlueprintCallable, Category = "FPS|Buy")
	bool BuyItem(int32 CatalogIndex);

	UFUNCTION(BlueprintPure, Category = "FPS|Buy")
	TArray<FBuyShopItem> GetCatalog() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Buy")
	int32 GetLocalMoney() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Buy")
	bool CanBuyNow() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Buy")
	void OnShopOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Buy")
	void OnShopClosed();

	// 金钱变化时刷新列表（蓝图里绑 Text_Money / 禁用买不起的按钮）
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Buy")
	void OnMoneyUpdated(int32 NewMoney);

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Buy")
	void OnCatalogUpdated();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Money;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleMoneyChanged(int32 NewMoney);

	void BindLocalMoney();
	void UnbindLocalMoney();

	TWeakObjectPtr<class AShooterPlayerState> BoundPlayerState;
};