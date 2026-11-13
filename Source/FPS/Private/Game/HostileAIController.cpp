#include "Game/HostileAIController.h"
#include "Combat/HitZoneVitalsComponent.h"
#include "ShooterCharacter.h"
#include "ShooterType/ShooterType.h"
#include "ShooterGamePlayTags.h"


#include "Game/HostileAICharacter.h"
#include "Game/HostileSquadSubsystem.h"
#include "Game/CoopCqbGameMode.h"
#include "Game/CoopCqbGameState.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "DrawDebugHelpers.h"
#include "Interface/PlayerInterface.h"
#include "Stats/StatsComponent.h"
#include "ShooterCharacter.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"

AHostileAIController::AHostileAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = false;

	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	SetPerceptionComponent(*AIPerception);

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 3500.f;
	HearingConfig->LoSHearingRange = 3500.f;
	HearingConfig->bUseLoSHearing = false;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	AIPerception->ConfigureSense(*HearingConfig);

	// TODO(phase 2): vision / light. Sight config is registered but not used for alert yet.
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 0.f;
	SightConfig->LoseSightRadius = 0.f;
	SightConfig->PeripheralVisionAngleDegrees = 90.f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	SightConfig->SetMaxAge(0.f);
	AIPerception->ConfigureSense(*SightConfig);

	AIPerception->SetDominantSense(UAISense_Hearing::StaticClass());
	AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AHostileAIController::OnTargetPerceptionUpdated);
}

void AHostileAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	NextFireTime = 0.f;
	NextPathTime = 0.f;
	NextPeekTime = 0.f;
	NextFootSenseTime = 0.f;
	LostSightAt = -1.f;
	InvestigateUntil = 0.f;
	UnderFireUntil = 0.f;
	EnteredCoverAt = 0.f;
	RelocateUntil = 0.f;
	CoverCooldownUntil = 0.f;
	bHasLastKnown = false;
	bHasInvestigate = false;
	bHasCover = false;
	bPeeking = false;
	bHasRelocate = false;
	bWantFlank = false;
	bHasFlank = false;
	bFlankWaypointReached = false;
	bHasRetreatPoint = false;
	NextFlankPickTime = 0.f;
	FlankCommitUntil = 0.f;
	BurstShotsLeft = 0;
	BurstResumeTime = 0.f;
	State = EHostileBrainState::Idle;
	AlertState = EHostileAlertState::Relaxed;
	InvestigateMoveAt = 0.f;
	SearchUntil = 0.f;
	NextAlertBroadcastTime = 0.f;
	SearchLookTimer = 0.f;
	Role = EHostileSquadRole::Closer;
	HomeLocation = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;
	if (AHostileAICharacter* Hostile = Cast<AHostileAICharacter>(InPawn))
	{
		Role = Hostile->SquadRole;
		Hostile->SetMorale(EHostileMorale::Fighting);
	}
	if (UWorld* World = GetWorld())
	{
		if (UHostileSquadSubsystem* Squad = World->GetSubsystem<UHostileSquadSubsystem>())
		{
			Squad->RegisterAlly(InPawn, Role);
		}
	}
	SetActorTickEnabled(true);
	UE_LOG(LogTemp, Log, TEXT("[Hostile] %s possessed as %s"),
		*GetNameSafe(InPawn),
		*StaticEnum<EHostileSquadRole>()->GetNameStringByValue(static_cast<int64>(Role)));
}

void AHostileAIController::SetSquadRole(EHostileSquadRole NewRole)
{
	Role = NewRole;
	if (AHostileAICharacter* Hostile = Cast<AHostileAICharacter>(GetPawn()))
	{
		Hostile->SquadRole = NewRole;
	}
	if (UWorld* World = GetWorld())
	{
		if (UHostileSquadSubsystem* Squad = World->GetSubsystem<UHostileSquadSubsystem>())
		{
			Squad->RegisterAlly(GetPawn(), Role);
		}
	}
}

void AHostileAIController::OnUnPossess()
{
	if (UWorld* World = GetWorld())
	{
		if (UHostileSquadSubsystem* Squad = World->GetSubsystem<UHostileSquadSubsystem>())
		{
			Squad->UnregisterAlly(GetPawn());
		}
	}
	Super::OnUnPossess();
}

void AHostileAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickAlertDebug();
	if (!HasAuthority()) return;
	Think(DeltaSeconds);
}

void AHostileAIController::HearNoise(const FVector& Location, float Loudness, AActor* NoiseInstigator, AINoiseEvent::EKind Kind)
{
	AHostileAICharacter* Me = Cast<AHostileAICharacter>(GetPawn());
	if (!Me || !Me->IsHostileAlive()) return;
	if (NoiseInstigator == Me) return;
	if (Cast<AHostileAICharacter>(NoiseInstigator)) return;
	if (Me->GetMorale() == EHostileMorale::Surrender || Me->GetMorale() == EHostileMorale::Flee) return;

	if (Kind == AINoiseEvent::EKind::SurrenderWarning)
	{
		const float MaxRange = AINoiseEvent::GetRadius(GetWorld(), Kind) * FMath::Clamp(Loudness, 0.05f, 1.f);
		const float Dist = FVector::Dist(Me->GetActorLocation(), Location);
		if (Dist > MaxRange) return;
		if (!Me->IsInFlashSurrenderWindow()) return;

		Me->SetMorale(EHostileMorale::Surrender);
		SetCrouch(true);
		SetState(EHostileBrainState::Surrender);
		UE_LOG(LogTemp, Log, TEXT("[Hostile] %s heard surrender warning -> Surrender"), *GetNameSafe(Me));
		return;
	}

	const float MaxRange = AINoiseEvent::GetRadius(GetWorld(), Kind) * FMath::Clamp(Loudness, 0.05f, 1.f);
	const float Dist = FVector::Dist(Me->GetActorLocation(), Location);
	if (Dist > MaxRange) return;

	// Already fighting with eyes on someone — don't drop combat for a distant echo.
	if (State == EHostileBrainState::Combat || State == EHostileBrainState::TakeCover)
	{
		if (APawn* Seen = FindBestPlayerTarget())
		{
			if (HasLineOfSightTo(Seen) && Dist > Me->SightRange * 0.35f)
			{
				return;
			}
		}
	}

	const float Now = GetWorld()->GetTimeSeconds();
	InvestigatePoint = Location;
	bHasInvestigate = true;

	if (AINoiseEvent::IsLoud(Kind))
	{
		LastKnownEnemy = Location;
		bHasLastKnown = true;
		SetAlertState(EHostileAlertState::Alert);
		NotifyLevelAlarm();
		BroadcastAlert(Location, NoiseInstigator);
		InvestigateUntil = Now + InvestigateTimeout;
		InvestigateMoveAt = Now;
		if (State == EHostileBrainState::Idle || State == EHostileBrainState::Investigate || State == EHostileBrainState::Search)
		{
			SetState(EHostileBrainState::Combat);
		}
		return;
	}

	if (AlertState == EHostileAlertState::Alert)
	{
		return;
	}

	const FAINoiseTuning Tuning = AINoiseEvent::GetTuning(GetWorld());
	const bool bWasRelaxed = (AlertState == EHostileAlertState::Relaxed);
	SetAlertState(EHostileAlertState::Suspicious);
	InvestigateUntil = Now + InvestigateTimeout;
	InvestigateMoveAt = bWasRelaxed ? (Now + FMath::Max(0.f, Tuning.RelaxedReactionDelay)) : Now;
	if (State == EHostileBrainState::Idle || State == EHostileBrainState::Investigate || State == EHostileBrainState::Search)
	{
		SetState(EHostileBrainState::Investigate);
	}
}

EHostileMorale AHostileAIController::GetMorale() const
{
	const AHostileAICharacter* Me = Cast<AHostileAICharacter>(GetPawn());
	return Me ? Me->GetMorale() : EHostileMorale::Fighting;
}

void AHostileAIController::NotifyTookDamage(AActor* DamageInstigator, const FVector& HitFrom)
{
	AHostileAICharacter* Me = Cast<AHostileAICharacter>(GetPawn());
	if (!Me || !Me->IsHostileAlive()) return;

	LastKnownEnemy = HitFrom;
	bHasLastKnown = true;
	UnderFireUntil = GetWorld()->GetTimeSeconds() + UnderFireDuration;
	SetAlertState(EHostileAlertState::Alert);
	NotifyLevelAlarm();
	NotifyPlayerDetected();
	BroadcastAlert(HitFrom, DamageInstigator);

	if (Me->GetMorale() != EHostileMorale::Fighting) return;

	float SurrenderW = FMath::Max(0.f, SurrenderChance);
	float LastStandW = FMath::Max(0.f, LastStandChance);
	float FleeW = FMath::Max(0.f, FleeChance);
	if (Me->IsInFlashSurrenderWindow())
	{
		SurrenderW = FMath::Max(0.f, FlashSurrenderChance);
		LastStandW = FMath::Max(0.f, FlashLastStandChance);
		FleeW = FMath::Max(0.f, FlashFleeChance);
	}
	const float Sum = SurrenderW + LastStandW + FleeW;
	EHostileMorale Rolled = EHostileMorale::Flee;
	if (Sum > KINDA_SMALL_NUMBER)
	{
		const float Roll = FMath::FRand() * Sum;
		if (Roll < SurrenderW)
		{
			Rolled = EHostileMorale::Surrender;
		}
		else if (Roll < SurrenderW + LastStandW)
		{
			Rolled = EHostileMorale::LastStand;
		}
	}

	Me->SetMorale(Rolled);
	FleeSideSign = FMath::RandBool() ? 1.f : -1.f;
	SetCrouch(Rolled == EHostileMorale::Surrender);
	if (Rolled == EHostileMorale::Surrender)
	{
		SetState(EHostileBrainState::Surrender);
	}
	else if (Rolled == EHostileMorale::Flee)
	{
		SetState(EHostileBrainState::Flee);
	}
	else
	{
		SetState(EHostileBrainState::Combat);
	}

	UE_LOG(LogTemp, Log, TEXT("[Hostile] %s hit -> %s"),
		*GetNameSafe(Me),
		*StaticEnum<EHostileMorale>()->GetNameStringByValue(static_cast<int64>(Rolled)));
}

void AHostileAIController::SetState(EHostileBrainState NewState)
{
	if (State == NewState) return;
	State = NewState;
	if (NewState != EHostileBrainState::TakeCover)
	{
		bPeeking = false;
	}
	if (NewState == EHostileBrainState::Idle || NewState == EHostileBrainState::Combat
		|| NewState == EHostileBrainState::Investigate || NewState == EHostileBrainState::Flee)
	{
		SetCrouch(false);
	}
	if (NewState == EHostileBrainState::Retreat)
	{
		bHasRetreatPoint = false;
	}
}

void AHostileAIController::SetCrouch(bool bCrouch)
{
	ACharacter* Body = Cast<ACharacter>(GetPawn());
	if (!Body) return;
	if (bCrouch)
	{
		Body->Crouch();
	}
	else
	{
		Body->UnCrouch();
	}
}

bool AHostileAIController::IsNear(const FVector& Loc, float DistCm) const
{
	const APawn* Me = GetPawn();
	if (!Me) return false;
	return FVector::DistSquared2D(Me->GetActorLocation(), Loc) <= FMath::Square(DistCm);
}

void AHostileAIController::Think(float DeltaSeconds)
{
	AHostileAICharacter* Me = Cast<AHostileAICharacter>(GetPawn());
	if (!Me || !Me->IsHostileAlive()) return;

	const ACoopCqbGameState* GS = GetWorld() ? GetWorld()->GetGameState<ACoopCqbGameState>() : nullptr;
	if (GS && !GS->IsMissionLive()) return;

	const float Now = GetWorld()->GetTimeSeconds();
	const bool bUnderFire = Now < UnderFireUntil;

	TickFootstepSense(Now);

	APawn* Target = FindBestPlayerTarget();
	const bool bHasTarget = IsValid(Target);
	float Dist = TNumericLimits<float>::Max();
	bool bCanSee = false;

	if (bHasTarget)
	{
		Dist = FVector::Dist(Me->GetActorLocation(), Target->GetActorLocation());
		bCanSee = Dist <= Me->SightRange && HasLineOfSightTo(Target);
		if (bCanSee)
		{
			LastKnownEnemy = Target->GetActorLocation();
			bHasLastKnown = true;
			LostSightAt = -1.f;
			if (Me->GetMorale() == EHostileMorale::Fighting || Me->GetMorale() == EHostileMorale::LastStand)
			{
				SetAlertState(EHostileAlertState::Alert);
				NotifyLevelAlarm();
				NotifyPlayerDetected();
			}
		}
		else if (bHasLastKnown)
		{
			if (LostSightAt < 0.f) LostSightAt = Now;
		}
	}

	RefreshSquadAwareness(Now, bCanSee);

	const EHostileMorale Morale = Me->GetMorale();
	if (Morale == EHostileMorale::Surrender)
	{
		if (State != EHostileBrainState::Surrender)
		{
			SetState(EHostileBrainState::Surrender);
		}
	}
	else if (Morale == EHostileMorale::Flee)
	{
		if (State != EHostileBrainState::Flee)
		{
			SetState(EHostileBrainState::Flee);
		}
	}
	else if (bCanSee && State != EHostileBrainState::TakeCover)
	{
		SetState(EHostileBrainState::Combat);
	}

	switch (State)
	{
	case EHostileBrainState::Idle:
	{
		SetCrouch(false);
		if (UCharacterMovementComponent* Move = Me->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
		}
		if (bHasInvestigate && Now < InvestigateUntil)
		{
			SetState(EHostileBrainState::Investigate);
		}
		break;
	}
	case EHostileBrainState::Investigate:
	{
		SetCrouch(false);
		if (!bHasInvestigate || Now > InvestigateUntil)
		{
			bHasInvestigate = false;
			if (IsLevelAlarmRaised())
			{
				SetAlertState(EHostileAlertState::Suspicious);
			}
			else
			{
				SetAlertState(EHostileAlertState::Relaxed);
			}
			SetState(EHostileBrainState::Idle);
			break;
		}
		if (Now < InvestigateMoveAt)
		{
			FaceLocation(InvestigatePoint);
			break;
		}
		FVector Goal = InvestigatePoint;
		MoveToward(Goal, DeltaSeconds);
		if (IsNear(Goal, 120.f))
		{
			bHasInvestigate = false;
			if (AlertState == EHostileAlertState::Alert || (bHasLastKnown && IsLevelAlarmRaised()))
			{
				EnterSearch(bHasLastKnown ? LastKnownEnemy : InvestigatePoint);
			}
			else
			{
				if (IsLevelAlarmRaised())
				{
					SetAlertState(EHostileAlertState::Suspicious);
				}
				else
				{
					SetAlertState(EHostileAlertState::Relaxed);
				}
				SetState(EHostileBrainState::Idle);
			}
		}
		break;
	}
	case EHostileBrainState::Search:
	{
		TickSearchLook(Now, DeltaSeconds);
		break;
	}
	case EHostileBrainState::Combat:
	{
		SetCrouch(false);

		if (bHasRelocate && Now < RelocateUntil)
		{
			if (!IsNear(RelocatePoint, 110.f))
			{
				MoveToward(RelocatePoint, DeltaSeconds);
			}
			else
			{
				bHasRelocate = false;
			}
			const FVector Face = bHasTarget ? Target->GetActorLocation() : LastKnownEnemy;
			if (bHasLastKnown || bHasTarget)
			{
				FaceLocation(Face);
			}
			if (bCanSee && Dist <= Me->ShootRange)
			{
				TryShoot(Target, Now);
			}
			break;
		}
		bHasRelocate = false;

		if (bCanSee && Dist > 280.f)
		{
			MoveToward(Target->GetActorLocation(), DeltaSeconds);
		}
		else if (!bCanSee && bHasLastKnown)
		{
			MoveToward(LastKnownEnemy, DeltaSeconds);
			if (LostSightAt > 0.f && (Now - LostSightAt) > LoseSightGrace)
			{
				EnterSearch(LastKnownEnemy);
				break;
			}
		}
		else if (UCharacterMovementComponent* Move = Me->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
		}

		if (bCanSee && Dist <= Me->ShootRange)
		{
			FaceLocation(Target->GetActorLocation());
			TryShoot(Target, Now);
		}
		break;
	}
	case EHostileBrainState::TakeCover:
	{
		if (!bHasCover)
		{
			SetState(EHostileBrainState::Combat);
			break;
		}

		const bool bAtCover = IsNear(CoverPoint, 110.f);
		const FVector MoveGoal = bPeeking ? PeekPoint : CoverPoint;
		if (!IsNear(MoveGoal, 90.f))
		{
			SetCrouch(false);
			MoveToward(MoveGoal, DeltaSeconds);
		}
		else if (UCharacterMovementComponent* Move = Me->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
			if (!bPeeking)
			{
				SetCrouch(true);
			}
		}

		// Face threat while in cover.
		const FVector ThreatLoc = bHasTarget ? Target->GetActorLocation() : LastKnownEnemy;
		if (bHasLastKnown || bHasTarget)
		{
			FaceLocation(ThreatLoc);
		}

		if (bAtCover && !bPeeking && Now >= NextPeekTime)
		{
			// Peek: step toward threat a bit so we get LOS, then shoot.
			const FVector Away = (ThreatLoc - CoverPoint).GetSafeNormal2D();
			PeekPoint = CoverPoint + Away * 110.f;
			bPeeking = true;
			NextPeekTime = Now + PeekInterval;
			SetCrouch(false);
		}
		else if (bPeeking && IsNear(PeekPoint, 90.f))
		{
			if (bCanSee && Dist <= Me->ShootRange)
			{
				TryShoot(Target, Now);
			}
			// After a short peek window, duck back.
			if (Now >= NextPeekTime)
			{
				bPeeking = false;
				NextPeekTime = Now + CoverHoldMin;
				SetCrouch(true);
			}
		}
		else if (bAtCover && bCanSee && Dist <= Me->ShootRange)
		{
			// Shoot from crouched cover if we already have a lane.
			TryShoot(Target, Now);
		}

		// A hold is a beat, then they move. Staying crouched while the target is still visible reads as frozen.
		const bool bHeldLongEnough = (Now - EnteredCoverAt) >= CoverHoldMax;
		const bool bThreatGone = !bUnderFire && !bCanSee && LostSightAt > 0.f && (Now - LostSightAt) > LoseSightGrace;
		if ((bHeldLongEnough || bThreatGone) && (Now - EnteredCoverAt) > CoverHoldMin)
		{
			bHasCover = false;
			bPeeking = false;
			CoverCooldownUntil = Now + CoverCooldown;
			SetCrouch(false);
			if (bHeldLongEnough && (bCanSee || bHasLastKnown))
			{
				SetState(EHostileBrainState::Combat);
			}
			else if (Role == EHostileSquadRole::Suppressor || bWantFlank)
			{
				SetState(EHostileBrainState::Combat);
			}
			else if (bHasLastKnown)
			{
				EnterSearch(LastKnownEnemy);
			}
			else if (IsLevelAlarmRaised())
			{
				SetAlertState(EHostileAlertState::Suspicious);
				SetState(EHostileBrainState::Idle);
			}
			else
			{
				SetAlertState(EHostileAlertState::Relaxed);
				SetState(EHostileBrainState::Idle);
			}
		}
		break;
	}
	case EHostileBrainState::Retreat:
	{
		TickRetreat(Me, Target, bCanSee, Dist, Now, DeltaSeconds);
		break;
	}
	case EHostileBrainState::Surrender:
	{
		SetCrouch(true);
		if (UCharacterMovementComponent* Move = Me->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
		}
		break;
	}
	case EHostileBrainState::Flee:
	{
		SetCrouch(false);
		if (!bCanSee)
		{
			if (UCharacterMovementComponent* Move = Me->GetCharacterMovement())
			{
				Move->StopMovementImmediately();
			}
			break;
		}

		const FVector ThreatLoc = bHasTarget ? Target->GetActorLocation() : LastKnownEnemy;
		FVector Away = (Me->GetActorLocation() - ThreatLoc).GetSafeNormal2D();
		if (Away.IsNearlyZero())
		{
			Away = -Me->GetActorForwardVector().GetSafeNormal2D();
		}
		FVector Goal = ProjectToNav(Me->GetActorLocation() + Away * 650.f);
		if (FVector::Dist2D(Me->GetActorLocation(), Goal) < 160.f)
		{
			const FVector Side = Away.RotateAngleAxis(75.f * FleeSideSign, FVector::UpVector);
			const FVector SideGoal = ProjectToNav(Me->GetActorLocation() + Side * 650.f);
			if (FVector::Dist2D(Me->GetActorLocation(), SideGoal) > 160.f)
			{
				Goal = SideGoal;
			}
		}
		if (FVector::Dist2D(Me->GetActorLocation(), Goal) < 120.f)
		{
			if (UCharacterMovementComponent* Move = Me->GetCharacterMovement())
			{
				Move->StopMovementImmediately();
			}
		}
		else
		{
			MoveToward(Goal, DeltaSeconds);
		}
		break;
	}
	default:
		break;
	}

	PublishSquad(bCanSee ? Target : nullptr);
}

void AHostileAIController::TickFootstepSense(float /*Now*/)
{
	// Footsteps are reported from AShooterCharacter (authority), including dedicated servers.
}

void AHostileAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Stimulus.WasSuccessfullySensed()) return;
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		// Report() already iterates controllers; this is a hook for the perception component.
		return;
	}
	// TODO(phase 2): sight / light stimulus.
	(void)Actor;
}

void AHostileAIController::SetAlertState(EHostileAlertState NewState)
{
	if (AlertState == NewState) return;
	if (NewState == EHostileAlertState::Relaxed && IsLevelAlarmRaised())
	{
		NewState = EHostileAlertState::Suspicious;
	}
	if (AlertState == NewState) return;
	AlertState = NewState;
	UE_LOG(LogTemp, Verbose, TEXT("[Hostile] %s alert -> %s"),
		*GetNameSafe(GetPawn()),
		*StaticEnum<EHostileAlertState>()->GetNameStringByValue(static_cast<int64>(AlertState)));
}

void AHostileAIController::EnterSearch(const FVector& Where)
{
	LastKnownEnemy = Where;
	bHasLastKnown = true;
	InvestigatePoint = Where;
	bHasInvestigate = true;
	const FAINoiseTuning Tuning = AINoiseEvent::GetTuning(GetWorld());
	const float Now = GetWorld()->GetTimeSeconds();
	SearchUntil = Now + FMath::Max(1.f, Tuning.SearchTimeout);
	SearchLookTimer = 0.f;
	SetAlertState(EHostileAlertState::Searching);
	SetState(EHostileBrainState::Search);
}

void AHostileAIController::TickSearchLook(float Now, float DeltaSeconds)
{
	AHostileAICharacter* Me = Cast<AHostileAICharacter>(GetPawn());
	if (!Me) return;
	SetCrouch(false);

	if (Now > SearchUntil)
	{
		bHasInvestigate = false;
		SetAlertState(EHostileAlertState::Suspicious);
		SetState(EHostileBrainState::Idle);
		if (UCharacterMovementComponent* Move = Me->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
		}
		return;
	}

	const FVector Goal = bHasLastKnown ? LastKnownEnemy : InvestigatePoint;
	if (!IsNear(Goal, 120.f))
	{
		MoveToward(Goal, DeltaSeconds);
		return;
	}

	if (UCharacterMovementComponent* Move = Me->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}
	SearchLookTimer += DeltaSeconds;
	const FVector To = (Goal - Me->GetActorLocation()).GetSafeNormal2D();
	const FVector Base = To.IsNearlyZero() ? Me->GetActorForwardVector() : To;
	const float Yaw = FMath::Sin(SearchLookTimer * 1.6f) * 70.f;
	FaceLocation(Me->GetActorLocation() + Base.RotateAngleAxis(Yaw, FVector::UpVector) * 200.f);
}

void AHostileAIController::BroadcastAlert(const FVector& Location, AActor* InstigatorActor)
{
	UWorld* World = GetWorld();
	APawn* Me = GetPawn();
	if (!World || !Me) return;
	const float Now = World->GetTimeSeconds();
	if (Now < NextAlertBroadcastTime) return;
	const float Radius = AINoiseEvent::GetTuning(World).AlertBroadcastRadius;
	NextAlertBroadcastTime = Now + 0.75f;
	for (TActorIterator<AHostileAIController> It(World); It; ++It)
	{
		AHostileAIController* Other = *It;
		if (!IsValid(Other) || Other == this) continue;
		const APawn* OtherPawn = Other->GetPawn();
		if (!IsValid(OtherPawn)) continue;
		if (FVector::Dist(Me->GetActorLocation(), OtherPawn->GetActorLocation()) > Radius) continue;
		Other->ReceiveAllyAlert(Location, InstigatorActor);
	}
}

void AHostileAIController::ReceiveAllyAlert(const FVector& Location, AActor* InstigatorActor)
{
	AHostileAICharacter* Me = Cast<AHostileAICharacter>(GetPawn());
	if (!Me || !Me->IsHostileAlive()) return;
	if (Me->GetMorale() == EHostileMorale::Surrender || Me->GetMorale() == EHostileMorale::Flee) return;

	LastKnownEnemy = Location;
	bHasLastKnown = true;
	InvestigatePoint = Location;
	bHasInvestigate = true;
	SetAlertState(EHostileAlertState::Alert);
	NotifyLevelAlarm();
	if (Cast<APawn>(InstigatorActor) && !Cast<AHostileAICharacter>(InstigatorActor))
	{
		// Hearing a teammate's alarm is not itself a confirmed player sighting.
	}
	if (State == EHostileBrainState::Idle || State == EHostileBrainState::Investigate || State == EHostileBrainState::Search)
	{
		SetState(EHostileBrainState::Combat);
	}
}

void AHostileAIController::NotifyLevelAlarm()
{
	if (UWorld* World = GetWorld())
	{
		if (ACoopCqbGameMode* GM = World->GetAuthGameMode<ACoopCqbGameMode>())
		{
			GM->NotifyAlarmRaised();
		}
	}
}

void AHostileAIController::NotifyPlayerDetected()
{
	if (UWorld* World = GetWorld())
	{
		if (ACoopCqbGameMode* GM = World->GetAuthGameMode<ACoopCqbGameMode>())
		{
			GM->NotifyPlayersDetected();
		}
	}
}

bool AHostileAIController::IsLevelAlarmRaised() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const ACoopCqbGameState* GS = World->GetGameState<ACoopCqbGameState>())
		{
			return GS->IsAlarmRaised();
		}
	}
	return false;
}

void AHostileAIController::TickAlertDebug() const
{
	if (!AINoiseEvent::IsAlertDebugEnabled()) return;
	const APawn* Me = GetPawn();
	UWorld* World = GetWorld();
	if (!Me || !World) return;
	const UEnum* AlertEnum = StaticEnum<EHostileAlertState>();
	const UEnum* BrainEnum = StaticEnum<EHostileBrainState>();
	const FString Label = FString::Printf(TEXT("%s / %s"),
		AlertEnum ? *AlertEnum->GetNameStringByValue(static_cast<int64>(AlertState)) : TEXT("?"),
		BrainEnum ? *BrainEnum->GetNameStringByValue(static_cast<int64>(State)) : TEXT("?"));
	DrawDebugString(World, Me->GetActorLocation() + FVector(0.f, 0.f, 110.f), Label, nullptr, FColor::Cyan, 0.f, true, 1.15f);
}

APawn* AHostileAIController::FindBestPlayerTarget() const
{
	const APawn* Me = GetPawn();
	UWorld* World = GetWorld();
	if (!Me || !World) return nullptr;

	APawn* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!IsValid(PC)) continue;
		APawn* TargetPawn = PC->GetPawn();
		if (!IsValid(TargetPawn)) continue;
		const UStatsComponent* Stats = UStatsComponent::FindStatsComponent(TargetPawn);
		if (Stats && Stats->DeathState != EDeathState::NotDead) continue;
		const float DistSq = FVector::DistSquared(Me->GetActorLocation(), TargetPawn->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			Best = TargetPawn;
			BestDistSq = DistSq;
		}
	}
	return Best;
}

bool AHostileAIController::HasLineOfSightTo(APawn* Target) const
{
	const APawn* Me = GetPawn();
	if (!Me || !IsValid(Target)) return false;
	const ACharacter* Ch = Cast<ACharacter>(Me);
	const float EyeZ = (Ch && Ch->bIsCrouched) ? 40.f : 60.f;
	const FVector Eye = Me->GetActorLocation() + FVector(0.f, 0.f, EyeZ);
	const FVector Chest = Target->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	return HasLineOfSightFrom(Eye, Chest);
}

bool AHostileAIController::HasLineOfSightFrom(const FVector& Eye, const FVector& TargetLoc) const
{
	const APawn* Me = GetPawn();
	UWorld* World = GetWorld();
	if (!Me || !World) return false;

	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(HostileSight), false, Me);
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, Eye, TargetLoc, ECC_Visibility, Query);
	return !bBlocked;
}

void AHostileAIController::RefreshSquadAwareness(float /*Now*/, bool bCanSee)
{
	APawn* Me = GetPawn();
	UWorld* World = GetWorld();
	if (!Me || !World || bCanSee) return;
	if (State == EHostileBrainState::Flee || State == EHostileBrainState::Surrender) return;

	UHostileSquadSubsystem* Squad = World->GetSubsystem<UHostileSquadSubsystem>();
	if (!Squad) return;

	if (AActor* Shared = Squad->MostEngagedTarget(Me))
	{
		LastKnownEnemy = Shared->GetActorLocation();
		bHasLastKnown = true;
		if (GetMorale() == EHostileMorale::Fighting || GetMorale() == EHostileMorale::LastStand)
		{
			if (State == EHostileBrainState::Idle || State == EHostileBrainState::Investigate)
			{
				SetState(EHostileBrainState::Combat);
			}
		}
	}
}

void AHostileAIController::PublishSquad(AActor* TargetActor) const
{
	UWorld* World = GetWorld();
	APawn* Me = GetPawn();
	if (!World || !Me) return;
	UHostileSquadSubsystem* Squad = World->GetSubsystem<UHostileSquadSubsystem>();
	if (!Squad) return;

	EHostileSquadActivity Activity = EHostileSquadActivity::Idle;
	switch (State)
	{
	case EHostileBrainState::Investigate:
		Activity = EHostileSquadActivity::Alert;
		break;
	case EHostileBrainState::Retreat:
	case EHostileBrainState::Flee:
		Activity = EHostileSquadActivity::Retreat;
		break;
	case EHostileBrainState::Surrender:
		Activity = EHostileSquadActivity::Idle;
		break;
	case EHostileBrainState::Combat:
	case EHostileBrainState::TakeCover:
		Activity = EHostileSquadActivity::Engage;
		break;
	default:
		break;
	}
	Squad->Publish(Me, Activity, TargetActor);
}

bool AHostileAIController::IsHealthLow() const
{
	const AHostileAICharacter* Me = Cast<AHostileAICharacter>(GetPawn());
	if (!Me || !Me->GetStats()) return false;
	const float MaxHp = Me->GetStats()->MaxHealth;
	if (MaxHp <= KINDA_SMALL_NUMBER) return false;
	return (Me->GetStats()->Health / MaxHp) <= RetreatHealthFraction;
}

void AHostileAIController::FaceLocation(const FVector& Loc)
{
	AHostileAICharacter* Me = Cast<AHostileAICharacter>(GetPawn());
	if (!Me) return;
	const FVector To = (Loc - Me->GetActorLocation()).GetSafeNormal2D();
	if (To.IsNearlyZero()) return;
	const float Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	const FRotator Desired = To.Rotation();
	const float Turn = AimTurnRate * Me->GetArmInjuryTurnScale() * Me->GetStingerLookScale();
	Me->SetActorRotation(FMath::RInterpConstantTo(Me->GetActorRotation(), Desired, Delta, Turn));
}

FVector AHostileAIController::ProjectToNav(const FVector& Point) const
{
	UWorld* World = GetWorld();
	if (!World) return Point;
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		FNavLocation NavLoc;
		if (Nav->ProjectPointToNavigation(Point, NavLoc, FVector(250.f, 250.f, 400.f)))
		{
			return NavLoc.Location;
		}
	}
	return Point;
}

bool AHostileAIController::IsPlayerFacingMe(const APawn* Player) const
{
	const APawn* Me = GetPawn();
	if (!Me || !Player) return false;
	const AController* PC = Player->GetController();
	const FVector AimFlat = (PC ? PC->GetControlRotation().Vector() : Player->GetActorForwardVector()).GetSafeNormal2D();
	const FVector ToMe = (Me->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
	if (AimFlat.IsNearlyZero() || ToMe.IsNearlyZero()) return false;
	const float CosLimit = FMath::Cos(FMath::DegreesToRadians(FacingConeHalfAngle));
	return FVector::DotProduct(AimFlat, ToMe) >= CosLimit;
}

bool AHostileAIController::PickSidestep(const FVector& ThreatLoc, FVector& OutPoint) const
{
	const APawn* Me = GetPawn();
	if (!Me) return false;

	const FVector ToThreat = (ThreatLoc - Me->GetActorLocation()).GetSafeNormal2D();
	if (ToThreat.IsNearlyZero()) return false;
	const FVector Side = FVector::CrossProduct(FVector::UpVector, ToThreat).GetSafeNormal();
	if (Side.IsNearlyZero()) return false;

	bool bFound = false;
	float BestScore = -1.f;
	const float Distances[] = { 280.f, 420.f };
	const float Signs[] = { 1.f, -1.f };
	for (float Sign : Signs)
	{
		for (float Dist : Distances)
		{
			const FVector Candidate = ProjectToNav(Me->GetActorLocation() + Side * Sign * Dist);
			const float Moved = FVector::Dist2D(Me->GetActorLocation(), Candidate);
			if (Moved < 160.f) continue;

			const bool bLos = HasLineOfSightFrom(
				Candidate + FVector(0.f, 0.f, 60.f),
				ThreatLoc + FVector(0.f, 0.f, 60.f));
			const float Score = (bLos ? 500.f : 40.f) + Moved;
			if (Score > BestScore)
			{
				BestScore = Score;
				OutPoint = Candidate;
				bFound = true;
			}
		}
	}
	return bFound;
}

bool AHostileAIController::PickFlankPoint(const FVector& ThreatLoc, FVector& OutPoint, FVector& OutWaypoint)
{
	const APawn* Me = GetPawn();
	if (!Me) return false;

	FVector ToMe = (Me->GetActorLocation() - ThreatLoc).GetSafeNormal2D();
	if (ToMe.IsNearlyZero())
	{
		ToMe = Me->GetActorForwardVector().GetSafeNormal2D();
	}
	if (ToMe.IsNearlyZero()) return false;

	const FVector HomeDir = (HomeLocation - ThreatLoc).GetSafeNormal2D();
	float PreferSide = 1.f;
	if (!HomeDir.IsNearlyZero())
	{
		PreferSide = -FMath::Sign(FVector::CrossProduct(HomeDir, ToMe).Z);
		if (FMath::IsNearlyZero(PreferSide)) PreferSide = 1.f;
	}

	bool bFound = false;
	float BestScore = -1.f;
	float ChosenSide = PreferSide;
	FVector Best = Me->GetActorLocation();
	const float Angles[] = {70.f, 100.f, 130.f};
	const float Radii[] = {FlankRadius * 0.8f, FlankRadius, FlankRadius * 1.35f};

	for (float Angle : Angles)
	{
		for (float Radius : Radii)
		{
			for (float SideSign : {PreferSide, -PreferSide})
			{
				const FVector Dir = ToMe.RotateAngleAxis(Angle * SideSign, FVector::UpVector);
				const FVector Candidate = ProjectToNav(ThreatLoc + Dir * Radius);
				const float DistMe = FVector::Dist2D(Me->GetActorLocation(), Candidate);
				if (DistMe < 180.f) continue;

				const bool bLos = HasLineOfSightFrom(Candidate + FVector(0.f, 0.f, 60.f), ThreatLoc + FVector(0.f, 0.f, 60.f));
				float Score = (bLos ? 900.f : 80.f) + FMath::Min(DistMe, 1500.f) * 0.15f;
				if (SideSign == PreferSide) Score += 250.f;
				if (Score > BestScore)
				{
					BestScore = Score;
					Best = Candidate;
					ChosenSide = SideSign;
					bFound = true;
				}
			}
		}
	}

	if (!bFound) return false;

	OutPoint = Best;
	const FVector ArcDir = ToMe.RotateAngleAxis(FlankApproachAngle * ChosenSide, FVector::UpVector);
	OutWaypoint = ProjectToNav(ThreatLoc + ArcDir * FlankRadius);
	return true;
}

void AHostileAIController::TickSuppress(AHostileAICharacter* Me, APawn* Target, bool bCanSee, float Dist, bool /*bUnderFire*/, float Now, float DeltaSeconds)
{
	if (!Me) return;
	SetCrouch(false);
	const FVector Threat = IsValid(Target) ? Target->GetActorLocation() : LastKnownEnemy;

	if (bHasRelocate && Now < RelocateUntil && !IsNear(RelocatePoint, 90.f))
	{
		MoveToward(RelocatePoint, DeltaSeconds);
	}
	else
	{
		bHasRelocate = false;
		if (!IsNear(HomeLocation, SuppressorHoldRadius))
		{
			MoveToward(HomeLocation, DeltaSeconds);
		}
		else if (UCharacterMovementComponent* Move = Me->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
		}
	}

	if (bHasLastKnown || IsValid(Target))
	{
		FaceLocation(Threat);
	}
	if (bCanSee && Dist <= Me->ShootRange)
	{
		TryShoot(Target, Now);
	}
}

void AHostileAIController::TickFlank(AHostileAICharacter* Me, APawn* Target, bool bCanSee, float Dist, float Now, float DeltaSeconds)
{
	if (!Me) return;
	const bool bHaveThreat = IsValid(Target) || bHasLastKnown;
	if (!bHaveThreat)
	{
		SetState(EHostileBrainState::Idle);
		return;
	}

	const FVector ThreatLoc = IsValid(Target) ? Target->GetActorLocation() : LastKnownEnemy;
	if (Now >= NextFlankPickTime)
	{
		NextFlankPickTime = Now + 1.25f;
		FVector NewPoint = FVector::ZeroVector;
		FVector NewWay = FVector::ZeroVector;
		if (PickFlankPoint(ThreatLoc, NewPoint, NewWay))
		{
			const bool bFar = !bHasFlank || FVector::DistSquared2D(NewPoint, FlankPoint) >= FMath::Square(ReflankThreshold);
			const bool bFacing = IsValid(Target) && IsPlayerFacingMe(Target);
			if (!bHasFlank || (bFar && bFacing))
			{
				FlankPoint = NewPoint;
				FlankWaypoint = NewWay;
				bHasFlank = true;
				bFlankWaypointReached = IsNear(FlankWaypoint, 140.f);
			}
		}
	}

	if (!bHasFlank)
	{
		MoveToward(ThreatLoc, DeltaSeconds);
		if (bCanSee && Dist <= Me->ShootRange)
		{
			FaceLocation(ThreatLoc);
			TryShoot(Target, Now);
		}
		return;
	}

	const FVector Goal = bFlankWaypointReached ? FlankPoint : FlankWaypoint;
	if (!IsNear(Goal, 130.f))
	{
		MoveToward(Goal, DeltaSeconds);
	}
	else
	{
		bFlankWaypointReached = true;
		if (IsNear(FlankPoint, 130.f))
		{
			if (UCharacterMovementComponent* Move = Me->GetCharacterMovement())
			{
				Move->StopMovementImmediately();
			}
		}
		else
		{
			MoveToward(FlankPoint, DeltaSeconds);
		}
	}

	if (IsNear(FlankPoint, 170.f))
	{
		FaceLocation(ThreatLoc);
		if (bCanSee && Dist <= Me->ShootRange)
		{
			TryShoot(Target, Now);
		}
	}
}

void AHostileAIController::TickRetreat(AHostileAICharacter* Me, APawn* Target, bool bCanSee, float Dist, float Now, float DeltaSeconds)
{
	if (!Me) return;

	const FVector ThreatLoc = IsValid(Target)
		? Target->GetActorLocation()
		: (bHasLastKnown ? LastKnownEnemy : Me->GetActorLocation() + Me->GetActorForwardVector() * 200.f);

	if (!bHasRetreatPoint)
	{
		FVector Cover;
		if (FindCoverPoint(ThreatLoc, Cover))
		{
			RetreatPoint = Cover;
		}
		else
		{
			FVector Away = (Me->GetActorLocation() - ThreatLoc).GetSafeNormal2D();
			if (Away.IsNearlyZero())
			{
				Away = -Me->GetActorForwardVector().GetSafeNormal2D();
			}
			RetreatPoint = ProjectToNav(Me->GetActorLocation() + Away * 750.f);
		}
		bHasRetreatPoint = true;
	}

	if (!IsNear(RetreatPoint, 120.f))
	{
		SetCrouch(false);
		MoveToward(RetreatPoint, DeltaSeconds);
	}
	else if (UCharacterMovementComponent* Move = Me->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		SetCrouch(true);
	}

	FaceLocation(ThreatLoc);
	if (bCanSee && Dist <= Me->ShootRange)
	{
		TryShoot(Target, Now);
	}
}

bool AHostileAIController::FindCoverPoint(const FVector& ThreatLoc, FVector& OutCover) const
{
	const APawn* Me = GetPawn();
	UWorld* World = GetWorld();
	if (!Me || !World) return false;

	const FVector Origin = Me->GetActorLocation();
	float BestScore = -1.f;
	FVector Best = Origin;
	bool bFound = false;

	FCollisionQueryParams Query(SCENE_QUERY_STAT(HostileCover), false, Me);

	for (int32 i = 0; i < 12; ++i)
	{
		const float Ang = i * 30.f;
		const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
		const FVector TraceEnd = Origin + Dir * CoverSearchRadius;
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Origin + FVector(0.f, 0.f, 40.f), TraceEnd + FVector(0.f, 0.f, 40.f), ECC_WorldStatic, Query))
		{
			continue;
		}

		FVector Candidate = Hit.ImpactPoint - Dir * 85.f;
		Candidate.Z = Origin.Z;

		// Prefer points that break LOS to the threat at crouch height.
		const FVector Eye = Candidate + FVector(0.f, 0.f, 40.f);
		const FVector ThreatEye = ThreatLoc + FVector(0.f, 0.f, 60.f);
		FHitResult LosHit;
		const bool bBlocked = World->LineTraceSingleByChannel(LosHit, Eye, ThreatEye, ECC_Visibility, Query);
		if (!bBlocked) continue;

		const float DistFromSelf = FVector::Dist2D(Origin, Candidate);
		if (DistFromSelf < 200.f) continue;

		const FVector ToThreat = (ThreatLoc - Origin).GetSafeNormal2D();
		const FVector ToCover = (Candidate - Origin).GetSafeNormal2D();
		float Side = 0.f;
		if (!ToThreat.IsNearlyZero() && !ToCover.IsNearlyZero())
		{
			Side = 1.f - FMath::Abs(FVector::DotProduct(ToThreat, ToCover));
		}
		const float DistThreat = FVector::Dist2D(Candidate, ThreatLoc);
		const float Score = Side * 600.f + DistFromSelf * 0.25f + FMath::Min(DistThreat, 2000.f) * 0.02f;
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
			bFound = true;
		}
	}

	// Optional: project onto navmesh.
	if (bFound)
	{
		if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			FNavLocation NavLoc;
			if (Nav->ProjectPointToNavigation(Best, NavLoc, FVector(200.f, 200.f, 200.f)))
			{
				Best = NavLoc.Location;
			}
		}
		if (FVector::Dist2D(Origin, Best) < 180.f)
		{
			return false;
		}
		OutCover = Best;
	}
	return bFound;
}

void AHostileAIController::MoveToward(const FVector& Goal, float DeltaSeconds)
{
	ACharacter* Body = Cast<ACharacter>(GetPawn());
	if (!Body || DeltaSeconds <= KINDA_SMALL_NUMBER) return;
	UCharacterMovementComponent* Move = Body->GetCharacterMovement();
	if (!Move) return;

	const float Now = GetWorld()->GetTimeSeconds();
	FVector Dest = Goal;
	if (Now >= NextPathTime || FVector::DistSquared2D(Goal, CachedGoal) > FMath::Square(200.f))
	{
		CachedGoal = Goal;
		NextPathTime = Now + 0.55f;
		if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			if (UNavigationPath* Path = Nav->FindPathToLocationSynchronously(GetWorld(), Body->GetActorLocation(), Goal))
			{
				if (Path->PathPoints.Num() >= 2)
				{
					Dest = Path->PathPoints[1];
				}
			}
		}
	}

	FVector Dir = (Dest - Body->GetActorLocation()).GetSafeNormal2D();
	if (Dir.IsNearlyZero()) return;
	Body->AddMovementInput(Dir, 1.f);
}

void AHostileAIController::TryShoot(APawn* Target, float Now)
{
	AHostileAICharacter* Me = Cast<AHostileAICharacter>(GetPawn());
	if (!Me || !IsValid(Target) || Now < NextFireTime) return;
	if (Now < BurstResumeTime) return;
	if (!HasLineOfSightTo(Target)) return;

	if (BurstShotsLeft <= 0)
	{
		const int32 Lo = FMath::Max(1, BurstShotsMin);
		const int32 Hi = FMath::Max(Lo, BurstShotsMax);
		BurstShotsLeft = FMath::RandRange(Lo, Hi);
	}

	NextFireTime = Now + Me->FireInterval;
	--BurstShotsLeft;
	if (BurstShotsLeft <= 0)
	{
		const float PauseLo = FMath::Max(0.f, BurstPauseMin);
		const float PauseHi = FMath::Max(PauseLo, BurstPauseMax);
		BurstResumeTime = Now + FMath::FRandRange(PauseLo, PauseHi);
	}

	const float EyeZ = Me->bIsCrouched ? 40.f : 60.f;
	const FVector Start = Me->GetActorLocation() + FVector(0.f, 0.f, EyeZ);

	float LegW = FMath::Max(0.f, AimLegChance);
	float ChestW = FMath::Max(0.f, AimChestChance);
	float HeadW = FMath::Max(0.f, AimHeadChance);
	const float WeightSum = LegW + ChestW + HeadW;
	if (WeightSum <= KINDA_SMALL_NUMBER)
	{
		LegW = 0.70f;
		ChestW = 0.25f;
		HeadW = 0.05f;
	}
	const float Roll = FMath::FRand() * (LegW + ChestW + HeadW);
	float AimZ = AimLegHeight;
	if (Roll >= LegW && Roll < LegW + ChestW)
	{
		AimZ = AimChestHeight;
	}
	else if (Roll >= LegW + ChestW)
	{
		AimZ = AimHeadHeight;
	}
	if (const ACharacter* TargetBody = Cast<ACharacter>(Target))
	{
		if (TargetBody->bIsCrouched)
		{
			AimZ *= 0.55f;
		}
	}
	const FVector Aim = Target->GetActorLocation() + FVector(0.f, 0.f, AimZ);
	FVector Dir = (Aim - Start).GetSafeNormal();
	if (Dir.IsNearlyZero()) return;
	float Spread = AimSpreadDegrees;
	if (IsValid(Me))
	{
		Spread *= Me->GetArmInjurySpreadScale();
	}
	if (Spread > 0.f)
	{
		Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(Spread));
	}
	const FVector End = Start + Dir * 8000.f;
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(HostileShot), false, Me);
	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query);

	const FVector Impact = Hit.bBlockingHit ? Hit.ImpactPoint : End;
	Me->Multicast_HostileFired(Start, Impact);
	AINoiseEvent::Report(GetWorld(), Start, 1.f, Me, AINoiseEvent::EKind::Gunshot);

	AActor* HitActor = Hit.GetActor();
	if (!IsValid(HitActor)) return;
	if (!HitActor->GetClass()->ImplementsInterface(UPlayerInterface::StaticClass())) return;
	if (Cast<AHostileAICharacter>(HitActor)) return;

	const FName Bone = ResolveDamageBone(Hit);
	const EHitZone Zone = ResolveHitZone(Bone);
	if (HitZoneCombat::IsHitZoneVictim(HitActor))
	{
		HitZoneCombat::ApplyBallisticToActor(HitActor, Me, Zone, false, 1.f, true);
	}
	else
	{
		IPlayerInterface::Execute_DoDamage(HitActor, Me->ShotDamage, Me);
	}
	if (Cast<AShooterCharacter>(HitActor) || Cast<AHostileAICharacter>(HitActor))
	{
		Me->Multicast_FleshHit(Hit.ImpactPoint, Hit.ImpactNormal);
	}
	if (UHitZoneVitalsComponent* OwnerVitals = UHitZoneVitalsComponent::Find(Me))
	{
		OwnerVitals->NoteCombatEvent();
	}
}

