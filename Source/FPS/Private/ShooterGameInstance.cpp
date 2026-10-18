#include "ShooterGameInstance.h"

#include "Loadout/ShooterLoadoutSave.h"
#include "Weapon/Weapon.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

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

bool UShooterGameInstance::TravelTo(const TSoftObjectPtr<UWorld>& Map, const TSoftClassPtr<AGameModeBase>& GameMode, bool bListen)
{
	if (Map.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Menu] Travel failed: map is not set."));
		return false;
	}

	TArray<FString> Options;
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
	const FShooterModeInfo& Info = Modes.FindChecked(Mode);
	return TravelTo(Info.Map, Info.GameModeClass, true);
}

bool UShooterGameInstance::JoinByIP(const FString& Address)
{
	const FString Trimmed = Address.TrimStartAndEnd();
	APlayerController* PC = GetFirstLocalPlayerController();
	if (Trimmed.IsEmpty() || !IsValid(PC)) return false;
	PC->ClientTravel(Trimmed, TRAVEL_Absolute);
	return true;
}

bool UShooterGameInstance::OpenRange()
{
	return TravelTo(RangeMap, RangeGameMode, false);
}

bool UShooterGameInstance::GoToMainMenu()
{
	return TravelTo(MainMenuMap, nullptr, false);
}

void UShooterGameInstance::SetWeaponPreset(const FWeaponPreset& Preset)
{
	if (!Preset.WeaponClass) return;
	const EWeaponKind Kind = Preset.WeaponClass->GetDefaultObject<AWeapon>()->WeaponKind;
	for (FWeaponPreset& Existing : CurrentLoadout.Weapons)
	{
		if (Existing.WeaponClass && Existing.WeaponClass->GetDefaultObject<AWeapon>()->WeaponKind == Kind)
		{
			Existing = Preset;
			return;
		}
	}
	CurrentLoadout.Weapons.Add(Preset);
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