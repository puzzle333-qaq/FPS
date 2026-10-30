#pragma once

#include "GameFramework/Actor.h"
#include "ShooterType/ShooterType.h"

// Owner HUD marks for the A-pose. Stored on the actor (not a new replicated field)
// so Live Coding can pick this up while the editor is open.
// Side: 0 = unknown, 1 = left, 2 = right. Unknown arm/leg marks both sides.
namespace HudBodyMarks
{
	inline FName TagHead() { return FName(TEXT("HudHit.Head")); }
	inline FName TagTorso() { return FName(TEXT("HudHit.Torso")); }
	inline FName TagArmL() { return FName(TEXT("HudHit.ArmL")); }
	inline FName TagArmR() { return FName(TEXT("HudHit.ArmR")); }
	inline FName TagLegL() { return FName(TEXT("HudHit.LegL")); }
	inline FName TagLegR() { return FName(TEXT("HudHit.LegR")); }

	inline void AddUnique(AActor* Actor, FName Tag)
	{
		if (IsValid(Actor) && !Actor->ActorHasTag(Tag))
		{
			Actor->Tags.Add(Tag);
		}
	}

	inline int32 SideFromBone(FName BoneName)
	{
		const FString Name = BoneName.ToString();
		if (Name.EndsWith(TEXT("_l")) || Name.Contains(TEXT("_l_")) || Name.Contains(TEXT("left"), ESearchCase::IgnoreCase))
		{
			return 1;
		}
		if (Name.EndsWith(TEXT("_r")) || Name.Contains(TEXT("_r_")) || Name.Contains(TEXT("right"), ESearchCase::IgnoreCase))
		{
			return 2;
		}
		return 0;
	}

	inline void AddHit(AActor* Actor, EHitZone Zone, int32 Side)
	{
		if (!IsValid(Actor)) return;
		switch (Zone)
		{
		case EHitZone::Head:
			AddUnique(Actor, TagHead());
			break;
		case EHitZone::Torso:
			AddUnique(Actor, TagTorso());
			break;
		case EHitZone::Arm:
			if (Side == 2) AddUnique(Actor, TagArmR());
			else if (Side == 1) AddUnique(Actor, TagArmL());
			else
			{
				AddUnique(Actor, TagArmL());
				AddUnique(Actor, TagArmR());
			}
			break;
		case EHitZone::Leg:
			if (Side == 2) AddUnique(Actor, TagLegR());
			else if (Side == 1) AddUnique(Actor, TagLegL());
			else
			{
				AddUnique(Actor, TagLegL());
				AddUnique(Actor, TagLegR());
			}
			break;
		default:
			break;
		}
	}

	// Head and torso have no side. Limbs are marked from the hit bone so left stays left.
	inline void NoteZone(AActor* Actor, EHitZone Zone)
	{
		if (!IsValid(Actor)) return;
		if (Zone == EHitZone::Head) AddUnique(Actor, TagHead());
		else if (Zone == EHitZone::Torso) AddUnique(Actor, TagTorso());
	}

	inline bool IsHurt(const AActor* Actor, EHitZone Zone, int32 Side)
	{
		if (!IsValid(Actor)) return false;
		switch (Zone)
		{
		case EHitZone::Head: return Actor->ActorHasTag(TagHead());
		case EHitZone::Torso: return Actor->ActorHasTag(TagTorso());
		case EHitZone::Arm:
			if (Side == 2) return Actor->ActorHasTag(TagArmR());
			if (Side == 1) return Actor->ActorHasTag(TagArmL());
			return Actor->ActorHasTag(TagArmL()) || Actor->ActorHasTag(TagArmR());
		case EHitZone::Leg:
			if (Side == 2) return Actor->ActorHasTag(TagLegR());
			if (Side == 1) return Actor->ActorHasTag(TagLegL());
			return Actor->ActorHasTag(TagLegL()) || Actor->ActorHasTag(TagLegR());
		default:
			return false;
		}
	}

	inline void RemoveTag(AActor* Actor, FName Tag)
	{
		if (IsValid(Actor))
		{
			Actor->Tags.Remove(Tag);
		}
	}

	inline void Clear(AActor* Actor)
	{
		RemoveTag(Actor, TagHead());
		RemoveTag(Actor, TagTorso());
		RemoveTag(Actor, TagArmL());
		RemoveTag(Actor, TagArmR());
		RemoveTag(Actor, TagLegL());
		RemoveTag(Actor, TagLegR());
	}

	inline void ClearArms(AActor* Actor)
	{
		RemoveTag(Actor, TagArmL());
		RemoveTag(Actor, TagArmR());
	}

	inline void ClearLegs(AActor* Actor)
	{
		RemoveTag(Actor, TagLegL());
		RemoveTag(Actor, TagLegR());
	}
}
