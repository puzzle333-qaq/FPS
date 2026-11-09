#include "Online/PartySubsystem.h"

#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "ShooterGameInstance.h"
#include "TextureResource.h"

THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	const FName GSteamRoomName(TEXT("FPSParty"));
	const FName GEosSessionSetting(TEXT("EOSSessionId"));

	IOnlineSubsystem* EosOss()
	{
		return IOnlineSubsystem::Get(EOS_SUBSYSTEM);
	}

	IOnlineSubsystem* SteamOss()
	{
		return IOnlineSubsystem::Get(STEAM_SUBSYSTEM);
	}

	UTexture2D* MakeSteamAvatar(uint64 SteamId64)
	{
		if (SteamId64 == 0 || !SteamAPI_IsSteamRunning() || !SteamFriends() || !SteamUtils()) return nullptr;
		const CSteamID SteamId(SteamId64);
		const int Image = SteamFriends()->GetMediumFriendAvatar(SteamId);
		if (Image <= 0)
		{
			SteamFriends()->RequestUserInformation(SteamId, false);
			return nullptr;
		}
		uint32 Width = 0;
		uint32 Height = 0;
		if (!SteamUtils()->GetImageSize(Image, &Width, &Height) || Width == 0 || Height == 0) return nullptr;
		TArray<uint8> Rgba;
		Rgba.SetNumUninitialized(Width * Height * 4);
		if (!SteamUtils()->GetImageRGBA(Image, Rgba.GetData(), Rgba.Num())) return nullptr;
		UTexture2D* Tex = UTexture2D::CreateTransient(Width, Height, PF_R8G8B8A8);
		if (!Tex || !Tex->GetPlatformData() || Tex->GetPlatformData()->Mips.Num() == 0) return nullptr;
		void* Mip = Tex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Mip, Rgba.GetData(), Rgba.Num());
		Tex->GetPlatformData()->Mips[0].BulkData.Unlock();
		Tex->SRGB = true;
		Tex->UpdateResource();
		return Tex;
	}

	FString LocalSteamId()
	{
		if (!SteamAPI_IsSteamRunning() || !SteamUser()) return FString();
		const uint64 Id = SteamUser()->GetSteamID().ConvertToUint64();
		return Id == 0 ? FString() : FString::Printf(TEXT("%llu"), Id);
	}
}

void UPartySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StatusText = TEXT("正在登录…");
	MapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UPartySubsystem::OnMapLoaded);
	BindOnlineDelegates();
	StartLogin();
}

void UPartySubsystem::Deinitialize()
{
	if (IOnlineSubsystem* Eos = EosOss())
	{
		if (IOnlineIdentityPtr Identity = Eos->GetIdentityInterface())
		{
			Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
		}
		if (IOnlineSessionPtr Session = Eos->GetSessionInterface())
		{
			Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
			Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
			Session->ClearOnSessionUserInviteAcceptedDelegate_Handle(EosInviteHandle);
		}
	}
	if (IOnlineSubsystem* Steam = SteamOss())
	{
		if (IOnlineSessionPtr Session = Steam->GetSessionInterface())
		{
			Session->ClearOnSessionUserInviteAcceptedDelegate_Handle(SteamInviteHandle);
			Session->ClearOnCreateSessionCompleteDelegate_Handle(SteamCreateHandle);
		}
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapHandle);
	Super::Deinitialize();
}

void UPartySubsystem::BindOnlineDelegates()
{
	if (bDelegatesBound) return;
	IOnlineSubsystem* Eos = EosOss();
	if (!Eos) return;
	if (IOnlineIdentityPtr Identity = Eos->GetIdentityInterface())
	{
		LoginHandle = Identity->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &UPartySubsystem::OnLoginComplete));
	}
	if (IOnlineSessionPtr Session = Eos->GetSessionInterface())
	{
		CreateHandle = Session->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UPartySubsystem::OnCreateSessionComplete));
		JoinHandle = Session->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UPartySubsystem::OnJoinSessionComplete));
		EosInviteHandle = Session->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UPartySubsystem::OnEosInviteAccepted));
	}
	if (IOnlineSubsystem* Steam = SteamOss())
	{
		if (IOnlineSessionPtr Session = Steam->GetSessionInterface())
		{
			SteamInviteHandle = Session->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UPartySubsystem::OnSteamInviteAccepted));
			SteamCreateHandle = Session->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UPartySubsystem::OnSteamSessionCreated));
		}
	}
	bDelegatesBound = true;
}

void UPartySubsystem::StartLogin()
{
	BindOnlineDelegates();
	IOnlineSubsystem* Eos = EosOss();
	IOnlineIdentityPtr Identity = Eos ? Eos->GetIdentityInterface() : nullptr;
	if (!Identity.IsValid())
	{
		StatusText = TEXT("EOS 还没就绪");
		return;
	}
	if (Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
	{
		bLoggedIn = true;
		if (UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance()))
		{
			GI->LocalPlayerName = GetLocalDisplayName();
		}
		StatusText = FString::Printf(TEXT("已登录 %s"), *GetLocalDisplayName());
		if (!PendingEosSessionId.IsEmpty())
		{
			const FString Pending = PendingEosSessionId;
			PendingEosSessionId.Reset();
			JoinEosSessionById(Pending);
		}
		else if (bHostAfterLogin)
		{
			bHostAfterLogin = false;
			HostRoom();
		}
		else
		{
			EnterOwnRangeIfNeeded();
		}
		return;
	}
	StatusText = TEXT("正在登录 EOS…");
	if (!Identity->AutoLogin(0))
	{
		StatusText = TEXT("登录没有开始。打开 Steam 后再试");
	}
}

void UPartySubsystem::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	(void)LocalUserNum;
	(void)UserId;
	if (!bWasSuccessful && !Error.Contains(TEXT("Already logged in")))
	{
		bLoggedIn = false;
		StatusText = FString::Printf(TEXT("登录失败：%s"), *Error);
		UE_LOG(LogTemp, Warning, TEXT("[Party] EOS login failed: %s"), *Error);
		return;
	}
	bLoggedIn = true;
	if (UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance()))
	{
		GI->LocalPlayerName = GetLocalDisplayName();
	}
	StatusText = FString::Printf(TEXT("已登录 %s"), *GetLocalDisplayName());
	UE_LOG(LogTemp, Log, TEXT("[Party] EOS login %s"), *GetLocalDisplayName());
	if (!PendingEosSessionId.IsEmpty())
	{
		const FString Pending = PendingEosSessionId;
		PendingEosSessionId.Reset();
		JoinEosSessionById(Pending);
		return;
	}
	if (bHostAfterLogin)
	{
		bHostAfterLogin = false;
		HostRoom();
		return;
	}
	EnterOwnRangeIfNeeded();
}

FString UPartySubsystem::GetLocalDisplayName() const
{
	if (IOnlineSubsystem* Eos = EosOss())
	{
		if (IOnlineIdentityPtr Identity = Eos->GetIdentityInterface())
		{
			const FString Nick = Identity->GetPlayerNickname(0);
			if (!Nick.IsEmpty()) return Nick.Left(24);
		}
	}
	if (const UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance()))
	{
		if (!GI->LocalPlayerName.IsEmpty()) return GI->LocalPlayerName.Left(24);
	}
	const FString User = FPlatformProcess::UserName();
	return User.IsEmpty() ? FString(TEXT("Operator")) : User.Left(24);
}

FString UPartySubsystem::GetLocalPlatformId() const
{
	return LocalSteamId();
}

UTexture2D* UPartySubsystem::GetAvatar(const FString& PlatformId)
{
	if (PlatformId.IsEmpty()) return nullptr;
	if (TObjectPtr<UTexture2D>* Found = Avatars.Find(PlatformId))
	{
		if (IsValid(*Found)) return *Found;
	}
	const uint64 SteamId = FCString::Strtoui64(*PlatformId, nullptr, 10);
	UTexture2D* Tex = MakeSteamAvatar(SteamId);
	if (Tex) Avatars.Add(PlatformId, Tex);
	return Tex;
}

void UPartySubsystem::UseEosDriver() const
{
	if (!GEngine) return;
	const FName EosDriver(TEXT("/Script/OnlineSubsystemEOS.NetDriverEOS"));
	const FName IpDriver(TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"));
	for (FNetDriverDefinition& Def : GEngine->NetDriverDefinitions)
	{
		if (Def.DefName == NAME_GameNetDriver)
		{
			Def.DriverClassName = EosDriver;
			Def.DriverClassNameFallback = IpDriver;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Party] GameNetDriver -> NetDriverEOS"));
}

void UPartySubsystem::HostRoom()
{
	if (bIsHost && bInRoom)
	{
		OpenInvite();
		return;
	}
	BindOnlineDelegates();
	IOnlineSubsystem* Eos = EosOss();
	IOnlineIdentityPtr Identity = Eos ? Eos->GetIdentityInterface() : nullptr;
	IOnlineSessionPtr Session = Eos ? Eos->GetSessionInterface() : nullptr;
	if (!Identity.IsValid() || !Session.IsValid())
	{
		StatusText = TEXT("EOS 还没就绪");
		return;
	}
	if (Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn)
	{
		bHostAfterLogin = true;
		StatusText = TEXT("登录完成后会开房");
		StartLogin();
		return;
	}
	if (UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance()))
	{
		GI->LocalPlayerName = GetLocalDisplayName();
	}
	UseEosDriver();
	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = false;
	Settings.NumPublicConnections = 4;
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bAllowJoinViaPresenceFriendsOnly = false;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUseLobbiesVoiceChatIfAvailable = false;
	Settings.bIsDedicated = false;
	Settings.Set(SETTING_MAPNAME, FString(TEXT("Lvl_Range")), EOnlineDataAdvertisementType::ViaOnlineService);
	bOpenInviteAfterTravel = true;
	bSteamLobbyPublished = false;
	StatusText = TEXT("正在创建房间…");
	if (!Session->CreateSession(0, NAME_GameSession, Settings))
	{
		bOpenInviteAfterTravel = false;
		StatusText = TEXT("创建房间失败");
	}
}

void UPartySubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession) return;
	if (!bWasSuccessful)
	{
		bOpenInviteAfterTravel = false;
		bInRoom = false;
		bIsHost = false;
		StatusText = TEXT("EOS 没有建成房间");
		UE_LOG(LogTemp, Warning, TEXT("[Party] CreateSession failed"));
		return;
	}
	bInRoom = true;
	bIsHost = true;
	bReturningHome = false;
	StatusText = TEXT("正在打开你的靶场…");
	BeginListenTravel();
}

void UPartySubsystem::BeginListenTravel()
{
	UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance());
	if (!GI)
	{
		StatusText = TEXT("没有游戏实例");
		return;
	}
	TSoftObjectPtr<UWorld> Map = GI->RangeMap;
	if (Map.IsNull())
	{
		Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/FPS/Level/Lvl_Range.Lvl_Range")));
	}
	UseEosDriver();
	GI->TravelTo(Map, GI->RangeGameMode, true, {TEXT("RangeHub=1")});
}

void UPartySubsystem::OnMapLoaded(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance()) return;
	if (bReturningHome && LoadedWorld->GetNetMode() == NM_Standalone)
	{
		bReturningHome = false;
		const FString Pkg = LoadedWorld->GetOutermost() ? LoadedWorld->GetOutermost()->GetName() : FString();
		if (Pkg.Contains(TEXT("Lvl_Range"))) StatusText = TEXT("在自己的靶场");
	}
	if (bIsHost && bInRoom && LoadedWorld->GetNetMode() == NM_ListenServer)
	{
		StartListenSession();
		PublishSteamLobby();
	}
	else if (bLoggedIn)
	{
		EnterOwnRangeIfNeeded();
	}
}

void UPartySubsystem::StartListenSession()
{
	IOnlineSessionPtr Session = EosOss() ? EosOss()->GetSessionInterface() : nullptr;
	if (!Session.IsValid()) return;
	const EOnlineSessionState::Type State = Session->GetSessionState(NAME_GameSession);
	if (State == EOnlineSessionState::Pending || State == EOnlineSessionState::Ended)
	{
		Session->StartSession(NAME_GameSession);
	}
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (It->IsValid()) RegisterJoinedPlayer(It->Get());
		}
	}
}

void UPartySubsystem::PublishSteamLobby()
{
	if (bSteamLobbyPublished) return;
	IOnlineSubsystem* Steam = SteamOss();
	IOnlineSessionPtr SteamSession = Steam ? Steam->GetSessionInterface() : nullptr;
	IOnlineSessionPtr EosSession = EosOss() ? EosOss()->GetSessionInterface() : nullptr;
	if (!SteamSession.IsValid() || !EosSession.IsValid())
	{
		if (bOpenInviteAfterTravel)
		{
			bOpenInviteAfterTravel = false;
			OpenInvite();
		}
		return;
	}
	FNamedOnlineSession* Named = EosSession->GetNamedSession(NAME_GameSession);
	if (!Named || !Named->SessionInfo.IsValid()) return;
	const FString EosId = Named->SessionInfo->GetSessionId().ToString();
	if (EosId.IsEmpty()) return;
	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = false;
	Settings.NumPublicConnections = 4;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.Set(GEosSessionSetting, EosId, EOnlineDataAdvertisementType::ViaOnlineService);
	if (!SteamSession->CreateSession(0, GSteamRoomName, Settings))
	{
		StatusText = TEXT("房间已开。Steam 邀请没建起来");
	}
}

void UPartySubsystem::OnSteamSessionCreated(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != GSteamRoomName) return;
	bSteamLobbyPublished = bWasSuccessful;
	if (!bWasSuccessful)
	{
		StatusText = TEXT("房间已开。Steam 邀请失败，Epic 邀请仍可用");
		UE_LOG(LogTemp, Warning, TEXT("[Party] Steam lobby create failed"));
	}
	else
	{
		StatusText = TEXT("房间已开，可以邀请好友");
		UE_LOG(LogTemp, Log, TEXT("[Party] Steam lobby ready"));
	}
	if (bOpenInviteAfterTravel)
	{
		bOpenInviteAfterTravel = false;
		OpenInvite();
	}
}

void UPartySubsystem::OpenInvite()
{
	if (!bInRoom || !bIsHost)
	{
		HostRoom();
		return;
	}
	bool bOpened = false;
	if (IOnlineSubsystem* Steam = SteamOss())
	{
		if (IOnlineExternalUIPtr Ui = Steam->GetExternalUIInterface())
		{
			bOpened |= Ui->ShowInviteUI(0, GSteamRoomName);
		}
	}
	if (IOnlineSubsystem* Eos = EosOss())
	{
		if (IOnlineExternalUIPtr Ui = Eos->GetExternalUIInterface())
		{
			bOpened |= Ui->ShowInviteUI(0, NAME_GameSession);
		}
	}
	StatusText = bOpened ? TEXT("选择要邀请的好友") : TEXT("邀请界面没打开。确认 Steam 已运行，并且 EOS 浮层已开");
}

void UPartySubsystem::OnEosInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	(void)ControllerId;
	(void)UserId;
	if (!bWasSuccessful)
	{
		StatusText = TEXT("Epic 邀请无效");
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[Party] EOS invite accepted"));
	JoinFromEosResult(InviteResult);
}

void UPartySubsystem::OnSteamInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	(void)ControllerId;
	(void)UserId;
	if (!bWasSuccessful)
	{
		StatusText = TEXT("Steam 邀请无效");
		return;
	}
	FString EosId;
	InviteResult.Session.SessionSettings.Get(GEosSessionSetting, EosId);
	if (EosId.IsEmpty())
	{
		StatusText = TEXT("这份 Steam 邀请里没有房间号");
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[Party] Steam invite -> EOS %s"), *EosId);
	JoinEosSessionById(EosId);
}

void UPartySubsystem::JoinEosSessionById(const FString& EosSessionId)
{
	IOnlineSubsystem* Eos = EosOss();
	IOnlineSessionPtr Session = Eos ? Eos->GetSessionInterface() : nullptr;
	IOnlineIdentityPtr Identity = Eos ? Eos->GetIdentityInterface() : nullptr;
	if (!Session.IsValid() || !Identity.IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn)
	{
		PendingEosSessionId = EosSessionId;
		StatusText = TEXT("登录后加入邀请");
		StartLogin();
		return;
	}
	FUniqueNetIdPtr LocalId = Identity->GetUniquePlayerId(0);
	FUniqueNetIdPtr SessionId = Session->CreateSessionIdFromString(EosSessionId);
	if (!LocalId.IsValid() || !SessionId.IsValid())
	{
		StatusText = TEXT("房间号无法识别");
		return;
	}
	UseEosDriver();
	bJoinInProgress = true;
	bReturningHome = false;
	if (!Session->FindSessionById(*LocalId, *SessionId, *LocalId, FOnSingleSessionResultCompleteDelegate::CreateUObject(this, &UPartySubsystem::OnFoundInviteSession)))
	{
		bJoinInProgress = false;
		StatusText = TEXT("查找房间失败");
	}
	else
	{
		StatusText = TEXT("正在查找房间…");
	}
}

void UPartySubsystem::OnFoundInviteSession(int32 LocalUserNum, bool bWasSuccessful, const FOnlineSessionSearchResult& Result)
{
	(void)LocalUserNum;
	if (!bWasSuccessful)
	{
		bJoinInProgress = false;
		StatusText = TEXT("找不到这个房间");
		return;
	}
	JoinFromEosResult(Result);
}

void UPartySubsystem::JoinFromEosResult(const FOnlineSessionSearchResult& Result)
{
	IOnlineSessionPtr Session = EosOss() ? EosOss()->GetSessionInterface() : nullptr;
	if (!Session.IsValid())
	{
		bJoinInProgress = false;
		StatusText = TEXT("EOS 会话不可用");
		return;
	}
	UseEosDriver();
	bJoinInProgress = true;
	bReturningHome = false;
	bIsHost = false;
	StatusText = TEXT("正在加入房间…");
	if (!Session->JoinSession(0, NAME_GameSession, Result))
	{
		bJoinInProgress = false;
		StatusText = TEXT("加入房间失败");
	}
}

void UPartySubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionName != NAME_GameSession) return;
	bJoinInProgress = false;
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		StatusText = TEXT("加入房间失败");
		UE_LOG(LogTemp, Warning, TEXT("[Party] JoinSession result %d"), (int32)Result);
		return;
	}
	IOnlineSessionPtr Session = EosOss() ? EosOss()->GetSessionInterface() : nullptr;
	FString Address;
	if (!Session.IsValid() || !Session->GetResolvedConnectString(NAME_GameSession, Address) || Address.IsEmpty())
	{
		StatusText = TEXT("没有拿到房主地址");
		return;
	}
	UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance());
	APlayerController* PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (!PC)
	{
		StatusText = TEXT("没有本地玩家");
		return;
	}
	if (GI) GI->LocalPlayerName = GetLocalDisplayName();
	const FString NameOpt = GI ? GI->GetPlayerNameOption() : FString();
	if (!NameOpt.IsEmpty())
	{
		Address += Address.Contains(TEXT("?")) ? TEXT("&") + NameOpt.RightChop(1) : NameOpt;
	}
	bInRoom = true;
	bIsHost = false;
	bReturningHome = false;
	StatusText = TEXT("正在进入房主的靶场");
	UE_LOG(LogTemp, Log, TEXT("[Party] ClientTravel %s"), *Address);
	PC->ClientTravel(Address, TRAVEL_Absolute);
}

void UPartySubsystem::LeaveRoom()
{
	const bool bWasHost = bIsHost;
	bInRoom = false;
	bIsHost = false;
	bJoinInProgress = false;
	bOpenInviteAfterTravel = false;
	bHostAfterLogin = false;
	bSteamLobbyPublished = false;
	bReturningHome = true;
	PendingEosSessionId.Reset();
	if (bWasHost)
	{
		if (IOnlineSessionPtr Session = EosOss() ? EosOss()->GetSessionInterface() : nullptr)
		{
			Session->DestroySession(NAME_GameSession);
		}
		if (IOnlineSessionPtr Session = SteamOss() ? SteamOss()->GetSessionInterface() : nullptr)
		{
			Session->DestroySession(GSteamRoomName);
		}
	}
	StatusText = TEXT("正在回到自己的靶场");
	OpenOwnRange();
}

void UPartySubsystem::OpenOwnRange()
{
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() == NM_Standalone)
	{
		const FString Pkg = World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
		if (Pkg.Contains(TEXT("Lvl_Range")))
		{
			bReturningHome = false;
			StatusText = TEXT("在自己的靶场");
			return;
		}
	}
	UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance());
	if (!GI || !GI->OpenRange())
	{
		StatusText = GI ? GI->GetLastOnlineError() : FString(TEXT("无法打开靶场"));
	}
}

void UPartySubsystem::EnterOwnRangeIfNeeded()
{
#if WITH_EDITOR
	if (GIsEditor) return;
#endif
	if (bJoinInProgress || bInRoom || bReturningHome || bHostAfterLogin) return;
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Standalone) return;
	const FString Pkg = World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
	if (Pkg.Contains(TEXT("Lvl_Range")) || Pkg.Contains(TEXT("MainMenu"))) return;
	OpenOwnRange();
}

bool UPartySubsystem::HandleNetworkFailure()
{
	if (bReturningHome) return true;
	if (!bInRoom && !bJoinInProgress) return false;
	bInRoom = false;
	bIsHost = false;
	bJoinInProgress = false;
	bSteamLobbyPublished = false;
	bReturningHome = true;
	StatusText = TEXT("连接断开，正在回到自己的靶场");
	OpenOwnRange();
	return true;
}

bool UPartySubsystem::HandleTravelFailure()
{
	StatusText = TEXT("跳转失败");
	if (bReturningHome || bJoinInProgress)
	{
		bJoinInProgress = false;
		bInRoom = false;
		bIsHost = false;
		bReturningHome = true;
		OpenOwnRange();
		return true;
	}
	return bInRoom;
}

void UPartySubsystem::RegisterJoinedPlayer(APlayerController* Joining)
{
	if (!bIsHost || !IsValid(Joining) || !Joining->PlayerState) return;
	IOnlineSessionPtr Session = EosOss() ? EosOss()->GetSessionInterface() : nullptr;
	if (!Session.IsValid()) return;
	const FUniqueNetIdPtr NetId = Joining->PlayerState->GetUniqueId().GetUniqueNetId();
	if (!NetId.IsValid()) return;
	Session->RegisterPlayer(NAME_GameSession, *NetId, false);
}

void UPartySubsystem::UnregisterExitingPlayer(AController* Exiting)
{
	if (!bIsHost || !IsValid(Exiting) || !Exiting->PlayerState) return;
	IOnlineSessionPtr Session = EosOss() ? EosOss()->GetSessionInterface() : nullptr;
	if (!Session.IsValid()) return;
	const FUniqueNetIdPtr NetId = Exiting->PlayerState->GetUniqueId().GetUniqueNetId();
	if (!NetId.IsValid()) return;
	Session->UnregisterPlayer(NAME_GameSession, *NetId);
}

static FAutoConsoleCommandWithWorldAndArgs GFPSHostPartyCmd(
	TEXT("fps.HostParty"),
	TEXT("Create an EOS room and reopen your range as the host."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (!World || !World->GetGameInstance()) return;
		if (UPartySubsystem* Party = World->GetGameInstance()->GetSubsystem<UPartySubsystem>()) Party->HostRoom();
	}));

static FAutoConsoleCommandWithWorldAndArgs GFPSLeavePartyCmd(
	TEXT("fps.LeaveParty"),
	TEXT("Leave the room and return to your own range."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (!World || !World->GetGameInstance()) return;
		if (UPartySubsystem* Party = World->GetGameInstance()->GetSubsystem<UPartySubsystem>()) Party->LeaveRoom();
	}));

static FAutoConsoleCommandWithWorldAndArgs GFPSInvitePartyCmd(
	TEXT("fps.Invite"),
	TEXT("Open the Steam and EOS invite UI for the current room."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (!World || !World->GetGameInstance()) return;
		if (UPartySubsystem* Party = World->GetGameInstance()->GetSubsystem<UPartySubsystem>()) Party->OpenInvite();
	}));
