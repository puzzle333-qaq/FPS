#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "ShooterType.generated.h"

UENUM(BlueprintType)
enum class ETurningInPlace : uint8
{
	Left UMETA(DisplayName = "TurningLeft"),
	Right UMETA(DisplayName = "TurningRight"),
	NotTurning UMETA(DisplayName = "NotTurning")
};

UENUM(BlueprintType)
enum class EHitZone : uint8
{
	Head,
	Torso,
	Limb
};

// Bone from the weapon hit, then a damage scale. Empty or unknown bones count as torso.
EHitZone ResolveHitZone(FName BoneName);
FName ResolveDamageBone(const FHitResult& Hit);
float ScaleDamageForBone(float BaseDamage, FName BoneName, float HeadshotMultiplier, float LimbMultiplier);
