#pragma once

#include "CoreMinimal.h"
#include "Weapon/Weapon.h"
#include "BombWeapon.generated.h"

class UStaticMeshComponent;

// 爆破模式 5 号位：切出来拿在手上。动画可空，安放走包点长按 F。
UCLASS()
class FPS_API ABombWeapon : public AWeapon
{
	GENERATED_BODY()

public:
	ABombWeapon();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Bomb")
	TObjectPtr<UStaticMeshComponent> Visual1P;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Bomb")
	TObjectPtr<UStaticMeshComponent> Visual3P;
};
