#include "Weapon/WeaponObstacle.h"

#include "Engine/World.h"

namespace WeaponObstacle
{
	FProbeResult Probe(const UWorld& World, const FVector& Eye, const FQuat& ViewRotation,
		float WeaponLength, const FCollisionQueryParams& Params)
	{
		FProbeResult Result;
		if (WeaponLength <= 1.f) return Result;
		const FVector Forward = ViewRotation.GetForwardVector();
		const FVector Offsets[] = { FVector::ZeroVector, FVector(0.f, 8.f, -18.f) };
		constexpr float Radius = 4.f;
		constexpr float StartDistance = 8.f; // Entire sphere starts in front of the eye.
		const float Reach = WeaponLength + 8.f;
		float ClosestDistance = Reach;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Offsets); ++Index)
		{
			const FVector Origin = Eye + ViewRotation.RotateVector(Offsets[Index]);
			Result.Starts[Index] = Origin + Forward * StartDistance;
			Result.Ends[Index] = Origin + Forward * Reach;
			// Respect physical Pawn blocking. Object-type queries also pick up non-solid
			// WorldDynamic trigger volumes, which must not cause a high-ready pose.
			FCollisionQueryParams ProbeParams = Params;
			for (int32 Attempt = 0; Attempt < 4; ++Attempt)
			{
				TArray<FHitResult> Hits;
				World.SweepMultiByChannel(Hits, Result.Starts[Index], Result.Ends[Index], FQuat::Identity,
					ECC_Pawn, FCollisionShape::MakeSphere(Radius), ProbeParams);
				bool bRetry = false;
				bool bFoundForwardBlock = false;
				for (const FHitResult& Hit : Hits)
				{
					if (!Hit.bBlockingHit) continue;
					const float ForwardDistance = FVector::DotProduct(Hit.ImpactPoint - Eye, Forward);
					// A rear/parallel contact is not a muzzle obstruction. A blocking
					// sweep stops at its first hit, so retry past that component to
					// retain front-wall detection when standing in a corner.
					if (Hit.bStartPenetrating || ForwardDistance <= Radius
						|| FVector::DotProduct(Hit.ImpactNormal, Forward) >= -0.1f)
					{
						if (Hit.GetComponent())
						{
							ProbeParams.AddIgnoredComponent(Hit.GetComponent());
							bRetry = true;
						}
						continue;
					}
					bFoundForwardBlock = true;
					if (ForwardDistance < ClosestDistance)
					{
						ClosestDistance = ForwardDistance;
						Result.Hit = Hit;
					}
				}
				if (!bRetry || bFoundForwardBlock) break;
			}
		}
		const float Intrusion = FMath::Clamp((Reach - ClosestDistance) / FMath::Max(1.f, WeaponLength * 0.5f), 0.f, 1.f);
		Result.Alpha = FMath::SmoothStep(0.f, 1.f, Intrusion);
		return Result;
	}

	FVector PivotCorrection(const FVector& MeshOrigin, const FVector& Grip, const FQuat& Rotation)
	{
		const FVector ToGrip = Grip - MeshOrigin;
		return ToGrip - Rotation.RotateVector(ToGrip);
	}
}
