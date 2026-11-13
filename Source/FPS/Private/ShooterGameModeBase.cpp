// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGameModeBase.h"
#include "ShooterPlayerController.h"
#include "Online/PartySubsystem.h"

#include "Game/ShooterPlayerState.h"
#include "Game/ShooterGameState.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
AShooterGameModeBase::AShooterGameModeBase()
{
	bUseSeamlessTravel = true;
	PlayerStateClass = AShooterPlayerState::StaticClass();
	GameStateClass = AShooterGameState::StaticClass();
}

bool AShooterGameModeBase::CanDamage(AController* InstigatorController, AController* VictimController) const
{
	const AShooterGameState* GS = GetGameState<AShooterGameState>();
	return !GS || GS->GetMatchState() != EShooterMatchState::PostMatch;
}

void AShooterGameModeBase::NotifyDamage(AController* InstigatorController, AController* VictimController)
{
	if (!IsValid(InstigatorController) || !IsValid(VictimController) || InstigatorController == VictimController) return;
	RecentDamage.FindOrAdd(VictimController).Add(InstigatorController, GetWorld()->GetTimeSeconds());
}

void AShooterGameModeBase::OnPlayerKilled(AController* KillerController, AController* VictimController)
{
	AShooterPlayerState* VictimPS = IsValid(VictimController) ? VictimController->GetPlayerState<AShooterPlayerState>() : nullptr;
	AShooterPlayerState* KillerPS = IsValid(KillerController) ? KillerController->GetPlayerState<AShooterPlayerState>() : nullptr;
	const bool bSuicide = !KillerPS || KillerPS == VictimPS;

	if (VictimPS) VictimPS->AddDeath();
	if (!bSuicide) KillerPS->AddKill();

	// 助攻：AssistWindow 秒内打过受害者、且不是击杀者的人
	if (IsValid(VictimController))
	{
		if (TMap<TWeakObjectPtr<AController>, float>* Attackers = RecentDamage.Find(VictimController))
		{
			const float Now = GetWorld()->GetTimeSeconds();
			for (const TPair<TWeakObjectPtr<AController>, float>& Pair : *Attackers)
			{
				AController* Attacker = Pair.Key.Get();
				if (!IsValid(Attacker) || Attacker == KillerController || Attacker == VictimController) continue;
				if (Now - Pair.Value > AssistWindow) continue;
				if (AShooterPlayerState* AssistPS = Attacker->GetPlayerState<AShooterPlayerState>())
				{
					AssistPS->AddAssist();
				}
			}
		}
		RecentDamage.Remove(VictimController);
	}

	if (AShooterGameState* GS = GetGameState<AShooterGameState>())
	{
		UTexture2D* WeaponIcon = nullptr;
		// 优先用真正的伤害来源（子弹/刀/手雷/炸弹），没有才退回手里拿的枪
		if (bHasPendingDamageIcon)
		{
			WeaponIcon = PendingDamageIcon;
		}
		else if (APawn* KillerPawn = IsValid(KillerController) ? KillerController->GetPawn() : nullptr)
		{
			if (const UCombatComponent* Combat = UCombatComponent::FindCombatComponent(KillerPawn))
			{
				if (IsValid(Combat->CurrentWeapon))
				{
					WeaponIcon = Combat->CurrentWeapon->WeaponIcon;
				}
			}
		}
		const FString KillerName = KillerPS ? KillerPS->GetPlayerName() : FString();
		GS->Multicast_KillFeed(KillerName, KillerPS ? KillerPS->GetTeamId() : AShooterPlayerState::NoTeam,
			VictimPS ? VictimPS->GetPlayerName() : FString(), VictimPS ? VictimPS->GetTeamId() : AShooterPlayerState::NoTeam,
			WeaponIcon);
	}

	if (!bSuicide)
	{
		ScoreKill(KillerPS, VictimPS);
	}

	if (AShooterGameState* AliveGS = GetGameState<AShooterGameState>())
	{
		AliveGS->RefreshTeamAliveCounts();
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
	else if (IsValid(VictimController) && !CanRespawn(VictimController))
	{
		if (AShooterPlayerController* VictimPC = Cast<AShooterPlayerController>(VictimController))
		{
			VictimPC->Auth_StartSpectating();
		}
	}
}

void AShooterGameModeBase::RespawnPlayer(AController* Controller, bool bForce)
{
	if (!IsValid(Controller) || (!bForce && !CanRespawn(Controller))) return;

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
	if (AShooterGameState* GS = GetGameState<AShooterGameState>())
	{
		GS->RefreshTeamAliveCounts();
	}
}

void AShooterGameModeBase::ClearRespawnTimers()
{
	for (TPair<TWeakObjectPtr<AController>, FTimerHandle>& Pair : RespawnTimers)
	{
		GetWorldTimerManager().ClearTimer(Pair.Value);
	}
	RespawnTimers.Reset();
}

void AShooterGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	AssignShortPlayerName(NewPlayer);
	if (AShooterPlayerState* JoinerState = IsValid(NewPlayer) ? NewPlayer->GetPlayerState<AShooterPlayerState>() : nullptr)
	{
		JoinerState->SetPartyHost(NewPlayer->IsLocalController() && GetNetMode() != NM_Standalone);
	}
	if (UPartySubsystem* Party = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPartySubsystem>() : nullptr)
	{
		Party->RegisterJoinedPlayer(NewPlayer);
	}
	if (AShooterGameState* GS = GetGameState<AShooterGameState>())
	{
		GS->RefreshTeamAliveCounts();
	}
}

void AShooterGameModeBase::AssignShortPlayerName(APlayerController* NewPlayer)
{
	APlayerState* PS = IsValid(NewPlayer) ? NewPlayer->PlayerState : nullptr;
	if (!IsValid(PS)) return;

	// 不再覆盖名字：保留 ?Name= / 在线服务给的名字；只有空名字才补 PlayerN
	const FString Current = PS->GetPlayerName();
	if (!Current.TrimStartAndEnd().IsEmpty()) return;

	int32 Number = 1;
	if (const AGameStateBase* GS = GameState)
	{
		for (const APlayerState* Other : GS->PlayerArray)
		{
			if (IsValid(Other) && Other != PS)
			{
				++Number;
			}
		}
	}
	PS->SetPlayerName(FString::Printf(TEXT("Player%d"), Number));
}

void AShooterGameModeBase::Logout(AController* Exiting)
{
	if (UPartySubsystem* Party = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPartySubsystem>() : nullptr)
	{
		Party->UnregisterExitingPlayer(Exiting);
	}
	Super::Logout(Exiting);
	if (AShooterGameState* GS = GetGameState<AShooterGameState>())
	{
		GS->RefreshTeamAliveCounts();
	}
}
