#pragma once

#include "CoreMinimal.h"
#include "Weapon/Weapon.h"
#include "WeaponFlash.generated.h"

/** MVP police flashbang holdable. Prefer BP child for mesh; native works for throw logic. */
UCLASS()
class FPS_API AWeaponFlash : public AWeapon
{
	GENERATED_BODY()
public:
	AWeaponFlash();
};
