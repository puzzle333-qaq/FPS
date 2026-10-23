// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGameModeBase.h"

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
#include "ShooterGameInstance.h"

AShooterGameModeBase::AShooterGameModeBase()
{
	// 房间 <-> 对局之间无缝切图（PlayerState::CopyProperties 带队伍）
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

// ================= 房间 =================

bool AShooterGameModeBase::HasActiveRoom() const
{
	const UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>();
	return GI && GI->Room.bActive && !GI->RoomLobbyMap.IsNull();
}

bool AShooterGameModeBase::ReturnToRoomLobby()
{
	UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>();
	if (!GI || !GI->Room.bActive || GI->RoomLobbyMap.IsNull()) return false;
	FString URL = GI->RoomLobbyMap.ToSoftObjectPath().GetLongPackageName();
	if (!GI->LobbyGameModeClass.IsNull())
	{
		URL += FString::Printf(TEXT("?game=%s"), *GI->LobbyGameModeClass.ToSoftObjectPath().ToString());
	}
	UE_LOG(LogTemp, Log, TEXT("[Room] ServerTravel back to lobby: %s (room %s)"), *URL, *GI->Room.RoomCode);
	GetWorld()->ServerTravel(URL);
	return true;
}

int32 AShooterGameModeBase::CountActivePlayersOnTeam(uint8 TeamId) const
{
	int32 Count = 0;
	if (GameState)
	{
		for (const APlayerState* PS : GameState->PlayerArray)
		{
			const AShooterPlayerState* SPS = Cast<AShooterPlayerState>(PS);
			if (!IsValid(SPS) || SPS->IsInactive() || SPS->IsOnlyASpectator()) continue;
			if (!IsValid(SPS->GetOwner())) continue; // 控制器已销毁（掉线中）
			if (SPS->GetTeamId() == TeamId) ++Count;
		}
	}
	return Count;
}

bool AShooterGameModeBase::AreExpectedPlayersPresent(int32& OutArrived, int32& OutExpected) const
{
	OutArrived = 0;
	OutExpected = 0;
	const UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>();
	if (!GI || !GI->Room.bActive || GI->Room.ExpectedPlayerKeys.Num() == 0) return true;
	OutExpected = GI->Room.ExpectedPlayerKeys.Num();
	if (GameState)
	{
		for (const FString& Key : GI->Room.ExpectedPlayerKeys)
		{
			for (const APlayerState* PS : GameState->PlayerArray)
			{
				if (IsValid(PS) && !PS->IsInactive() && UShooterGameInstance::GetRoomKey(PS) == Key)
				{
					++OutArrived;
					break;
				}
			}
		}
	}
	return OutArrived >= OutExpected;
}

void AShooterGameModeBase::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	// 下一帧再数（这一帧 PlayerState 还在列表里）
	GetWorldTimerManager().SetTimer(LogoutCheckTimer, this, &ThisClass::CheckTeamsAfterLogout, 0.2f, false);
}

void AShooterGameModeBase::CheckTeamsAfterLogout()
{
	const AShooterGameState* GS = GetGameState<AShooterGameState>();
	if (!GS || GS->GetMatchState() != EShooterMatchState::InProgress || GS->GetNumTeams() < 2) return;
	const int32 A = CountActivePlayersOnTeam(0);
	const int32 B = CountActivePlayersOnTeam(1);
	if (A > 0 && B > 0) return;
	if (A == 0 && B == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Room] Everyone left mid-match"));
		if (!ReturnToRoomLobby())
		{
			HandleTeamForfeit(AShooterPlayerState::NoTeam);
		}
		return;
	}
	const uint8 Winner = A > 0 ? 0 : 1;
	UE_LOG(LogTemp, Warning, TEXT("[Room] Team %d fully disconnected -> team %d wins"), 1 - Winner, Winner);
	HandleTeamForfeit(Winner);
}
