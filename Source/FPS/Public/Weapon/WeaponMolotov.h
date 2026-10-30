#pragma once

#include "CoreMinimal.h"
#include "Weapon/Weapon.h"
#include "WeaponMolotov.generated.h"

/** MVP criminal molotov holdable. Prefer BP child for mesh; native works for throw logic. */
UCLASS()
class FPS_API AWeaponMolotov : public AWeapon
{
	GENERATED_BODY()
public:
	AWeaponMolotov();
};
