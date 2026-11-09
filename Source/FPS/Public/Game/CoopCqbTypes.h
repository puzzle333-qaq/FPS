#pragma once

#include "CoreMinimal.h"
#include "CoopCqbTypes.generated.h"

/** Operators = human (and later AI teammate) squad. Hostiles are not on a PlayerState team. */
namespace CoopCqbTeams
{
	constexpr uint8 Operators = 0;
}

UENUM(BlueprintType)
enum class ECoopCqbPhase : uint8
{
	Waiting      UMETA(DisplayName = "Waiting"),
	Briefing     UMETA(DisplayName = "Briefing"),
	InProgress   UMETA(DisplayName = "In Progress"),
	PostMission  UMETA(DisplayName = "Post Mission")
};

UENUM(BlueprintType)
enum class ECoopCqbObjectiveType : uint8
{
	ClearEnemies UMETA(DisplayName = "Clear All Hostiles"),
	Custom       UMETA(DisplayName = "Custom (later)")
};

UENUM(BlueprintType)
enum class ECoopCqbWinReason : uint8
{
	None            UMETA(DisplayName = "None"),
	ObjectivesMet   UMETA(DisplayName = "Objectives Complete"),
	TeamWiped       UMETA(DisplayName = "Team Wiped"),
	Aborted         UMETA(DisplayName = "Aborted"),
 HostageLost UMETA(DisplayName="Hostage Lost"),
 LoadTimeout UMETA(DisplayName="Load Timeout")
};

/** Look-pitch open mode for BreakableDoor (push vs kick). */
UENUM(BlueprintType)
enum class EDoorOpenMode : uint8
{
	Push UMETA(DisplayName = "Push (slow quiet)"),
	Kick UMETA(DisplayName = "Kick (fast loud)")
};

/** Legacy aim-zone enum kept for older notes; door now uses look pitch (EDoorOpenMode). */
UENUM(BlueprintType)
enum class EDoorAimZone : uint8
{
	None    UMETA(DisplayName = "None"),
	Upper   UMETA(DisplayName = "Upper / Gentle Push"),
	Handle  UMETA(DisplayName = "Handle / Open"),
	Bottom  UMETA(DisplayName = "Bottom / Kick")
};
