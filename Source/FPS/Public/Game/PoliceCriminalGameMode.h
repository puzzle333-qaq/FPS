#pragma once

#include "CoreMinimal.h"
#include "ShooterGameModeBase.h"
#include "Game/PoliceCriminalTypes.h"
class UWorld;
#include "PoliceCriminalGameMode.generated.h"

class AWeapon;
class APoliceCriminalGameState;

/**
 * 警匪战术模式骨架：
 * - 选警察/匪（控制台 PickFaction）
 * - MVP：匪 AK+手枪+燃烧瓶/手雷；警 RAR+X24+闪光/手雷；无刀
 * - 匪：面包车固定配装（VanPick），不可改装
 * - 警：军械库可改装（ArmoryPick + 改枪台预设），默认轻甲；无甲匪移速更快
 * - 匪布置时间结束后警察才能突击
 * - 本批默认剿匪：全灭匪徒警察胜，时间到匪胜；一条命
 */
UCLASS()
class FPS_API APoliceCriminalGameMode : public AShooterGameModeBase
{
	GENERATED_BODY()

public:
	APoliceCriminalGameMode();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal")
	EPoliceCriminalMatchType MatchType = EPoliceCriminalMatchType::Elimination;

	/** Criminal setup seconds before police can assault. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal", meta = (ClampMin = "5"))
	float CriminalSetupDuration = 60.f;

	/** Assault / elimination duration (seconds). Elimination default 10 min. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal", meta = (ClampMin = "30"))
	float AssaultDuration = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal")
	float WarmupTime = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal")
	float PostMatchDelay = 8.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal")
	bool bFriendlyFire = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal")
	FName PoliceStartTag = TEXT("TeamA");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal")
	FName CriminalStartTag = TEXT("TeamB");

	/** MvpRound: alternate PlayerStart tags also accepted (Police / Criminal). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal")
	FName PoliceStartTagAlt = TEXT("Police");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal")
	FName CriminalStartTagAlt = TEXT("Criminal");

	/** Team that wins when assault timer runs out (1=Criminal, 0=Police). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal")
	uint8 TimeoutWinningTeam = 1;

	/** Seconds after death before switching to spectate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal")
	float SpectateDelay = 3.f;

	/** After PostMatchDelay: true = ServerTravel ?restart same map; false = room lobby / main menu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal")
	bool bRestartMapAfterMatch = false;

	/** 面包车/军械库关卡切进对局图时用。软引用填对局 umap；空着时 TravelToMatch 命令必须带地图名。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal|Travel")
	TSoftObjectPtr<UWorld> MatchMap;

	/** ServerTravel 进对局图时是否 Seamless（保留 PlayerState/队伍）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal|Travel")
	bool bSeamlessTravelToMatch = true;

	/** Soft paths filled in ctor; override on BP. Criminal: fixed, no mods. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal|Catalog")
	TArray<TSoftClassPtr<AWeapon>> CriminalVanWeapons;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal|Catalog")
	TArray<TSoftClassPtr<AWeapon>> PoliceArmoryWeapons;

	/** Soft paths; prefer BP children. Native /Script/FPS.WeaponFlash|WeaponMolotov used as fallback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal|Catalog")
	TArray<TSoftClassPtr<AWeapon>> PoliceThrowables;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal|Catalog")
	TArray<TSoftClassPtr<AWeapon>> CriminalThrowables;

	/** Deprecated MVP: knife removed. Kept soft for old BP overrides; ignored when empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal|Catalog")
	TSoftClassPtr<AWeapon> SharedMeleeWeapon;

	/** Police default armor on MVP loadout (1=Light). Criminals stay None. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal|Catalog", meta = (ClampMin = "0", ClampMax = "2"))
	int32 PoliceDefaultArmorLevel = 1;

	virtual void InitGameState() override;
	virtual void StartPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual bool CanDamage(AController* InstigatorController, AController* VictimController) const override;

	/** Server: assign faction/team + auto FactionLook. */
	bool Auth_AssignFaction(APlayerController* PC, EPoliceCriminalFaction Faction);

	/** Server: criminal fixed loadout (no attachments). */
	bool Auth_ApplyVanLoadout(APlayerController* PC, TSubclassOf<AWeapon> PrimaryOrPistol);

	/** Server: police armory pick (attachments from GameInstance gunsmith save when available). */
	bool Auth_ApplyArmoryLoadout(APlayerController* PC, TSubclassOf<AWeapon> WeaponClass, bool bAllowAttachments);

	void Auth_SkipSetup();

	/** Debug (MvpRound): force winner / set remaining assault time / kill a controller. */
	void Auth_DebugForceWin(uint8 WinningTeam);
	void Auth_DebugSetRemaining(float Seconds);
	void Auth_DebugKill(AController* Controller);

	/** Server: van/armory map -> match map (ServerTravel). MapOverride empty = use MatchMap. */
	bool Auth_TravelToMatchMap(const FString& MapOverride = FString());

	UFUNCTION(BlueprintPure, Category = "FPS|PoliceCriminal")
	TArray<TSubclassOf<AWeapon>> GetResolvedCriminalWeapons() const { return ResolvedCriminalWeapons; }

	UFUNCTION(BlueprintPure, Category = "FPS|PoliceCriminal")
	TArray<TSubclassOf<AWeapon>> GetResolvedPoliceWeapons() const { return ResolvedPoliceWeapons; }

	UFUNCTION(BlueprintPure, Category = "FPS|PoliceCriminal")
	TSubclassOf<AWeapon> GetResolvedMelee() const { return ResolvedMelee; }

	UFUNCTION(BlueprintPure, Category = "FPS|PoliceCriminal")
	TArray<TSubclassOf<AWeapon>> GetResolvedPoliceThrowables() const { return ResolvedPoliceThrowables; }

	UFUNCTION(BlueprintPure, Category = "FPS|PoliceCriminal")
	TArray<TSubclassOf<AWeapon>> GetResolvedCriminalThrowables() const { return ResolvedCriminalThrowables; }

	/** Server: give full MVP kit for current faction (guns + throwables, no knife). */
	bool Auth_ApplyMvpLoadout(APlayerController* PC);

protected:
	virtual void ScoreKill(AShooterPlayerState* KillerPS, AShooterPlayerState* VictimPS) override;
	virtual bool CanRespawn(AController* Controller) const override;
	virtual void HandleTeamForfeit(uint8 WinningTeam) override;
	virtual void OnPlayerKilled(AController* KillerController, AController* VictimController) override;

private:
	void ResolveWeaponCatalogs();
	void ApplyMatchRules();
	void BeginWaitingOrSetup();
	void StartCriminalSetup();
	void StartAssault();
	void TickPhaseTimers();
	void CheckEliminationWin();
	void EndMatch(uint8 WinningTeam, EPoliceCriminalWinReason Reason);
	void ReturnAllToMainMenu();
	void ScheduleSpectate(AController* Controller, float Delay);
	void ValidateSpectators();
	void SaveLoadoutForTravel(APlayerController* PC);
	void RestoreLoadoutAfterTravel(APlayerController* PC);
	void ApplyFactionLookForController(AController* Controller);
	AActor* FindTaggedStart(FName Tag) const;

	UPROPERTY(Transient)
	TArray<TSubclassOf<AWeapon>> ResolvedCriminalWeapons;

	UPROPERTY(Transient)
	TArray<TSubclassOf<AWeapon>> ResolvedPoliceWeapons;

	UPROPERTY(Transient)
	TSubclassOf<AWeapon> ResolvedMelee;

	UPROPERTY(Transient)
	TArray<TSubclassOf<AWeapon>> ResolvedPoliceThrowables;

	UPROPERTY(Transient)
	TArray<TSubclassOf<AWeapon>> ResolvedCriminalThrowables;

	FTimerHandle WarmupTimer;
	FTimerHandle PhaseTimer;
	FTimerHandle PostMatchTimer;
	bool bFlowStarted = false;
};
