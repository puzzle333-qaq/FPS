#include "Game/SearchDestroyGameMode.h"

#include "Game/SearchDestroyGameState.h"
#include "Game/ShooterPlayerState.h"
#include "Game/BombActor.h"
#include "Game/BombSite.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "Weapon/AttachmentData.h"
#include "Loadout/LoadoutTypes.h"
#include "ShooterPlayerController.h"
#include "ShooterGameInstance.h"
#include "Stats/StatsComponent.h"
#include "Interface/PlayerInterface.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "EngineUtils.h"
#include "Game/WeaponPickup.h"
#include "TimerManager.h"
#include "Engine/World.h"


ASearchDestroyGameMode::ASearchDestroyGameMode()
{
	GameStateClass = ASearchDestroyGameState::StaticClass();
	RespawnDelay = 0.f;
	// 回合结束展示时间（头文件默认 5，这里改 8）
	RoundEndDelay = 8.f;
	BombClass = ABombActor::StaticClass();
}

ASearchDestroyGameState* ASearchDestroyGameMode::GetSDState() const
{
	return GetGameState<ASearchDestroyGameState>();
}

void ASearchDestroyGameMode::InitGameState()
{
	Super::InitGameState();
	ApplyMatchRules();
}

void ASearchDestroyGameMode::StartPlay()
{
	Super::StartPlay();
	ApplyMatchRules();
	BeginWaitingForPlayers();
	GetWorldTimerManager().SetTimer(SpectateValidateTimer, this, &ThisClass::ValidateSpectators, 0.5f, true);
}

void ASearchDestroyGameMode::ApplyMatchRules()
{
	ASearchDestroyGameState* GS = GetSDState();
	if (!GS) return;
	GS->SetMatchRules(EShooterGameMode::SearchDestroy);
	GS->SetBuyCatalog(BuyCatalog);
	if (GS->GetNumTeams() < 2)
	{
		GS->InitTeams(2, RoundsToWin);
	}
	if (GS->GetMatchState() == EShooterMatchState::WaitingToStart)
	{
		if (!bMatchClockStarted)
		{
			// 等人阶段：倒计时显示关房剩余时间
			GS->SetRoundPhase(EBombRoundPhase::WaitingForPlayers);
			const float Now = GS->GetServerWorldTimeSeconds();
			GS->SetMatchStartServerTime(WaitForPlayersTimeout > 0.f ? Now + WaitForPlayersTimeout : 0.f);
		}
	}
}

int32 ASearchDestroyGameMode::CountPlayersOnTeam(uint8 TeamId) const
{
	int32 Count = 0;
	if (GameState)
	{
		for (const APlayerState* PS : GameState->PlayerArray)
		{
			const AShooterPlayerState* SPS = Cast<AShooterPlayerState>(PS);
			if (SPS && !SPS->IsInactive() && SPS->GetTeamId() == TeamId) ++Count;
		}
	}
	return Count;
}

void ASearchDestroyGameMode::BeginWaitingForPlayers()
{
	if (bMatchClockStarted) return;
	WaitForPlayersStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(WaitForPlayersTimer, this, &ThisClass::TickWaitingForPlayers, 0.5f, true);
	UE_LOG(LogTemp, Log, TEXT("[SD] Waiting for players (min %d per team, timeout %.0fs)"), MinPlayersPerTeam, WaitForPlayersTimeout);
	TickWaitingForPlayers();
}

void ASearchDestroyGameMode::TickWaitingForPlayers()
{
	ASearchDestroyGameState* GS = GetSDState();
	if (!GS || bMatchClockStarted)
	{
		GetWorldTimerManager().ClearTimer(WaitForPlayersTimer);
		return;
	}
	const int32 A = CountPlayersOnTeam(0);
	const int32 B = CountPlayersOnTeam(1);
	int32 Arrived = 0, Expected = 0;
	const bool bAllHere = AreExpectedPlayersPresent(Arrived, Expected);
	if (bAllHere && A >= MinPlayersPerTeam && B >= MinPlayersPerTeam)
	{
		GetWorldTimerManager().ClearTimer(WaitForPlayersTimer);
		UE_LOG(LogTemp, Log, TEXT("[SD] Teams ready (%d vs %d) -> warmup"), A, B);
		GS->SetRoundPhase(EBombRoundPhase::Warmup);
		const float Now = GS->GetServerWorldTimeSeconds();
		GS->SetMatchStartServerTime(WarmupTime > 0.f ? Now + WarmupTime : Now);
		StartWarmupOrMatch();
		return;
	}
	if (WaitForPlayersTimeout > 0.f && GetWorld()->GetTimeSeconds() - WaitForPlayersStartTime >= WaitForPlayersTimeout)
	{
		GetWorldTimerManager().ClearTimer(WaitForPlayersTimer);
		UE_LOG(LogTemp, Warning, TEXT("[SD] Wait for players timed out (%d vs %d, %d/%d) -> back to room / close"), A, B, Arrived, Expected);
		if (!ReturnToRoomLobby())
		{
			CloseRoomToMainMenu();
		}
	}
}

void ASearchDestroyGameMode::CloseRoomToMainMenu()
{
	UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>();
	const FString MenuPath = GI && !GI->MainMenuMap.IsNull() ? GI->MainMenuMap.ToSoftObjectPath().GetLongPackageName() : FString();
	if (MenuPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SD] CloseRoom: MainMenuMap not set in BP_ShooterGameInstance"));
		return;
	}
	// 先把远端客户端送回主菜单，再让主机自己回去（主机 OpenLevel 会关掉监听服务器）
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (IsValid(PC) && !PC->IsLocalController())
		{
			PC->ClientTravel(MenuPath, TRAVEL_Absolute);
		}
	}
	FTimerHandle Tmp;
	GetWorldTimerManager().SetTimer(Tmp, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (UShooterGameInstance* HostGI = GetGameInstance<UShooterGameInstance>())
		{
			if (GetNetMode() != NM_DedicatedServer) HostGI->GoToMainMenu();
		}
	}), 1.0f, false);
}

void ASearchDestroyGameMode::StartWarmupOrMatch()
{
	if (bMatchClockStarted) return;
	bMatchClockStarted = true;
	if (WarmupTime > 0.f)
	{
		GetWorldTimerManager().SetTimer(WarmupTimer, this, &ThisClass::StartMatch, WarmupTime, false);
	}
	else
	{
		StartMatch();
	}
}

void ASearchDestroyGameMode::StartMatch()
{
	ASearchDestroyGameState* GS = GetSDState();
	if (!GS || GS->GetMatchState() == EShooterMatchState::InProgress || GS->GetMatchState() == EShooterMatchState::PostMatch) return;
	GS->SetMatchState(EShooterMatchState::InProgress);
	GS->SetAttackerTeam(0);
	GetWorldTimerManager().SetTimer(ObjectiveTimer, this, &ThisClass::TickObjective, 0.05f, true);
	StartRound();
	UE_LOG(LogTemp, Log, TEXT("[SD] Match started. RoundsToWin=%d"), RoundsToWin);
}

uint8 ASearchDestroyGameMode::PickTeamForNewPlayer() const
{
	int32 Counts[2] = { 0, 0 };
	if (const AGameStateBase* GS = GameState)
	{
		for (const APlayerState* PS : GS->PlayerArray)
		{
			const AShooterPlayerState* SPS = Cast<AShooterPlayerState>(PS);
			if (SPS && !SPS->IsInactive() && SPS->GetTeamId() < 2) ++Counts[SPS->GetTeamId()];
		}
	}
	// 进人少的那队；满 MaxPlayersPerTeam 就去另一队
	uint8 Team = Counts[1] < Counts[0] ? 1 : 0;
	if (Counts[Team] >= MaxPlayersPerTeam && Counts[1 - Team] < MaxPlayersPerTeam) Team = 1 - Team;
	return Team;
}

bool ASearchDestroyGameMode::ShouldSpawnNewPlayer() const
{
	const ASearchDestroyGameState* GS = GetSDState();
	if (!GS) return true;
	if (GS->GetMatchState() == EShooterMatchState::PostMatch) return false;
	if (GS->GetMatchState() == EShooterMatchState::WaitingToStart) return true;
	const EBombRoundPhase Phase = GS->GetRoundPhase();
	// 购买阶段/准备/等人：直接参战；回合中/回合结束：观战，下回合再出生
	return Phase == EBombRoundPhase::Freeze || Phase == EBombRoundPhase::Warmup || Phase == EBombRoundPhase::WaitingForPlayers;
}

void ASearchDestroyGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	AssignShortPlayerName(NewPlayer);
	if (AShooterPlayerState* PS = IsValid(NewPlayer) ? NewPlayer->GetPlayerState<AShooterPlayerState>() : nullptr)
	{
		// 断线重连：还原队伍和钱
		const FString Key = UShooterGameInstance::GetRoomKey(PS);
		if (const FSDReconnectInfo* Info = Key.IsEmpty() ? nullptr : ReconnectCache.Find(Key))
		{
			if (Info->TeamId < 2) PS->SetTeamId(Info->TeamId);
			if (Info->Money > 0) PS->SetMoney(Info->Money);
			UE_LOG(LogTemp, Log, TEXT("[SD] %s reconnected -> team %d, $%d"), *PS->GetPlayerName(), PS->GetTeamId(), PS->GetMoney());
			ReconnectCache.Remove(Key);
		}
		// 从大厅来的已经带着队伍（CopyProperties）；中途新加入的进人少的队
		if (!PS->HasTeam())
		{
			PS->SetTeamId(PickTeamForNewPlayer());
			UE_LOG(LogTemp, Log, TEXT("[SD] %s -> team %d"), *PS->GetPlayerName(), PS->GetTeamId());
		}
		if (PS->GetMoney() <= 0)
		{
			PS->SetMoney(StartMoney);
		}
	}
	if (ShouldSpawnNewPlayer())
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	}
	else if (IsMidRound())
	{
		UE_LOG(LogTemp, Log, TEXT("[SD] %s joined mid-round -> spectate"), *GetNameSafe(NewPlayer));
		ScheduleSpectate(NewPlayer, 0.5f);
	}
}

AActor* ASearchDestroyGameMode::FindStartWithTag(FName Tag, bool bRequireFree) const
{
	TArray<APlayerStart*> Free, Any;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->PlayerStartTag != Tag) continue;
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
	const TArray<APlayerStart*>& Pool = (bRequireFree && Free.Num() > 0) ? Free : Any;
	if (Pool.Num() == 0) return nullptr;
	return Pool[FMath::RandRange(0, Pool.Num() - 1)];
}

AActor* ASearchDestroyGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const AShooterPlayerState* PS = IsValid(Player) ? Player->GetPlayerState<AShooterPlayerState>() : nullptr;
	const ASearchDestroyGameState* GS = GetSDState();
	if (PS && PS->GetTeamId() < 2 && GS)
	{
		const bool bAttacker = GS->IsAttackingTeam(PS->GetTeamId());
		const FName RoleTag = bAttacker ? AttackerStartTag : DefenderStartTag;
		if (AActor* Start = FindStartWithTag(RoleTag, true)) return Start;
		const FName TeamTag = PS->GetTeamId() == 0 ? TeamAStartTag : TeamBStartTag;
		if (AActor* Start = FindStartWithTag(TeamTag, true)) return Start;
		UE_LOG(LogTemp, Warning, TEXT("[SD] No PlayerStart tagged '%s' or team tag. Using any start."), *RoleTag.ToString());
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

bool ASearchDestroyGameMode::CanDamage(AController* InstigatorController, AController* VictimController) const
{
	if (!Super::CanDamage(InstigatorController, VictimController)) return false;
	const ASearchDestroyGameState* GS = GetSDState();
	if (GS && GS->IsActionLocked()) return false;
	if (bFriendlyFire || !IsValid(InstigatorController) || !IsValid(VictimController) || InstigatorController == VictimController) return true;
	const AShooterPlayerState* A = InstigatorController->GetPlayerState<AShooterPlayerState>();
	const AShooterPlayerState* B = VictimController->GetPlayerState<AShooterPlayerState>();
	return !(A && B && A->HasTeam() && A->GetTeamId() == B->GetTeamId());
}

bool ASearchDestroyGameMode::CanRespawn(AController* Controller) const
{
	(void)Controller;
	return false;
}

void ASearchDestroyGameMode::ScoreKill(AShooterPlayerState* KillerPS, AShooterPlayerState* VictimPS)
{
	(void)VictimPS;
	if (IsValid(KillerPS) && KillReward > 0)
	{
		KillerPS->SetMoney(FMath::Min(MaxMoney, KillerPS->GetMoney() + KillReward));
	}
}

bool ASearchDestroyGameMode::IsPawnAlive(const APawn* Pawn) const
{
	if (!IsValid(Pawn)) return false;
	const UStatsComponent* Stats = UStatsComponent::FindStatsComponent(Pawn);
	return !Stats || Stats->DeathState == EDeathState::NotDead;
}

bool ASearchDestroyGameMode::IsAttacker(const AController* Controller) const
{
	const ASearchDestroyGameState* GS = GetSDState();
	const AShooterPlayerState* PS = IsValid(Controller) ? Controller->GetPlayerState<AShooterPlayerState>() : nullptr;
	return GS && PS && PS->HasTeam() && GS->IsAttackingTeam(PS->GetTeamId());
}

bool ASearchDestroyGameMode::CanPlayerAct(AController* Controller) const
{
	const ASearchDestroyGameState* GS = GetSDState();
	if (!GS || GS->GetMatchState() != EShooterMatchState::InProgress || GS->IsActionLocked() || bRoundLocked) return false;
	if (!IsValid(Controller)) return false;
	const APawn* Pawn = Controller->GetPawn();
	return IsPawnAlive(Pawn);
}

int32 ASearchDestroyGameMode::CountAliveOnTeam(uint8 TeamId) const
{
	int32 Count = 0;
	if (const AGameStateBase* GS = GameState)
	{
		for (const APlayerState* PS : GS->PlayerArray)
		{
			const AShooterPlayerState* SPS = Cast<AShooterPlayerState>(PS);
			if (!SPS || SPS->GetTeamId() != TeamId) continue;
			const AController* C = Cast<AController>(SPS->GetOwner());
			if (C && IsPawnAlive(C->GetPawn())) ++Count;
		}
	}
	return Count;
}

void ASearchDestroyGameMode::OnPlayerKilled(AController* KillerController, AController* VictimController)
{
	// 先掉枪（主枪/手枪），再走通用击杀逻辑
	if (AShooterPlayerController* VictimPC = Cast<AShooterPlayerController>(VictimController))
	{
		VictimPC->Auth_DropWeaponsOnDeath(VictimPC->GetPawn());
	}
	Super::OnPlayerKilled(KillerController, VictimController);
	if (Interactor.Get() == VictimController)
	{
		CancelAction();
	}
	if (APawn* VictimPawn = IsValid(VictimController) ? VictimController->GetPawn() : nullptr)
	{
		if (const AShooterPlayerState* PS = VictimController->GetPlayerState<AShooterPlayerState>())
		{
			if (PS->IsCarryingBomb())
			{
				DropBombFrom(VictimPawn);
			}
		}
	}
	CheckElimination();
	if (IsMidRound()) ScheduleSpectate(VictimController, SpectateDelay);
}

void ASearchDestroyGameMode::CheckElimination()
{
	ASearchDestroyGameState* GS = GetSDState();
	if (!GS || bRoundLocked || GS->GetRoundPhase() != EBombRoundPhase::Live) return;

	const int32 AliveAtk = CountAliveOnTeam(GS->GetAttackerTeam());
	const int32 AliveDef = CountAliveOnTeam(GS->GetDefenderTeam());
	const bool bBombDown = GS->IsBombDown();

	if (AliveDef <= 0)
	{
		FinishRound(GS->GetAttackerTeam(), ERoundWinReason::Elimination);
		return;
	}
	if (AliveAtk <= 0 && !bBombDown)
	{
		FinishRound(GS->GetDefenderTeam(), ERoundWinReason::Elimination);
	}
}

void ASearchDestroyGameMode::StartRound()
{
	ASearchDestroyGameState* GS = GetSDState();
	if (!GS || GS->GetMatchState() != EShooterMatchState::InProgress) return;

	bRoundLocked = false;
	Interactor.Reset();
	PlantingSite.Reset();
	GetWorldTimerManager().ClearTimer(RoundEndTimer);
	ClearRespawnTimers();

	GS->SetRoundIndex(GS->GetRoundIndex() + 1);
	GS->SetLastWinReason(ERoundWinReason::None);
	GS->SetRoundWinningTeam(AShooterPlayerState::NoTeam);
	GS->SetPlantedSite(EBombSiteId::None);
	GS->SetBombExplodeServerTime(0.f);
	GS->SetActionClock(0.f, 0.f);
	GS->SetWinningTeam(AShooterPlayerState::NoTeam);

	if (ABombActor* ExistingBomb = GS->GetBomb(); IsValid(ExistingBomb))
	{
		ExistingBomb->Destroy();
		GS->SetBomb(nullptr);
	}

	// 清掉上回合地上的枪
	for (TActorIterator<AWeaponPickup> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
	RespawnAllPlayers();
	SpawnOrResetBomb();
	GiveBombToRandomAttacker();

	const float Now = GS->GetServerWorldTimeSeconds();
	const float BuyWindow = BuyTime > 0.f ? BuyTime : FreezeTime;
	if (BuyWindow > 0.f)
	{
		GS->SetRoundPhase(EBombRoundPhase::Freeze);
		GS->SetFreezeEndServerTime(Now + BuyWindow);
		GS->SetMatchEndServerTime(0.f);
		GetWorldTimerManager().SetTimer(FreezeTimer, this, &ThisClass::GoLive, BuyWindow, false);
	}
	else
	{
		GoLive();
	}
	UE_LOG(LogTemp, Log, TEXT("[SD] Round %d freeze=%.1fs attackers=%d"), GS->GetRoundIndex(), FreezeTime, GS->GetAttackerTeam());
}

void ASearchDestroyGameMode::GoLive()
{
	ASearchDestroyGameState* GS = GetSDState();
	if (!GS || bRoundLocked || GS->GetMatchState() != EShooterMatchState::InProgress) return;
	GS->SetRoundPhase(EBombRoundPhase::Live);
	GS->SetMatchEndServerTime(RoundTime > 0.f ? GS->GetServerWorldTimeSeconds() + RoundTime : 0.f);
}

void ASearchDestroyGameMode::RespawnAllPlayers()
{
	if (!GetWorld()) return;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (AShooterPlayerController* SPC = Cast<AShooterPlayerController>(PC))
			{
				SPC->bPawnAlive = true;
			}
			RespawnPlayer(PC, true);
		}
	}
}

void ASearchDestroyGameMode::ClearBombCarry()
{
	if (const AGameStateBase* GS = GameState)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (AShooterPlayerState* SPS = Cast<AShooterPlayerState>(PS))
			{
				SPS->SetHasBomb(false);
				if (AController* C = Cast<AController>(SPS->GetOwner()))
				{
					if (UCombatComponent* Combat = UCombatComponent::FindCombatComponent(C->GetPawn()))
					{
						Combat->RemoveBombWeapon();
					}
				}
			}
		}
	}
}

void ASearchDestroyGameMode::SpawnOrResetBomb()
{
	ASearchDestroyGameState* GS = GetSDState();
	if (!GS) return;
	ClearBombCarry();

	ABombActor* Bomb = GS->GetBomb();
	if (!IsValid(Bomb))
	{
		UClass* ClassToSpawn = BombClass ? BombClass.Get() : ABombActor::StaticClass();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Bomb = GetWorld()->SpawnActor<ABombActor>(ClassToSpawn, FTransform::Identity, Params);
		GS->SetBomb(Bomb);
	}
	if (IsValid(Bomb))
	{
		FVector DropLoc = FVector::ZeroVector;
		AActor* Start = FindStartWithTag(AttackerStartTag, false);
		if (!Start) Start = FindStartWithTag(TeamAStartTag, false);
		if (!Start)
		{
			for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
			{
				Start = *It;
				break;
			}
		}
		if (Start)
		{
			DropLoc = Start->GetActorLocation() + FVector(0.f, 0.f, 40.f);
		}
		Bomb->DropAt(DropLoc);
		UE_LOG(LogTemp, Log, TEXT("[SD] Bomb spawned at %s"), *DropLoc.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[SD] Failed to spawn bomb actor."));
	}
	GS->SetBombState(EBombState::Idle);
}

void ASearchDestroyGameMode::GiveBombToRandomAttacker()
{
	ASearchDestroyGameState* GS = GetSDState();
	ABombActor* Bomb = GS ? GS->GetBomb() : nullptr;
	if (!IsValid(Bomb) || !GS) return;

	TArray<APawn*> Attackers;
	for (APlayerState* PS : GS->PlayerArray)
	{
		AShooterPlayerState* SPS = Cast<AShooterPlayerState>(PS);
		if (!SPS || !GS->IsAttackingTeam(SPS->GetTeamId())) continue;
		AController* C = Cast<AController>(SPS->GetOwner());
		APawn* Pawn = C ? C->GetPawn() : nullptr;
		if (IsPawnAlive(Pawn)) Attackers.Add(Pawn);
	}
	if (Attackers.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SD] No alive attacker to receive the bomb. Left at drop point."));
		return;
	}

	APawn* Carrier = Attackers[FMath::RandRange(0, Attackers.Num() - 1)];
	ClearBombCarry();
	if (AShooterPlayerState* PS = Carrier->GetPlayerState<AShooterPlayerState>())
	{
		PS->SetHasBomb(true);
	}
	Bomb->GiveTo(Carrier);
	if (UCombatComponent* Combat = UCombatComponent::FindCombatComponent(Carrier))
	{
		Combat->GrantBombWeapon();
	}
	GS->SetBombState(EBombState::Carried);
	UE_LOG(LogTemp, Log, TEXT("[SD] Bomb given to %s"), *GetNameSafe(Carrier));
}

void ASearchDestroyGameMode::DropBombFrom(APawn* Pawn)
{
	ASearchDestroyGameState* GS = GetSDState();
	ABombActor* Bomb = GS ? GS->GetBomb() : nullptr;
	if (!IsValid(Bomb) || GS->IsBombDown()) return;

	const FVector Loc = IsValid(Pawn) ? Pawn->GetActorLocation() + FVector(0.f, 0.f, 30.f) : Bomb->GetActorLocation();
	ClearBombCarry();
	Bomb->DropAt(Loc);
	GS->SetBombState(EBombState::Idle);
	GS->SetActionClock(0.f, 0.f);
}

void ASearchDestroyGameMode::TryPickupBomb(APawn* Pawn)
{
	ASearchDestroyGameState* GS = GetSDState();
	if (!GS || !IsValid(Pawn) || !IsPawnAlive(Pawn) || GS->IsActionLocked() || GS->IsBombDown()) return;
	ABombActor* Bomb = GS->GetBomb();
	if (!IsValid(Bomb) || !Bomb->IsDropped()) return;

	const AShooterPlayerState* PS = Pawn->GetPlayerState<AShooterPlayerState>();
	if (!PS || !GS->IsAttackingTeam(PS->GetTeamId())) return;

	ClearBombCarry();
	if (AShooterPlayerState* MutablePS = Pawn->GetPlayerState<AShooterPlayerState>())
	{
		MutablePS->SetHasBomb(true);
	}
	Bomb->GiveTo(Pawn);
	if (UCombatComponent* Combat = UCombatComponent::FindCombatComponent(Pawn))
	{
		Combat->GrantBombWeapon();
	}
	GS->SetBombState(EBombState::Carried);
}

void ASearchDestroyGameMode::NotifyBombOverlap(ABombActor* OverlappedBomb, AActor* OtherActor)
{
	ASearchDestroyGameState* GS = GetSDState();
	if (!GS || OverlappedBomb != GS->GetBomb()) return;
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		TryPickupBomb(Pawn);
	}
}

ABombSite* ASearchDestroyGameMode::FindSiteContaining(const APawn* Pawn) const
{
	if (!IsValid(Pawn)) return nullptr;
	for (TActorIterator<ABombSite> It(GetWorld()); It; ++It)
	{
		if (It->GetSiteId() != EBombSiteId::None && It->ContainsPawn(Pawn))
		{
			return *It;
		}
	}
	return nullptr;
}

void ASearchDestroyGameMode::BeginBombInteract(AController* Controller)
{
	ASearchDestroyGameState* GS = GetSDState();
	if (!CanPlayerAct(Controller) || !GS) return;
	APawn* Pawn = Controller->GetPawn();
	if (!IsPawnAlive(Pawn)) return;

	if (GS->GetBombState() == EBombState::Planting || GS->GetBombState() == EBombState::Defusing)
	{
		return;
	}

	if (GS->IsBombDown())
	{
		if (!IsAttacker(Controller))
		{
			StartDefuse(Controller);
		}
		return;
	}

	if (IsAttacker(Controller))
	{
		if (const AShooterPlayerState* PS = Controller->GetPlayerState<AShooterPlayerState>(); PS && PS->IsCarryingBomb())
		{
			if (ABombSite* Site = FindSiteContaining(Pawn))
			{
				StartPlant(Controller, Site);
			}
		}
		else
		{
			TryPickupBomb(Pawn);
		}
	}
}

void ASearchDestroyGameMode::EndBombInteract(AController* Controller)
{
	if (Interactor.Get() == Controller)
	{
		CancelAction();
	}
}

void ASearchDestroyGameMode::StartPlant(AController* Controller, ABombSite* Site)
{
	ASearchDestroyGameState* GS = GetSDState();
	APawn* Pawn = Controller->GetPawn();
	if (!GS || !IsValid(Site) || !IsValid(Pawn)) return;

	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(Controller))
	{
		PC->StopSprinting();
	}

	Interactor = Controller;
	PlantingSite = Site;
	GS->SetBombState(EBombState::Planting);
	GS->SetPlantedSite(Site->GetSiteId());
	GS->SetActionClock(GS->GetServerWorldTimeSeconds(), PlantTime);
}

void ASearchDestroyGameMode::StartDefuse(AController* Controller)
{
	ASearchDestroyGameState* GS = GetSDState();
	ABombActor* Bomb = GS ? GS->GetBomb() : nullptr;
	APawn* Pawn = Controller->GetPawn();
	if (!GS || !IsValid(Bomb) || !IsValid(Pawn)) return;
	if (FVector::DistSquared(Pawn->GetActorLocation(), Bomb->GetActorLocation()) > FMath::Square(DefuseRange)) return;

	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(Controller))
	{
		PC->StopSprinting();
	}

	Interactor = Controller;
	GS->SetBombState(EBombState::Defusing);
	GS->SetActionClock(GS->GetServerWorldTimeSeconds(), DefuseTime);
}

void ASearchDestroyGameMode::CancelAction()
{
	ASearchDestroyGameState* GS = GetSDState();
	if (!GS) return;
	const EBombState State = GS->GetBombState();
	if (State == EBombState::Planting)
	{
		GS->SetBombState(EBombState::Carried);
		GS->SetPlantedSite(EBombSiteId::None);
	}
	else if (State == EBombState::Defusing)
	{
		GS->SetBombState(EBombState::Planted);
	}
	GS->SetActionClock(0.f, 0.f);
	Interactor.Reset();
	PlantingSite.Reset();
}

void ASearchDestroyGameMode::CompletePlant()
{
	ASearchDestroyGameState* GS = GetSDState();
	APawn* Pawn = Interactor.IsValid() ? Interactor->GetPawn() : nullptr;
	ABombSite* Site = PlantingSite.Get();
	ABombActor* Bomb = GS ? GS->GetBomb() : nullptr;
	if (!GS || !IsValid(Bomb) || !IsValid(Site) || !IsValid(Pawn))
	{
		CancelAction();
		return;
	}

	ClearBombCarry();
	const FVector PlantLoc = Pawn->GetActorLocation() + FVector(0.f, 0.f, 10.f);
	Bomb->PlantAt(PlantLoc, Site->GetSiteId());
	GS->SetBombState(EBombState::Planted);
	GS->SetPlantedSite(Site->GetSiteId());
	GS->SetBombExplodeServerTime(GS->GetServerWorldTimeSeconds() + BombTimer);
	GS->SetActionClock(0.f, 0.f);
	GS->SetMatchEndServerTime(0.f);
	Interactor.Reset();
	PlantingSite.Reset();
	UE_LOG(LogTemp, Log, TEXT("[SD] Bomb planted at %s"), *UEnum::GetValueAsString(Site->GetSiteId()));
}

void ASearchDestroyGameMode::CompleteDefuse()
{
	ASearchDestroyGameState* GS = GetSDState();
	ABombActor* Bomb = GS ? GS->GetBomb() : nullptr;
	if (IsValid(Bomb))
	{
		Bomb->Multicast_Defused();
	}
	if (GS)
	{
		GS->SetBombState(EBombState::Defused);
		GS->SetActionClock(0.f, 0.f);
	}
	Interactor.Reset();
	FinishRound(GS ? GS->GetDefenderTeam() : 1, ERoundWinReason::BombDefused);
}

void ASearchDestroyGameMode::ExplodeBomb()
{
	ASearchDestroyGameState* GS = GetSDState();
	if (!GS || bRoundLocked || GS->GetRoundPhase() == EBombRoundPhase::PostRound) return;
	bRoundLocked = true;

	ABombActor* Bomb = GS->GetBomb();
	const FVector Origin = IsValid(Bomb) ? Bomb->GetActorLocation() : FVector::ZeroVector;
	if (IsValid(Bomb))
	{
		Bomb->Multicast_Exploded();
	}
	GS->SetBombState(EBombState::Exploded);

	if (ExplosionRadius > 0.f && ExplosionDamage > 0.f)
	{
		for (TActorIterator<APawn> It(GetWorld()); It; ++It)
		{
			if (!IsPawnAlive(*It)) continue;
			if (FVector::DistSquared(It->GetActorLocation(), Origin) > FMath::Square(ExplosionRadius)) continue;
			if (It->Implements<UPlayerInterface>())
			{
				SetPendingDamageIcon(BombKillIcon);
				IPlayerInterface::Execute_DoDamage(*It, ExplosionDamage, IsValid(Bomb) ? Bomb : static_cast<AActor*>(*It));
				ClearPendingDamageIcon();
			}
		}
	}

	FinishRound(GS->GetAttackerTeam(), ERoundWinReason::BombExploded);
}

void ASearchDestroyGameMode::TickObjective()
{
	ASearchDestroyGameState* GS = GetSDState();
	if (!GS || GS->GetMatchState() != EShooterMatchState::InProgress || bRoundLocked) return;

	APawn* InteractPawn = Interactor.IsValid() ? Interactor->GetPawn() : nullptr;
	const EBombState State = GS->GetBombState();

	if (State == EBombState::Planting)
	{
		ABombSite* Site = PlantingSite.Get();
		const bool bValid = CanPlayerAct(Interactor.Get()) && IsValid(Site) && Site->ContainsPawn(InteractPawn);
		const float Speed = IsValid(InteractPawn) ? InteractPawn->GetVelocity().Size2D() : 0.f;
		if (!bValid || Speed > PlantCancelSpeed)
		{
			CancelAction();
		}
		else if (GS->GetActionProgress() >= 1.f)
		{
			CompletePlant();
		}
		return;
	}

	if (State == EBombState::Defusing)
	{
		ABombActor* Bomb = GS->GetBomb();
		const bool bInRange = IsValid(InteractPawn) && IsValid(Bomb)
			&& FVector::DistSquared(InteractPawn->GetActorLocation(), Bomb->GetActorLocation()) <= FMath::Square(DefuseRange);
		const float Speed = IsValid(InteractPawn) ? InteractPawn->GetVelocity().Size2D() : 0.f;
		if (!CanPlayerAct(Interactor.Get()) || !bInRange || Speed > PlantCancelSpeed)
		{
			CancelAction();
		}
		else if (GS->GetActionProgress() >= 1.f)
		{
			CompleteDefuse();
		}
	}

	if (GS->IsBombDown() && GS->GetRemainingTime() <= 0.f)
	{
		ExplodeBomb();
		return;
	}

	if (GS->GetRoundPhase() == EBombRoundPhase::Live && !GS->IsBombDown() && GS->HasTimeLimit() && GS->GetRemainingTime() <= 0.f)
	{
		FinishRound(GS->GetDefenderTeam(), ERoundWinReason::TimeExpired);
	}
}

void ASearchDestroyGameMode::FinishRound(uint8 WinningTeam, ERoundWinReason Reason)
{
	ASearchDestroyGameState* GS = GetSDState();
	if (!GS || GS->GetMatchState() != EShooterMatchState::InProgress) return;
	if (GS->GetRoundPhase() == EBombRoundPhase::PostRound) return;

	bRoundLocked = true;
	Interactor.Reset();
	PlantingSite.Reset();
	GetWorldTimerManager().ClearTimer(FreezeTimer);

	GS->SetLastWinReason(Reason);
	GS->SetRoundWinningTeam(WinningTeam);
	if (WinningTeam < 2)
	{
		GS->AddTeamScore(WinningTeam, 1);
	}
	// 回合赏金
	for (APlayerState* BasePS : GS->PlayerArray)
	{
		AShooterPlayerState* SPS = Cast<AShooterPlayerState>(BasePS);
		if (!IsValid(SPS) || !SPS->HasTeam()) continue;
		const int32 Reward = (WinningTeam < 2 && SPS->GetTeamId() == WinningTeam) ? RoundWinReward : RoundLossReward;
		if (Reward > 0)
		{
			SPS->SetMoney(FMath::Min(MaxMoney, SPS->GetMoney() + Reward));
		}
	}
	GS->SetRoundPhase(EBombRoundPhase::PostRound);
	UE_LOG(LogTemp, Log, TEXT("[SD] Round %d over. Winner=%d Reason=%s (%d : %d)"),
		GS->GetRoundIndex(), WinningTeam, *UEnum::GetValueAsString(Reason), GS->GetTeamScore(0), GS->GetTeamScore(1));

	if (WinningTeam < 2 && GS->GetTeamScore(WinningTeam) >= RoundsToWin)
	{
		EndMatch(WinningTeam);
		return;
	}

	GetWorldTimerManager().SetTimer(RoundEndTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ASearchDestroyGameState* RoundGS = GetSDState();
		if (!RoundGS || RoundGS->GetMatchState() != EShooterMatchState::InProgress) return;
		if (RoundGS->GetRoundIndex() == RoundsToWin)
		{
			SwitchSides();
		}
		StartRound();
	}), FMath::Max(RoundEndDelay, 0.1f), false);
}

void ASearchDestroyGameMode::SwitchSides()
{
	ASearchDestroyGameState* GS = GetSDState();
	if (!GS) return;
	GS->SetAttackerTeam(GS->GetDefenderTeam());
	UE_LOG(LogTemp, Log, TEXT("[SD] Sides switched. Attackers now team %d"), GS->GetAttackerTeam());
}

void ASearchDestroyGameMode::EndMatch(uint8 WinningTeam)
{
	ASearchDestroyGameState* GS = GetSDState();
	if (!GS || GS->GetMatchState() == EShooterMatchState::PostMatch) return;

	GetWorldTimerManager().ClearTimer(ObjectiveTimer);
	GetWorldTimerManager().ClearTimer(FreezeTimer);
	GetWorldTimerManager().ClearTimer(RoundEndTimer);
	ClearRespawnTimers();
	GS->SetWinningTeam(WinningTeam);
	GS->SetMatchState(EShooterMatchState::PostMatch);
	GS->SetRoundPhase(EBombRoundPhase::PostRound);
	UE_LOG(LogTemp, Log, TEXT("[SD] Match ended. Winner=%d (%d : %d)"), WinningTeam, GS->GetTeamScore(0), GS->GetTeamScore(1));

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(It->Get()))
		{
			PC->Client_OnMatchEnded(WinningTeam);
		}
	}
	GetWorldTimerManager().SetTimer(PostMatchTimer, this, &ThisClass::ReturnAllToMainMenu, FMath::Max(PostMatchDelay, 0.1f), false);
}

void ASearchDestroyGameMode::HandleTeamForfeit(uint8 WinningTeam)
{
	// 爆破：一队全掉线 = 整场判负
	EndMatch(WinningTeam);
}

void ASearchDestroyGameMode::ReturnAllToMainMenu()
{
	if (ReturnToRoomLobby()) return;
	APlayerController* HostPC = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!IsValid(PC)) continue;
		if (PC->IsLocalController())
		{
			HostPC = PC;
			continue;
		}
		if (AShooterPlayerController* SPC = Cast<AShooterPlayerController>(PC))
		{
			SPC->Client_GoToMainMenu();
		}
	}

	FTimerHandle HostTimer;
	GetWorldTimerManager().SetTimer(HostTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance()))
		{
			GI->GoToMainMenu();
		}
	}), HostPC ? 1.0f : 0.1f, false);
}


bool ASearchDestroyGameMode::GetBuyCatalogItem(int32 Index, FBuyShopItem& OutItem) const
{
	if (!BuyCatalog.IsValidIndex(Index)) return false;
	OutItem = BuyCatalog[Index];
	return true;
}

bool ASearchDestroyGameMode::Auth_TryBuyItem(AShooterPlayerController* Buyer, int32 CatalogIndex)
{
	UE_LOG(LogTemp, Log, TEXT("[Shop] Buy request index=%d by %s"), CatalogIndex, *GetNameSafe(Buyer));
	if (!HasAuthority() || !IsValid(Buyer)) return false;
	ASearchDestroyGameState* GS = GetSDState();
	if (!GS || !GS->IsBuyPhase()) { UE_LOG(LogTemp, Warning, TEXT("[Shop] Buy %d failed: not buy phase"), CatalogIndex); return false; }
	if (!Buyer->bPawnAlive) { UE_LOG(LogTemp, Warning, TEXT("[Shop] Buy %d failed: dead"), CatalogIndex); return false; }
	APawn* Pawn = Buyer->GetPawn();
	if (!IsPawnAlive(Pawn)) { UE_LOG(LogTemp, Warning, TEXT("[Shop] Buy %d failed: pawn not alive"), CatalogIndex); return false; }

	FBuyShopItem Item;
	if (!GetBuyCatalogItem(CatalogIndex, Item) || !Item.WeaponClass) { UE_LOG(LogTemp, Warning, TEXT("[Shop] Buy %d failed: bad index or no WeaponClass (catalog=%d)"), CatalogIndex, BuyCatalog.Num()); return false; }

	AShooterPlayerState* PS = Buyer->GetPlayerState<AShooterPlayerState>();
	if (!IsValid(PS) || PS->GetMoney() < Item.Price) { UE_LOG(LogTemp, Warning, TEXT("[Shop] Buy %d failed: money %d < price %d"), CatalogIndex, PS ? PS->GetMoney() : -1, Item.Price); return false; }

	const AWeapon* CDO = Item.WeaponClass->GetDefaultObject<AWeapon>();
	if (!CDO || CDO->WeaponKind == EWeaponKind::Bomb) return false;

	FWeaponPreset Preset;
	Preset.WeaponClass = Item.WeaponClass;
	if (Item.DefaultAttachments.Num() > 0)
	{
		for (UAttachmentData* Data : Item.DefaultAttachments)
		{
			if (IsValid(Data) && CDO->CanEquipAttachment(Data))
			{
				Preset.Attachments.Add(Data);
			}
		}
	}
	else if (UShooterGameInstance* GI = Cast<UShooterGameInstance>(Buyer->GetGameInstance()))
	{
		FWeaponPreset Saved;
		if (GI->FindWeaponPreset(Item.WeaponClass, Saved))
		{
			for (UAttachmentData* Data : Saved.Attachments)
			{
				if (IsValid(Data) && CDO->CanEquipAttachment(Data))
				{
					Preset.Attachments.Add(Data);
				}
			}
		}
	}

	if (!Buyer->Auth_ApplyBoughtWeapon(Preset)) { UE_LOG(LogTemp, Warning, TEXT("[Shop] Buy %d failed: apply weapon"), CatalogIndex); return false; }

	PS->SetMoney(FMath::Max(0, PS->GetMoney() - Item.Price));
	UE_LOG(LogTemp, Log, TEXT("[Buy] %s bought %s for %d (left %d)"),
		*PS->GetPlayerName(), *GetNameSafe(Item.WeaponClass), Item.Price, PS->GetMoney());
	return true;
}

// ================= 观战 / 中途加入 / 重连 =================

bool ASearchDestroyGameMode::IsMidRound() const
{
	const ASearchDestroyGameState* GS = GetSDState();
	if (!GS || GS->GetMatchState() != EShooterMatchState::InProgress) return false;
	const EBombRoundPhase Phase = GS->GetRoundPhase();
	return Phase == EBombRoundPhase::Live || Phase == EBombRoundPhase::PostRound;
}

void ASearchDestroyGameMode::ScheduleSpectate(AController* Controller, float Delay)
{
	TWeakObjectPtr<AShooterPlayerController> WeakPC = Cast<AShooterPlayerController>(Controller);
	if (!WeakPC.IsValid()) return;
	FTimerHandle Tmp;
	GetWorldTimerManager().SetTimer(Tmp, FTimerDelegate::CreateWeakLambda(this, [this, WeakPC]()
	{
		if (WeakPC.IsValid() && IsMidRound()) WeakPC->Auth_StartSpectating();
	}), FMath::Max(Delay, 0.01f), false);
}

void ASearchDestroyGameMode::ValidateSpectators()
{
	if (!GetWorld()) return;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AShooterPlayerController* SPC = Cast<AShooterPlayerController>(It->Get()))
		{
			if (SPC->IsSpectating()) SPC->Auth_SpectateNext(true);
		}
	}
}

void ASearchDestroyGameMode::Logout(AController* Exiting)
{
	// 记下队伍和钱，断线重连时还原
	if (const AShooterPlayerState* PS = IsValid(Exiting) ? Exiting->GetPlayerState<AShooterPlayerState>() : nullptr)
	{
		const FString Key = UShooterGameInstance::GetRoomKey(PS);
		if (!Key.IsEmpty())
		{
			FSDReconnectInfo& Info = ReconnectCache.FindOrAdd(Key);
			Info.TeamId = PS->GetTeamId();
			Info.Money = PS->GetMoney();
		}
	}
	if (Interactor.Get() == Exiting) CancelAction();
	Super::Logout(Exiting);
}
