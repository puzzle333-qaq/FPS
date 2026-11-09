#include "Game/CoopCqbGameMode.h"

#include "Game/CoopCqbGameState.h"
#include "Game/HostageActor.h"
#include "World/MissionObjective.h"
#include "ShooterGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Game/HostileAICharacter.h"
#include "Game/HostileAIController.h"
#include "Game/HostileSpawner.h"
#include "Game/HostileSquadSubsystem.h"
#include "Game/ShooterPlayerState.h"
#include "ShooterPlayerController.h"
#include "ShooterCharacter.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "Weapon/WeaponFlash.h"
#include "Stats/StatsComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/ConstructorHelpers.h"
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
	static ConstructorHelpers::FClassFinder<AHostileAICharacter> HostileBP(TEXT("/Game/FPS/Coop/BP_Hostile"));
	if (HostileBP.Succeeded())
	{
		HostileClass = HostileBP.Class;
	}

	OperatorWeapons = {
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/RAR/BP_FPS_RAR.BP_FPS_RAR_C"))),
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/BP_Weapon_X24.BP_Weapon_X24_C"))),
	};
	OperatorThrowables = {
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/BP_WeaponFlash.BP_WeaponFlash_C"))),
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/Throwable/BP_Weapon_Grenade.BP_Weapon_Grenade_C"))),
	};
	OperatorArmorLevel = 1;

	static ConstructorHelpers::FClassFinder<APawn> PawnBP(TEXT("/Game/FPS/Character/BP_ShooterCharacter"));
	if (PawnBP.Succeeded())
	{
		DefaultPawnClass = PawnBP.Class;
	}
	static ConstructorHelpers::FClassFinder<APlayerController> ControllerBP(TEXT("/Game/FPS/Player/BP_ShooterPlayerController"));
	if (ControllerBP.Succeeded())
	{
		PlayerControllerClass = ControllerBP.Class;
	}
}

void ACoopCqbGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
 Super::InitGame(MapName, Options, ErrorMessage);
 bRangeHub = MapName.Contains(TEXT("Lvl_Range")) || UGameplayStatics::HasOption(Options, TEXT("RangeHub"));
 ExpectedOperators = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("ExpectedOperators"), 1), 1, MaxOperators);
}

void ACoopCqbGameMode::InitGameState()
{
	Super::InitGameState();
	if (ACoopCqbGameState* GS = GetGameState<ACoopCqbGameState>())
	{
		GS->bRangeHub = bRangeHub;
 GS->ExpectedOperators = ExpectedOperators;
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
	if (bRangeHub)
 {
  if (auto* GS = GetGameState<ACoopCqbGameState>())
  {
   GS->SetCoopPhase(ECoopCqbPhase::InProgress);
   GS->SetMatchState(EShooterMatchState::InProgress);
  }
  return;
 }
 LoadStartedAt = GetWorld()->GetTimeSeconds();
 GetWorldTimerManager().SetTimer(ReadinessTimer, this, &ThisClass::CheckOperatorReadiness, .25f, true);
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
 for (TActorIterator<AHostileAICharacter> It(GetWorld()); It; ++It) MissionSuspects.Add(*It);
 for (TActorIterator<AHostageActor> It(GetWorld()); It; ++It) MissionHostages.Add(*It);
 for (TActorIterator<AMissionObjective> It(GetWorld()); It; ++It) MissionObjectives.Add(*It);
 if (auto* GS = GetGameState<ACoopCqbGameState>()) GS->Report.OperatorsTotal = CountLivingOperators();
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

	TArray<AHostileAICharacter*> AlreadyThere;
	for (TActorIterator<AHostileAICharacter> It(World); It; ++It)
	{
		AlreadyThere.Add(*It);
	}

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
		UHostileSquadSubsystem* Squad = World->GetSubsystem<UHostileSquadSubsystem>();
		for (int32 i = 0; i < FallbackHostileCount; ++i)
		{
			const FVector Loc = Base + FVector(400.f + i * 120.f, 200.f, 0.f);
			const FTransform Xform(FRotator::ZeroRotator, Loc);
			AHostileAICharacter* H = World->SpawnActorDeferred<AHostileAICharacter>(
				HostileClass, Xform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
			if (!H) continue;
			if (Squad)
			{
				H->SquadRole = Squad->TakeNextRole();
				H->bAutoSquadRole = true;
			}
			H->FinishSpawning(Xform);
			++Spawned;
		}
	}

	TArray<AHostileAICharacter*> Born;
	for (TActorIterator<AHostileAICharacter> It(World); It; ++It)
	{
		if (!AlreadyThere.Contains(*It))
		{
			Born.Add(*It);
		}
	}
	if (Born.Num() == 1 && Born[0]->bAutoSquadRole)
	{
		if (AHostileAIController* Brain = Cast<AHostileAIController>(Born[0]->GetController()))
		{
			Brain->SetSquadRole(EHostileSquadRole::Closer);
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
	if (SPC && !SPC->GetSubmittedLoadout())
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
	if (bMissionEnded || bRangeHub) return;
 if (CountLivingOperators() <= 0)
	{
		EndMission(false, ECoopCqbWinReason::TeamWiped);
 }
 else UpdateReport();
}

void ACoopCqbGameMode::NotifyHostileKilled(AHostileAICharacter* Hostile)
{
 if (!HasAuthority() || bMissionEnded || bRangeHub) return;
 if (Hostile && Hostile->GetMorale() == EHostileMorale::Surrender) UnlawfulKills.Add(Hostile);
 Auth_RefreshObjectives();
}

int32 ACoopCqbGameMode::CountLivingHostiles() const
{
	int32 Count = 0;
	for (TActorIterator<AHostileAICharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsHostileAlive()) continue;
		if (It->IsArrested()) continue;
		++Count;
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

void ACoopCqbGameMode::UpdateReport(bool bFinal, bool bWon)
{
 auto* GS = GetGameState<ACoopCqbGameState>();
 if (!GS) return;
 FMissionReport& R = GS->Report;
 R.SuspectsTotal = MissionSuspects.Num();
 R.Neutralized = R.Arrested = 0;
 for (const auto& Ref : MissionSuspects)
 {
  if (const auto* H = Ref.Get())
  {
   if (!H->IsHostileAlive()) NeutralizedSuspects.Add(Ref);
   else if (H->IsArrested()) ArrestedSuspects.Add(Ref);
  }
  if (NeutralizedSuspects.Contains(Ref)) ++R.Neutralized;
  else if (ArrestedSuspects.Contains(Ref)) ++R.Arrested;
 }
 R.HostagesTotal = MissionHostages.Num();
 R.Rescued = R.HostagesLost = 0;
 for (const auto& Ref : MissionHostages)
 {
  const auto* H = Ref.Get();
  if (!H || !H->IsHostageAlive()) ++R.HostagesLost;
  else if (H->IsRescued()) ++R.Rescued;
 }
 R.MarkedTotal = R.MarkedCompleted = R.HiddenCompleted = 0;
 GS->ObjectiveDescriptions.Reset();
 for (const auto& Ref : MissionObjectives)
 {
  if (const auto* O = Ref.Get())
  {
   if (O->bHiddenObjective) OptionalObjectives.Add(Ref);
   if (O->bCompleted) CompletedObjectives.Add(Ref);
   if (!O->bHiddenObjective || O->bCompleted) GS->ObjectiveDescriptions.Add(FText::Format(
    NSLOCTEXT("Mission", "ObjectiveStatus", "{0} {1}"), FText::FromString(O->bCompleted ? TEXT("✓") : TEXT("○")), O->ObjectiveName));
   if (O->bHiddenObjective) { if (O->bCompleted) ++R.HiddenCompleted; }
   else { ++R.MarkedTotal; if (O->bCompleted) ++R.MarkedCompleted; }
  }
  else if (OptionalObjectives.Contains(Ref)) { if (CompletedObjectives.Contains(Ref)) ++R.HiddenCompleted; }
  else { ++R.MarkedTotal; if (CompletedObjectives.Contains(Ref)) ++R.MarkedCompleted; }
 }
 R.Survivors = CountLivingOperators();
 R.SurrenderedKilled = UnlawfulKills.Num();
 R.bPrimaryComplete = R.SuspectsTotal == R.Neutralized + R.Arrested && R.Rescued == R.HostagesTotal
  && R.MarkedCompleted == R.MarkedTotal && R.HostagesLost == 0;
 R.Calculate(ScoreRules, bWon, bFinal);
 GS->SetHostileCounts(FMath::Max(0, R.SuspectsTotal - R.Neutralized - R.Arrested), R.SuspectsTotal);
 GS->PublishMission();
}

void ACoopCqbGameMode::Auth_RefreshObjectives()
{
 if (!HasAuthority() || bMissionEnded || bRangeHub) return;
 auto* GS = GetGameState<ACoopCqbGameState>();
 if (!GS || !GS->IsMissionLive()) return;
 UpdateReport();
 if (GS->Report.HostagesLost > 0) { EndMission(false, ECoopCqbWinReason::HostageLost); return; }
 // Empty test maps stay playable; they must contain at least one real required task.
 const bool bHasTasks = GS->Report.SuspectsTotal + GS->Report.HostagesTotal + GS->Report.MarkedTotal > 0;
 if (bHasTasks && GS->Report.bPrimaryComplete) EndMission(true, ECoopCqbWinReason::ObjectivesMet);
}

void ACoopCqbGameMode::EndMission(bool bOperatorsWon, ECoopCqbWinReason Reason)
{
	if (!HasAuthority() || bMissionEnded) return;
	bMissionEnded = true;
	GetWorldTimerManager().ClearTimer(BriefingTimer);
 GetWorldTimerManager().ClearTimer(ReadinessTimer);
 UpdateReport(true, bOperatorsWon);

	if (ACoopCqbGameState* GS = GetGameState<ACoopCqbGameState>())
	{
		GS->SetMissionResult(bOperatorsWon, Reason);
		GS->SetCoopPhase(ECoopCqbPhase::PostMission);
		GS->SetMatchState(EShooterMatchState::PostMatch);
		GS->SetWinningTeam(bOperatorsWon ? CoopCqbTeams::Operators : 255);
		GS->BeginEvac(PostMissionDelay);
	}

	UE_LOG(LogTemp, Log, TEXT("[CoopCQB] Mission end won=%d reason=%d"), bOperatorsWon ? 1 : 0, (int32)Reason);
 OpenDebrief();
}

void ACoopCqbGameMode::OpenDebrief()
{
	if (ACoopCqbGameState* GS = GetGameState<ACoopCqbGameState>())
	{
		GS->ReturnEndServerTime = GS->GetServerWorldTimeSeconds() + FMath::Max(5.f, DebriefDuration);
  GS->OpenDebrief();
  GetWorldTimerManager().SetTimer(ReturnTimer, this, &ThisClass::Auth_ReturnToRange, FMath::Max(5.f, DebriefDuration), false);
	}
}

void ACoopCqbGameMode::Auth_AbortMission()
{
	EndMission(false, ECoopCqbWinReason::Aborted);
}

void ACoopCqbGameMode::Auth_OperatorReady(APlayerController* PC)
{
 if (!HasAuthority() || bRangeHub || bMissionEnded || !IsValid(PC) || !PC->GetPawn()) return;
 ReadyPlayers.Add(PC);
 CheckOperatorReadiness();
}

void ACoopCqbGameMode::CheckOperatorReadiness()
{
 auto* GS = GetGameState<ACoopCqbGameState>();
 if (!GS || GS->GetCoopPhase() != ECoopCqbPhase::Waiting || bMissionEnded) return;
 int32 Ready = 0, Connected = 0;
 for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
 {
  APlayerController* PC = It->Get();
  if (!IsValid(PC)) continue;
  ++Connected;
  if (ReadyPlayers.Contains(PC) && PC->GetPawn() && PC->HasClientLoadedCurrentWorld()) ++Ready;
 }
 GS->ReadyOperators = Ready;
 GS->MissionStatus = FString::Printf(TEXT("等待全员加载：%d / %d"), Ready, FMath::Max(Connected, ExpectedOperators));
 GS->PublishMission();
 if (Connected >= ExpectedOperators && Ready == Connected)
 {
  GetWorldTimerManager().ClearTimer(ReadinessTimer);
  StartBriefing();
 }
 else if (GetWorld()->GetTimeSeconds() - LoadStartedAt > LoadTimeout)
 {
  GS->MissionStatus = TEXT("队员加载超时，任务取消，即将返回靶场");
  EndMission(false, ECoopCqbWinReason::LoadTimeout);
 }
}

void ACoopCqbGameMode::Logout(AController* Exiting)
{
 const bool bDepartedOperator = Exiting && Exiting->IsPlayerController();
 if (auto* PC = Cast<APlayerController>(Exiting)) ReadyPlayers.Remove(PC);
 Super::Logout(Exiting);
 // Do not silently reduce a travel roster: cancel rather than starting without a teammate.
 if (bDepartedOperator && !bRangeHub && !bMissionEnded)
 {
  auto* GS = GetGameState<ACoopCqbGameState>();
  if (GS && GS->GetCoopPhase() == ECoopCqbPhase::InProgress)
  {
   if (CountLivingOperators() <= 0) EndMission(false, ECoopCqbWinReason::TeamWiped);
   else UpdateReport();
  }
  else EndMission(false, ECoopCqbWinReason::Aborted);
 }
}

void ACoopCqbGameMode::Auth_ReturnToRange()
{
 if (!HasAuthority()) return;
 auto* GI = Cast<UShooterGameInstance>(GetGameInstance());
 if (!GI) return;
 TSoftObjectPtr<UWorld> Map = GI->RangeMap;
 if (Map.IsNull()) Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/FPS/Level/Lvl_Range.Lvl_Range")));
 GI->TravelSquadTo(Map, GI->RangeGameMode, {TEXT("RangeHub=1")});
}
