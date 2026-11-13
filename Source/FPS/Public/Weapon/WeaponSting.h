#pragma once

#include "CoreMinimal.h"
#include "Weapon/Weapon.h"
#include "WeaponSting.generated.h"

/** Sting / stinger grenade holdable. Throws AThrownStinger (look-lock, no damage). */
UCLASS()
class FPS_API AWeaponSting : public AWeapon
{
	GENERATED_BODY()
public:
	AWeaponSting();
};
