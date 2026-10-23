#include "Online/ShooterSessionSubsystem.h"

#include "ShooterGameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "IOnlineSubsystemEOS.h"
#include "IEOSSDKManager.h"
#include "eos_sdk.h"
#include "eos_connect.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

// EOS 登录方式：deviceid（默认，开发用）/ steam（需门户 Steam 身份提供方；480 无法配置）/ persistentauth / accountportal / developer（后三种要 bUseEAS=true）
bool GFPS_SessionJoinInProgress = false;

static TAutoConsoleVariable<FString> CVarEOSLoginType(
	TEXT("fps.EOSLoginType"),
	TEXT("deviceid"),
	TEXT("EOS login type: deviceid | steam | persistentauth | accountportal | developer"),
	ECVF_Default);

// 1 = force Null LAN sessions (ignore DefaultPlatformService=EOS). Console: fps.OnlineUseNull 1
static TAutoConsoleVariable<int32> CVarOnlineUseNull(
	TEXT("fps.OnlineUseNull"),
	0,
	TEXT("If 1, ShooterSessionSubsystem uses OnlineSubsystemNull (LAN) instead of the default OSS."),
	ECVF_Default);
const FName UShooterSessionSubsystem::KeyRoomCode(TEXT("ROOMCODE"));
const FName UShooterSessionSubsystem::KeyMode(TEXT("ROOMMODE"));
const FName UShooterSessionSubsystem::KeyPlayers(TEXT("ROOMPLAYERS"));
const FName UShooterSessionSubsystem::KeyHost(TEXT("ROOMHOST"));
const FName UShooterSessionSubsystem::KeyInGame(TEXT("ROOMINGAME"));

namespace
{
	static const FName KeyRoomMap(TEXT("ROOMMAP"));

	// 从 EOS:PUID 或 EOS:PUID/Map?... 里取出 ProductUserId
	FString ExtractEosProductUserId(const FString& ConnectOrId)
	{
		FString S = ConnectOrId;
		if (S.StartsWith(TEXT("EOS:"), ESearchCase::IgnoreCase))
		{
			S.RightChopInline(4);
		}
		// FUniqueNetIdEOS 有时是 "Epic|Product" 或 "Product|Epic"
		int32 Pipe = INDEX_NONE;
		if (S.FindChar(TEXT('|'), Pipe))
		{
			const FString A = S.Left(Pipe);
			const FString B = S.Mid(Pipe + 1);
			// ProductUserId 一般是 32 位十六进制
			S = (B.Len() == 32) ? B : ((A.Len() == 32) ? A : B);
		}
		int32 Slash = INDEX_NONE;
		if (S.FindChar(TEXT('/'), Slash))
		{
			S.LeftInline(Slash);
		}
		int32 Q = INDEX_NONE;
		if (S.FindChar(TEXT('?'), Q))
		{
			S.LeftInline(Q);
		}
		return S;
	}

	bool IsSameEosUser(const FString& A, const FString& B)
	{
		const FString X = ExtractEosProductUserId(A);
		const FString Y = ExtractEosProductUserId(B);
		return !X.IsEmpty() && !Y.IsEmpty() && X.Equals(Y, ESearchCase::IgnoreCase);
	}
}


IOnlineSessionPtr UShooterSessionSubsystem::GetSessions() const
{
	IOnlineSubsystem* OSS = nullptr;
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (CVarOnlineUseNull.GetValueOnGameThread() != 0)
	{
		OSS = IOnlineSubsystem::Get(NULL_SUBSYSTEM);
	}
	else
	{
		OSS = Online::GetSubsystem(World);
	}
	return OSS ? OSS->GetSessionInterface() : nullptr;
}

FString UShooterSessionSubsystem::GetOnlineServiceName() const
{
	if (CVarOnlineUseNull.GetValueOnGameThread() != 0)
	{
		return TEXT("NULL(forced)");
	}
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	return OSS ? OSS->GetSubsystemName().ToString() : TEXT("None");
}

bool UShooterSessionSubsystem::IsLAN() const
{
	if (CVarOnlineUseNull.GetValueOnGameThread() != 0)
	{
		return true;
	}
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


// EOS 非 Presence 会话搜索结果里，自定义 int 属性经常读不到（默认 0），
// 但 FString 属性（如 ROOMCODE）能回来。模式/人数一律用字符串广告，读取时兼容旧 int。
static bool GetSessionIntAttr(const FOnlineSessionSettings& Settings, const FName Key, int32& OutValue)
{
	FString AsStr;
	if (Settings.Get(Key, AsStr) && !AsStr.IsEmpty() && AsStr.IsNumeric())
	{
		OutValue = FCString::Atoi(*AsStr);
		return true;
	}
	return Settings.Get(Key, OutValue);
}

void UShooterSessionSubsystem::FillSettings(FOnlineSessionSettings& S, const FString& RoomCode, EShooterGameMode Mode, int32 Players, int32 MaxPlayers, const FString& HostName, bool bInGame) const
{
	S.bIsLANMatch = IsLAN();
	S.NumPublicConnections = FMath::Max(1, MaxPlayers);
	S.NumPrivateConnections = 0;
	S.bShouldAdvertise = true;
	S.bAllowJoinInProgress = true;
	S.bAllowInvites = true;
	// DeviceId（Connect）用户没有 Presence 广告权限；开 Presence/Lobby 会导致
	// “user lacks permission to advertise presence” 且房间搜不到。
	// 仅 Steam/EAS 类登录以后再开 Presence+Lobby。
	const FString LoginType = CVarEOSLoginType.GetValueOnGameThread().ToLower();
	const bool bPresenceOk = !IsLAN() && (
		LoginType == TEXT("steam") || LoginType == TEXT("accountportal") ||
		LoginType == TEXT("persistentauth") || LoginType == TEXT("developer"));
	S.bUsesPresence = bPresenceOk;
	S.bUseLobbiesIfAvailable = bPresenceOk;
	S.bAllowJoinViaPresence = bPresenceOk;
	S.bIsDedicated = IsRunningDedicatedServer();
	S.Set(KeyRoomCode, RoomCode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	// 字符串：EOS DeviceId/非 Presence 搜索能带回；int 版常丢失导致 Mode=0(团队竞技)、Players=0→10/10
	S.Set(KeyMode, FString::FromInt((int32)Mode), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	S.Set(KeyPlayers, FString::FromInt(Players), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	S.Set(KeyHost, HostName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	S.Set(KeyInGame, bInGame ? FString(TEXT("1")) : FString(TEXT("0")), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	if (const UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance()))
	{
		if (!GI->RoomLobbyMap.IsNull())
		{
			S.Set(KeyRoomMap, GI->RoomLobbyMap.ToSoftObjectPath().GetLongPackageName(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		}
	}
}

void UShooterSessionSubsystem::AdvertiseRoom(const FString& RoomCode, EShooterGameMode Mode, int32 Players, int32 MaxPlayers, const FString& HostName, bool bInGame)
{
	IOnlineSessionPtr S = GetSessions();
	if (!S.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Session] No online subsystem; room %s not advertised"), *RoomCode);
		return;
	}
	// 始终记下最新广告参数；Create 完成后再 Update，避免 bCreating 期间丢 Mode/Players
	const int32 AdvertisePlayers = FMath::Max(1, Players);
	bHasLastAdvertise = true;
	LastAdvCode = RoomCode;
	LastAdvMode = Mode;
	LastAdvPlayers = AdvertisePlayers;
	LastAdvMaxPlayers = MaxPlayers;
	LastAdvHost = HostName;
	LastAdvInGame = bInGame;
	// 创建未完成时 GetNamedSession 可能已有占位，Update 会刷屏 “still being created”
	if (bCreating)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Session] Advertise skipped — create in flight (queued mode=%d players=%d)"), (int32)Mode, AdvertisePlayers);
		return;
	}
	FOnlineSessionSettings Settings;
	FillSettings(Settings, RoomCode, Mode, AdvertisePlayers, MaxPlayers, HostName, bInGame);
	UE_LOG(LogTemp, Log, TEXT("[Session] Advertise %s mode=%d players=%d/%d inGame=%d"), *RoomCode, (int32)Mode, AdvertisePlayers, MaxPlayers, bInGame ? 1 : 0);
	if (S->GetNamedSession(NAME_GameSession))
	{
		S->UpdateSession(NAME_GameSession, Settings, true);
		return;
	}
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
		if (bOk)
		{
			S->StartSession(SessionName);
			// 登记本地房主，NumOpenPublicConnections 才会减少，列表才能显示 1/N
			if (UGameInstance* GI = GetGameInstance())
			{
				if (ULocalPlayer* LP = GI->GetFirstGamePlayer())
				{
					FUniqueNetIdRepl Id = LP->GetPreferredUniqueNetId();
					if (Id.IsValid())
					{
						S->RegisterPlayer(SessionName, *Id, false);
					}
				}
			}
			// Create 期间被跳过的 Advertise（含正确 ROOMMODE/人数）现在补一次 Update
			if (bHasLastAdvertise && S->GetNamedSession(SessionName))
			{
				FOnlineSessionSettings Settings;
				FillSettings(Settings, LastAdvCode, LastAdvMode, LastAdvPlayers, LastAdvMaxPlayers, LastAdvHost, LastAdvInGame);
				UE_LOG(LogTemp, Log, TEXT("[Session] Post-create Update %s mode=%d players=%d"), *LastAdvCode, (int32)LastAdvMode, LastAdvPlayers);
				S->UpdateSession(SessionName, Settings, true);
			}
		}
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
	if (!IsOnlineReady())
	{
		OnRoomsFound.Broadcast(false, TArray<FRoomBrowserEntry>());
		return;
	}
	// 回到房间列表刷新时，允许再次加入
	GFPS_SessionJoinInProgress = false;
	if (bSearching) return;
	SearchMode = Mode;
	bSearchAnyMode = bAnyMode;
	Search = MakeShared<FOnlineSessionSearch>();
	Search->bIsLanQuery = IsLAN();
	Search->MaxSearchResults = 100;
	Search->PingBucketSize = 50;
	// DeviceId 走普通 Session（非 Lobby）；不要 SEARCH_LOBBIES
	Search->QuerySettings.Set(SEARCH_LOBBIES, false, EOnlineComparisonOp::Equals);
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
			GetSessionIntAttr(R.Session.SessionSettings, KeyMode, ModeInt);
			GetSessionIntAttr(R.Session.SessionSettings, KeyPlayers, Players);
			R.Session.SessionSettings.Get(KeyHost, E.HostName);
			GetSessionIntAttr(R.Session.SessionSettings, KeyInGame, InGame);
			if (E.RoomCode.IsEmpty()) continue; // 不是我们的房间
			E.Mode = static_cast<EShooterGameMode>(ModeInt);
			E.MaxPlayers = R.Session.SessionSettings.NumPublicConnections;
			// 优先 ROOMPLAYERS。EOS 非 Presence 时常把 NumOpenPublicConnections 留成 0，
			// 若用 Max-Open 会误显示成 10/10 并把加入按钮灰掉。
			const int32 OpenSlots = R.Session.NumOpenPublicConnections;
			if (Players > 0)
			{
				E.Players = Players;
			}
			else if (OpenSlots > 0 && E.MaxPlayers > 0)
			{
				E.Players = FMath::Clamp(E.MaxPlayers - OpenSlots, 0, E.MaxPlayers);
			}
			else
			{
				// 属性缺失且 Open==0：按有房主显示 1，绝不要当成已满
				E.Players = 1;
			}
			if (E.MaxPlayers > 0)
			{
				E.Players = FMath::Clamp(E.Players, 0, E.MaxPlayers);
			}
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
	if (!IsOnlineReady())
	{
		FailJoin(FText::FromString(OnlineStatus.IsEmpty() ? TEXT("在线服务未登录") : OnlineStatus));
		return false;
	}
	if (GFPS_SessionJoinInProgress)
	{
		FailJoin(NSLOCTEXT("Session", "JoinBusy", "正在加入房间，请稍候"));
		return false;
	}
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
	if (GFPS_SessionJoinInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Session] DoJoin ignored — join already in progress"));
		return;
	}

	const FOnlineSessionSearchResult& Result = Search->SearchResults[ResultIndex];
	const FString LocalId = GetProductUserId();
	FString OwnerId;
	if (Result.Session.OwningUserId.IsValid())
	{
		OwnerId = Result.Session.OwningUserId->ToString();
	}
	if (!LocalId.IsEmpty() && !OwnerId.IsEmpty() && IsSameEosUser(LocalId, OwnerId))
	{
		FailJoin(NSLOCTEXT("Session", "SelfJoin", "不能加入自己的房间（两个窗口 ProductUserId 相同）。Number of Players=2 应自动分 primary/secondary；请看日志 [Online] EOS instance role="));
		return;
	}

	S->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	JoinHandle = S->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleJoinComplete));
	GFPS_SessionJoinInProgress = true;
	if (!S->JoinSession(0, NAME_GameSession, Result))
	{
		GFPS_SessionJoinInProgress = false;
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
	FString Connect;
	if (!S->GetResolvedConnectString(SessionName, Connect))
	{
		FailJoin(NSLOCTEXT("Session", "NoAddr", "拿不到房间地址"));
		return;
	}

	// 再次拦截：加入自己的会话（同一 DeviceId / ProductUserId）
	if (!ProductUserId.IsEmpty() && IsSameEosUser(ProductUserId, Connect))
	{
		FailJoin(NSLOCTEXT("Session", "SelfJoin", "不能加入自己的房间（两个窗口 ProductUserId 相同）。Number of Players=2 应自动分 primary/secondary；请看日志 [Online] EOS instance role="));
		S->DestroySession(NAME_GameSession);
		return;
	}

	UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance());
	APlayerController* PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (!PC)
	{
		FailJoin(NSLOCTEXT("Session", "NoPC", "本地玩家无效"));
		return;
	}
	if (!GI)
	{
		FailJoin(NSLOCTEXT("Session", "NoGI", "GameInstance 无效"));
		return;
	}

	// GetResolvedConnectString 只有 EOS:PUID，ClientTravel Absolute 会拼上「当前」地图（主菜单），
	// 房主其实在房间大厅 → 加入端没有 GameMode 然后崩溃。这里强制拼上 RoomLobby。
	FString MapPkg;
	if (FNamedOnlineSession* Named = S->GetNamedSession(SessionName))
	{
		Named->SessionSettings.Get(KeyRoomMap, MapPkg);
	}
	if (MapPkg.IsEmpty() && !GI->RoomLobbyMap.IsNull())
	{
		MapPkg = GI->RoomLobbyMap.ToSoftObjectPath().GetLongPackageName();
	}
	if (MapPkg.IsEmpty())
	{
		FailJoin(NSLOCTEXT("Session", "NoLobbyMap", "加入失败：BP_ShooterGameInstance 没填 RoomLobbyMap"));
		return;
	}

	int32 ModeInt = (int32)GI->SelectedMode;
	if (FNamedOnlineSession* Named = S->GetNamedSession(SessionName))
	{
		GetSessionIntAttr(Named->SessionSettings, KeyMode, ModeInt);
	}

	// EOS:PUID + /Game/.../Lvl_RoomLobby?game=...&Mode=...
	FString TravelURL = Connect;
	if (!MapPkg.StartsWith(TEXT("/")))
	{
		TravelURL += TEXT("/");
	}
	TravelURL += MapPkg;
	if (!GI->LobbyGameModeClass.IsNull())
	{
		TravelURL += FString::Printf(TEXT("?game=%s"), *GI->LobbyGameModeClass.ToSoftObjectPath().ToString());
		TravelURL += FString::Printf(TEXT("?Mode=%d"), ModeInt);
	}
	else
	{
		TravelURL += FString::Printf(TEXT("?Mode=%d"), ModeInt);
	}
	TravelURL += GI->GetNameOption(); // 空，或 ?Name=xxx

	UE_LOG(LogTemp, Log, TEXT("[Session] Joined -> travel %s (connect was %s)"), *TravelURL, *Connect);
	PC->ClientTravel(TravelURL, TRAVEL_Absolute);
	// 保持 joining，阻止连点加入 / 同时点创建房间；进厅或失败后再清
}

void UShooterSessionSubsystem::FailJoin(const FText& Reason)
{
	GFPS_SessionJoinInProgress = false;
	UE_LOG(LogTemp, Warning, TEXT("[Session] %s"), *Reason.ToString());
	if (UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance()))
	{
		GI->SetLastOnlineError(Reason.ToString());
	}
	OnJoinFailed.Broadcast(Reason);
}

// ======================= 在线登录 =======================

bool UShooterSessionSubsystem::IsUsingNullSessions()
{
	return CVarOnlineUseNull.GetValueOnGameThread() != 0;
}

bool UShooterSessionSubsystem::IsOnlineReady() const
{
	return IsUsingNullSessions() || bOnlineReady;
}

void UShooterSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (FParse::Param(FCommandLine::Get(), TEXT("OnlineUseNull")))
	{
		CVarOnlineUseNull->Set(1, ECVF_SetByCommandline);
	}
	FString TypeArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("EOSLoginType="), TypeArg))
	{
		IConsoleManager::Get().FindConsoleVariable(TEXT("fps.EOSLoginType"))->Set(*TypeArg, ECVF_SetByCommandline);
	}
	CVarOnlineUseNull.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&UShooterSessionSubsystem::OnUseNullChanged));
	ApplyNetDriverForMode();
	StartOnlineLogin();
}

void UShooterSessionSubsystem::OnUseNullChanged(IConsoleVariable* Var)
{
	// 切换 Null/EOS 时同步改 GameNetDriver（Null 必须用 IpNetDriver，EOS 用 NetDriverEOS）
	if (!GEngine) return;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
	{
		if (UGameInstance* GI = Ctx.OwningGameInstance)
		{
			if (UShooterSessionSubsystem* S = GI->GetSubsystem<UShooterSessionSubsystem>())
			{
				S->ApplyNetDriverForMode();
				S->StartOnlineLogin();
			}
		}
	}
}

void UShooterSessionSubsystem::ApplyNetDriverForMode()
{
	if (!GEngine) return;
	const bool bNull = IsUsingNullSessions();
	const FName Ip(TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"));
	const FName Eos(TEXT("/Script/SocketSubsystemEOS.NetDriverEOS"));
	for (FNetDriverDefinition& Def : GEngine->NetDriverDefinitions)
	{
		if (Def.DefName == NAME_GameNetDriver)
		{
			Def.DriverClassName = bNull ? Ip : Eos;
			Def.DriverClassNameFallback = Ip;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Online] GameNetDriver=%s (Null=%d)"), bNull ? TEXT("IpNetDriver") : TEXT("NetDriverEOS"), bNull ? 1 : 0);
}

void UShooterSessionSubsystem::SetOnlineState(bool bReady, const FString& Status)
{
	bOnlineReady = bReady;
	OnlineStatus = Status;
	OnOnlineLoginChanged.Broadcast(IsOnlineReady(), Status);
}

FString UShooterSessionSubsystem::GetLoginDisplayName() const
{
	FString Name;
	if (IOnlineSubsystem* Steam = IOnlineSubsystem::Get(STEAM_SUBSYSTEM))
	{
		if (IOnlineIdentityPtr Id = Steam->GetIdentityInterface()) Name = Id->GetPlayerNickname(0);
	}
	if (Name.IsEmpty()) Name = FPlatformProcess::UserName();
	if (Name.IsEmpty()) Name = FPlatformProcess::ComputerName();
	// DisplayName 带 PID，双窗口更好区分（真正的用户隔离靠互斥量 + secondary Delete/Create）
	Name += FString::Printf(TEXT("_%u"), FPlatformProcess::GetCurrentProcessId() % 100000);
	return Name.Left(32);
}

void UShooterSessionSubsystem::RetryOnlineLogin()
{
	bLoggingIn = false;
	StartOnlineLogin();
}

void UShooterSessionSubsystem::StartOnlineLogin()
{
	if (IsUsingNullSessions())
	{
		bLoggingIn = false;
		SetOnlineState(true, TEXT("局域网模式（Null）"));
		return;
	}
	if (bLoggingIn) return;

	IOnlineSubsystem* EOS = IOnlineSubsystem::Get(EOS_SUBSYSTEM);
	IOnlineIdentityPtr Identity = EOS ? EOS->GetIdentityInterface() : nullptr;
	if (!Identity.IsValid())
	{
		SetOnlineState(false, TEXT("登录失败: 没有 EOS 在线服务"));
		UE_LOG(LogTemp, Warning, TEXT("[Online] EOS login failed: EOS OSS missing"));
		return;
	}
	if (Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
	{
		FUniqueNetIdPtr Id = Identity->GetUniquePlayerId(0);
		ProductUserId = Id.IsValid() ? Id->ToString() : FString();
		SetOnlineState(true, TEXT("已登录"));
		return;
	}

	bLoggingIn = true;
	SetOnlineState(false, TEXT("正在连接在线服务…"));
	const FString Type = CVarEOSLoginType.GetValueOnGameThread().ToLower();
	UE_LOG(LogTemp, Log, TEXT("[Online] EOS login start type=%s"), *Type);
	if (Type == TEXT("deviceid"))
	{
		UE_LOG(LogTemp, Log, TEXT("[Online] DeviceId：命名互斥量区分本机多窗口（Number of Players=2 同参数即可）"));
		UE_LOG(LogTemp, Log, TEXT("[Online]   primary=沿用本机 DeviceId；secondary=Delete+Create(含PID) 得新 ProductUserId"));
		UE_LOG(LogTemp, Log, TEXT("[Online]   -EOSFreshDevice 已可选（双开自动认 secondary）"));
		EnsureDeviceIdThenLogin(false);
	}
	else
	{
		LoginWithType(Type);
	}
}

namespace
{
	struct FDeviceIdCtx
	{
		TWeakObjectPtr<UShooterSessionSubsystem> Owner;
		EOS_HConnect Connect = nullptr;
	};

	// EOS DeviceId：每 OS 用户本机只存一份；DeviceModel 只是元数据，不能靠它开多个 Id。
	// Number of Players=2（参数相同）→ 命名互斥量分角色：
	//   primary 持有 Local\FPS_EOS_Primary，CreateDeviceId（不删）；
	//   secondary 串行 DeleteDeviceId → CreateDeviceId(FPS-sec-<pid>) → Login。
	static void* GEosPrimaryMutex = nullptr;
	static void* GEosSecondaryOpMutex = nullptr;
	static void* GEosPrimaryLoginDoneEvent = nullptr; // 手动复位：primary 登录结束后 Set，secondary 才能 Delete
	static bool GEosRoleResolved = false;
	static bool GEosIsSecondary = false;
	static FString GEosInstanceRole(TEXT("primary"));

	void EOS_CALL OnCreateDeviceIdDone(const EOS_Connect_CreateDeviceIdCallbackInfo* Data);
	void EOS_CALL OnDeleteDeviceIdDone(const EOS_Connect_DeleteDeviceIdCallbackInfo* Data);

	void ReleaseEosSecondaryOpMutex()
	{
#if PLATFORM_WINDOWS
		if (GEosSecondaryOpMutex)
		{
			ReleaseMutex(static_cast<HANDLE>(GEosSecondaryOpMutex));
			CloseHandle(static_cast<HANDLE>(GEosSecondaryOpMutex));
			GEosSecondaryOpMutex = nullptr;
		}
#endif
	}

	// primary 启动时创建并保持非信号；登录结束（成功或失败）后 Set，secondary 才能 DeleteDeviceId
	void EnsureEosPrimaryLoginDoneEvent(bool bSignalNow)
	{
#if PLATFORM_WINDOWS
		if (!GEosPrimaryLoginDoneEvent)
		{
			// bManualReset=TRUE, bInitialState=FALSE
			GEosPrimaryLoginDoneEvent = CreateEventW(nullptr, 1, 0, L"Local\\FPS_EOS_PrimaryLoginDone");
			if (!GEosPrimaryLoginDoneEvent)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Online] PrimaryLoginDone event create failed (%lu)"), GetLastError());
				return;
			}
		}
		if (bSignalNow)
		{
			SetEvent(static_cast<HANDLE>(GEosPrimaryLoginDoneEvent));
			UE_LOG(LogTemp, Log, TEXT("[Online] PrimaryLoginDone signaled role=%s"), *GEosInstanceRole);
		}
		else
		{
			ResetEvent(static_cast<HANDLE>(GEosPrimaryLoginDoneEvent));
			UE_LOG(LogTemp, Log, TEXT("[Online] PrimaryLoginDone reset (primary starting login)"));
		}
#endif
	}

	void SignalEosPrimaryLoginDone()
	{
#if PLATFORM_WINDOWS
		EnsureEosPrimaryLoginDoneEvent(true);
#endif
	}

	bool WaitEosPrimaryLoginDone(uint32 WaitMs)
	{
#if PLATFORM_WINDOWS
		HANDLE Ev = CreateEventW(nullptr, 1, 0, L"Local\\FPS_EOS_PrimaryLoginDone");
		if (!Ev)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Online] PrimaryLoginDone open/create failed (%lu)"), GetLastError());
			return false;
		}
		const DWORD W = WaitForSingleObject(Ev, WaitMs);
		if (Ev != GEosPrimaryLoginDoneEvent) { CloseHandle(Ev); }
		if (W == WAIT_OBJECT_0)
		{
			UE_LOG(LogTemp, Log, TEXT("[Online] PrimaryLoginDone wait ok (secondary may Delete)"));
			return true;
		}
		UE_LOG(LogTemp, Warning, TEXT("[Online] PrimaryLoginDone wait timeout/fail code=%lu"), W);
		return false;
#else
		return true;
#endif
	}

	bool ResolveEosInstanceRole()
	{
		if (GEosRoleResolved)
		{
			return GEosIsSecondary;
		}
		GEosRoleResolved = true;
		const uint32 Pid = FPlatformProcess::GetCurrentProcessId();
#if PLATFORM_WINDOWS
		HANDLE Primary = CreateMutexW(nullptr, 0, L"Local\\FPS_EOS_Primary");
		if (Primary && WaitForSingleObject(Primary, 0) == WAIT_OBJECT_0)
		{
			GEosPrimaryMutex = Primary; // 进程结束才释放，保证第二窗是 secondary
			GEosIsSecondary = false;
			GEosInstanceRole = TEXT("primary");
		}
		else
		{
			if (Primary) { CloseHandle(Primary); }
			GEosIsSecondary = true;
			GEosInstanceRole = TEXT("secondary");
		}
#else
		GEosIsSecondary = FParse::Param(FCommandLine::Get(), TEXT("EOSFreshDevice"));
		GEosInstanceRole = GEosIsSecondary ? TEXT("secondary") : TEXT("primary");
#endif
		UE_LOG(LogTemp, Log, TEXT("[Online] EOS instance role=%s pid=%u"), *GEosInstanceRole, Pid);
		return GEosIsSecondary;
	}

	void AcquireEosSecondaryOpMutex()
	{
#if PLATFORM_WINDOWS
		GEosSecondaryOpMutex = CreateMutexW(nullptr, 0, L"Local\\FPS_EOS_SecondaryDeviceOp");
		if (!GEosSecondaryOpMutex)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Online] SecondaryDeviceOp mutex create failed (%lu)"), GetLastError());
			return;
		}
		const DWORD Wait = WaitForSingleObject(static_cast<HANDLE>(GEosSecondaryOpMutex), 60000);
		if (Wait != WAIT_OBJECT_0 && Wait != WAIT_ABANDONED)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Online] SecondaryDeviceOp mutex wait failed (%lu)"), Wait);
		}
#endif
	}

	EOS_HConnect GetConnectHandle()
	{
		IOnlineSubsystemEOS* EOS = static_cast<IOnlineSubsystemEOS*>(IOnlineSubsystem::Get(EOS_SUBSYSTEM));
		IEOSPlatformHandlePtr Platform = EOS ? EOS->GetEOSPlatformHandle() : nullptr;
		return Platform.IsValid() ? EOS_Platform_GetConnectInterface(*Platform) : nullptr;
	}

	void BeginCreateDeviceId(UShooterSessionSubsystem* Owner, EOS_HConnect Connect, bool bSecondaryModel)
	{
		FDeviceIdCtx* Ctx = new FDeviceIdCtx{ Owner, Connect };
		EOS_Connect_CreateDeviceIdOptions Opt = {};
		Opt.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
		FString Model = bSecondaryModel
			? FString::Printf(TEXT("FPS-sec-%u"), FPlatformProcess::GetCurrentProcessId())
			: FString::Printf(TEXT("PC-%s"), FPlatformProcess::ComputerName());
		Model.LeftInline(EOS_CONNECT_CREATEDEVICEID_DEVICEMODEL_MAX_LENGTH);
		const FTCHARToUTF8 ModelUtf8(*Model);
		Opt.DeviceModel = ModelUtf8.Get();
		UE_LOG(LogTemp, Log, TEXT("[Online] CreateDeviceId model=%s role=%s"), *Model, *GEosInstanceRole);
		EOS_Connect_CreateDeviceId(Connect, &Opt, Ctx, &OnCreateDeviceIdDone);
	}

	void EOS_CALL OnCreateDeviceIdDone(const EOS_Connect_CreateDeviceIdCallbackInfo* Data)
	{
		FDeviceIdCtx* Ctx = static_cast<FDeviceIdCtx*>(Data->ClientData);
		const EOS_EResult Code = Data->ResultCode;
		// 已有 DeviceId 时常见 Success / DuplicateNotAllowed / AccessDenied —— 都应继续 Connect Login
		const bool bProceed =
			Code == EOS_EResult::EOS_Success ||
			Code == EOS_EResult::EOS_DuplicateNotAllowed ||
			Code == EOS_EResult::EOS_AccessDenied;
		UE_LOG(LogTemp, Log, TEXT("[Online] CreateDeviceId done code=%s proceed=%d role=%s"),
			UTF8_TO_TCHAR(EOS_EResult_ToString(Code)), bProceed ? 1 : 0, *GEosInstanceRole);
		UShooterSessionSubsystem* S = Ctx->Owner.Get();
		delete Ctx;
		// secondary 的 Create 完成后即可放 SecondaryOp；PrimaryLoginDone 事件仍由 LoginComplete 发出
		ReleaseEosSecondaryOpMutex();
		if (S)
		{
			S->OnDeviceIdReady(bProceed, UTF8_TO_TCHAR(EOS_EResult_ToString(Code)));
		}
	}

	void EOS_CALL OnDeleteDeviceIdDone(const EOS_Connect_DeleteDeviceIdCallbackInfo* Data)
	{
		FDeviceIdCtx* Ctx = static_cast<FDeviceIdCtx*>(Data->ClientData);
		UE_LOG(LogTemp, Log, TEXT("[Online] DeleteDeviceId result=%s role=%s"),
			UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)), *GEosInstanceRole);
		UShooterSessionSubsystem* S = Ctx->Owner.Get();
		delete Ctx;
		if (S)
		{
			S->OnDeviceIdDeleted();
		}
		else
		{
			ReleaseEosSecondaryOpMutex();
		}
	}
}

void UShooterSessionSubsystem::EnsureDeviceIdThenLogin(bool /*bFreshIgnored*/)
{
	EOS_HConnect Connect = GetConnectHandle();
	if (!Connect)
	{
		bLoggingIn = false;
		SetOnlineState(false, TEXT("登录失败: EOS 平台未初始化（检查 Artifacts 配置）"));
		UE_LOG(LogTemp, Warning, TEXT("[Online] EOS login failed: no EOS platform/connect handle"));
		return;
	}

	const bool bSecondary = ResolveEosInstanceRole();
	if (bSecondary)
	{
		// 等 primary 登录完（事件）再 Delete，避免踩 primary 的 CreateDeviceId
		if (!WaitEosPrimaryLoginDone(30000))
		{
			bLoggingIn = false;
			SetOnlineState(false, TEXT("登录失败: 等待主窗口登录超时"));
			return;
		}
		bFreshDeviceDone = true;
		AcquireEosSecondaryOpMutex();
		FDeviceIdCtx* Ctx = new FDeviceIdCtx{ this, Connect };
		EOS_Connect_DeleteDeviceIdOptions Opt = {};
		Opt.ApiVersion = EOS_CONNECT_DELETEDEVICEID_API_LATEST;
		UE_LOG(LogTemp, Log, TEXT("[Online] secondary: DeleteDeviceId then Create (pid=%u)"), FPlatformProcess::GetCurrentProcessId());
		EOS_Connect_DeleteDeviceId(Connect, &Opt, Ctx, &OnDeleteDeviceIdDone);
		return;
	}

	// 先 Reset 事件，再 Create+Login；登录结束 Signal，secondary 才能继续
	EnsureEosPrimaryLoginDoneEvent(false);
	BeginCreateDeviceId(this, Connect, false);
}

void UShooterSessionSubsystem::OnDeviceIdDeleted()
{
	EOS_HConnect Connect = GetConnectHandle();
	if (!Connect)
	{
		ReleaseEosSecondaryOpMutex();
		bLoggingIn = false;
		SetOnlineState(false, TEXT("登录失败: EOS 平台未初始化"));
		return;
	}
	BeginCreateDeviceId(this, Connect, true);
}

void UShooterSessionSubsystem::OnDeviceIdReady(bool bOk, const FString& Info)
{
	if (!bOk)
	{
		ReleaseEosSecondaryOpMutex();
		SignalEosPrimaryLoginDone();
		bLoggingIn = false;
		SetOnlineState(false, FString::Printf(TEXT("登录失败: 创建设备 ID 失败 (%s)"), *Info));
		UE_LOG(LogTemp, Warning, TEXT("[Online] EOS login failed: CreateDeviceId %s"), *Info);
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[Online] DeviceId ready role=%s -> Connect Login"), *GEosInstanceRole);
	LoginWithType(TEXT("deviceid"));
}

void UShooterSessionSubsystem::LoginWithType(const FString& Type)
{
	IOnlineSubsystem* EOS = IOnlineSubsystem::Get(EOS_SUBSYSTEM);
	IOnlineIdentityPtr Identity = EOS ? EOS->GetIdentityInterface() : nullptr;
	if (!Identity.IsValid()) { bLoggingIn = false; return; }
	Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
	LoginHandle = Identity->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &ThisClass::HandleLoginComplete));

	if (Type == TEXT("steam"))
	{
		// 走 Steam 票据（需要 NativePlatformService=Steam + 门户 Steam 身份提供方）
		Identity->AutoLogin(0);
		return;
	}
	FOnlineAccountCredentials Creds;
	if (Type == TEXT("deviceid"))
	{
		Creds.Type = TEXT("externalauth:DeviceIdAccessToken");
		Creds.Id = GetLoginDisplayName(); // 覆盖插件里当作 DisplayName
		Creds.Token = FString();
	}
	else
	{
		Creds.Type = Type; // persistentauth / accountportal / developer（需 bUseEAS=true）
		FParse::Value(FCommandLine::Get(), TEXT("EOSDevHost="), Creds.Id);   // developer: localhost:6547
		FParse::Value(FCommandLine::Get(), TEXT("EOSDevCred="), Creds.Token); // developer: DevAuthTool 里的凭据名
	}
	Identity->Login(0, Creds);
}

void UShooterSessionSubsystem::HandleLoginComplete(int32 LocalUserNum, bool bOk, const FUniqueNetId& UserId, const FString& Error)
{
	bLoggingIn = false;
	if (IOnlineSubsystem* EOS = IOnlineSubsystem::Get(EOS_SUBSYSTEM))
	{
		if (IOnlineIdentityPtr Identity = EOS->GetIdentityInterface())
		{
			Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
		}
	}
	if (bOk)
	{
		ProductUserId = UserId.ToString();
		UE_LOG(LogTemp, Log, TEXT("[Online] EOS login ok role=%s pid=%u ProductUserId=%s name=%s"), *GEosInstanceRole, FPlatformProcess::GetCurrentProcessId(), *ProductUserId, *GetLoginDisplayName());
		SetOnlineState(true, TEXT("已登录"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Online] EOS login failed: %s"), *Error);
		SetOnlineState(false, FString::Printf(TEXT("登录失败: %s"), *Error));
	}
	// primary 登录结束后 secondary 才能 DeleteDeviceId
	SignalEosPrimaryLoginDone();
}