#include "Game/HostileAIController.h"

#include "Game/HostileAICharacter.h"
#include "Game/CoopCqbGameState.h"
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
}

void AHostileAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	NextFireTime = 0.f;
	NextPathTime = 0.f;
	NextCoverSearchTime = 0.f;
	NextPeekTime = 0.f;
	NextFootSenseTime = 0.f;
	LostSightAt = -1.f;
	InvestigateUntil = 0.f;
	UnderFireUntil = 0.f;
	EnteredCoverAt = 0.f;
	bHasLastKnown = false;
	bHasInvestigate = false;
	bHasCover = false;
	bPeeking = false;
	bWantsCoverThisFight = false;
	State = EHostileBrainState::Idle;
	SetActorTickEnabled(true);
}

void AHostileAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) return;
	Think(DeltaSeconds);
}

void AHostileAIController::HearNoise(const FVector& Location, float Loudness, AActor* NoiseInstigator, AINoiseEvent::EKind Kind)
{
	AHostileAICharacter* Me = Cast<AHostileAICharacter>(GetPawn());
	if (!Me || !Me->IsHostileAlive()) return;
	if (NoiseInstigator == Me) return;
	if (Cast<AHostileAICharacter>(NoiseInstigator)) return;

	const float MaxRange = (Kind == AINoiseEvent::EKind::Gunshot)
		? HearGunshotRange * Loudness
		: HearFootstepRange * Loudness;
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

	InvestigatePoint = Location;
	bHasInvestigate = true;
	InvestigateUntil = GetWorld()->GetTimeSeconds() + InvestigateTimeout;
	if (State == EHostileBrainState::Idle || State == EHostileBrainState::Investigate)
	{
		SetState(EHostileBrainState::Investigate);
	}
}

void AHostileAIController::NotifyTookDamage(AActor* DamageInstigator, const FVector& HitFrom)
{
	AHostileAICharacter* Me = Cast<AHostileAICharacter>(GetPawn());
	if (!Me || !Me->IsHostileAlive()) return;

	const float Now = GetWorld()->GetTimeSeconds();
	UnderFireUntil = Now + UnderFireDuration;
	LastKnownEnemy = HitFrom;
	bHasLastKnown = true;
	bWantsCoverThisFight = true;

	FVector Cover;
	if (FindCoverPoint(HitFrom, Cover))
	{
		CoverPoint = Cover;
		bHasCover = true;
		bPeeking = false;
		EnteredCoverAt = Now;
		NextPeekTime = Now + CoverHoldMin;
		SetState(EHostileBrainState::TakeCover);
	}
	else
	{
		InvestigatePoint = HitFrom;
		bHasInvestigate = true;
		InvestigateUntil = Now + InvestigateTimeout;
		SetState(EHostileBrainState::Combat);
	}
}

void AHostileAIController::SetState(EHostileBrainState NewState)
{
	if (State == NewState) return;
	State = NewState;
	if (NewState != EHostileBrainState::TakeCover)
	{
		bPeeking = false;
		if (NewState == EHostileBrainState::Idle)
		{
			SetCrouch(false);
		}
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
		}
		else if (bHasLastKnown)
		{
			if (LostSightAt < 0.f) LostSightAt = Now;
		}
	}

	// Promote to combat when we see someone.
	if (bCanSee && State != EHostileBrainState::TakeCover)
	{
		if (State != EHostileBrainState::Combat)
		{
			bWantsCoverThisFight = FMath::FRand() < CombatPreferCoverChance;
		}
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
			SetState(EHostileBrainState::Idle);
			break;
		}
		MoveToward(InvestigatePoint, DeltaSeconds);
		if (IsNear(InvestigatePoint, 120.f))
		{
			bHasInvestigate = false;
			// Linger briefly looking around then idle.
			SetState(bHasLastKnown ? EHostileBrainState::Combat : EHostileBrainState::Idle);
		}
		break;
	}
	case EHostileBrainState::Combat:
	{
		const bool bWantCover = bUnderFire || bWantsCoverThisFight;
		if (bWantCover && Now >= NextCoverSearchTime)
		{
			NextCoverSearchTime = Now + 1.2f;
			const FVector Threat = bHasTarget ? Target->GetActorLocation() : LastKnownEnemy;
			FVector Cover;
			if (FindCoverPoint(Threat, Cover))
			{
				CoverPoint = Cover;
				bHasCover = true;
				bPeeking = false;
				EnteredCoverAt = Now;
				NextPeekTime = Now + CoverHoldMin;
				SetState(EHostileBrainState::TakeCover);
				break;
			}
		}

		SetCrouch(bUnderFire);

		if (bCanSee && Dist > 280.f)
		{
			MoveToward(Target->GetActorLocation(), DeltaSeconds);
		}
		else if (!bCanSee && bHasLastKnown)
		{
			MoveToward(LastKnownEnemy, DeltaSeconds);
			if (LostSightAt > 0.f && (Now - LostSightAt) > LoseSightGrace)
			{
				InvestigatePoint = LastKnownEnemy;
				bHasInvestigate = true;
				InvestigateUntil = Now + InvestigateTimeout;
				SetState(EHostileBrainState::Investigate);
				break;
			}
		}
		else if (UCharacterMovementComponent* Move = Me->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
		}

		if (bCanSee && Dist <= Me->ShootRange)
		{
			const FVector To = (Target->GetActorLocation() - Me->GetActorLocation()).GetSafeNormal2D();
			if (!To.IsNearlyZero())
			{
				Me->SetActorRotation(To.Rotation());
			}
			TryShoot(Target, Now);
		}
		break;
	}
	case EHostileBrainState::TakeCover:
	{
		SetCrouch(true);
		if (!bHasCover)
		{
			SetState(EHostileBrainState::Combat);
			break;
		}

		const FVector MoveGoal = bPeeking ? PeekPoint : CoverPoint;
		if (!IsNear(MoveGoal, 90.f))
		{
			MoveToward(MoveGoal, DeltaSeconds);
		}
		else if (UCharacterMovementComponent* Move = Me->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
		}

		// Face threat while in cover.
		const FVector ThreatLoc = bHasTarget ? Target->GetActorLocation() : LastKnownEnemy;
		if (bHasLastKnown || bHasTarget)
		{
			const FVector To = (ThreatLoc - Me->GetActorLocation()).GetSafeNormal2D();
			if (!To.IsNearlyZero())
			{
				Me->SetActorRotation(To.Rotation());
			}
		}

		const bool bAtCover = IsNear(CoverPoint, 110.f);
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

		// Leave cover if threat gone and not under fire.
		if (!bUnderFire && !bCanSee && LostSightAt > 0.f && (Now - LostSightAt) > LoseSightGrace
			&& (Now - EnteredCoverAt) > CoverHoldMin)
		{
			bHasCover = false;
			bPeeking = false;
			bWantsCoverThisFight = false;
			if (bHasLastKnown)
			{
				InvestigatePoint = LastKnownEnemy;
				bHasInvestigate = true;
				InvestigateUntil = Now + InvestigateTimeout;
				SetState(EHostileBrainState::Investigate);
			}
			else
			{
				SetState(EHostileBrainState::Idle);
			}
		}
		break;
	}
	default:
		break;
	}
}

void AHostileAIController::TickFootstepSense(float Now)
{
	if (Now < NextFootSenseTime) return;
	NextFootSenseTime = Now + 0.35f;

	AHostileAICharacter* Me = Cast<AHostileAICharacter>(GetPawn());
	if (!Me) return;

	APawn* Best = nullptr;
	float BestDist = HearFootstepRange;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!IsValid(PC)) continue;
		APawn* P = PC->GetPawn();
		if (!IsValid(P)) continue;
		const UStatsComponent* Stats = UStatsComponent::FindStatsComponent(P);
		if (Stats && Stats->DeathState != EDeathState::NotDead) continue;

		const FVector Vel = P->GetVelocity();
		const float Speed2D = Vel.Size2D();
		if (Speed2D < 80.f) continue; // standing / quiet creep ignored

		float Range = HearFootstepRange;
		if (const ACharacter* Ch = Cast<ACharacter>(P))
		{
			if (Ch->bIsCrouched) Range *= 0.45f;
		}
		if (Speed2D > 450.f) Range *= 1.35f; // sprint louder

		const float Dist = FVector::Dist(Me->GetActorLocation(), P->GetActorLocation());
		if (Dist < Range && Dist < BestDist)
		{
			BestDist = Dist;
			Best = P;
		}
	}

	if (!Best) return;
	// Don't spam investigate if we already see them.
	if (HasLineOfSightTo(Best)) return;

	HearNoise(Best->GetActorLocation(), 0.7f, Best, AINoiseEvent::EKind::Footstep);
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

		// Prefer cover that is toward a side relative to threat, not behind the AI away from fight forever.
		const float DistFromSelf = FVector::Dist2D(Origin, Candidate);
		const float DistThreat = FVector::Dist2D(Candidate, ThreatLoc);
		const float Score = 500.f + (CoverSearchRadius - DistFromSelf) + FMath::Min(DistThreat, 2000.f) * 0.05f;
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
	if (!HasLineOfSightTo(Target)) return;

	NextFireTime = Now + Me->FireInterval;

	FVector Start = Me->GetActorLocation() + FVector(0.f, 0.f, Me->bIsCrouched ? 40.f : 60.f);
	FVector End = Target->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(HostileShot), false, Me);
	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query);

	AActor* HitActor = Hit.GetActor();
	if (!IsValid(HitActor)) return;
	if (!HitActor->GetClass()->ImplementsInterface(UPlayerInterface::StaticClass())) return;
	if (Cast<AHostileAICharacter>(HitActor)) return;

	IPlayerInterface::Execute_DoDamage(HitActor, Me->ShotDamage, Me);
}

