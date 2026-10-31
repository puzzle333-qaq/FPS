#include "Game/AINoiseEvent.h"

#include "Game/HostileAIController.h"
#include "EngineUtils.h"

void AINoiseEvent::Report(UWorld* World, const FVector& Location, float Loudness, AActor* NoiseInstigator, EKind Kind)
{
	if (!World || !World->GetAuthGameMode()) return;
	if (World->GetNetMode() == NM_Client) return;
	const float Clamped = FMath::Clamp(Loudness, 0.05f, 1.f);
	for (TActorIterator<AHostileAIController> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			It->HearNoise(Location, Clamped, NoiseInstigator, Kind);
		}
	}
}

