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
	Arm,
	Leg
};

// Police body armor. Only torso (chest/abdomen) is reduced; explosives/fire bypass via non-ballistic damage paths.
UENUM(BlueprintType)
enum class EBodyArmorType : uint8
{
	None  UMETA(DisplayName = "None"),
	Light UMETA(DisplayName = "Light"),
	Heavy UMETA(DisplayName = "Heavy")
};

// 3P appearance for PIE / future faction spawn. Default = stock Mannequin from character BP.
UENUM(BlueprintType)
enum class ECharacterLookFaction : uint8
{
	Default   UMETA(DisplayName = "DefaultManny"),
	Criminal  UMETA(DisplayName = "Criminal"),
	Police    UMETA(DisplayName = "Police")
};

// Police tactical optic: Off / Night Vision / Thermal. Local camera PP; battery drains while on.
UENUM(BlueprintType)
enum class ETacticalVisionMode : uint8
{
	Off     UMETA(DisplayName = "Off"),
	NVG     UMETA(DisplayName = "NVG"),
	Thermal UMETA(DisplayName = "Thermal")
};


// Bone from the weapon hit, then a damage scale. Empty or unknown bones count as torso.
EHitZone ResolveHitZone(FName BoneName);
FName ResolveDamageBone(const FHitResult& Hit);
float ScaleDamageForBone(float BaseDamage, FName BoneName, float HeadshotMultiplier, float LimbMultiplier);
