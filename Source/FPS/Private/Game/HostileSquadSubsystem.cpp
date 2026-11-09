#include "Game/HostileSquadSubsystem.h"

#include "GameFramework/Pawn.h"

void UHostileSquadSubsystem::RegisterAlly(APawn* Pawn, EHostileSquadRole Role)
{
	if (!Pawn) return;

	Allies.RemoveAll([Pawn](const FAllyInfo& A)
	{
		return A.Pawn.Get() == Pawn;
	});

	FAllyInfo Info;
	Info.Pawn = Pawn;
	Info.Role = Role;
	Info.Activity = EHostileSquadActivity::Idle;
	Allies.Add(MoveTemp(Info));
}

EHostileSquadRole UHostileSquadSubsystem::TakeNextRole()
{
	const EHostileSquadRole Order[] = {
		EHostileSquadRole::Suppressor,
		EHostileSquadRole::Closer,
		EHostileSquadRole::Flanker
	};
	const EHostileSquadRole Role = Order[NextRoleIndex % 3];
	++NextRoleIndex;
	return Role;
}

void UHostileSquadSubsystem::UnregisterAlly(APawn* Pawn)
{
	if (!Pawn) return;

	Allies.RemoveAll([Pawn](const FAllyInfo& A)
	{
		return A.Pawn.Get() == Pawn;
	});
}

void UHostileSquadSubsystem::Publish(APawn* Pawn, EHostileSquadActivity Activity, AActor* Target)
{
	for (FAllyInfo& A : Allies)
	{
		if (A.Pawn.Get() == Pawn)
		{
			A.Activity = Activity;
			A.Target = Target;
			return;
		}
	}
}

int32 UHostileSquadSubsystem::NumberOfEngagedAllies(const APawn* Asker) const
{
	int32 Count = 0;
	for (const FAllyInfo& A : Allies)
	{
		const APawn* Other = A.Pawn.Get();
		if (!Other || Other == Asker) continue;
		if (A.Activity == EHostileSquadActivity::Engage
			|| A.Activity == EHostileSquadActivity::Flank
			|| A.Activity == EHostileSquadActivity::Suppress)
		{
			++Count;
		}
	}
	return Count;
}

AActor* UHostileSquadSubsystem::MostEngagedTarget(const APawn* Asker) const
{
	for (const FAllyInfo& A : Allies)
	{
		const APawn* Other = A.Pawn.Get();
		if (!Other || Other == Asker) continue;
		if (!A.Target.IsValid()) continue;
		if (A.Activity == EHostileSquadActivity::Engage
			|| A.Activity == EHostileSquadActivity::Suppress
			|| A.Activity == EHostileSquadActivity::Flank)
		{
			return A.Target.Get();
		}
	}
	return nullptr;
}
