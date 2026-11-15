#include "Game/AINoiseEvent.h"

#include "Game/CoopCqbGameMode.h"
#include "Game/HostileAIController.h"
#include "Game/HostageActor.h"
#include "Weapon/AttachmentData.h"
#include "Weapon/Weapon.h"
#include "ShooterGamePlayTags.h"
#include "Perception/AISense_Hearing.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarAIAlertDebug(
	TEXT("fps.AIAlertDebug"),
	0,
	TEXT("Draw hostile alert state text and noise spheres. 0=off, 1=on."),
	ECVF_Cheat);

FAINoiseTuning AINoiseEvent::GetTuning(const UWorld* World)
{
	if (World)
	{
		if (const ACoopCqbGameMode* GM = World->GetAuthGameMode<ACoopCqbGameMode>())
		{
			return GM->NoiseTuning;
		}
	}
	return FAINoiseTuning();
}

float AINoiseEvent::GetRadius(const UWorld* World, EKind Kind)
{
	const FAINoiseTuning T = GetTuning(World);
	switch (Kind)
	{
	case EKind::Gunshot: return T.GunshotRadius;
	case EKind::SuppressedGunshot: return T.SuppressedGunshotRadius;
	case EKind::DoorKick: return T.DoorKickRadius;
	case EKind::DoorPush: return T.DoorPushRadius;
	case EKind::SprintFootstep: return T.SprintFootstepRadius;
	case EKind::WalkFootstep: return T.WalkFootstepRadius;
	case EKind::Footstep: return T.WalkFootstepRadius;
	case EKind::CrouchFootstep: return T.CrouchFootstepRadius;
	case EKind::Explosion: return T.ExplosionRadius;
	case EKind::SurrenderWarning: return T.SurrenderWarningRadius;
	default: return T.WalkFootstepRadius;
	}
}

bool AINoiseEvent::IsLoud(EKind Kind)
{
	switch (Kind)
	{
	case EKind::Gunshot:
	case EKind::SuppressedGunshot:
	case EKind::DoorKick:
	case EKind::Explosion:
		return true;
	default:
		return false;
	}
}

FName AINoiseEvent::TagFor(EKind Kind)
{
	switch (Kind)
	{
	case EKind::Gunshot: return TEXT("Gunshot");
	case EKind::SuppressedGunshot: return TEXT("SuppressedGunshot");
	case EKind::DoorKick: return TEXT("DoorKick");
	case EKind::DoorPush: return TEXT("DoorPush");
	case EKind::SprintFootstep: return TEXT("SprintFootstep");
	case EKind::WalkFootstep: return TEXT("WalkFootstep");
	case EKind::Footstep: return TEXT("Footstep");
	case EKind::CrouchFootstep: return TEXT("CrouchFootstep");
	case EKind::Explosion: return TEXT("Explosion");
	case EKind::SurrenderWarning: return TEXT("SurrenderWarning");
	default: return TEXT("Noise");
	}
}

AINoiseEvent::EKind AINoiseEvent::KindFromTag(FName Tag)
{
	if (Tag == TEXT("Gunshot")) return EKind::Gunshot;
	if (Tag == TEXT("SuppressedGunshot")) return EKind::SuppressedGunshot;
	if (Tag == TEXT("DoorKick")) return EKind::DoorKick;
	if (Tag == TEXT("DoorPush")) return EKind::DoorPush;
	if (Tag == TEXT("SprintFootstep")) return EKind::SprintFootstep;
	if (Tag == TEXT("WalkFootstep")) return EKind::WalkFootstep;
	if (Tag == TEXT("CrouchFootstep")) return EKind::CrouchFootstep;
	if (Tag == TEXT("Explosion")) return EKind::Explosion;
	if (Tag == TEXT("SurrenderWarning")) return EKind::SurrenderWarning;
	return EKind::Footstep;
}

bool AINoiseEvent::IsMuzzleSuppressed(const AWeapon* Weapon)
{
	if (!IsValid(Weapon)) return false;
	const UAttachmentData* Muzzle = Weapon->GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Muzzle);
	if (!IsValid(Muzzle)) return false;

	const FString Name = Muzzle->DisplayName.ToString() + TEXT(" ") + Muzzle->GetName();
	return Name.Contains(TEXT("suppress"), ESearchCase::IgnoreCase)
		|| Name.Contains(TEXT("silencer"), ESearchCase::IgnoreCase)
		|| Name.Contains(TEXT("消音"));
}

bool AINoiseEvent::IsAlertDebugEnabled()
{
	return CVarAIAlertDebug.GetValueOnGameThread() > 0;
}

void AINoiseEvent::Report(UWorld* World, const FVector& Location, float Loudness, AActor* NoiseInstigator, EKind Kind)
{
	if (!World || !World->GetAuthGameMode()) return;
	if (World->GetNetMode() == NM_Client) return;

	const float ClampedLoudness = FMath::Clamp(Loudness, 0.05f, 1.f);
	const float Radius = GetRadius(World, Kind) * ClampedLoudness;
	if (Radius <= 1.f) return;
 if(Kind==EKind::SurrenderWarning)return;
 if(IsLoud(Kind))for(TActorIterator<AHostageActor> It(World);It;++It)
 {
  if(FVector::DistSquared(It->GetActorLocation(),Location)>FMath::Square(Radius))continue;
  FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(CivilianFright),false,NoiseInstigator);Q.AddIgnoredActor(*It);
  // Occluded sounds only frighten a civilian at short range, not across the entire map.
  const bool Blocked=World->LineTraceSingleByChannel(Hit,Location,It->GetActorLocation(),ECC_Visibility,Q);
  if(!Blocked||FVector::DistSquared(It->GetActorLocation(),Location)<FMath::Square(500.f))It->Auth_Frighten(Location);
 }

	UAISense_Hearing::ReportNoiseEvent(World, Location, 1.f, NoiseInstigator, Radius, TagFor(Kind));

	if (IsAlertDebugEnabled())
	{
		const FColor Color = IsLoud(Kind) ? FColor::Red : FColor::Yellow;
		DrawDebugSphere(World, Location, Radius, 16, Color, false, 2.0f, 0, 1.5f);
		DrawDebugString(World, Location + FVector(0.f, 0.f, 40.f),
			TagFor(Kind).ToString(), nullptr, Color, 2.0f, false, 1.1f);
	}

	for (TActorIterator<AHostileAIController> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			It->HearNoise(Location, ClampedLoudness, NoiseInstigator, Kind);
		}
	}
}
