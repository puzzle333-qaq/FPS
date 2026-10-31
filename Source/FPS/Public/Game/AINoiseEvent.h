#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

/** Simple CQB noise broadcast for Hostile AI (gunshots / footsteps). Server/listen-host only. */
namespace AINoiseEvent
{
	enum class EKind : uint8
	{
		Gunshot = 0,
		Footstep = 1,
	};

	/** Loudness 0..1 scales hear distance. Instigator ignored by same-team hostiles. */
	FPS_API void Report(UWorld* World, const FVector& Location, float Loudness, AActor* NoiseInstigator, EKind Kind);
}
