#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PartySubsystem.generated.h"

class AController;
class APlayerController;
class UTexture2D;

/** EOS room on the host's range. Steam and Epic invites join that same room. */
UCLASS()
class FPS_API UPartySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void HostRoom();
	void LeaveRoom();
	void OpenInvite();

	bool IsInRoom() const { return bInRoom; }
	bool IsRoomHost() const { return bIsHost; }
	FString GetStatusText() const { return StatusText; }
	FString GetLocalDisplayName() const;
	FString GetLocalPlatformId() const;
	UTexture2D* GetAvatar(const FString& PlatformId);

	/** True when this disconnect belongs to the party flow and the caller should not open the main menu. */
	bool HandleNetworkFailure();
	bool HandleTravelFailure();

	void RegisterJoinedPlayer(APlayerController* Joining);
	void UnregisterExitingPlayer(AController* Exiting);

private:
	void StartLogin();
	void UseEosDriver() const;
	void OpenOwnRange();
	void EnterOwnRangeIfNeeded();
	void BindOnlineDelegates();
	void BeginListenTravel();
	void StartListenSession();
	void PublishSteamLobby();
	void JoinFromEosResult(const FOnlineSessionSearchResult& Result);
	void JoinEosSessionById(const FString& EosSessionId);

	void OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnEosInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
	void OnSteamInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
	void OnSteamSessionCreated(FName SessionName, bool bWasSuccessful);
	void OnFoundInviteSession(int32 LocalUserNum, bool bWasSuccessful, const FOnlineSessionSearchResult& Result);
	void OnMapLoaded(UWorld* LoadedWorld);

	FDelegateHandle LoginHandle;
	FDelegateHandle CreateHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle EosInviteHandle;
	FDelegateHandle SteamInviteHandle;
	FDelegateHandle SteamCreateHandle;
	FDelegateHandle MapHandle;

	bool bDelegatesBound = false;
	bool bLoggedIn = false;
	bool bInRoom = false;
	bool bIsHost = false;
	bool bReturningHome = false;
	bool bJoinInProgress = false;
	bool bOpenInviteAfterTravel = false;
	bool bHostAfterLogin = false;
	bool bSteamLobbyPublished = false;
	FString StatusText;
	FString PendingEosSessionId;

	UPROPERTY()
	TMap<FString, TObjectPtr<UTexture2D>> Avatars;
};
