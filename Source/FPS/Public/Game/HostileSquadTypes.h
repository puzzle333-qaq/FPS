#pragma once

#include "CoreMinimal.h"
#include "HostileSquadTypes.generated.h"

/** Job inside a CQB stack. Spawners with several hostiles hand these out in order. */
UENUM(BlueprintType)
enum class EHostileSquadRole : uint8
{
	/** Hold the post, face the contact, and shoot. Does not chase. */
	Suppressor UMETA(DisplayName = "Suppressor"),
	/** Push the contact, using the existing cover and peek fight. */
	Closer UMETA(DisplayName = "Closer"),
	/** Arc to a side angle once an ally is already fighting. */
	Flanker UMETA(DisplayName = "Flanker")
};

/** Rolled once, the first time a bandit is hit. */
UENUM(BlueprintType)
enum class EHostileMorale : uint8
{
	/** Still fighting. The next hit rolls surrender, last stand, or flee. */
	Fighting UMETA(DisplayName = "Fighting"),
	/** Dropped the gunfight. Stays down. */
	Surrender UMETA(DisplayName = "Surrender"),
	/** Keeps shooting. Later hits do not make this one run. */
	LastStand UMETA(DisplayName = "Last Stand"),
	/** Runs until line of sight to the player is gone, then stops. */
	Flee UMETA(DisplayName = "Flee")
};

/** What the squad subsystem is told this pawn is doing right now. */
UENUM()
enum class EHostileSquadActivity : uint8
{
	Idle,
	Alert,
	Engage,
	Flank,
	Suppress,
	Retreat
};
