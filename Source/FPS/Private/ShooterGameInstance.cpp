#include "ShooterGameInstance.h"

#include "Loadout/ShooterLoadoutSave.h"
#include "Weapon/Weapon.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerState.h"
#include "Online/ShooterSessionSubsystem.h"

UShooterGameInstance::UShooterGameInstance()
{
	auto AddMode = [this](EShooterGameMode Mode, const TCHAR* Name, int32 MaxPlayers)
	{
		FShooterModeInfo Info;
		Info.DisplayName = FText::FromString(Name);
		Info.MaxPlayers = MaxPlayers;
		Modes.Add(Mode, Info);
	};
	AddMode(EShooterGameMode::TDM, TEXT("团队竞技"), 12);
	AddMode(EShooterGameMode::FFA, TEXT("死斗"), 8);
	AddMode(EShooterGameMode::SearchDestroy, TEXT("爆破"), 10);
	AddMode(EShooterGameMode::Story, TEXT("剧情"), 4);
	AddMode(EShooterGameMode::Duel, TEXT("单挑"), 2);
	AddMode(EShooterGameMode::Extraction, TEXT("搜打撤"), 12);
}

void UShooterGameInstance::Init()
{
	Super::Init();
	LoadLoadout();
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
	if (!IsModeAvailable(Mode))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Menu] Mode %d is not implemented or has no map."), static_cast<int32>(Mode));
		return false;
	}
	SelectedMode = Mode;
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
	return TravelTo(RangeMap, RangeGameMode, false);
}

bool UShooterGameInstance::GoToMainMenu()
{
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
	if (Name.IsEmpty()) return FString();
	Name.ReplaceInline(TEXT("?"), TEXT(""));
	Name.ReplaceInline(TEXT("="), TEXT(""));
	Name.ReplaceInline(TEXT(" "), TEXT("_"));
	return FString::Printf(TEXT("?Name=%s"), *Name.Left(20));
}

bool UShooterGameInstance::CreateRoom(EShooterGameMode Mode)
{
	if (RoomLobbyMap.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Room] CreateRoom failed: RoomLobbyMap not set in BP_ShooterGameInstance"));
		return false;
	}
	SelectedMode = Mode;
	Room = FShooterRoomState(); // 本地当服务器：大厅 GameMode 会新建房间
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
