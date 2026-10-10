#include "ShooterType/ShooterType.h"

#include "Components/SkeletalMeshComponent.h"

namespace
{
	bool BoneNameHas(const FString& Name, const TCHAR* Token)
	{
		return Name.Contains(Token, ESearchCase::IgnoreCase);
	}
}

EHitZone ResolveHitZone(FName BoneName)
{
	if (BoneName.IsNone())
	{
		return EHitZone::Torso;
	}

	const FString Name = BoneName.ToString();
	if (BoneNameHas(Name, TEXT("head")) || BoneNameHas(Name, TEXT("neck")))
	{
		return EHitZone::Head;
	}

	if (BoneNameHas(Name, TEXT("arm")) || BoneNameHas(Name, TEXT("hand"))
		|| BoneNameHas(Name, TEXT("thigh")) || BoneNameHas(Name, TEXT("calf"))
		|| BoneNameHas(Name, TEXT("foot")) || BoneNameHas(Name, TEXT("toe"))
		|| BoneNameHas(Name, TEXT("ball")) || BoneNameHas(Name, TEXT("leg")))
	{
		return EHitZone::Limb;
	}

	return EHitZone::Torso;
}

FName ResolveDamageBone(const FHitResult& Hit)
{
	if (!Hit.BoneName.IsNone())
	{
		return Hit.BoneName;
	}

	// A mesh hit with no physics body still has a skeleton. Use the bone nearest the impact.
	const USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(Hit.GetComponent());
	if (!IsValid(Mesh))
	{
		return NAME_None;
	}

	return Mesh->FindClosestBone(Hit.ImpactPoint);
}

float ScaleDamageForBone(float BaseDamage, FName BoneName, float HeadshotMultiplier, float LimbMultiplier)
{
	switch (ResolveHitZone(BoneName))
	{
	case EHitZone::Head:
		return BaseDamage * HeadshotMultiplier;
	case EHitZone::Limb:
		return BaseDamage * LimbMultiplier;
	default:
		return BaseDamage;
	}
}
