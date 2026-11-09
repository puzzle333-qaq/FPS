#include "Loadout/WarehouseLoadoutSaveGame.h"

#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Crc.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Weapon/AttachmentData.h"
#include "Weapon/Weapon.h"

namespace
{
	FString SavePath(const FString& Slot)
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), FPaths::GetCleanFilename(Slot) + TEXT(".sav"));
	}

	UWarehouseLoadoutSaveGame* ReadSave(const FString& Path)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path)) return nullptr;
		// Validate the complete payload before asking UE to deserialize any object data.
		if (Bytes.Num() < 12 || Bytes.Num() > 1024 * 1024) return nullptr;
		FMemoryReader Reader(Bytes);
		uint32 Magic = 0, Checksum = 0;
		int32 Size = 0;
		Reader << Magic << Size << Checksum;
		if (Magic != 0x57484C44 || Size <= 0 || Size != Bytes.Num() - 12) return nullptr;
		if (FCrc::MemCrc32(Bytes.GetData() + 12, Size) != Checksum) return nullptr;
		TArray<uint8> Payload;
		Payload.Append(Bytes.GetData() + 12, Size);
		UWarehouseLoadoutSaveGame* Save = Cast<UWarehouseLoadoutSaveGame>(UGameplayStatics::LoadGameFromMemory(Payload));
		return Save && Save->Version == 1 && Save->Weapons.Num() > 0 && Save->Weapons.Num() <= 8 ? Save : nullptr;
	}
}

bool UWarehouseLoadoutSaveGame::SaveLoadout(const FLoadoutPreset& Loadout, const FString& Slot)
{
	if (Loadout.Weapons.IsEmpty() || Loadout.Weapons.Num() > 8) return false;
	UWarehouseLoadoutSaveGame* Save = NewObject<UWarehouseLoadoutSaveGame>();
	for (const FWeaponPreset& Preset : Loadout.Weapons)
	{
		if (!Preset.WeaponClass) continue;
		FWarehouseSavedWeapon& Saved = Save->Weapons.AddDefaulted_GetRef();
		Saved.WeaponClass = FSoftClassPath(Preset.WeaponClass.Get());
		Saved.StackCount = FMath::Clamp(Preset.StackCount, 0, 13);
		for (const UAttachmentData* Attachment : Preset.Attachments)
		{
			if (IsValid(Attachment)) Saved.Attachments.Add(FSoftObjectPath(Attachment));
		}
	}
	TArray<uint8> Bytes;
	if (Save->Weapons.IsEmpty() || !UGameplayStatics::SaveGameToMemory(Save, Bytes)) return false;
	FBufferArchive FileBytes;
	uint32 Magic = 0x57484C44;
	int32 Size = Bytes.Num();
	uint32 Checksum = FCrc::MemCrc32(Bytes.GetData(), Size);
	FileBytes << Magic << Size << Checksum;
	FileBytes.Append(Bytes);

	const FString Path = SavePath(Slot);
	const FString Temp = Path + TEXT(".tmp");
	IFileManager& Files = IFileManager::Get();
	if (!Files.MakeDirectory(*FPaths::GetPath(Path), true) || !FFileHelper::SaveArrayToFile(FileBytes, *Temp)) return false;
	// Keep the last valid file before replacing it. A failed write never overwrites the backup.
	if (ReadSave(Path) && Files.Copy(*(Path + TEXT(".bak")), *Path, true, true) != COPY_OK)
	{
		Files.Delete(*Temp);
		return false;
	}
	if (!Files.Move(*Path, *Temp, true, true))
	{
		Files.Delete(*Temp);
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[Warehouse] Saved %d weapon presets to %s"), Save->Weapons.Num(), *Path);
	return true;
}

bool UWarehouseLoadoutSaveGame::LoadLoadout(FLoadoutPreset& OutLoadout, const FString& Slot)
{
	OutLoadout = FLoadoutPreset();
	const FString Path = SavePath(Slot);
	UWarehouseLoadoutSaveGame* Save = ReadSave(Path);
	if (!Save) Save = ReadSave(Path + TEXT(".bak"));
	if (!Save) return false;
	OutLoadout.Name = NSLOCTEXT("Warehouse", "SavedLoadout", "仓库配装");
	OutLoadout.bReplaceThrowables = true;
	for (const FWarehouseSavedWeapon& Saved : Save->Weapons)
	{
		UClass* Class = Saved.WeaponClass.TryLoadClass<AWeapon>();
		if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated)) continue;
		const AWeapon* CDO = Class->GetDefaultObject<AWeapon>();
		if (CDO->WeaponKind == EWeaponKind::Bomb) continue;
		FWeaponPreset& Preset = OutLoadout.Weapons.AddDefaulted_GetRef();
		Preset.WeaponClass = Class;
		Preset.StackCount = FMath::Clamp(Saved.StackCount, 0, 13);
		for (const FSoftObjectPath& AttachmentPath : Saved.Attachments)
		{
			UAttachmentData* Data = Cast<UAttachmentData>(AttachmentPath.TryLoad());
			if (!IsValid(Data) || !CDO->CanEquipAttachment(Data)) continue;
			const int32 Existing = Preset.Attachments.IndexOfByPredicate([&](const UAttachmentData* Other)
			{
				return CDO->GetEquipSlot(Other) == CDO->GetEquipSlot(Data);
			});
			if (Existing == INDEX_NONE) Preset.Attachments.Add(Data);
			else if ((Data->bProvidesLaser && Data->bProvidesFlashlight)
				|| !(Preset.Attachments[Existing]->bProvidesLaser && Preset.Attachments[Existing]->bProvidesFlashlight))
			{
				// Old saves may contain both former slots; retain the combined device.
				Preset.Attachments[Existing] = Data;
			}
		}
	}
	return !OutLoadout.Weapons.IsEmpty();
}
