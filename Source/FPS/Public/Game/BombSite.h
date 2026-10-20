#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/SearchDestroyTypes.h"
#include "BombSite.generated.h"

class UBoxComponent;
class UBillboardComponent;

// 关卡里摆两个：A 点和 B 点。爆破模式用重叠盒判断能不能安放。
UCLASS()
class FPS_API ABombSite : public AActor
{
	GENERATED_BODY()

public:
	ABombSite();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Bomb")
	EBombSiteId SiteId = EBombSiteId::A;

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	EBombSiteId GetSiteId() const { return SiteId; }

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	bool ContainsPawn(const APawn* Pawn) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Bomb")
	static FText GetSiteLetter(EBombSiteId Id);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Bomb")
	TObjectPtr<UBoxComponent> Zone;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif
};
