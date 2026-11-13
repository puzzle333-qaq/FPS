#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Loadout/LoadoutTypes.h"
#include "ShooterGameInstance.generated.h"

// Travel and listen-host for solo or coop. Rooms and the gunsmith save are gone; those screens will be rebuilt.
UCLASS()
class FPS_API UShooterGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UShooterGameInstance();
	virtual void Init() override;
	virtual void Shutdown() override;

	UFUNCTION(BlueprintPure, Category = "FPS|Online")
	FString GetLastOnlineError() const { return LastOnlineError; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Online")
	void ClearLastOnlineError() { LastOnlineError.Reset(); }

	UFUNCTION(BlueprintCallable, Category = "FPS|Online")
	void SetLastOnlineError(const FString& Error);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Mode")
	TMap<EShooterGameMode, FShooterModeInfo> Modes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Mode")
	TSoftObjectPtr<UWorld> RangeMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Mode")
	TSoftClassPtr<AGameModeBase> RangeGameMode;

	// RangeGameMode when it is set, otherwise the coop mode that understands the range hub.
	TSoftClassPtr<AGameModeBase> GetRangeGameMode() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Mode")
	TSoftObjectPtr<UWorld> MainMenuMap;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Mode")
	EShooterGameMode SelectedMode = EShooterGameMode::CoopCQB;

	UFUNCTION(BlueprintCallable, Category = "FPS|Mode")
	void SelectMode(EShooterGameMode Mode) { SelectedMode = Mode; }

	UFUNCTION(BlueprintPure, Category = "FPS|Mode")
	bool GetModeInfo(EShooterGameMode Mode, FShooterModeInfo& OutInfo) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Mode")
	bool IsModeAvailable(EShooterGameMode Mode) const;

	UFUNCTION(BlueprintCallable, Category = "FPS|Mode")
	bool JoinByIP(const FString& Address);

	UFUNCTION(BlueprintCallable, Category = "FPS|Mode")
	bool OpenRange();

	// Solo: open the mode map with no listen server.
	UFUNCTION(BlueprintCallable, Category = "FPS|Mode")
	bool StartPractice(EShooterGameMode Mode);

	UFUNCTION(BlueprintPure, Category = "FPS|Mode")
	bool IsPracticeMode() const { return bPracticeMode; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Mode")
	bool GoToMainMenu();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Online")
	FString LocalPlayerName;

	// Force Null/IpNetDriver for a local listen test, then open range or coop.
	// Target: range | coop
	UFUNCTION(BlueprintCallable, Category = "FPS|LocalMP")
	bool HostLocalListen(FString Target);

	UFUNCTION(BlueprintCallable, Category = "FPS|LocalMP")
	bool JoinLocalhost(FString Address);

	UFUNCTION(BlueprintCallable, Category = "FPS|LocalMP")
	void EnsureLocalLanMode();

	FString GetPlayerNameOption() const { return GetNameOption(); }

	// Authority-only travel preserving the connected squad. Local menu travel continues to use TravelTo.
 bool TravelSquadTo(const TSoftObjectPtr<UWorld>& Map, const TSoftClassPtr<AGameModeBase>& GameMode, const TArray<FString>& ExtraOptions = TArray<FString>());

 bool TravelTo(const TSoftObjectPtr<UWorld>& Map, const TSoftClassPtr<AGameModeBase>& GameMode, bool bListen, const TArray<FString>& ExtraOptions = TArray<FString>());

private:
	bool bPracticeMode = false;
	FString LastOnlineError;
	FDelegateHandle NetFailHandle;
	FDelegateHandle TravelFailHandle;

	void HandleNetworkFailure(UWorld* World, class UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	FString GetNameOption() const;
};
