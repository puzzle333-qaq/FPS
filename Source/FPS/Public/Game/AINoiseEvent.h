#pragma once

#include "CoreMinimal.h"
#include "AINoiseEvent.generated.h"

class AActor;
class AWeapon;
class UWorld;

UENUM(BlueprintType)
enum class EAINoiseKind : uint8
{
	Gunshot = 0,
	Footstep = 1,
	DoorKick = 2,
	DoorPush = 3,
	SprintFootstep = 4,
	WalkFootstep = 5,
	CrouchFootstep = 6,
	Explosion = 7,
	SuppressedGunshot = 8,
	SurrenderWarning = 9
};

USTRUCT(BlueprintType)
struct FPS_API FAINoiseTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|AI|Noise", meta = (ClampMin = "0", Units = "cm"))
	float GunshotRadius = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|AI|Noise", meta = (ClampMin = "0", Units = "cm"))
	float SuppressedGunshotRadius = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|AI|Noise", meta = (ClampMin = "0", Units = "cm"))
	float DoorKickRadius = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|AI|Noise", meta = (ClampMin = "0", Units = "cm"))
	float DoorPushRadius = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|AI|Noise", meta = (ClampMin = "0", Units = "cm"))
	float SprintFootstepRadius = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|AI|Noise", meta = (ClampMin = "0", Units = "cm"))
	float WalkFootstepRadius = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|AI|Noise", meta = (ClampMin = "0", Units = "cm"))
	float CrouchFootstepRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|AI|Noise", meta = (ClampMin = "0", Units = "cm"))
	float ExplosionRadius = 3000.f;

	/** Police shout / drop weapons — hostiles only surrender during flash window. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|AI|Noise", meta = (ClampMin = "0", Units = "cm"))
	float SurrenderWarningRadius = 2200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|AI|Noise", meta = (ClampMin = "0", Units = "cm"))
	float AlertBroadcastRadius = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|AI|Alert", meta = (ClampMin = "1"))
	float SearchTimeout = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|AI|Alert", meta = (ClampMin = "0"))
	float RelaxedReactionDelay = 0.8f;
};

/** CQB noise broadcast for Hostile AI. Server/listen-host only. */
namespace AINoiseEvent
{
	using EKind = EAINoiseKind;

	FPS_API FAINoiseTuning GetTuning(const UWorld* World);
	FPS_API float GetRadius(const UWorld* World, EKind Kind);
	FPS_API bool IsLoud(EKind Kind);
	FPS_API FName TagFor(EKind Kind);
	FPS_API EKind KindFromTag(FName Tag);
	FPS_API bool IsMuzzleSuppressed(const AWeapon* Weapon);
	FPS_API bool IsAlertDebugEnabled();

	/** Loudness scales hear distance. Instigator ignored by same-team hostiles. */
	FPS_API void Report(UWorld* World, const FVector& Location, float Loudness, AActor* NoiseInstigator, EKind Kind);
}
