#include "Game/PoliceCriminalGameMode.h"

#include "Game/PoliceCriminalGameState.h"
#include "Game/ShooterPlayerState.h"
#include "ShooterPlayerController.h"
#include "ShooterCharacter.h"
#include "ShooterGameInstance.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "Loadout/LoadoutTypes.h"
#include "ShooterType/ShooterType.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/Engine.h"

APoliceCriminalGameMode::APoliceCriminalGameMode()
{
	GameStateClass = APoliceCriminalGameState::StaticClass();
	PlayerStateClass = AShooterPlayerState::StaticClass();
	RespawnDelay = 0.f;

	CriminalVanWeapons = {
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/BP_Weapon_AK.BP_Weapon_AK_C"))),
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/BP_Pistol.BP_Pistol_C"))),
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/Kit/BP_Weapon_Deagle.BP_Weapon_Deagle_C"))),
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/BP_Weapon_PCC.BP_Weapon_PCC_C"))),
	};
	PoliceArmoryWeapons = {
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/RAR/BP_FPS_RAR.BP_FPS_RAR_C"))),
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/BP_Weapon_X24.BP_Weapon_X24_C"))),
	};
	SharedMeleeWeapon = TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/BP_Knife.BP_Knife_C")));
}

void APoliceCriminalGameMode::InitGameState()
{
	Super::InitGameState();
	ApplyMatchRules();
}

void APoliceCriminalGameMode::StartPlay()
{
	Super::StartPlay();
	ResolveWeaponCatalogs();
	ApplyMatchRules();
	BeginWaitingOrSetup();
}

void APoliceCriminalGameMode::ResolveWeaponCatalogs()
{
	ResolvedCriminalWeapons.Reset();
	for (const TSoftClassPtr<AWeapon>& Soft : CriminalVanWeapons)
	{
		if (UClass* C = Soft.LoadSynchronous())
		{
			ResolvedCriminalWeapons.Add(C);
		}
	}
	ResolvedPoliceWeapons.Reset();
	for (const TSoftClassPtr<AWeapon>& Soft : PoliceArmoryWeapons)
	{
		if (UClass* C = Soft.LoadSynchronous())
		{
			ResolvedPoliceWeapons.Add(C);
		}
	}
	ResolvedMelee = SharedMeleeWeapon.LoadSynchronous();
	UE_LOG(LogTemp, Log, TEXT("[PCMode] Catalog criminal=%d police=%d melee=%s"),
		ResolvedCriminalWeapons.Num(), ResolvedPoliceWeapons.Num(), *GetNameSafe(ResolvedMelee));
}

void APoliceCriminalGameMode::ApplyMatchRules()
{
	APoliceCriminalGameState* GS = GetGameState<APoliceCriminalGameState>();
	if (!GS) return;
	GS->SetMatchRules(EShooterGameMode::PoliceCriminal);
	GS->SetMatchType(MatchType);
	if (GS->GetNumTeams() < 2)
	{
		GS->InitTeams(2, 1);
	}
}

void APoliceCriminalGameMode::BeginWaitingOrSetup()
{
	if (bFlowStarted) return;
	bFlowStarted = true;
	if (WarmupTime > 0.f)
	{
		GetWorldTimerManager().SetTimer(WarmupTimer, this, &ThisClass::StartCriminalSetup, WarmupTime, false);
	}
	else
	{
		StartCriminalSetup();
	}
}

void APoliceCriminalGameMode::StartCriminalSetup()
{
	APoliceCriminalGameState* GS = GetGameState<APoliceCriminalGameState>();
	if (!GS) return;
	const float Now = GS->GetServerWorldTimeSeconds();
	GS->SetSetupEndServerTime(Now + CriminalSetupDuration);
	GS->SetPhase(EPoliceCriminalPhase::CriminalSetup);
	GS->SetMatchState(EShooterMatchState::WaitingToStart);
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &ThisClass::TickPhaseTimers, 0.5f, true);
	UE_LOG(LogTemp, Log, TEXT("[PCMode] Criminal setup %.0fs (police locked)"), CriminalSetupDuration);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange,
			FString::Printf(TEXT("匪徒布置中 %.0f 秒 — 警察暂不可突击"), CriminalSetupDuration));
	}
}

void APoliceCriminalGameMode::StartAssault()
{
	APoliceCriminalGameState* GS = GetGameState<APoliceCriminalGameState>();
	if (!GS || GS->GetPhase() == EPoliceCriminalPhase::Assault || GS->GetPhase() == EPoliceCriminalPhase::PostMatch) return;
	const float Now = GS->GetServerWorldTimeSeconds();
	GS->SetAssaultEndServerTime(AssaultDuration > 0.f ? Now + AssaultDuration : 0.f);
	GS->SetMatchStartServerTime(Now);
	GS->SetPhase(EPoliceCriminalPhase::Assault);
	GS->SetMatchState(EShooterMatchState::InProgress);
	GS->RefreshTeamAliveCounts();
	UE_LOG(LogTemp, Log, TEXT("[PCMode] Assault started. Duration=%.0fs Type=%d"), AssaultDuration, (int32)MatchType);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Green, TEXT("突击开始 — 警察可推进"));
	}
}

void APoliceCriminalGameMode::TickPhaseTimers()
{
	APoliceCriminalGameState* GS = GetGameState<APoliceCriminalGameState>();
	if (!GS) return;
	if (GS->GetPhase() == EPoliceCriminalPhase::CriminalSetup)
	{
		if (GS->GetSetupRemaining() <= 0.f)
		{
			StartAssault();
		}
		return;
	}
	if (GS->GetPhase() == EPoliceCriminalPhase::Assault)
	{
		if (MatchType == EPoliceCriminalMatchType::Elimination)
		{
			CheckEliminationWin();
		}
		if (AssaultDuration > 0.f && GS->GetRemainingTime() <= 0.f)
		{
			EndMatch(PoliceCriminalTeams::Criminal);
		}
	}
}

void APoliceCriminalGameMode::CheckEliminationWin()
{
	APoliceCriminalGameState* GS = GetGameState<APoliceCriminalGameState>();
	if (!GS || GS->GetPhase() != EPoliceCriminalPhase::Assault) return;
	GS->RefreshTeamAliveCounts();
	const int32 CriminalAlive = GS->GetTeamAliveCount(PoliceCriminalTeams::Criminal);
	const int32 CriminalPlayers = GS->GetTeamPlayerCount(PoliceCriminalTeams::Criminal);
	if (CriminalPlayers > 0 && CriminalAlive <= 0)
	{
		EndMatch(PoliceCriminalTeams::Police);
	}
}

void APoliceCriminalGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	AssignShortPlayerName(NewPlayer);
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	if (GEngine && IsValid(NewPlayer) && NewPlayer->IsLocalController())
	{
		GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::Cyan,
			TEXT("警匪: PickFaction 0/1 | VanPick | ArmoryPick | SkipSetup | PlaceTripwire/PlaceGasCan | SpawnTripwire/SpawnGasCan | SpawnTestHostage"));
	}
}

AActor* APoliceCriminalGameMode::FindTaggedStart(FName Tag) const
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
	if (Free.Num() > 0) return Free[FMath::RandRange(0, Free.Num() - 1)];
	if (Any.Num() > 0) return Any[FMath::RandRange(0, Any.Num() - 1)];
	return nullptr;
}

AActor* APoliceCriminalGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const AShooterPlayerState* PS = IsValid(Player) ? Player->GetPlayerState<AShooterPlayerState>() : nullptr;
	if (PS && PS->HasTeam())
	{
		const FName Tag = PS->GetTeamId() == PoliceCriminalTeams::Police ? PoliceStartTag : CriminalStartTag;
		if (AActor* Start = FindTaggedStart(Tag)) return Start;
		UE_LOG(LogTemp, Warning, TEXT("[PCMode] No PlayerStart tagged '%s'"), *Tag.ToString());
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

bool APoliceCriminalGameMode::CanDamage(AController* InstigatorController, AController* VictimController) const
{
	if (!Super::CanDamage(InstigatorController, VictimController)) return false;
	const APoliceCriminalGameState* GS = GetGameState<APoliceCriminalGameState>();
	if (GS && GS->GetPhase() != EPoliceCriminalPhase::Assault) return false;
	if (bFriendlyFire || !IsValid(InstigatorController) || !IsValid(VictimController) || InstigatorController == VictimController) return true;
	const AShooterPlayerState* A = InstigatorController->GetPlayerState<AShooterPlayerState>();
	const AShooterPlayerState* B = VictimController->GetPlayerState<AShooterPlayerState>();
	return !(A && B && A->HasTeam() && A->GetTeamId() == B->GetTeamId());
}

bool APoliceCriminalGameMode::CanRespawn(AController* Controller) const
{
	return false;
}

void APoliceCriminalGameMode::ScoreKill(AShooterPlayerState* KillerPS, AShooterPlayerState* VictimPS)
{
	APoliceCriminalGameState* GS = GetGameState<APoliceCriminalGameState>();
	if (!GS || !KillerPS || GS->GetPhase() != EPoliceCriminalPhase::Assault) return;
	if (!KillerPS->HasTeam() || (VictimPS && VictimPS->GetTeamId() == KillerPS->GetTeamId())) return;
	GS->AddTeamScore(KillerPS->GetTeamId(), 1);
}

void APoliceCriminalGameMode::OnPlayerKilled(AController* KillerController, AController* VictimController)
{
	Super::OnPlayerKilled(KillerController, VictimController);
	if (MatchType == EPoliceCriminalMatchType::Elimination)
	{
		CheckEliminationWin();
	}
}

void APoliceCriminalGameMode::HandleTeamForfeit(uint8 WinningTeam)
{
	EndMatch(WinningTeam);
}

void APoliceCriminalGameMode::ApplyFactionLookForController(AController* Controller)
{
	if (!IsValid(Controller)) return;
	AShooterPlayerState* PS = Controller->GetPlayerState<AShooterPlayerState>();
	AShooterCharacter* Char = Cast<AShooterCharacter>(Controller->GetPawn());
	if (!PS || !PS->HasTeam() || !IsValid(Char)) return;
	const ECharacterLookFaction Look = PS->GetTeamId() == PoliceCriminalTeams::Police
		? ECharacterLookFaction::Police
		: ECharacterLookFaction::Criminal;
	Char->SetFactionLookLocal(Look);
}

bool APoliceCriminalGameMode::Auth_AssignFaction(APlayerController* PC, EPoliceCriminalFaction Faction)
{
	if (!HasAuthority() || !IsValid(PC)) return false;
	AShooterPlayerState* PS = PC->GetPlayerState<AShooterPlayerState>();
	if (!PS) return false;
	const uint8 Team = FactionToTeamId(Faction);
	PS->SetTeamId(Team);
	ApplyFactionLookForController(PC);
	if (APawn* Pawn = PC->GetPawn())
	{
		if (AActor* Start = ChoosePlayerStart(PC))
		{
			Pawn->SetActorLocationAndRotation(Start->GetActorLocation(), Start->GetActorRotation());
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[PCMode] %s -> faction %d team %d"), *PS->GetPlayerName(), (int32)Faction, Team);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Cyan,
			Faction == EPoliceCriminalFaction::Police
				? TEXT("已选警察 — ArmoryPick RAR|X24 + Armor/Vision；外观=警察")
				: TEXT("已选匪徒 — VanPick AK|Glock|Deagle|PCC；外观=匪"));
	}
	return true;
}

bool APoliceCriminalGameMode::Auth_ApplyVanLoadout(APlayerController* PC, TSubclassOf<AWeapon> PrimaryOrPistol)
{
	if (!HasAuthority() || !IsValid(PC) || !PrimaryOrPistol) return false;
	AShooterPlayerState* PS = PC->GetPlayerState<AShooterPlayerState>();
	if (!PS || PS->GetTeamId() != PoliceCriminalTeams::Criminal)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PCMode] VanLoadout denied: not criminal"));
		return false;
	}
	if (!ResolvedCriminalWeapons.Contains(PrimaryOrPistol))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PCMode] VanLoadout denied: weapon not in van catalog"));
		return false;
	}
	AShooterPlayerController* SPC = Cast<AShooterPlayerController>(PC);
	if (!SPC) return false;

	FLoadoutPreset Loadout;
	Loadout.Name = FText::FromString(TEXT("Van"));
	FWeaponPreset Main;
	Main.WeaponClass = PrimaryOrPistol;
	Loadout.Weapons.Add(Main);
	if (ResolvedMelee)
	{
		FWeaponPreset Knife;
		Knife.WeaponClass = ResolvedMelee;
		Loadout.Weapons.Add(Knife);
	}
	SPC->Auth_SetSubmittedLoadout(Loadout, true);
	UE_LOG(LogTemp, Log, TEXT("[PCMode] Van loadout %s"), *GetNameSafe(PrimaryOrPistol));
	return true;
}

bool APoliceCriminalGameMode::Auth_ApplyArmoryLoadout(APlayerController* PC, TSubclassOf<AWeapon> WeaponClass, bool bAllowAttachments)
{
	if (!HasAuthority() || !IsValid(PC) || !WeaponClass) return false;
	AShooterPlayerState* PS = PC->GetPlayerState<AShooterPlayerState>();
	if (!PS || PS->GetTeamId() != PoliceCriminalTeams::Police)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PCMode] Armory denied: not police"));
		return false;
	}
	if (!ResolvedPoliceWeapons.Contains(WeaponClass) && WeaponClass != ResolvedMelee)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PCMode] Armory denied: weapon not in police catalog"));
		return false;
	}
	AShooterPlayerController* SPC = Cast<AShooterPlayerController>(PC);
	if (!SPC) return false;

	FLoadoutPreset Loadout;
	Loadout.Name = FText::FromString(TEXT("Armory"));
	FWeaponPreset Main;
	Main.WeaponClass = WeaponClass;
	if (bAllowAttachments)
	{
		if (const UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance()))
		{
			const FLoadoutPreset Saved = GI->GetCurrentLoadout();
			for (const FWeaponPreset& P : Saved.Weapons)
			{
				if (P.WeaponClass == WeaponClass)
				{
					Main.Attachments = P.Attachments;
					break;
				}
			}
		}
	}
	Loadout.Weapons.Add(Main);
	if (ResolvedMelee)
	{
		FWeaponPreset Knife;
		Knife.WeaponClass = ResolvedMelee;
		Loadout.Weapons.Add(Knife);
	}
	SPC->Auth_SetSubmittedLoadout(Loadout, false);
	UE_LOG(LogTemp, Log, TEXT("[PCMode] Armory loadout %s atts=%d"), *GetNameSafe(WeaponClass), Main.Attachments.Num());
	return true;
}

void APoliceCriminalGameMode::Auth_SkipSetup()
{
	if (!HasAuthority()) return;
	APoliceCriminalGameState* GS = GetGameState<APoliceCriminalGameState>();
	if (!GS || GS->GetPhase() != EPoliceCriminalPhase::CriminalSetup) return;
	StartAssault();
}

void APoliceCriminalGameMode::EndMatch(uint8 WinningTeam)
{
	APoliceCriminalGameState* GS = GetGameState<APoliceCriminalGameState>();
	if (!GS || GS->GetPhase() == EPoliceCriminalPhase::PostMatch) return;
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	ClearRespawnTimers();
	GS->SetWinningTeam(WinningTeam);
	GS->SetPhase(EPoliceCriminalPhase::PostMatch);
	GS->SetMatchState(EShooterMatchState::PostMatch);
	UE_LOG(LogTemp, Log, TEXT("[PCMode] Match ended. Winner team=%d (0=Police 1=Criminal)"), WinningTeam);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(It->Get()))
		{
			PC->Client_OnMatchEnded(WinningTeam);
		}
	}
	GetWorldTimerManager().SetTimer(PostMatchTimer, this, &ThisClass::ReturnAllToMainMenu, FMath::Max(PostMatchDelay, 0.1f), false);
}

void APoliceCriminalGameMode::ReturnAllToMainMenu()
{
	if (ReturnToRoomLobby()) return;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!IsValid(PC) || PC->IsLocalController()) continue;
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
	}), 0.35f, false);
}
