#include "ShooterGameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Engine/Engine.h"

extern bool GFPS_SessionJoinInProgress;

#include "Loadout/ShooterLoadoutSave.h"
#include "Weapon/Weapon.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerState.h"
#include "Online/ShooterSessionSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Ui/ShooterLoadingScreen.h"
#include "UObject/SoftObjectPath.h"


namespace
{
	static const TCHAR* GPCTestMapPath = TEXT("/Game/FPS/Level/Lvl_Range.Lvl_Range");
	static const TCHAR* GPCGameModePath = TEXT("/Script/FPS.PoliceCriminalGameMode");

	/** Fill PoliceCriminal Modes entry so main menu / lobby / practice can open the range as test map. */
	void EnsurePoliceCriminalTestDefaults(UShooterGameInstance* GI)
	{
		if (!GI) return;
		FShooterModeInfo& Info = GI->Modes.FindOrAdd(EShooterGameMode::PoliceCriminal);
		if (Info.DisplayName.IsEmpty())
		{
			Info.DisplayName = FText::FromString(TEXT("警匪战术"));
		}
		if (Info.Map.IsNull())
		{
			if (!GI->RangeMap.IsNull())
			{
				Info.Map = GI->RangeMap;
			}
			else
			{
				Info.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(GPCTestMapPath));
			}
		}
		if (Info.GameModeClass.IsNull())
		{
			Info.GameModeClass = TSoftClassPtr<AGameModeBase>(FSoftObjectPath(GPCGameModePath));
		}
		if (Info.MaxPlayers < 1)
		{
			Info.MaxPlayers = 10;
		}
		Info.bImplemented = true;
		UE_LOG(LogTemp, Log, TEXT("[Menu] PoliceCriminal test map -> %s game=%s"),
			*Info.Map.ToSoftObjectPath().ToString(),
			*Info.GameModeClass.ToSoftObjectPath().ToString());
	}
}

UShooterGameInstance::UShooterGameInstance()
{
	auto AddMode = [this](EShooterGameMode Mode, const TCHAR* Name, int32 MaxPlayers)
	{
		FShooterModeInfo Info;
		Info.DisplayName = FText::FromString(Name);
		Info.MaxPlayers = MaxPlayers;
		Modes.Add(Mode, Info);
	};
	AddMode(EShooterGameMode::FFA, TEXT("死斗"), 8);
	AddMode(EShooterGameMode::Story, TEXT("剧情"), 4);
	AddMode(EShooterGameMode::Duel, TEXT("单挑"), 2);
	AddMode(EShooterGameMode::Extraction, TEXT("搜打撤"), 12);
	AddMode(EShooterGameMode::PoliceCriminal, TEXT("警匪战术"), 10);
}

void UShooterGameInstance::Init()
{
	Super::Init();
	ShooterLoadingScreen::Register(this);
	LoadLoadout();
	EnsurePoliceCriminalTestDefaults(this);

	if (GEngine)
	{
		NetFailHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
		TravelFailHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
	}
	// Phase1/2 smoke: Steam/EOS plugin presence (Steam often fails in PIE — use Standalone)
	IOnlineSubsystem* DefaultOSS = IOnlineSubsystem::Get();
	IOnlineSubsystem* SteamOSS = IOnlineSubsystem::Get(STEAM_SUBSYSTEM);
	IOnlineSubsystem* EOSS = IOnlineSubsystem::Get(EOS_SUBSYSTEM);
	IOnlineSubsystem* NullOSS = IOnlineSubsystem::Get(NULL_SUBSYSTEM);
	UE_LOG(LogTemp, Log, TEXT("[Online] Default=%s Steam=%s EOS=%s Null=%s"),
		DefaultOSS ? *DefaultOSS->GetSubsystemName().ToString() : TEXT("None"),
		SteamOSS ? TEXT("ok") : TEXT("missing"),
		EOSS ? TEXT("ok") : TEXT("missing"),
		NullOSS ? TEXT("ok") : TEXT("missing"));
	if (SteamOSS)
	{
		if (IOnlineIdentityPtr Id = SteamOSS->GetIdentityInterface())
		{
			UE_LOG(LogTemp, Log, TEXT("[Online] Steam nick=%s login=%d"),
				*Id->GetPlayerNickname(0), (int32)Id->GetLoginStatus(0));
		}
	}
}

bool UShooterGameInstance::GetModeInfo(EShooterGameMode Mode, FShooterModeInfo& OutInfo) const
{
	if (const FShooterModeInfo* Found = Modes.Find(Mode))
	{
		OutInfo = *Found;
		return true;
	}
	return false;
}

bool UShooterGameInstance::IsModeAvailable(EShooterGameMode Mode) const
{
	const FShooterModeInfo* Found = Modes.Find(Mode);
	return Found && Found->bImplemented && !Found->Map.IsNull();
}

bool UShooterGameInstance::TravelTo(const TSoftObjectPtr<UWorld>& Map, const TSoftClassPtr<AGameModeBase>& GameMode, bool bListen, const TArray<FString>& ExtraOptions)
{
	if (Map.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Menu] Travel failed: map is not set."));
		return false;
	}

	TArray<FString> Options = ExtraOptions;
	if (bListen)
	{
		Options.Add(TEXT("listen"));
	}
	if (!GameMode.IsNull())
	{
		Options.Add(FString::Printf(TEXT("game=%s"), *GameMode.ToSoftObjectPath().ToString()));
	}

	const FString MapName = Map.ToSoftObjectPath().GetLongPackageName();
	UE_LOG(LogTemp, Log, TEXT("[Menu] OpenLevel %s ?%s"), *MapName, *FString::Join(Options, TEXT("?")));
	UGameplayStatics::OpenLevel(this, FName(*MapName), true, FString::Join(Options, TEXT("?")));
	return true;
}

bool UShooterGameInstance::HostRoom(EShooterGameMode Mode)
{
	if (Mode == EShooterGameMode::PoliceCriminal)
	{
		EnsurePoliceCriminalTestDefaults(this);
	}
	if (!IsModeAvailable(Mode))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Menu] Mode %d is not implemented or has no map."), static_cast<int32>(Mode));
		return false;
	}
	SelectedMode = Mode;
	bPracticeMode = false;
	// 有房间大厅时，开房 = 进房间大厅（不再直接进对局地图）
	if (!RoomLobbyMap.IsNull())
	{
		return CreateRoom(Mode);
	}
	const FShooterModeInfo& Info = Modes.FindChecked(Mode);
	return TravelTo(Info.Map, Info.GameModeClass, true);
}

bool UShooterGameInstance::JoinByIP(const FString& Address)
{
	const FString Trimmed = Address.TrimStartAndEnd();
	APlayerController* PC = GetFirstLocalPlayerController();
	if (Trimmed.IsEmpty() || !IsValid(PC)) return false;
	PC->ClientTravel(Trimmed + GetNameOption(), TRAVEL_Absolute);
	return true;
}

bool UShooterGameInstance::OpenRange()
{
	bPracticeMode = false;
	return TravelTo(RangeMap, RangeGameMode, false);
}

bool UShooterGameInstance::StartPractice(EShooterGameMode Mode)
{
	if (Mode == EShooterGameMode::PoliceCriminal)
	{
		EnsurePoliceCriminalTestDefaults(this);
	}
	if (!IsModeAvailable(Mode))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Practice] Mode %d is not implemented or has no map."), static_cast<int32>(Mode));
		return false;
	}
	// 练习不进房间：清房间状态，避免等人 / 回大厅逻辑
	bPracticeMode = true;
	Room = FShooterRoomState();
	SelectedMode = Mode;
	if (LocalPlayerName.TrimStartAndEnd().IsEmpty())
	{
		FString Hint = GetNameOption();
		if (!Hint.IsEmpty() && Hint.StartsWith(TEXT("?Name=")))
		{
			LocalPlayerName = Hint.RightChop(6);
		}
	}
	const FShooterModeInfo& Info = Modes.FindChecked(Mode);
	TArray<FString> Extra;
	Extra.Add(TEXT("Practice=1"));
	const FString NameOpt = GetNameOption();
	if (!NameOpt.IsEmpty()) Extra.Add(NameOpt.RightChop(1));
	UE_LOG(LogTemp, Log, TEXT("[Practice] Solo practice mode=%d (no room)"), (int32)Mode);
	return TravelTo(Info.Map, Info.GameModeClass, false, Extra);
}

bool UShooterGameInstance::GoToMainMenu()
{
	bPracticeMode = false;
	if (UShooterSessionSubsystem* Sessions = GetSubsystem<UShooterSessionSubsystem>())
	{
		Sessions->DestroyRoomSession();
	}
	return TravelTo(MainMenuMap, nullptr, false);
}

namespace
{
	EWeaponKind GetPresetKind(const FWeaponPreset& Preset)
	{
		const AWeapon* CDO = Preset.WeaponClass ? Preset.WeaponClass->GetDefaultObject<AWeapon>() : nullptr;
		return CDO ? CDO->WeaponKind : EWeaponKind::LongGun;
	}
}

void UShooterGameInstance::SetWeaponPreset(const FWeaponPreset& Preset)
{
	if (!Preset.WeaponClass) return;
	const EWeaponKind Kind = GetPresetKind(Preset);
	if (Kind == EWeaponKind::Bomb) return;

	for (FWeaponPreset& Existing : CurrentLoadout.Weapons)
	{
		if (Existing.WeaponClass == Preset.WeaponClass)
		{
			Existing = Preset;
			return;
		}
	}

	if (Kind != EWeaponKind::LongGun)
	{
		for (FWeaponPreset& Existing : CurrentLoadout.Weapons)
		{
			if (Existing.WeaponClass && GetPresetKind(Existing) == Kind)
			{
				Existing = Preset;
				return;
			}
		}
	}

	CurrentLoadout.Weapons.Add(Preset);
}

void UShooterGameInstance::EquipPrimaryWeapon(const FWeaponPreset& Preset)
{
	if (!Preset.WeaponClass) return;
	const EWeaponKind Kind = GetPresetKind(Preset);
	if (Kind == EWeaponKind::Bomb) return;

	CurrentLoadout.Weapons.RemoveAll([&Preset, Kind](const FWeaponPreset& Existing)
	{
		if (!Existing.WeaponClass) return true;
		if (Existing.WeaponClass == Preset.WeaponClass) return true;
		return Kind != EWeaponKind::LongGun && GetPresetKind(Existing) == Kind;
	});

	int32 InsertAt = CurrentLoadout.Weapons.Num();
	if (Kind == EWeaponKind::LongGun)
	{
		InsertAt = 0;
		for (int32 i = 0; i < CurrentLoadout.Weapons.Num(); ++i)
		{
			if (GetPresetKind(CurrentLoadout.Weapons[i]) == EWeaponKind::LongGun)
			{
				InsertAt = i;
				break;
			}
		}
	}
	CurrentLoadout.Weapons.Insert(Preset, InsertAt);

	if (Kind == EWeaponKind::LongGun)
	{
		int32 Longs = 0;
		for (int32 i = 0; i < CurrentLoadout.Weapons.Num();)
		{
			if (GetPresetKind(CurrentLoadout.Weapons[i]) != EWeaponKind::LongGun)
			{
				++i;
				continue;
			}
			++Longs;
			if (Longs > 2)
			{
				CurrentLoadout.Weapons.RemoveAt(i);
				continue;
			}
			++i;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Loadout] Equipped %s as %s"),
		*GetNameSafe(Preset.WeaponClass),
		Kind == EWeaponKind::LongGun ? TEXT("primary long gun") : TEXT("slot weapon"));
}

TSubclassOf<AWeapon> UShooterGameInstance::GetPrimaryLongGun() const
{
	for (const FWeaponPreset& Existing : CurrentLoadout.Weapons)
	{
		if (Existing.WeaponClass && GetPresetKind(Existing) == EWeaponKind::LongGun)
		{
			return Existing.WeaponClass;
		}
	}
	return nullptr;
}

bool UShooterGameInstance::FindWeaponPreset(TSubclassOf<AWeapon> WeaponClass, FWeaponPreset& OutPreset) const
{
	for (const FWeaponPreset& Existing : CurrentLoadout.Weapons)
	{
		if (Existing.WeaponClass == WeaponClass)
		{
			OutPreset = Existing;
			return true;
		}
	}
	return false;
}

bool UShooterGameInstance::SaveLoadout()
{
	UShooterLoadoutSave* Save = Cast<UShooterLoadoutSave>(UGameplayStatics::CreateSaveGameObject(UShooterLoadoutSave::StaticClass()));
	if (!IsValid(Save)) return false;
	Save->Presets = { CurrentLoadout };
	Save->SelectedIndex = 0;
	return UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, 0);
}

bool UShooterGameInstance::LoadLoadout()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0)) return false;
	const UShooterLoadoutSave* Save = Cast<UShooterLoadoutSave>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	if (!IsValid(Save) || Save->Presets.Num() == 0) return false;
	CurrentLoadout = Save->Presets.IsValidIndex(Save->SelectedIndex) ? Save->Presets[Save->SelectedIndex] : Save->Presets[0];
	return true;
}

// ================= 房间 =================

FString UShooterGameInstance::MakeRoomCode()
{
	static const TCHAR* Alphabet = TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
	const int32 Len = FCString::Strlen(Alphabet);
	FString Code;
	for (int32 i = 0; i < 6; ++i)
	{
		Code.AppendChar(Alphabet[FMath::RandRange(0, Len - 1)]);
	}
	return Code;
}

void UShooterGameInstance::ResetRoom(EShooterGameMode Mode)
{
	Room = FShooterRoomState();
	Room.bActive = true;
	Room.RoomCode = MakeRoomCode();
	Room.Mode = Mode;
	UE_LOG(LogTemp, Log, TEXT("[Room] New room %s mode=%d"), *Room.RoomCode, (int32)Mode);
}

FString UShooterGameInstance::GetRoomKey(const APlayerState* PS)
{
	if (!PS) return FString();
	const FUniqueNetIdRepl& Id = PS->GetUniqueId();
	if (Id.IsValid())
	{
		return Id.ToString();
	}
	return PS->GetPlayerName();
}

FString UShooterGameInstance::GetNameOption() const
{
	FString Name = LocalPlayerName.TrimStartAndEnd();
	// 懒填充：登录完成前就开房时，尽量用 Steam / 系统名，避免计分板变成 Player0
	if (Name.IsEmpty())
	{
		if (IOnlineSubsystem* Steam = IOnlineSubsystem::Get(STEAM_SUBSYSTEM))
		{
			if (IOnlineIdentityPtr Id = Steam->GetIdentityInterface())
			{
				if (Id->GetLoginStatus(0) == ELoginStatus::LoggedIn)
				{
					Name = Id->GetPlayerNickname(0);
				}
			}
		}
		if (Name.IsEmpty()) Name = FPlatformProcess::UserName();
		if (Name.IsEmpty()) Name = FPlatformProcess::ComputerName();
	}
	if (Name.IsEmpty()) return FString();
	Name.ReplaceInline(TEXT("?"), TEXT(""));
	Name.ReplaceInline(TEXT("="), TEXT(""));
	Name.ReplaceInline(TEXT(" "), TEXT("_"));
	return FString::Printf(TEXT("?Name=%s"), *Name.Left(20));
}

void UShooterGameInstance::Shutdown()
{
	ShooterLoadingScreen::Unregister();
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetFailHandle);
		GEngine->OnTravelFailure().Remove(TravelFailHandle);
	}
	Super::Shutdown();
}

void UShooterGameInstance::SetLastOnlineError(const FString& Error)
{
	LastOnlineError = Error;
	UE_LOG(LogTemp, Warning, TEXT("[Online] LastError: %s"), *Error);
}

void UShooterGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	ShooterLoadingScreen::Hide();
	SetLastOnlineError(FString::Printf(TEXT("网络错误（%s）：%s"), ENetworkFailure::ToString(FailureType), *ErrorString));
	GFPS_SessionJoinInProgress = false;

	// PIE / local listen: EOS NetDriver often cannot bind (no local user). Do NOT tear down into main menu —
	// that leaves Client 1 black with PC=NULL. Use IpNetDriver (fps.OnlineUseNull 1) instead.
	if (World && World->WorldType == EWorldType::PIE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Online] NetworkFailure in PIE ignored (no GoToMainMenu): %s - %s"),
			ENetworkFailure::ToString(FailureType), *ErrorString);
		return;
	}
	if (FailureType == ENetworkFailure::NetDriverListenFailure)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Online] NetDriverListenFailure ignored (try fps.OnlineUseNull 1): %s"), *ErrorString);
		return;
	}

	// 对局中掉线 / 房主关服：自动回主菜单，避免卡在空图
	if (World && !MainMenuMap.IsNull())
	{
		const FString MenuPkg = MainMenuMap.ToSoftObjectPath().GetLongPackageName();
		const FString CurPkg = World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
		if (!CurPkg.Equals(MenuPkg, ESearchCase::IgnoreCase))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Online] NetworkFailure -> GoToMainMenu"));
			GoToMainMenu();
		}
	}
}

void UShooterGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	ShooterLoadingScreen::Hide();
	SetLastOnlineError(FString::Printf(TEXT("跳转失败（%s）：%s"), ETravelFailure::ToString(FailureType), *ErrorString));
	GFPS_SessionJoinInProgress = false;
	if (World && !MainMenuMap.IsNull())
	{
		const FString MenuPkg = MainMenuMap.ToSoftObjectPath().GetLongPackageName();
		const FString CurPkg = World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
		if (!CurPkg.Equals(MenuPkg, ESearchCase::IgnoreCase))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Online] TravelFailure -> GoToMainMenu"));
			GoToMainMenu();
		}
	}
}

bool UShooterGameInstance::CreateRoom(EShooterGameMode Mode)
{
	if (Mode == EShooterGameMode::PoliceCriminal)
	{
		EnsurePoliceCriminalTestDefaults(this);
	}
	// EOS NetDriver 需要已登录的本地用户才能 Listen；没登录就别跳图
	if (UShooterSessionSubsystem* Sessions = GetSubsystem<UShooterSessionSubsystem>())
	{
		if (!Sessions->IsOnlineReady())
		{
			SetLastOnlineError(Sessions->GetOnlineStatus().IsEmpty() ? TEXT("在线服务未登录，暂时不能创建房间") : Sessions->GetOnlineStatus());
			return false;
		}
	}
	if (GFPS_SessionJoinInProgress)
	{
		SetLastOnlineError(TEXT("正在加入房间，不能同时创建房间"));
		return false;
	}
	if (GEngine)
	{
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (Ctx.OwningGameInstance == this && Ctx.PendingNetGame)
			{
				SetLastOnlineError(TEXT("正在连接房间，请稍候再创建"));
				return false;
			}
		}
	}
	if (RoomLobbyMap.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Room] CreateRoom failed: RoomLobbyMap not set in BP_ShooterGameInstance"));
		return false;
	}
	SelectedMode = Mode;
	bPracticeMode = false;
	Room = FShooterRoomState(); // 本地当服务器：大厅 GameMode 会新建房间
	// 开房前尽量把 Steam/系统昵称写入 LocalPlayerName，这样 ?Name= 和房主广告一致
	if (LocalPlayerName.TrimStartAndEnd().IsEmpty())
	{
		FString Hint = GetNameOption(); // 懒填充读 Steam/系统名
		if (!Hint.IsEmpty() && Hint.StartsWith(TEXT("?Name=")))
		{
			LocalPlayerName = Hint.RightChop(6);
		}
	}
	TArray<FString> Extra;
	Extra.Add(FString::Printf(TEXT("Mode=%d"), (int32)Mode));
	Extra.Add(TEXT("Host=1"));
	const FString NameOpt = GetNameOption();
	if (!NameOpt.IsEmpty()) Extra.Add(NameOpt.RightChop(1)); // 去掉开头的 ?
	return TravelTo(RoomLobbyMap, LobbyGameModeClass, true, Extra);
}

bool UShooterGameInstance::CreateRoomOnServer(const FString& Address, EShooterGameMode Mode)
{
	const FString Trimmed = Address.TrimStartAndEnd();
	APlayerController* PC = GetFirstLocalPlayerController();
	if (Trimmed.IsEmpty() || !IsValid(PC)) return false;
	SelectedMode = Mode;
	PC->ClientTravel(FString::Printf(TEXT("%s?Mode=%d?Host=1%s"), *Trimmed, (int32)Mode, *GetNameOption()), TRAVEL_Absolute);
	return true;
}


// 控制台：fps.Practice bomb | fps.Practice tdm

namespace
{
	void ForceLocalLanMode(UGameInstance* GIObj)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("fps.OnlineUseNull")))
		{
			if (CVar->GetInt() == 0)
			{
				CVar->Set(1, ECVF_SetByCode);
				UE_LOG(LogTemp, Log, TEXT("[LocalMP] fps.OnlineUseNull -> 1 (IpNetDriver)"));
			}
		}
		// CVar change callback (OnUseNullChanged) already calls ApplyNetDriverForMode.
		(void)GIObj;
	}
}

void UShooterGameInstance::EnsureLocalLanMode()
{
	ForceLocalLanMode(this);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Cyan, TEXT("[LocalMP] 局域网模式已开（Null / IpNetDriver）"));
	}
}

bool UShooterGameInstance::HostLocalListen(FString Target)
{
	ForceLocalLanMode(this);
	ClearLastOnlineError();

	FString Key = Target.TrimStartAndEnd().ToLower();
	if (Key.IsEmpty())
	{
		Key = TEXT("range");
	}

	bPracticeMode = false;
	Room = FShooterRoomState();

	if (Key.Contains(TEXT("lobby")) || Key.Contains(TEXT("room")))
	{
		UE_LOG(LogTemp, Log, TEXT("[LocalMP] HostLocalListen -> lobby (CreateRoom PoliceCriminal)"));
		const bool bOk = CreateRoom(EShooterGameMode::PoliceCriminal);
		if (!bOk && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Orange, TEXT("[LocalMP] 开大厅失败，检查 RoomLobbyMap / 在线状态"));
		}
		return bOk;
	}

	if (Key.Contains(TEXT("tdm")) || Key.Contains(TEXT("team")) || Key.Contains(TEXT("bomb")) || Key.Contains(TEXT("sd")) || Key.Contains(TEXT("bdm")))
	{
		UE_LOG(LogTemp, Log, TEXT("[LocalMP] arena mode aliases now open PoliceCriminal"));
		Key = TEXT("pc");
	}

if (Key.Contains(TEXT("pc")) || Key.Contains(TEXT("police")) || Key.Contains(TEXT("criminal")) || Key.Contains(TEXT("jingfei")))
	{
		EnsurePoliceCriminalTestDefaults(this);
		if (!IsModeAvailable(EShooterGameMode::PoliceCriminal))
		{
			SetLastOnlineError(TEXT("PoliceCriminal map not configured"));
			UE_LOG(LogTemp, Warning, TEXT("[LocalMP] PoliceCriminal mode map missing"));
			return false;
		}
		SelectedMode = EShooterGameMode::PoliceCriminal;
		const FShooterModeInfo& Info = Modes.FindChecked(EShooterGameMode::PoliceCriminal);
		UE_LOG(LogTemp, Log, TEXT("[LocalMP] HostLocalListen -> PoliceCriminal listen"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("[LocalMP] PoliceCriminal listen (test map). Other window: fps.LocalJoin"));
		}
		return TravelTo(Info.Map, Info.GameModeClass, true);
	}

		if (RangeMap.IsNull())
	{
		SetLastOnlineError(TEXT("RangeMap 未配置"));
		UE_LOG(LogTemp, Warning, TEXT("[LocalMP] RangeMap not set"));
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[LocalMP] HostLocalListen -> range listen"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("[LocalMP] 本机房主：靶场 listen。另一窗口 fps.LocalJoin"));
	}
	return TravelTo(RangeMap, RangeGameMode, true);
}

bool UShooterGameInstance::JoinLocalhost(FString Address)
{
	FString Addr = Address.TrimStartAndEnd();
	if (Addr.IsEmpty())
	{
		Addr = TEXT("127.0.0.1");
	}
	ForceLocalLanMode(this);
	UE_LOG(LogTemp, Log, TEXT("[LocalMP] JoinLocalhost %s"), *Addr);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Cyan, FString::Printf(TEXT("[LocalMP] 正在加入 %s …"), *Addr));
	}
	return JoinByIP(Addr);
}

// 控制台：fps.LocalHost range|tdm|bomb|lobby

// Console: fps.StartPractice [pc|tdm|bomb] — main-menu logic test, no room
static FAutoConsoleCommandWithWorldAndArgs GFPSStartPracticeCmd(
	TEXT("fps.StartPractice"),
	TEXT("Solo practice travel (no room). Usage: fps.StartPractice [pc|PoliceCriminal|tdm|bomb]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		UShooterGameInstance* GI = Cast<UShooterGameInstance>(World->GetGameInstance());
		if (!GI)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Practice] No ShooterGameInstance"));
			return;
		}
		EnsurePoliceCriminalTestDefaults(GI);
		FString Key = Args.Num() > 0 ? Args[0].TrimStartAndEnd().ToLower() : TEXT("pc");
		EShooterGameMode Mode = EShooterGameMode::PoliceCriminal;
		if (!GI->StartPractice(Mode))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Practice] StartPractice failed mode=%d err=%s"), (int32)Mode, *GI->GetLastOnlineError());
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Orange, TEXT("[Practice] failed: check Modes map / GameMode"));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GFPSLocalHostCmd(
	TEXT("fps.LocalHost"),
	TEXT("Local listen-server test (forces Null/IpNet). Usage: fps.LocalHost range|tdm|bomb|lobby|pc"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		UShooterGameInstance* GI = Cast<UShooterGameInstance>(World->GetGameInstance());
		if (!GI)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LocalMP] No ShooterGameInstance"));
			return;
		}
		const FString Target = Args.Num() > 0 ? Args[0] : TEXT("range");
		if (!GI->HostLocalListen(Target))
		{
			UE_LOG(LogTemp, Warning, TEXT("[LocalMP] HostLocalListen failed: %s"), *GI->GetLastOnlineError());
		}
	}));

// 控制台：fps.LocalJoin [127.0.0.1]
static FAutoConsoleCommandWithWorldAndArgs GFPSLocalJoinCmd(
	TEXT("fps.LocalJoin"),
	TEXT("Join local listen host. Usage: fps.LocalJoin [ip[:port]]  (default 127.0.0.1)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		UShooterGameInstance* GI = Cast<UShooterGameInstance>(World->GetGameInstance());
		if (!GI)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LocalMP] No ShooterGameInstance"));
			return;
		}
		const FString Addr = Args.Num() > 0 ? Args[0] : TEXT("127.0.0.1");
		if (!GI->JoinLocalhost(Addr))
		{
			UE_LOG(LogTemp, Warning, TEXT("[LocalMP] JoinLocalhost failed"));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GFPSPracticeCmd(
	TEXT("fps.Practice"),
	TEXT("Solo practice without a room. Usage: fps.Practice bomb|tdm"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		UShooterGameInstance* GI = Cast<UShooterGameInstance>(World->GetGameInstance());
		if (!GI)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Practice] No ShooterGameInstance"));
			return;
		}
		EShooterGameMode Mode = EShooterGameMode::PoliceCriminal;
		if (!GI->StartPractice(Mode))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Practice] StartPractice failed — check Modes map on BP_ShooterGameInstance"));
		}
	}));


