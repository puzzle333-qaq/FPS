#pragma once

#include "CoreMinimal.h"
#include "ShooterGameModeBase.h"
#include "Game/PoliceCriminalTypes.h"
#include "Ui/BuyShopTypes.h"
#include "PoliceCriminalGameMode.generated.h"

class AWeapon;
class APoliceCriminalGameState;

/**
 * 警匪战术模式骨架：
 * - 选警察/匪（控制台 PickFaction）
 * - 匪：面包车固定配装（VanPick），不可改装
 * - 警：军械库可改装（ArmoryPick + 改枪台预设），护甲/热成像夜视
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

	/** Soft paths filled in ctor; override on BP. Criminal: fixed, no mods. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal|Catalog")
	TArray<TSoftClassPtr<AWeapon>> CriminalVanWeapons;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal|Catalog")
	TArray<TSoftClassPtr<AWeapon>> PoliceArmoryWeapons;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PoliceCriminal|Catalog")
	TSoftClassPtr<AWeapon> SharedMeleeWeapon;

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

	UFUNCTION(BlueprintPure, Category = "FPS|PoliceCriminal")
	TArray<TSubclassOf<AWeapon>> GetResolvedCriminalWeapons() const { return ResolvedCriminalWeapons; }

	UFUNCTION(BlueprintPure, Category = "FPS|PoliceCriminal")
	TArray<TSubclassOf<AWeapon>> GetResolvedPoliceWeapons() const { return ResolvedPoliceWeapons; }

	UFUNCTION(BlueprintPure, Category = "FPS|PoliceCriminal")
	TSubclassOf<AWeapon> GetResolvedMelee() const { return ResolvedMelee; }

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
	void EndMatch(uint8 WinningTeam);
	void ReturnAllToMainMenu();
	void ApplyFactionLookForController(AController* Controller);
	AActor* FindTaggedStart(FName Tag) const;

	UPROPERTY(Transient)
	TArray<TSubclassOf<AWeapon>> ResolvedCriminalWeapons;

	UPROPERTY(Transient)
	TArray<TSubclassOf<AWeapon>> ResolvedPoliceWeapons;

	UPROPERTY(Transient)
	TSubclassOf<AWeapon> ResolvedMelee;

	FTimerHandle WarmupTimer;
	FTimerHandle PhaseTimer;
	FTimerHandle PostMatchTimer;
	bool bFlowStarted = false;
};
