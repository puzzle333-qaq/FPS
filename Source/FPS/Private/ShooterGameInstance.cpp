#include "ShooterGameInstance.h"

#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Ui/ShooterLoadingScreen.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	static const TCHAR* GCoopTestMapPath = TEXT("/Game/FPS/Level/Lvl_Range.Lvl_Range");
	static const TCHAR* GCoopGameModePath = TEXT("/Script/FPS.CoopCqbGameMode");

	void EnsureCoopCqbDefaults(UShooterGameInstance* GI)
	{
		if (!GI) return;
		FShooterModeInfo& Info = GI->Modes.FindOrAdd(EShooterGameMode::CoopCQB);
		if (Info.DisplayName.IsEmpty())
		{
			Info.DisplayName = FText::FromString(TEXT("Coop CQB"));
		}
		if (Info.Map.IsNull())
		{
			Info.Map = !GI->RangeMap.IsNull()
				? GI->RangeMap
				: TSoftObjectPtr<UWorld>(FSoftObjectPath(GCoopTestMapPath));
		}
		if (Info.GameModeClass.IsNull())
		{
			Info.GameModeClass = TSoftClassPtr<AGameModeBase>(FSoftObjectPath(GCoopGameModePath));
		}
		if (Info.MaxPlayers < 1)
		{
			Info.MaxPlayers = 4;
		}
		Info.bImplemented = true;
	}

	static TAutoConsoleVariable<int32> CVarOnlineUseNull(
		TEXT("fps.OnlineUseNull"),
		1,
		TEXT("If 1, GameNetDriver uses IpNetDriver so a listen server does not need EOS login."),
		ECVF_Default);

	void ForceLocalLanMode()
	{
		CVarOnlineUseNull->Set(1, ECVF_SetByCode);
		if (!GEngine) return;
		const FName IpDriver(TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"));
		for (FNetDriverDefinition& Def : GEngine->NetDriverDefinitions)
		{
			if (Def.DefName == NAME_GameNetDriver)
			{
				Def.DriverClassName = IpDriver;
				Def.DriverClassNameFallback = IpDriver;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[LocalMP] GameNetDriver -> IpNetDriver"));
	}
}

UShooterGameInstance::UShooterGameInstance()
{
	FShooterModeInfo Info;
	Info.DisplayName = FText::FromString(TEXT("Coop CQB"));
	Info.MaxPlayers = 4;
	Info.bImplemented = true;
	Modes.Add(EShooterGameMode::CoopCQB, Info);
}

void UShooterGameInstance::Init()
{
	Super::Init();
	ShooterLoadingScreen::Register(this);
	EnsureCoopCqbDefaults(this);

	if (GEngine)
	{
		NetFailHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
		TravelFailHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
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

bool UShooterGameInstance::TravelSquadTo(const TSoftObjectPtr<UWorld>& Map, const TSoftClassPtr<AGameModeBase>& GameMode, const TArray<FString>& ExtraOptions)
{
 UWorld* World = GetWorld();
 if (!World || Map.IsNull() || World->GetNetMode() == NM_Client) return false;
 if (World->GetNetMode() == NM_Standalone) return TravelTo(Map, GameMode, false, ExtraOptions);
 TArray<FString> Options = ExtraOptions;
 if (World->GetNetMode() == NM_ListenServer) Options.AddUnique(TEXT("listen"));
 if (!GameMode.IsNull()) Options.Add(FString::Printf(TEXT("game=%s"), *GameMode.ToSoftObjectPath().ToString()));
 // Fresh controllers deliberately acknowledge possession and streaming in the destination.
 if (auto* CurrentMode = World->GetAuthGameMode()) CurrentMode->bUseSeamlessTravel = false;
 return World->ServerTravel(Map.ToSoftObjectPath().GetLongPackageName() + TEXT("?") + FString::Join(Options, TEXT("?")), true);
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
	if (Mode == EShooterGameMode::CoopCQB)
	{
		EnsureCoopCqbDefaults(this);
	}
	if (!IsModeAvailable(Mode))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Practice] Mode %d is not implemented or has no map."), static_cast<int32>(Mode));
		return false;
	}
	bPracticeMode = true;
	SelectedMode = Mode;
	const FShooterModeInfo& Info = Modes.FindChecked(Mode);
	TArray<FString> Extra;
	Extra.Add(TEXT("Practice=1"));
	const FString NameOpt = GetNameOption();
	if (!NameOpt.IsEmpty()) Extra.Add(NameOpt.RightChop(1));
	UE_LOG(LogTemp, Log, TEXT("[Practice] Solo mode=%d"), (int32)Mode);
	return TravelTo(Info.Map, Info.GameModeClass, false, Extra);
}

bool UShooterGameInstance::GoToMainMenu()
{
	bPracticeMode = false;
	return TravelTo(MainMenuMap, nullptr, false);
}

FString UShooterGameInstance::GetNameOption() const
{
	FString Name = LocalPlayerName.TrimStartAndEnd();
	if (Name.IsEmpty()) Name = FPlatformProcess::UserName();
	if (Name.IsEmpty()) Name = FPlatformProcess::ComputerName();
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
	(void)NetDriver;
	ShooterLoadingScreen::Hide();
	SetLastOnlineError(FString::Printf(TEXT("网络错误（%s）：%s"), ENetworkFailure::ToString(FailureType), *ErrorString));

	if (World && World->WorldType == EWorldType::PIE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Online] NetworkFailure in PIE ignored: %s - %s"),
			ENetworkFailure::ToString(FailureType), *ErrorString);
		return;
	}
	if (FailureType == ENetworkFailure::NetDriverListenFailure)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Online] NetDriverListenFailure ignored: %s"), *ErrorString);
		return;
	}

	if (World && !MainMenuMap.IsNull())
	{
		const FString MenuPkg = MainMenuMap.ToSoftObjectPath().GetLongPackageName();
		const FString CurPkg = World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
		if (!CurPkg.Equals(MenuPkg, ESearchCase::IgnoreCase))
		{
			GoToMainMenu();
		}
	}
}

void UShooterGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	ShooterLoadingScreen::Hide();
	SetLastOnlineError(FString::Printf(TEXT("跳转失败（%s）：%s"), ETravelFailure::ToString(FailureType), *ErrorString));
	if (World && !MainMenuMap.IsNull())
	{
		const FString MenuPkg = MainMenuMap.ToSoftObjectPath().GetLongPackageName();
		const FString CurPkg = World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
		if (!CurPkg.Equals(MenuPkg, ESearchCase::IgnoreCase))
		{
			GoToMainMenu();
		}
	}
}

void UShooterGameInstance::EnsureLocalLanMode()
{
	ForceLocalLanMode();
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Cyan, TEXT("[LocalMP] 局域网模式已开（Null / IpNetDriver）"));
	}
}

bool UShooterGameInstance::HostLocalListen(FString Target)
{
	ForceLocalLanMode();
	ClearLastOnlineError();

	FString Key = Target.TrimStartAndEnd().ToLower();
	if (Key.IsEmpty())
	{
		Key = TEXT("range");
	}

	bPracticeMode = false;

	if (Key.Contains(TEXT("coop")) || Key.Contains(TEXT("cqb")) || Key.Contains(TEXT("pc")) || Key.Contains(TEXT("police")))
	{
		EnsureCoopCqbDefaults(this);
		if (!IsModeAvailable(EShooterGameMode::CoopCQB))
		{
			SetLastOnlineError(TEXT("Coop CQB map is not configured"));
			return false;
		}
		SelectedMode = EShooterGameMode::CoopCQB;
		const FShooterModeInfo& Info = Modes.FindChecked(EShooterGameMode::CoopCQB);
		UE_LOG(LogTemp, Log, TEXT("[LocalMP] HostLocalListen -> Coop CQB"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("[LocalMP] Coop CQB listen. Other window: fps.LocalJoin"));
		}
		return TravelTo(Info.Map, Info.GameModeClass, true);
	}

	if (RangeMap.IsNull())
	{
		SetLastOnlineError(TEXT("RangeMap is not set"));
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[LocalMP] HostLocalListen -> range"));
	return TravelTo(RangeMap, RangeGameMode, true);
}

bool UShooterGameInstance::JoinLocalhost(FString Address)
{
	FString Addr = Address.TrimStartAndEnd();
	if (Addr.IsEmpty())
	{
		Addr = TEXT("127.0.0.1");
	}
	ForceLocalLanMode();
	UE_LOG(LogTemp, Log, TEXT("[LocalMP] JoinLocalhost %s"), *Addr);
	return JoinByIP(Addr);
}

static FAutoConsoleCommandWithWorldAndArgs GFPSStartPracticeCmd(
	TEXT("fps.StartPractice"),
	TEXT("Solo travel with no room. Usage: fps.StartPractice [coop]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (!World) return;
		UShooterGameInstance* GI = Cast<UShooterGameInstance>(World->GetGameInstance());
		if (!GI) return;
		if (!GI->StartPractice(EShooterGameMode::CoopCQB))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Practice] StartPractice failed: %s"), *GI->GetLastOnlineError());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GFPSLocalHostCmd(
	TEXT("fps.LocalHost"),
	TEXT("Local listen-server. Usage: fps.LocalHost range|coop"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		UShooterGameInstance* GI = Cast<UShooterGameInstance>(World->GetGameInstance());
		if (!GI) return;
		const FString Target = Args.Num() > 0 ? Args[0] : TEXT("range");
		if (!GI->HostLocalListen(Target))
		{
			UE_LOG(LogTemp, Warning, TEXT("[LocalMP] HostLocalListen failed: %s"), *GI->GetLastOnlineError());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GFPSLocalJoinCmd(
	TEXT("fps.LocalJoin"),
	TEXT("Join a local listen host. Usage: fps.LocalJoin [ip[:port]]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		UShooterGameInstance* GI = Cast<UShooterGameInstance>(World->GetGameInstance());
		if (!GI) return;
		const FString Addr = Args.Num() > 0 ? Args[0] : TEXT("127.0.0.1");
		GI->JoinLocalhost(Addr);
	}));

static FAutoConsoleCommandWithWorldAndArgs GFPSPracticeCmd(
	TEXT("fps.Practice"),
	TEXT("Solo coop practice. Usage: fps.Practice"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (!World) return;
		UShooterGameInstance* GI = Cast<UShooterGameInstance>(World->GetGameInstance());
		if (!GI) return;
		GI->StartPractice(EShooterGameMode::CoopCQB);
	}));
