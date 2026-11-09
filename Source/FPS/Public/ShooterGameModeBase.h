#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterGameModeBase.generated.h"

class AShooterPlayerState;
class UTexture2D;

// Shared match rules for the firing range and coop CQB: damage, kills, respawn.
UCLASS()
class FPS_API AShooterGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShooterGameModeBase();

	// Seconds after death before respawn. <= 0 disables auto respawn.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Match")
	float RespawnDelay = 5.f;

	virtual bool CanDamage(AController* InstigatorController, AController* VictimController) const;
	virtual void OnPlayerKilled(AController* KillerController, AController* VictimController);
	void NotifyDamage(AController* InstigatorController, AController* VictimController);

	void SetPendingDamageIcon(UTexture2D* Icon) { PendingDamageIcon = Icon; bHasPendingDamageIcon = true; }
	void ClearPendingDamageIcon() { PendingDamageIcon = nullptr; bHasPendingDamageIcon = false; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Match")
	float AssistWindow = 10.f;

	UFUNCTION(BlueprintCallable, Category = "FPS|Match")
	void RespawnPlayer(AController* Controller, bool bForce = false);

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

protected:
	virtual void ScoreKill(AShooterPlayerState* KillerPS, AShooterPlayerState* VictimPS) {}
	virtual bool CanRespawn(AController* Controller) const { return true; }

	TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimers;

	TMap<TWeakObjectPtr<AController>, TMap<TWeakObjectPtr<AController>, float>> RecentDamage;
	void ClearRespawnTimers();
	void AssignShortPlayerName(APlayerController* NewPlayer);

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PendingDamageIcon;
	bool bHasPendingDamageIcon = false;
};
