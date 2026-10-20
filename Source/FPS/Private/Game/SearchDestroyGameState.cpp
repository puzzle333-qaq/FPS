#include "Game/SearchDestroyGameState.h"

#include "Game/BombActor.h"
#include "Game/BombSite.h"
#include "Game/ShooterPlayerState.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

void ASearchDestroyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASearchDestroyGameState, RoundIndex);
	DOREPLIFETIME(ASearchDestroyGameState, AttackerTeam);
	DOREPLIFETIME(ASearchDestroyGameState, BombState);
	DOREPLIFETIME(ASearchDestroyGameState, PlantedSite);
	DOREPLIFETIME(ASearchDestroyGameState, RoundPhase);
	DOREPLIFETIME(ASearchDestroyGameState, LastWinReason);
	DOREPLIFETIME(ASearchDestroyGameState, RoundWinningTeam);
	DOREPLIFETIME(ASearchDestroyGameState, Bomb);
	DOREPLIFETIME(ASearchDestroyGameState, FreezeEndServerTime);
	DOREPLIFETIME(ASearchDestroyGameState, BombExplodeServerTime);
	DOREPLIFETIME(ASearchDestroyGameState, ActionStartServerTime);
	DOREPLIFETIME(ASearchDestroyGameState, ActionDuration);
}

float ASearchDestroyGameState::GetRemainingTime() const
{
	const float Now = GetServerWorldTimeSeconds();
	if (BombState == EBombState::Planted || BombState == EBombState::Defusing)
	{
		return FMath::Max(0.f, BombExplodeServerTime - Now);
	}
	if (RoundPhase == EBombRoundPhase::Freeze)
	{
		return FMath::Max(0.f, FreezeEndServerTime - Now);
	}
	if (RoundPhase == EBombRoundPhase::Live)
	{
		return Super::GetRemainingTime();
	}
	if (ShooterMatchState == EShooterMatchState::WaitingToStart)
	{
		return GetWarmupRemaining();
	}
	return 0.f;
}

bool ASearchDestroyGameState::IsMovementLocked() const
{
	if (Super::IsMovementLocked()) return true;
	return RoundPhase == EBombRoundPhase::Freeze || RoundPhase == EBombRoundPhase::PostRound;
}

bool ASearchDestroyGameState::IsActionLocked() const
{
	if (Super::IsActionLocked()) return true;
	return RoundPhase == EBombRoundPhase::Freeze || RoundPhase == EBombRoundPhase::PostRound;
}

float ASearchDestroyGameState::GetActionProgress() const
{
	if (BombState != EBombState::Planting && BombState != EBombState::Defusing) return 0.f;
	if (ActionDuration <= KINDA_SMALL_NUMBER) return 1.f;
	return FMath::Clamp((GetServerWorldTimeSeconds() - ActionStartServerTime) / ActionDuration, 0.f, 1.f);
}

EBombPrompt ASearchDestroyGameState::GetPromptForPawn(const APawn* Pawn) const
{
	if (!IsValid(Pawn) || IsActionLocked() || ShooterMatchState != EShooterMatchState::InProgress)
	{
		return EBombPrompt::None;
	}
	const AShooterPlayerState* PS = Pawn->GetPlayerState<AShooterPlayerState>();
	if (!PS || !PS->HasTeam()) return EBombPrompt::None;

	const bool bAttacker = IsAttackingTeam(PS->GetTeamId());
	if (IsBombDown())
	{
		if (!bAttacker && IsValid(Bomb) && FVector::DistSquared(Pawn->GetActorLocation(), Bomb->GetActorLocation()) <= FMath::Square(140.f))
		{
			return EBombPrompt::Defuse;
		}
		return EBombPrompt::None;
	}

	if (bAttacker && PS->IsCarryingBomb())
	{
		for (TActorIterator<ABombSite> It(GetWorld()); It; ++It)
		{
			if (It->GetSiteId() != EBombSiteId::None && It->ContainsPawn(Pawn))
			{
				return EBombPrompt::Plant;
			}
		}
	}

	if (bAttacker && !PS->IsCarryingBomb() && IsValid(Bomb) && Bomb->IsDropped()
		&& FVector::DistSquared(Pawn->GetActorLocation(), Bomb->GetActorLocation()) <= FMath::Square(120.f))
	{
		return EBombPrompt::Pickup;
	}
	return EBombPrompt::None;
}

void ASearchDestroyGameState::SetRoundIndex(int32 Index)
{
	RoundIndex = Index;
	OnRep_RoundIndex();
	ForceNetUpdate();
}

void ASearchDestroyGameState::SetAttackerTeam(uint8 Team)
{
	AttackerTeam = Team;
	OnRep_AttackerTeam();
	ForceNetUpdate();
}

void ASearchDestroyGameState::SetBombState(EBombState NewState)
{
	if (BombState == NewState) return;
	BombState = NewState;
	OnRep_BombState();
	ForceNetUpdate();
}

void ASearchDestroyGameState::SetPlantedSite(EBombSiteId Site)
{
	PlantedSite = Site;
	ForceNetUpdate();
}

void ASearchDestroyGameState::SetRoundPhase(EBombRoundPhase NewPhase)
{
	if (RoundPhase == NewPhase) return;
	RoundPhase = NewPhase;
	OnRep_RoundPhase();
	ForceNetUpdate();
}

void ASearchDestroyGameState::SetLastWinReason(ERoundWinReason Reason)
{
	LastWinReason = Reason;
	ForceNetUpdate();
}

void ASearchDestroyGameState::SetRoundWinningTeam(uint8 Team)
{
	RoundWinningTeam = Team;
	ForceNetUpdate();
}

void ASearchDestroyGameState::SetBomb(ABombActor* InBomb)
{
	Bomb = InBomb;
	ForceNetUpdate();
}

void ASearchDestroyGameState::SetFreezeEndServerTime(float ServerTime)
{
	FreezeEndServerTime = ServerTime;
	ForceNetUpdate();
}

void ASearchDestroyGameState::SetBombExplodeServerTime(float ServerTime)
{
	BombExplodeServerTime = ServerTime;
	ForceNetUpdate();
}

void ASearchDestroyGameState::SetActionClock(float StartServerTime, float Duration)
{
	ActionStartServerTime = StartServerTime;
	ActionDuration = Duration;
	ForceNetUpdate();
}

void ASearchDestroyGameState::OnRep_RoundIndex()
{
}

void ASearchDestroyGameState::OnRep_AttackerTeam()
{
}

void ASearchDestroyGameState::OnRep_BombState()
{
	OnBombStateChanged.Broadcast(BombState);
}

void ASearchDestroyGameState::OnRep_RoundPhase()
{
	OnRoundPhaseChanged.Broadcast(RoundPhase);
	if (RoundPhase == EBombRoundPhase::PostRound)
	{
		OnRoundEnded.Broadcast(RoundWinningTeam, LastWinReason);
	}
}
