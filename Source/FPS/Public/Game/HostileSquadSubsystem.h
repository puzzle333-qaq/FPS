#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Game/HostileSquadTypes.h"
#include "HostileSquadSubsystem.generated.h"

/**
 * Live roster of hostile pawns: role, what they are doing, and who they are shooting.
 * AHostileAIController registers on possess.
 *
 * Registry shape adapted from TacticalAIShowcase by Mesbah Ur Rahman (MIT):
 * https://github.com/murahman-dev/TacticalAIShowcase
 */
UCLASS()
class FPS_API UHostileSquadSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterAlly(APawn* Pawn, EHostileSquadRole Role);
	void UnregisterAlly(APawn* Pawn);

	/** Next job in the stack: Suppressor, Closer, Flanker, then repeat. */
	EHostileSquadRole TakeNextRole();
	void Publish(APawn* Pawn, EHostileSquadActivity Activity, AActor* Target);

	/** Other allies currently suppressing, pushing, or flanking. */
	int32 NumberOfEngagedAllies(const APawn* Asker) const;

	/** A player some other ally is actively fighting, if one exists. */
	AActor* MostEngagedTarget(const APawn* Asker) const;

private:
	struct FAllyInfo
	{
		TWeakObjectPtr<APawn> Pawn;
		EHostileSquadRole Role = EHostileSquadRole::Closer;
		EHostileSquadActivity Activity = EHostileSquadActivity::Idle;
		TWeakObjectPtr<AActor> Target;
	};

	TArray<FAllyInfo> Allies;
	int32 NextRoleIndex = 0;
};
