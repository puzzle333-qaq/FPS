#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"

class UWorld;

namespace WeaponObstacle
{
	struct FProbeResult
	{
		float Alpha = 0.f;
		FHitResult Hit;
		FVector Starts[2] = { FVector::ZeroVector, FVector::ZeroVector };
		FVector Ends[2] = { FVector::ZeroVector, FVector::ZeroVector };
	};

	// Stable camera-space probes, independent of the animated/raised muzzle position.
	FProbeResult Probe(const UWorld& World, const FVector& Eye, const FQuat& ViewRotation,
		float WeaponLength, const FCollisionQueryParams& Params);

	// Translation needed to rotate a mesh around its grip rather than its imported root.
	FVector PivotCorrection(const FVector& MeshOrigin, const FVector& Grip, const FQuat& Rotation);
}
