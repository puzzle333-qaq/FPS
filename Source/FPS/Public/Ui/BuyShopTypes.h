#pragma once

#include "CoreMinimal.h"
#include "BuyShopTypes.generated.h"

class AWeapon;
class UAttachmentData;
class UTexture2D;

// 爆破商店里的一件可购商品（在 BP_BDMGameMode / SearchDestroy 的 BuyCatalog 里填）
USTRUCT(BlueprintType)
struct FBuyShopItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Buy")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Buy")
	TSubclassOf<AWeapon> WeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Buy", meta = (ClampMin = "0"))
	int32 Price = 0;

	// 为空则购买时尽量用改枪台里该枪的预设配件
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Buy")
	TArray<TObjectPtr<UAttachmentData>> DefaultAttachments;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Buy")
	TObjectPtr<UTexture2D> Icon;
};