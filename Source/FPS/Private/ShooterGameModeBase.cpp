// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGameModeBase.h"

#include "Game/ShooterPlayerState.h"
#include "Game/ShooterGameState.h"
#include "CombatComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"
#include "Engine/World.h"

AShooterGameModeBase::AShooterGameModeBase()
{
	PlayerStateClass = AShooterPlayerState::StaticClass();
	GameStateClass = AShooterGameState::StaticClass();
}

bool AShooterGameModeBase::CanDamage(AController* InstigatorController, AController* VictimController) const
{
	const AShooterGameState* GS = GetGameState<AShooterGameState>();
	return !GS || GS->GetMatchState() != EShooterMatchState::PostMatch;
}

void AShooterGameModeBase::OnPlayerKilled(AController* KillerController, AController* VictimController)
{
	AShooterPlayerState* VictimPS = IsValid(VictimController) ? VictimController->GetPlayerState<AShooterPlayerState>() : nullptr;
	AShooterPlayerState* KillerPS = IsValid(KillerController) ? KillerController->GetPlayerState<AShooterPlayerState>() : nullptr;
	const bool bSuicide = !KillerPS || KillerPS == VictimPS;

	if (VictimPS) VictimPS->AddDeath();
	if (!bSuicide) KillerPS->AddKill();

	if (AShooterGameState* GS = GetGameState<AShooterGameState>())
	{
		const FString KillerName = KillerPS ? KillerPS->GetPlayerName() : FString();
		GS->Multicast_KillFeed(KillerName, KillerPS ? KillerPS->GetTeamId() : AShooterPlayerState::NoTeam,
			VictimPS ? VictimPS->GetPlayerName() : FString(), VictimPS ? VictimPS->GetTeamId() : AShooterPlayerState::NoTeam);
	}

	if (!bSuicide)
	{
		ScoreKill(KillerPS, VictimPS);
	}

	if (RespawnDelay > 0.f && IsValid(VictimController) && CanRespawn(VictimController))
	{
		FTimerHandle& Handle = RespawnTimers.FindOrAdd(VictimController);
		TWeakObjectPtr<AController> WeakController = VictimController;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakController]()
		{
			if (WeakController.IsValid()) RespawnPlayer(WeakController.Get());
		}), RespawnDelay, false);
		if (VictimPS) VictimPS->SetRespawnServerTime(GetWorld()->GetTimeSeconds() + RespawnDelay);
	}
}

void AShooterGameModeBase::RespawnPlayer(AController* Controller)
{
	if (!IsValid(Controller) || !CanRespawn(Controller)) return;

	if (APawn* OldPawn = Controller->GetPawn())
	{
		// 武器挂在角色身上，先清掉再销毁角色
		if (UCombatComponent* Combat = OldPawn->FindComponentByClass<UCombatComponent>())
		{
			Combat->DestroyInventory();
		}
		Controller->UnPossess();
		OldPawn->Destroy();
	}

	if (AShooterPlayerState* PS = Controller->GetPlayerState<AShooterPlayerState>())
	{
		PS->SetRespawnServerTime(0.f);
	}
	RespawnTimers.Remove(Controller);
	RestartPlayer(Controller);
}

void AShooterGameModeBase::ClearRespawnTimers()
{
	for (TPair<TWeakObjectPtr<AController>, FTimerHandle>& Pair : RespawnTimers)
	{
		GetWorldTimerManager().ClearTimer(Pair.Value);
	}
	RespawnTimers.Reset();
}