#include "Game/PoliceCriminalGameMode.h"

#include "Game/PoliceCriminalGameState.h"
#include "Game/ShooterPlayerState.h"
#include "ShooterPlayerController.h"
#include "ShooterCharacter.h"
#include "ShooterGameInstance.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "Weapon/WeaponFlash.h"
#include "Weapon/WeaponMolotov.h"
#include "Loadout/LoadoutTypes.h"
#include "ShooterType/ShooterType.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/Engine.h"

namespace
{
	// MvpRound: 服务器进程内跨 ServerTravel 保存配装（面包车/军械库关卡 -> 对局图）。key = 房间玩家 key
	struct FPCTravelLoadout { FLoadoutPreset Loadout; bool bStrip = false; };
	TMap<FString, FPCTravelLoadout>& PCTravelLoadouts()
	{
		static TMap<FString, FPCTravelLoadout> Map;
		return Map;
	}
}

APoliceCriminalGameMode::APoliceCriminalGameMode()
{
	GameStateClass = APoliceCriminalGameState::StaticClass();
	PlayerStateClass = AShooterPlayerState::StaticClass();
	RespawnDelay = 0.f;
	// Temp test match map = range; swap to CQB later via BP Class Defaults
	MatchMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/FPS/Level/Lvl_Range.Lvl_Range")));

	// MVP catalogs only — Deagle/PCC/knife stripped.
	CriminalVanWeapons = {
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/BP_Weapon_AK.BP_Weapon_AK_C"))),
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/BP_Pistol.BP_Pistol_C"))),
	};
	PoliceArmoryWeapons = {
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/RAR/BP_FPS_RAR.BP_FPS_RAR_C"))),
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/BP_Weapon_X24.BP_Weapon_X24_C"))),
	};
	PoliceThrowables = {
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Script/FPS.WeaponFlash"))),
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/Throwable/BP_Weapon_Grenade.BP_Weapon_Grenade_C"))),
	};
	CriminalThrowables = {
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Script/FPS.WeaponMolotov"))),
		TSoftClassPtr<AWeapon>(FSoftObjectPath(TEXT("/Game/FPS/Weapon/Throwable/BP_Weapon_Grenade.BP_Weapon_Grenade_C"))),
	};
	SharedMeleeWeapon = nullptr; // MVP: no knife
	PoliceDefaultArmorLevel = 1;
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

static void FPS_ResolveSoftWeaponList(const TArray<TSoftClassPtr<AWeapon>>& SoftList, TArray<TSubclassOf<AWeapon>>& Out)
{
	Out.Reset();
	for (const TSoftClassPtr<AWeapon>& Soft : SoftList)
	{
		if (UClass* C = Soft.LoadSynchronous())
		{
			Out.AddUnique(C);
		}
	}
}

static TSubclassOf<AWeapon> FPS_FindWeaponByKind(const TArray<TSubclassOf<AWeapon>>& List, EWeaponKind Kind)
{
	for (TSubclassOf<AWeapon> C : List)
	{
		if (C && C->GetDefaultObject<AWeapon>()->WeaponKind == Kind)
		{
			return C;
		}
	}
	return nullptr;
}

void APoliceCriminalGameMode::ResolveWeaponCatalogs()
{
	FPS_ResolveSoftWeaponList(CriminalVanWeapons, ResolvedCriminalWeapons);
	FPS_ResolveSoftWeaponList(PoliceArmoryWeapons, ResolvedPoliceWeapons);
	FPS_ResolveSoftWeaponList(PoliceThrowables, ResolvedPoliceThrowables);
	FPS_ResolveSoftWeaponList(CriminalThrowables, ResolvedCriminalThrowables);
	ResolvedMelee = SharedMeleeWeapon.IsNull() ? nullptr : SharedMeleeWeapon.LoadSynchronous();

	// Ensure flash + HE for police, molotov + HE for criminal (native fallbacks if soft missing).
	{
		bool bHasFlash = false;
		for (TSubclassOf<AWeapon> C : ResolvedPoliceThrowables)
		{
			if (C && (C == AWeaponFlash::StaticClass() || C->IsChildOf(AWeaponFlash::StaticClass()))) bHasFlash = true;
		}
		if (!bHasFlash) ResolvedPoliceThrowables.Insert(AWeaponFlash::StaticClass(), 0);
		TArray<TSubclassOf<AWeapon>> Ordered;
		for (TSubclassOf<AWeapon> C : ResolvedPoliceThrowables)
		{
			if (C && (C == AWeaponFlash::StaticClass() || C->IsChildOf(AWeaponFlash::StaticClass()))) Ordered.AddUnique(C);
		}
		for (TSubclassOf<AWeapon> C : ResolvedPoliceThrowables) Ordered.AddUnique(C);
		ResolvedPoliceThrowables = Ordered;
	}
	{
		bool bHasMolo = false;
		for (TSubclassOf<AWeapon> C : ResolvedCriminalThrowables)
		{
			if (C && (C == AWeaponMolotov::StaticClass() || C->IsChildOf(AWeaponMolotov::StaticClass()))) bHasMolo = true;
		}
		if (!bHasMolo) ResolvedCriminalThrowables.Insert(AWeaponMolotov::StaticClass(), 0);
		TArray<TSubclassOf<AWeapon>> Ordered;
		for (TSubclassOf<AWeapon> C : ResolvedCriminalThrowables)
		{
			if (C && (C == AWeaponMolotov::StaticClass() || C->IsChildOf(AWeaponMolotov::StaticClass()))) Ordered.AddUnique(C);
		}
		for (TSubclassOf<AWeapon> C : ResolvedCriminalThrowables) Ordered.AddUnique(C);
		ResolvedCriminalThrowables = Ordered;
	}

		// Ensure HE grenade present on both sides when soft path resolves.
	{
		TSubclassOf<AWeapon> Grenade = nullptr;
		const FSoftObjectPath GrenadePath(TEXT("/Game/FPS/Weapon/Throwable/BP_Weapon_Grenade.BP_Weapon_Grenade_C"));
		if (UClass* GC = Cast<UClass>(GrenadePath.TryLoad())) Grenade = GC;
		if (Grenade)
		{
			ResolvedPoliceThrowables.AddUnique(Grenade);
			ResolvedCriminalThrowables.AddUnique(Grenade);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[PCMode] MVP catalog criminal=%d police=%d throwP=%d throwC=%d melee=%s"),
		ResolvedCriminalWeapons.Num(), ResolvedPoliceWeapons.Num(),
		ResolvedPoliceThrowables.Num(), ResolvedCriminalThrowables.Num(), *GetNameSafe(ResolvedMelee));
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
		ValidateSpectators();
		if (MatchType == EPoliceCriminalMatchType::Elimination)
		{
			CheckEliminationWin();
		}
		if (AssaultDuration > 0.f && GS->GetRemainingTime() <= 0.f)
		{
			EndMatch(TimeoutWinningTeam, EPoliceCriminalWinReason::Timeout);
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
	const int32 PoliceAlive = GS->GetTeamAliveCount(PoliceCriminalTeams::Police);
	const int32 PolicePlayers = GS->GetTeamPlayerCount(PoliceCriminalTeams::Police);
	const bool bCriminalsOut = CriminalPlayers > 0 && CriminalAlive <= 0;
	const bool bPoliceOut = PolicePlayers > 0 && PoliceAlive <= 0;
	if (bCriminalsOut && bPoliceOut)
	{
		EndMatch(AShooterPlayerState::NoTeam, EPoliceCriminalWinReason::Draw); // 同归于尽 = 平局
	}
	else if (bCriminalsOut)
	{
		EndMatch(PoliceCriminalTeams::Police, EPoliceCriminalWinReason::Elimination);
	}
	else if (bPoliceOut)
	{
		EndMatch(PoliceCriminalTeams::Criminal, EPoliceCriminalWinReason::Elimination);
	}
}

void APoliceCriminalGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	AssignShortPlayerName(NewPlayer);
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	RestoreLoadoutAfterTravel(NewPlayer);
	ApplyFactionLookForController(NewPlayer);
	if (const APoliceCriminalGameState* JoinGS = GetGameState<APoliceCriminalGameState>())
	{
		if (JoinGS->GetPhase() == EPoliceCriminalPhase::Assault || JoinGS->GetPhase() == EPoliceCriminalPhase::PostMatch)
		{
			UE_LOG(LogTemp, Log, TEXT("[PCMode] %s joined mid-round -> spectate"), *GetNameSafe(NewPlayer));
			if (APawn* P = NewPlayer->GetPawn())
			{
				if (UCombatComponent* Combat = P->FindComponentByClass<UCombatComponent>()) Combat->DestroyInventory();
				NewPlayer->UnPossess();
				P->Destroy();
			}
			ScheduleSpectate(NewPlayer, 0.5f);
		}
	}
	if (GEngine && IsValid(NewPlayer) && NewPlayer->IsLocalController())
	{
		GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::Cyan,
			TEXT("警匪MVP: PickFaction 0/1(=满配) | VanPick/ArmoryPick mvp | 1主2副3投掷循环 | SkipSetup"));
	}
}

AActor* APoliceCriminalGameMode::FindTaggedStart(FName Tag) const
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

AActor* APoliceCriminalGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const AShooterPlayerState* PS = IsValid(Player) ? Player->GetPlayerState<AShooterPlayerState>() : nullptr;
	if (PS && PS->HasTeam())
	{
		const FName Tag = PS->GetTeamId() == PoliceCriminalTeams::Police ? PoliceStartTag : CriminalStartTag;
		if (AActor* Start = FindTaggedStart(Tag)) return Start;
		const FName Alt = PS->GetTeamId() == PoliceCriminalTeams::Police ? PoliceStartTagAlt : CriminalStartTagAlt;
		if (AActor* Start = FindTaggedStart(Alt)) return Start;
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
	if (const APoliceCriminalGameState* KGS = GetGameState<APoliceCriminalGameState>())
	{
		if (KGS->GetPhase() != EPoliceCriminalPhase::PostMatch) ScheduleSpectate(VictimController, SpectateDelay);
	}
	if (MatchType == EPoliceCriminalMatchType::Elimination)
	{
		CheckEliminationWin();
	}
}

void APoliceCriminalGameMode::HandleTeamForfeit(uint8 WinningTeam)
{
	EndMatch(WinningTeam, EPoliceCriminalWinReason::Forfeit);
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

	// MVP: auto full kit on faction pick (guns + throwables; no knife).
	Auth_ApplyMvpLoadout(PC);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
			Faction == EPoliceCriminalFaction::Police
				? TEXT("已选警察 MVP：1=RAR 2=X24 3=闪光↔手雷（轻甲）")
				: TEXT("已选匪徒 MVP：1=AK 2=手枪 3=燃烧瓶↔手雷（无甲更快）"));
	}
	return true;
}

bool APoliceCriminalGameMode::Auth_ApplyMvpLoadout(APlayerController* PC)
{
	if (!HasAuthority() || !IsValid(PC)) return false;
	AShooterPlayerState* PS = PC->GetPlayerState<AShooterPlayerState>();
	if (!PS || !PS->HasTeam()) return false;
	if (PS->GetTeamId() == PoliceCriminalTeams::Police)
	{
		TSubclassOf<AWeapon> Primary = FPS_FindWeaponByKind(ResolvedPoliceWeapons, EWeaponKind::LongGun);
		if (!Primary && ResolvedPoliceWeapons.Num() > 0) Primary = ResolvedPoliceWeapons[0];
		return Auth_ApplyArmoryLoadout(PC, Primary, true);
	}
	TSubclassOf<AWeapon> Primary = FPS_FindWeaponByKind(ResolvedCriminalWeapons, EWeaponKind::LongGun);
	if (!Primary && ResolvedCriminalWeapons.Num() > 0) Primary = ResolvedCriminalWeapons[0];
	return Auth_ApplyVanLoadout(PC, Primary);
}

bool APoliceCriminalGameMode::Auth_ApplyVanLoadout(APlayerController* PC, TSubclassOf<AWeapon> PrimaryOrPistol)
{
	if (!HasAuthority() || !IsValid(PC)) return false;
	AShooterPlayerState* PS = PC->GetPlayerState<AShooterPlayerState>();
	if (!PS || PS->GetTeamId() != PoliceCriminalTeams::Criminal)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PCMode] VanLoadout denied: not criminal"));
		return false;
	}
	AShooterPlayerController* SPC = Cast<AShooterPlayerController>(PC);
	if (!SPC) return false;

	TSubclassOf<AWeapon> Primary = FPS_FindWeaponByKind(ResolvedCriminalWeapons, EWeaponKind::LongGun);
	TSubclassOf<AWeapon> Pistol = FPS_FindWeaponByKind(ResolvedCriminalWeapons, EWeaponKind::Pistol);
	if (PrimaryOrPistol)
	{
		const EWeaponKind Kind = PrimaryOrPistol->GetDefaultObject<AWeapon>()->WeaponKind;
		if (Kind == EWeaponKind::LongGun) Primary = PrimaryOrPistol;
		else if (Kind == EWeaponKind::Pistol) Pistol = PrimaryOrPistol;
	}
	if (!Primary) Primary = PrimaryOrPistol;

	FLoadoutPreset Loadout;
	Loadout.Name = FText::FromString(TEXT("VanMVP"));
	if (Primary)
	{
		FWeaponPreset Main; Main.WeaponClass = Primary; Loadout.Weapons.Add(Main);
	}
	if (Pistol)
	{
		FWeaponPreset Side; Side.WeaponClass = Pistol; Loadout.Weapons.Add(Side);
	}
	for (TSubclassOf<AWeapon> T : ResolvedCriminalThrowables)
	{
		if (!T) continue;
		FWeaponPreset Throw; Throw.WeaponClass = T; Loadout.Weapons.Add(Throw);
	}
	// MVP: no knife (ResolvedMelee ignored)

	SPC->Auth_SetSubmittedLoadout(Loadout, true);
	if (AShooterCharacter* Char = Cast<AShooterCharacter>(PC->GetPawn()))
	{
		Char->SetBodyArmorLocal(EBodyArmorType::None);
	}
	SaveLoadoutForTravel(PC);
	UE_LOG(LogTemp, Log, TEXT("[PCMode] Van MVP primary=%s pistol=%s throws=%d"),
		*GetNameSafe(Primary), *GetNameSafe(Pistol), ResolvedCriminalThrowables.Num());
	return Loadout.Weapons.Num() > 0;
}

bool APoliceCriminalGameMode::Auth_ApplyArmoryLoadout(APlayerController* PC, TSubclassOf<AWeapon> WeaponClass, bool bAllowAttachments)
{
	if (!HasAuthority() || !IsValid(PC)) return false;
	AShooterPlayerState* PS = PC->GetPlayerState<AShooterPlayerState>();
	if (!PS || PS->GetTeamId() != PoliceCriminalTeams::Police)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PCMode] Armory denied: not police"));
		return false;
	}
	AShooterPlayerController* SPC = Cast<AShooterPlayerController>(PC);
	if (!SPC) return false;

	TSubclassOf<AWeapon> Primary = FPS_FindWeaponByKind(ResolvedPoliceWeapons, EWeaponKind::LongGun);
	TSubclassOf<AWeapon> Pistol = FPS_FindWeaponByKind(ResolvedPoliceWeapons, EWeaponKind::Pistol);
	if (WeaponClass)
	{
		const EWeaponKind Kind = WeaponClass->GetDefaultObject<AWeapon>()->WeaponKind;
		if (Kind == EWeaponKind::LongGun) Primary = WeaponClass;
		else if (Kind == EWeaponKind::Pistol) Pistol = WeaponClass;
	}
	if (!Primary) Primary = WeaponClass;

	FLoadoutPreset Loadout;
	Loadout.Name = FText::FromString(TEXT("ArmoryMVP"));
	if (Primary)
	{
		FWeaponPreset Main;
		Main.WeaponClass = Primary;
		if (bAllowAttachments)
		{
			if (const UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance()))
			{
				const FLoadoutPreset Saved = GI->GetCurrentLoadout();
				for (const FWeaponPreset& P : Saved.Weapons)
				{
					if (P.WeaponClass == Primary)
					{
						Main.Attachments = P.Attachments;
						break;
					}
				}
			}
		}
		Loadout.Weapons.Add(Main);
	}
	if (Pistol)
	{
		FWeaponPreset Side; Side.WeaponClass = Pistol; Loadout.Weapons.Add(Side);
	}
	for (TSubclassOf<AWeapon> T : ResolvedPoliceThrowables)
	{
		if (!T) continue;
		FWeaponPreset Throw; Throw.WeaponClass = T; Loadout.Weapons.Add(Throw);
	}

	SPC->Auth_SetSubmittedLoadout(Loadout, false);
	if (AShooterCharacter* Char = Cast<AShooterCharacter>(PC->GetPawn()))
	{
		EBodyArmorType Armor = EBodyArmorType::None;
		if (PoliceDefaultArmorLevel == 1) Armor = EBodyArmorType::Light;
		else if (PoliceDefaultArmorLevel >= 2) Armor = EBodyArmorType::Heavy;
		Char->SetBodyArmorLocal(Armor);
	}
	SaveLoadoutForTravel(PC);
	UE_LOG(LogTemp, Log, TEXT("[PCMode] Armory MVP primary=%s pistol=%s throws=%d armor=%d"),
		*GetNameSafe(Primary), *GetNameSafe(Pistol), ResolvedPoliceThrowables.Num(), PoliceDefaultArmorLevel);
	return Loadout.Weapons.Num() > 0;
}

void APoliceCriminalGameMode::Auth_SkipSetup()
{
	if (!HasAuthority()) return;
	APoliceCriminalGameState* GS = GetGameState<APoliceCriminalGameState>();
	if (!GS || GS->GetPhase() != EPoliceCriminalPhase::CriminalSetup) return;
	StartAssault();
}

void APoliceCriminalGameMode::EndMatch(uint8 WinningTeam, EPoliceCriminalWinReason Reason)
{
	APoliceCriminalGameState* GS = GetGameState<APoliceCriminalGameState>();
	if (!GS || GS->GetPhase() == EPoliceCriminalPhase::PostMatch) return;
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	ClearRespawnTimers();
	if (WinningTeam == AShooterPlayerState::NoTeam && Reason == EPoliceCriminalWinReason::Elimination)
	{
		Reason = EPoliceCriminalWinReason::Draw;
	}
	GS->SetLastWinReason(Reason);
	GS->SetWinningTeam(WinningTeam);
	PCTravelLoadouts().Reset();
	GS->SetPhase(EPoliceCriminalPhase::PostMatch);
	GS->SetMatchState(EShooterMatchState::PostMatch);
	UE_LOG(LogTemp, Log, TEXT("[PCMode] Match ended. Winner team=%d Reason=%d (0=Police 1=Criminal)"), WinningTeam, (int32)Reason);
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
	if (bRestartMapAfterMatch && GetWorld())
	{
		UE_LOG(LogTemp, Log, TEXT("[PCMode] Restart map"));
		GetWorld()->ServerTravel(TEXT("?restart"));
		return;
	}
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

// ---------------- MvpRound ----------------

void APoliceCriminalGameMode::ScheduleSpectate(AController* Controller, float Delay)
{
	AShooterPlayerController* SPC = Cast<AShooterPlayerController>(Controller);
	if (!IsValid(SPC)) return;
	TWeakObjectPtr<AShooterPlayerController> WeakPC = SPC;
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, WeakPC]()
	{
		const APoliceCriminalGameState* GS = GetGameState<APoliceCriminalGameState>();
		if (!WeakPC.IsValid() || !GS || GS->GetPhase() == EPoliceCriminalPhase::PostMatch) return;
		WeakPC->Auth_StartSpectating();
	}), FMath::Max(Delay, 0.05f), false);
}

void APoliceCriminalGameMode::ValidateSpectators()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AShooterPlayerController* SPC = Cast<AShooterPlayerController>(It->Get());
		if (IsValid(SPC) && SPC->IsSpectating()) SPC->Auth_SpectateNext(true);
	}
}

void APoliceCriminalGameMode::SaveLoadoutForTravel(APlayerController* PC)
{
	const AShooterPlayerController* SPC = Cast<AShooterPlayerController>(PC);
	const FLoadoutPreset* L = SPC ? SPC->GetSubmittedLoadout() : nullptr;
	if (!L || !PC->PlayerState) return;
	const FString Key = UShooterGameInstance::GetRoomKey(PC->PlayerState);
	if (Key.IsEmpty()) return;
	const AShooterPlayerState* PS = PC->GetPlayerState<AShooterPlayerState>();
	FPCTravelLoadout& Entry = PCTravelLoadouts().FindOrAdd(Key);
	Entry.Loadout = *L;
	Entry.bStrip = PS && PS->GetTeamId() == PoliceCriminalTeams::Criminal;
}

void APoliceCriminalGameMode::RestoreLoadoutAfterTravel(APlayerController* PC)
{
	AShooterPlayerController* SPC = Cast<AShooterPlayerController>(PC);
	if (!SPC || !PC->PlayerState || SPC->GetSubmittedLoadout()) return;
	const FString Key = UShooterGameInstance::GetRoomKey(PC->PlayerState);
	if (const FPCTravelLoadout* Entry = PCTravelLoadouts().Find(Key))
	{
		SPC->Auth_SetSubmittedLoadout(Entry->Loadout, Entry->bStrip);
		UE_LOG(LogTemp, Log, TEXT("[PCMode] Restored travel loadout for %s (%d weapons)"), *Key, Entry->Loadout.Weapons.Num());
	}
}

void APoliceCriminalGameMode::Auth_DebugForceWin(uint8 WinningTeam)
{
	if (!HasAuthority()) return;
	EndMatch(WinningTeam > 1 ? AShooterPlayerState::NoTeam : WinningTeam,
		WinningTeam > 1 ? EPoliceCriminalWinReason::Draw : EPoliceCriminalWinReason::Elimination);
}

void APoliceCriminalGameMode::Auth_DebugSetRemaining(float Seconds)
{
	APoliceCriminalGameState* GS = GetGameState<APoliceCriminalGameState>();
	if (!HasAuthority() || !GS) return;
	const float Now = GS->GetServerWorldTimeSeconds();
	if (GS->GetPhase() == EPoliceCriminalPhase::CriminalSetup) GS->SetSetupEndServerTime(Now + Seconds);
	else if (GS->GetPhase() == EPoliceCriminalPhase::Assault) GS->SetAssaultEndServerTime(Now + Seconds);
}

void APoliceCriminalGameMode::Auth_DebugKill(AController* Controller)
{
	AShooterCharacter* Char = IsValid(Controller) ? Cast<AShooterCharacter>(Controller->GetPawn()) : nullptr;
	if (!HasAuthority() || !Char) return;
	const APoliceCriminalGameState* GS = GetGameState<APoliceCriminalGameState>();
	if (GS && GS->GetPhase() != EPoliceCriminalPhase::Assault)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("KillSelf 只在突击阶段有效（先 SkipSetup）"));
		return;
	}
	Char->DoDamage_Implementation(100000.f, Char);
}

bool APoliceCriminalGameMode::Auth_TravelToMatchMap(const FString& MapOverride)
{
	if (!HasAuthority() || !GetWorld()) return false;
	FString MapPath = MapOverride;
	if (MapPath.IsEmpty())
	{
		if (!MatchMap.IsNull())
		{
			MapPath = MatchMap.ToSoftObjectPath().GetLongPackageName();
		}
	}
	MapPath.TrimStartAndEndInline();
	if (MapPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PCMode] TravelToMatch: set MatchMap on GameMode or pass map name (e.g. /Game/FPS/Maps/CQB)"));
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Red, TEXT("TravelToMatch 失败：先填 GameMode.MatchMap 或命令带地图路径"));
		return false;
	}
	// Accept short names like CQB -> leave as-is for ServerTravel (UE resolves /Game/... if absolute)
	bUseSeamlessTravel = bSeamlessTravelToMatch;
	UE_LOG(LogTemp, Log, TEXT("[PCMode] ServerTravel -> %s (seamless=%d)"), *MapPath, bUseSeamlessTravel ? 1 : 0);
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Cyan, FString::Printf(TEXT("切图到对局: %s"), *MapPath));
	GetWorld()->ServerTravel(MapPath);
	return true;
}
