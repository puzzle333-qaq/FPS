#include "Game/CoopCqbGameMode.h"

#include "Game/CoopCqbGameState.h"
#include "Game/HostileAICharacter.h"
#include "Game/HostileSpawner.h"
#include "Game/ShooterPlayerState.h"
#include "ShooterPlayerController.h"
#include "ShooterCharacter.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "Weapon/WeaponFlash.h"
#include "Stats/StatsComponent.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/SoftObjectPath.h"
#include "Loadout/LoadoutTypes.h"
#include "ShooterType/ShooterType.h"

ACoopCqbGameMode::ACoopCqbGameMode()
{
	GameStateClass = ACoopCqbGameState::StaticClass();
	PlayerStateClass = AShooterPlayerState::StaticClass();
	RespawnDelay = 0.f;
	MaxOperators = 4;
	PrimaryObjective = ECoopCqbObjectiveType::ClearEnemies;
	HostileClass = AHostileAICharacter::StaticClass();

	OperatorWeapons = {
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/RAR/BP_FPS_RAR.BP_FPS_RAR_C"))),
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/BP_Weapon_X24.BP_Weapon_X24_C"))),
	};
	OperatorThrowables = {
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Script/FPS.WeaponFlash"))),
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/Throwable/BP_Weapon_Grenade.BP_Weapon_Grenade_C"))),
	};
	OperatorArmorLevel = 1;
}

void ACoopCqbGameMode::InitGameState()
{
	Super::InitGameState();
	if (ACoopCqbGameState* GS = GetGameState<ACoopCqbGameState>())
	{
		GS->InitTeams(1, 0);
		GS->SetMatchRules(EShooterGameMode::CoopCQB);
		GS->SetPrimaryObjective(PrimaryObjective);
		GS->SetCoopPhase(ECoopCqbPhase::Waiting);
		GS->SetMatchState(EShooterMatchState::WaitingToStart);
	}
}

void ACoopCqbGameMode::StartPlay()
{
	Super::StartPlay();
	ResolveLoadout();
	BeginMissionFlow();
}

void ACoopCqbGameMode::ResolveLoadout()
{
	ResolvedWeapons.Reset();
	for (const TSoftClassPtr<AWeapon>& Soft : OperatorWeapons)
	{
		if (UClass* C = Soft.LoadSynchronous())
		{
			ResolvedWeapons.AddUnique(C);
		}
	}
	ResolvedThrowables.Reset();
	for (const TSoftClassPtr<AWeapon>& Soft : OperatorThrowables)
	{
		if (UClass* C = Soft.LoadSynchronous())
		{
			ResolvedThrowables.AddUnique(C);
		}
	}
	bool bHasFlash = false;
	for (TSubclassOf<AWeapon> C : ResolvedThrowables)
	{
		if (C && (C == AWeaponFlash::StaticClass() || C->IsChildOf(AWeaponFlash::StaticClass())))
		{
			bHasFlash = true;
			break;
		}
	}
	if (!bHasFlash)
	{
		ResolvedThrowables.Insert(AWeaponFlash::StaticClass(), 0);
	}
}

void ACoopCqbGameMode::BeginMissionFlow()
{
	if (bFlowStarted) return;
	bFlowStarted = true;
	StartBriefing();
}

void ACoopCqbGameMode::StartBriefing()
{
	if (ACoopCqbGameState* GS = GetGameState<ACoopCqbGameState>())
	{
		GS->SetCoopPhase(ECoopCqbPhase::Briefing);
		GS->SetMatchState(EShooterMatchState::WaitingToStart);
	}
	const float Delay = FMath::Max(0.1f, BriefingDuration);
	GetWorldTimerManager().SetTimer(BriefingTimer, this, &ACoopCqbGameMode::StartMission, Delay, false);
	UE_LOG(LogTemp, Log, TEXT("[CoopCQB] Briefing %.1fs"), Delay);
}

void ACoopCqbGameMode::StartMission()
{
	if (bMissionEnded) return;
	SpawnHostilesFromLevel();
	if (ACoopCqbGameState* GS = GetGameState<ACoopCqbGameState>())
	{
		GS->SetCoopPhase(ECoopCqbPhase::InProgress);
		GS->SetMatchState(EShooterMatchState::InProgress);
		GS->SetMatchStartServerTime(GetWorld()->GetTimeSeconds());
	}
	Auth_RefreshObjectives();
	UE_LOG(LogTemp, Log, TEXT("[CoopCQB] Mission live. Hostiles=%d"), CountLivingHostiles());
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Green,
			FString::Printf(TEXT("Coop CQB: 清敌目标已开始（匪徒 %d）"), CountLivingHostiles()));
	}
}

void ACoopCqbGameMode::SpawnHostilesFromLevel()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority()) return;

	int32 Spawned = 0;
	for (TActorIterator<AHostileSpawner> It(World); It; ++It)
	{
		Spawned += It->Auth_SpawnHostiles(HostileClass);
	}

	if (Spawned == 0 && FallbackHostileCount > 0 && HostileClass)
	{
		AActor* Anchor = FindTaggedStart(OperatorStartTag);
		if (!Anchor) Anchor = FindTaggedStart(OperatorStartTagAlt);
		const FVector Base = Anchor ? Anchor->GetActorLocation() : FVector(0.f, 0.f, 100.f);
		for (int32 i = 0; i < FallbackHostileCount; ++i)
		{
			const FVector Loc = Base + FVector(400.f + i * 120.f, 200.f, 0.f);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			if (AHostileAICharacter* H = World->SpawnActor<AHostileAICharacter>(HostileClass, Loc, FRotator::ZeroRotator, Params))
			{
				++Spawned;
				(void)H;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[CoopCQB] Spawned %d hostiles"), Spawned);
	if (ACoopCqbGameState* GS = GetGameState<ACoopCqbGameState>())
	{
		GS->SetHostileCounts(Spawned, Spawned);
	}
}

void ACoopCqbGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	AssignShortPlayerName(NewPlayer);
	if (IsValid(NewPlayer))
	{
		if (AShooterPlayerState* PS = NewPlayer->GetPlayerState<AShooterPlayerState>())
		{
			PS->SetTeamId(CoopCqbTeams::Operators);
		}
	}
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	ApplyOperatorLookAndKit(NewPlayer);

	if (GEngine && IsValid(NewPlayer) && NewPlayer->IsLocalController())
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Cyan,
			TEXT("Coop CQB PVE：出生即警察外观+配装。目标=清光匪徒。门：瞄上=轻推 / 把手=开 / 底=踹"));
	}
}

void ACoopCqbGameMode::ApplyOperatorLookAndKit(APlayerController* PC)
{
	if (!HasAuthority() || !IsValid(PC)) return;
	AShooterPlayerState* PS = PC->GetPlayerState<AShooterPlayerState>();
	if (PS && !PS->HasTeam())
	{
		PS->SetTeamId(CoopCqbTeams::Operators);
	}

	AShooterPlayerController* SPC = Cast<AShooterPlayerController>(PC);
	if (SPC)
	{
		FLoadoutPreset Loadout;
		Loadout.Name = FText::FromString(TEXT("CoopOperatorMVP"));
		auto AddByKind = [&](EWeaponKind Kind)
		{
			for (TSubclassOf<AWeapon> C : ResolvedWeapons)
			{
				if (C && C->GetDefaultObject<AWeapon>()->WeaponKind == Kind)
				{
					FWeaponPreset P; P.WeaponClass = C; Loadout.Weapons.Add(P); return;
				}
			}
		};
		AddByKind(EWeaponKind::LongGun);
		AddByKind(EWeaponKind::Pistol);
		for (TSubclassOf<AWeapon> T : ResolvedThrowables)
		{
			if (!T) continue;
			FWeaponPreset Throw; Throw.WeaponClass = T; Loadout.Weapons.Add(Throw);
		}
		if (Loadout.Weapons.Num() > 0)
		{
			SPC->Auth_SetSubmittedLoadout(Loadout, false);
		}
	}

	if (AShooterCharacter* Char = Cast<AShooterCharacter>(PC->GetPawn()))
	{
		Char->SetFactionLookLocal(ECharacterLookFaction::Police);
		EBodyArmorType Armor = EBodyArmorType::None;
		if (OperatorArmorLevel == 1) Armor = EBodyArmorType::Light;
		else if (OperatorArmorLevel >= 2) Armor = EBodyArmorType::Heavy;
		Char->SetBodyArmorLocal(Armor);
	}
}

AActor* ACoopCqbGameMode::FindTaggedStart(FName Tag) const
{
	TArray<APlayerStart*> Free, Any;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->PlayerStartTag != Tag && !It->Tags.Contains(Tag)) continue;
		Any.Add(*It);
		bool bOccupied = false;
		for (TActorIterator<APawn> PawnIt(GetWorld()); PawnIt; ++PawnIt)
		{
			if (FVector::DistSquared(PawnIt->GetActorLocation(), It->GetActorLocation()) < FMath::Square(150.f))
			{
				bOccupied = true;
				break;
			}
		}
		if (!bOccupied) Free.Add(*It);
	}
	if (Free.Num() > 0) return Free[FMath::RandRange(0, Free.Num() - 1)];
	if (Any.Num() > 0) return Any[FMath::RandRange(0, Any.Num() - 1)];
	return nullptr;
}

AActor* ACoopCqbGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (AActor* Start = FindTaggedStart(OperatorStartTag)) return Start;
	if (AActor* Start = FindTaggedStart(OperatorStartTagAlt)) return Start;
	return Super::ChoosePlayerStart_Implementation(Player);
}

bool ACoopCqbGameMode::CanDamage(AController* InstigatorController, AController* VictimController) const
{
	if (!Super::CanDamage(InstigatorController, VictimController)) return false;
	const ACoopCqbGameState* GS = GetGameState<ACoopCqbGameState>();
	if (GS && !GS->IsMissionLive()) return false;

	// Hostile AI has no PlayerController / or AIController — allow damage either way when victim is hostile pawn.
	if (IsValid(VictimController))
	{
		if (const AHostileAICharacter* Hostile = Cast<AHostileAICharacter>(VictimController->GetPawn()))
		{
			(void)Hostile;
			return true;
		}
	}

	if (bFriendlyFire || !IsValid(InstigatorController) || !IsValid(VictimController) || InstigatorController == VictimController)
	{
		return true;
	}
	const AShooterPlayerState* A = InstigatorController->GetPlayerState<AShooterPlayerState>();
	const AShooterPlayerState* B = VictimController->GetPlayerState<AShooterPlayerState>();
	if (A && B && A->HasTeam() && A->GetTeamId() == B->GetTeamId())
	{
		return false;
	}
	return true;
}

bool ACoopCqbGameMode::CanRespawn(AController* Controller) const
{
	(void)Controller;
	return false; // one life for now
}

void ACoopCqbGameMode::ScoreKill(AShooterPlayerState* KillerPS, AShooterPlayerState* VictimPS)
{
	(void)VictimPS;
	if (!KillerPS) return;
	if (ACoopCqbGameState* GS = GetGameState<ACoopCqbGameState>())
	{
		if (GS->IsMissionLive())
		{
			GS->AddTeamScore(CoopCqbTeams::Operators, 1);
		}
	}
}

void ACoopCqbGameMode::OnPlayerKilled(AController* KillerController, AController* VictimController)
{
	Super::OnPlayerKilled(KillerController, VictimController);
	if (bMissionEnded) return;
	if (CountLivingOperators() <= 0)
	{
		EndMission(false, ECoopCqbWinReason::TeamWiped);
	}
}

void ACoopCqbGameMode::NotifyHostileKilled(AHostileAICharacter* Hostile)
{
	(void)Hostile;
	if (!HasAuthority() || bMissionEnded) return;
	Auth_RefreshObjectives();
}

int32 ACoopCqbGameMode::CountLivingHostiles() const
{
	int32 Count = 0;
	for (TActorIterator<AHostileAICharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsHostileAlive()) ++Count;
	}
	return Count;
}

int32 ACoopCqbGameMode::CountLivingOperators() const
{
	int32 Count = 0;
	const AGameStateBase* GS = GameState;
	if (!GS) return 0;
	for (APlayerState* PSBase : GS->PlayerArray)
	{
		const AShooterPlayerState* PS = Cast<AShooterPlayerState>(PSBase);
		if (!PS || !PS->HasTeam() || PS->GetTeamId() != CoopCqbTeams::Operators) continue;
		APawn* Pawn = PS->GetPawn();
		if (!IsValid(Pawn)) continue;
		const UStatsComponent* Stats = UStatsComponent::FindStatsComponent(Pawn);
		if (Stats && Stats->DeathState != EDeathState::NotDead) continue;
		++Count;
	}
	return Count;
}

void ACoopCqbGameMode::Auth_RefreshObjectives()
{
	if (!HasAuthority() || bMissionEnded) return;
	ACoopCqbGameState* GS = GetGameState<ACoopCqbGameState>();
	if (!GS || !GS->IsMissionLive()) return;

	const int32 Alive = CountLivingHostiles();
	const int32 Total = FMath::Max(GS->GetHostileTotalCount(), Alive);
	if (GS->GetHostileTotalCount() <= 0)
	{
		GS->SetHostileCounts(Alive, Alive);
	}
	else
	{
		GS->SetHostileCounts(Alive, GS->GetHostileTotalCount());
	}

	if (PrimaryObjective == ECoopCqbObjectiveType::ClearEnemies && Alive <= 0 && GS->GetHostileTotalCount() > 0)
	{
		EndMission(true, ECoopCqbWinReason::ObjectivesMet);
	}
}

void ACoopCqbGameMode::EndMission(bool bOperatorsWon, ECoopCqbWinReason Reason)
{
	if (!HasAuthority() || bMissionEnded) return;
	bMissionEnded = true;
	GetWorldTimerManager().ClearTimer(BriefingTimer);

	if (ACoopCqbGameState* GS = GetGameState<ACoopCqbGameState>())
	{
		GS->SetMissionResult(bOperatorsWon, Reason);
		GS->SetCoopPhase(ECoopCqbPhase::PostMission);
		GS->SetMatchState(EShooterMatchState::PostMatch);
		GS->SetWinningTeam(bOperatorsWon ? CoopCqbTeams::Operators : 255);
	}

	UE_LOG(LogTemp, Log, TEXT("[CoopCQB] Mission end won=%d reason=%d"), bOperatorsWon ? 1 : 0, (int32)Reason);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 12.f, bOperatorsWon ? FColor::Green : FColor::Red,
			bOperatorsWon ? TEXT("任务完成：清敌成功") : TEXT("任务失败"));
	}
}

void ACoopCqbGameMode::Auth_AbortMission()
{
	EndMission(false, ECoopCqbWinReason::Aborted);
}
