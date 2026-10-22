#include "Online/ShooterSessionSubsystem.h"

#include "ShooterGameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

const FName UShooterSessionSubsystem::KeyRoomCode(TEXT("ROOMCODE"));
const FName UShooterSessionSubsystem::KeyMode(TEXT("ROOMMODE"));
const FName UShooterSessionSubsystem::KeyPlayers(TEXT("ROOMPLAYERS"));
const FName UShooterSessionSubsystem::KeyHost(TEXT("ROOMHOST"));
const FName UShooterSessionSubsystem::KeyInGame(TEXT("ROOMINGAME"));

IOnlineSessionPtr UShooterSessionSubsystem::GetSessions() const
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	return OSS ? OSS->GetSessionInterface() : nullptr;
}

FString UShooterSessionSubsystem::GetOnlineServiceName() const
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	return OSS ? OSS->GetSubsystemName().ToString() : TEXT("None");
}

bool UShooterSessionSubsystem::IsLAN() const
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	return !OSS || OSS->GetSubsystemName() == NULL_SUBSYSTEM;
}

void UShooterSessionSubsystem::Deinitialize()
{
	if (IOnlineSessionPtr S = GetSessions())
	{
		S->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		S->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		S->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		S->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
	Super::Deinitialize();
}

void UShooterSessionSubsystem::FillSettings(FOnlineSessionSettings& S, const FString& RoomCode, EShooterGameMode Mode, int32 Players, int32 MaxPlayers, const FString& HostName, bool bInGame) const
{
	S.bIsLANMatch = IsLAN();
	S.NumPublicConnections = FMath::Max(1, MaxPlayers);
	S.NumPrivateConnections = 0;
	S.bShouldAdvertise = true;
	S.bAllowJoinInProgress = true;
	S.bAllowInvites = true;
	S.bUsesPresence = false;          // Steam 以后改成 true + bUseLobbiesIfAvailable
	S.bAllowJoinViaPresence = false;
	S.bIsDedicated = IsRunningDedicatedServer();
	S.Set(KeyRoomCode, RoomCode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	S.Set(KeyMode, (int32)Mode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	S.Set(KeyPlayers, Players, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	S.Set(KeyHost, HostName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	S.Set(KeyInGame, bInGame ? 1 : 0, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
}

void UShooterSessionSubsystem::AdvertiseRoom(const FString& RoomCode, EShooterGameMode Mode, int32 Players, int32 MaxPlayers, const FString& HostName, bool bInGame)
{
	IOnlineSessionPtr S = GetSessions();
	if (!S.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Session] No online subsystem; room %s not advertised"), *RoomCode);
		return;
	}
	FOnlineSessionSettings Settings;
	FillSettings(Settings, RoomCode, Mode, Players, MaxPlayers, HostName, bInGame);
	if (S->GetNamedSession(NAME_GameSession))
	{
		S->UpdateSession(NAME_GameSession, Settings, true);
		return;
	}
	if (bCreating) return;
	bCreating = true;
	S->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	CreateHandle = S->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleCreateComplete));
	UE_LOG(LogTemp, Log, TEXT("[Session] Create %s (%s, LAN=%d, dedicated=%d)"), *RoomCode, *GetOnlineServiceName(), Settings.bIsLANMatch ? 1 : 0, Settings.bIsDedicated ? 1 : 0);
	if (!S->CreateSession(0, NAME_GameSession, Settings))
	{
		bCreating = false;
		UE_LOG(LogTemp, Warning, TEXT("[Session] CreateSession call failed"));
	}
}

void UShooterSessionSubsystem::HandleCreateComplete(FName SessionName, bool bOk)
{
	bCreating = false;
	UE_LOG(LogTemp, Log, TEXT("[Session] Create complete ok=%d"), bOk ? 1 : 0);
	if (IOnlineSessionPtr S = GetSessions())
	{
		S->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		if (bOk) S->StartSession(SessionName);
	}
}

void UShooterSessionSubsystem::DestroyRoomSession()
{
	IOnlineSessionPtr S = GetSessions();
	if (!S.IsValid() || !S->GetNamedSession(NAME_GameSession)) return;
	S->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	DestroyHandle = S->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroyComplete));
	S->DestroySession(NAME_GameSession);
}

void UShooterSessionSubsystem::HandleDestroyComplete(FName SessionName, bool bOk)
{
	if (IOnlineSessionPtr S = GetSessions()) S->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	UE_LOG(LogTemp, Log, TEXT("[Session] Destroyed ok=%d"), bOk ? 1 : 0);
	if (PendingJoinIndex != INDEX_NONE)
	{
		const int32 Idx = PendingJoinIndex;
		PendingJoinIndex = INDEX_NONE;
		DoJoin(Idx);
	}
}

void UShooterSessionSubsystem::FindRooms(EShooterGameMode Mode, bool bAnyMode)
{
	IOnlineSessionPtr S = GetSessions();
	if (!S.IsValid())
	{
		OnRoomsFound.Broadcast(false, TArray<FRoomBrowserEntry>());
		return;
	}
	if (bSearching) return;
	SearchMode = Mode;
	bSearchAnyMode = bAnyMode;
	Search = MakeShared<FOnlineSessionSearch>();
	Search->bIsLanQuery = IsLAN();
	Search->MaxSearchResults = 100;
	Search->PingBucketSize = 50;
	S->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	FindHandle = S->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::HandleFindComplete));
	bSearching = true;
	UE_LOG(LogTemp, Log, TEXT("[Session] FindRooms mode=%d any=%d (%s)"), (int32)Mode, bAnyMode ? 1 : 0, *GetOnlineServiceName());
	if (!S->FindSessions(0, Search.ToSharedRef()))
	{
		bSearching = false;
		OnRoomsFound.Broadcast(false, TArray<FRoomBrowserEntry>());
	}
}

void UShooterSessionSubsystem::HandleFindComplete(bool bOk)
{
	bSearching = false;
	if (IOnlineSessionPtr S = GetSessions()) S->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	LastRooms.Reset();
	if (Search.IsValid())
	{
		for (int32 i = 0; i < Search->SearchResults.Num(); ++i)
		{
			const FOnlineSessionSearchResult& R = Search->SearchResults[i];
			FRoomBrowserEntry E;
			E.ResultIndex = i;
			int32 ModeInt = 0, Players = 0, InGame = 0;
			R.Session.SessionSettings.Get(KeyRoomCode, E.RoomCode);
			R.Session.SessionSettings.Get(KeyMode, ModeInt);
			R.Session.SessionSettings.Get(KeyPlayers, Players);
			R.Session.SessionSettings.Get(KeyHost, E.HostName);
			R.Session.SessionSettings.Get(KeyInGame, InGame);
			if (E.RoomCode.IsEmpty()) continue; // 不是我们的房间
			E.Mode = static_cast<EShooterGameMode>(ModeInt);
			E.Players = Players;
			E.MaxPlayers = R.Session.SessionSettings.NumPublicConnections;
			E.PingMs = R.PingInMs;
			E.bInGame = InGame != 0;
			E.bDedicated = R.Session.SessionSettings.bIsDedicated;
			if (E.HostName.IsEmpty()) E.HostName = R.Session.OwningUserName;
			if (!bSearchAnyMode && E.Mode != SearchMode) continue;
			LastRooms.Add(E);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Session] Found %d rooms (ok=%d)"), LastRooms.Num(), bOk ? 1 : 0);

	if (!PendingJoinCode.IsEmpty())
	{
		const FString Code = PendingJoinCode;
		PendingJoinCode.Reset();
		for (const FRoomBrowserEntry& E : LastRooms)
		{
			if (E.RoomCode.Equals(Code, ESearchCase::IgnoreCase))
			{
				JoinRoom(E.ResultIndex);
				return;
			}
		}
		FailJoin(FText::Format(NSLOCTEXT("Session", "CodeNotFound", "找不到房间 {0}"), FText::FromString(Code)));
		return;
	}
	OnRoomsFound.Broadcast(bOk, LastRooms);
}

void UShooterSessionSubsystem::JoinByCode(const FString& RoomCode)
{
	const FString Code = RoomCode.TrimStartAndEnd().ToUpper();
	if (Code.IsEmpty())
	{
		FailJoin(NSLOCTEXT("Session", "EmptyCode", "请输入房间号"));
		return;
	}
	PendingJoinCode = Code;
	FindRooms(SearchMode, true);
}

bool UShooterSessionSubsystem::JoinRoom(int32 ResultIndex)
{
	if (!Search.IsValid() || !Search->SearchResults.IsValidIndex(ResultIndex))
	{
		FailJoin(NSLOCTEXT("Session", "BadIndex", "房间已失效，请刷新"));
		return false;
	}
	IOnlineSessionPtr S = GetSessions();
	if (!S.IsValid()) return false;
	// 本地还挂着旧会话（比如以前当过房主）先销毁
	if (S->GetNamedSession(NAME_GameSession))
	{
		PendingJoinIndex = ResultIndex;
		DestroyRoomSession();
		return true;
	}
	DoJoin(ResultIndex);
	return true;
}

void UShooterSessionSubsystem::DoJoin(int32 ResultIndex)
{
	IOnlineSessionPtr S = GetSessions();
	if (!S.IsValid() || !Search.IsValid() || !Search->SearchResults.IsValidIndex(ResultIndex)) return;
	S->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	JoinHandle = S->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleJoinComplete));
	if (!S->JoinSession(0, NAME_GameSession, Search->SearchResults[ResultIndex]))
	{
		FailJoin(NSLOCTEXT("Session", "JoinCallFail", "加入失败"));
	}
}

void UShooterSessionSubsystem::HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr S = GetSessions();
	if (S.IsValid()) S->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	if (Result != EOnJoinSessionCompleteResult::Success || !S.IsValid())
	{
		FailJoin(FText::Format(NSLOCTEXT("Session", "JoinFail", "加入失败（{0}）"), FText::AsNumber((int32)Result)));
		return;
	}
	FString URL;
	if (!S->GetResolvedConnectString(SessionName, URL))
	{
		FailJoin(NSLOCTEXT("Session", "NoAddr", "拿不到房间地址"));
		return;
	}
	UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance());
	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
	if (!PC)
	{
		FailJoin(NSLOCTEXT("Session", "NoPC", "本地玩家无效"));
		return;
	}
	URL += GI ? GI->GetNameOption() : FString();
	UE_LOG(LogTemp, Log, TEXT("[Session] Joined -> travel %s"), *URL);
	PC->ClientTravel(URL, TRAVEL_Absolute);
}

void UShooterSessionSubsystem::FailJoin(const FText& Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("[Session] %s"), *Reason.ToString());
	OnJoinFailed.Broadcast(Reason);
}
