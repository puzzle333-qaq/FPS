#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Loadout/WarehouseLoadoutSaveGame.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Weapon/WeaponFlash.h"
#include "Weapon/ThrownProjectile.h"
#include "WeaponData.h"
#include "Ui/RangeLoadoutWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarehouseLoadoutPersistenceTest, "FPS.Warehouse.Persistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarehouseLoadoutPersistenceTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("WarehouseTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), Slot + TEXT(".sav"));
	FLoadoutPreset Loadout;
 Loadout.Name=FText::FromString(TEXT("Night entry"));Loadout.bIncludesProtection=true;
 Loadout.FaceProtection=EPoliceFaceProtection::GasMask;Loadout.BodyArmor=EBodyArmorType::Heavy;
	Loadout.bReplaceThrowables = true;
	FWeaponPreset Flash;
	Flash.WeaponClass = AWeaponFlash::StaticClass();
	Flash.StackCount = 2;
	Loadout.Weapons.Add(Flash);
	FWeaponPreset Rifle;
	Rifle.WeaponClass = AWeapon::StaticClass();
	Rifle.StackCount = 0;
	Loadout.Weapons.Add(Rifle);

	TestTrue(TEXT("Write first loadout"), UWarehouseLoadoutSaveGame::SaveLoadout(Loadout, Slot));
	FLoadoutPreset Restored;
	if (TestTrue(TEXT("Reload from disk into new objects"), UWarehouseLoadoutSaveGame::LoadLoadout(Restored, Slot)))
	{
		TestEqual(TEXT("Preserve weapon class"), Restored.Weapons[0].WeaponClass.Get(), AWeaponFlash::StaticClass());
		TestEqual(TEXT("Preserve throwable count"), Restored.Weapons[0].StackCount, 2);
		TestTrue(TEXT("Explicit pouch counts survive"), Restored.bReplaceThrowables);
  TestEqual(TEXT("Named preset survives"),Restored.Name.ToString(),Loadout.Name.ToString());
  TestTrue(TEXT("Protection included"),Restored.bIncludesProtection);
  TestEqual(TEXT("Gas mask restored"),Restored.FaceProtection,EPoliceFaceProtection::GasMask);
  TestEqual(TEXT("Heavy armor restored"),Restored.BodyArmor,EBodyArmorType::Heavy);
		TestEqual(TEXT("Both weapon slots survive"), Restored.Weapons.Num(), 2);
		if (Restored.Weapons.Num() == 2) TestEqual(TEXT("Zero spare magazines stays zero"), Restored.Weapons[1].StackCount, 0);
	}
	Loadout.Weapons[0].StackCount = 4;
	TestTrue(TEXT("Replace existing save"), UWarehouseLoadoutSaveGame::SaveLoadout(Loadout, Slot));
	TestTrue(TEXT("Reload updated save"), UWarehouseLoadoutSaveGame::LoadLoadout(Restored, Slot));
	if (Restored.Weapons.Num()) TestEqual(TEXT("Updated count"), Restored.Weapons[0].StackCount, 4);

	// Simulate an unreadable primary while keeping the previous valid loadout intact.
	const TArray<uint8> BrokenBytes = {0, 1, 2, 3};
	TestTrue(TEXT("Simulate interrupted save"), FFileHelper::SaveArrayToFile(BrokenBytes, *Path));
	TestTrue(TEXT("Recover backup"), UWarehouseLoadoutSaveGame::LoadLoadout(Restored, Slot));
	if (Restored.Weapons.Num()) TestEqual(TEXT("Backup has previous count"), Restored.Weapons[0].StackCount, 2);

	FLoadoutPreset Empty;
	TestFalse(TEXT("Empty selection cannot destroy saved loadout"), UWarehouseLoadoutSaveGame::SaveLoadout(Empty, Slot));
	IFileManager::Get().Delete(*Path);
	IFileManager::Get().Delete(*(Path + TEXT(".bak")));
	IFileManager::Get().Delete(*(Path + TEXT(".tmp")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarehouseGrenadeConfigurationTest, "FPS.Warehouse.GrenadeConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWarehouseGrenadeConfigurationTest::RunTest(const FString& Parameters)
{
	UClass* GrenadeClass = LoadClass<AWeapon>(nullptr, TEXT("/Game/FPS/Weapon/Throwable/BP_Weapon_Grenade.BP_Weapon_Grenade_C"));
	UClass* FlashClass = LoadClass<AWeapon>(nullptr, TEXT("/Game/FPS/Weapon/BP_WeaponFlash.BP_WeaponFlash_C"));
	UWeaponData* Data = LoadObject<UWeaponData>(nullptr, TEXT("/Game/FPS/Data/DA_WeaponData.DA_WeaponData"));
	if (!TestNotNull(TEXT("Grenade exists"), GrenadeClass)
		|| !TestNotNull(TEXT("Flash exists"), FlashClass)
		|| !TestNotNull(TEXT("Animation data exists"), Data)) return false;
	const AWeapon* Grenade = GrenadeClass->GetDefaultObject<AWeapon>();
	const AWeapon* Flash = FlashClass->GetDefaultObject<AWeapon>();
	TestEqual(TEXT("Flash uses working grenade arms placement"), Flash->HipMesh1POffset, Grenade->HipMesh1POffset);
	TestEqual(TEXT("Flash uses working grenade arms rotation"), Flash->HipMesh1PRotationOffset, Grenade->HipMesh1PRotationOffset);
	TestEqual(TEXT("Flash keeps flash projectile effect"), Flash->ProjectileClass.Get(), AThrownFlash::StaticClass());
	const FPlayerAnim* FlashAnim = Data->FirstPersonAnims.Find(Flash->WeaponType);
	const FPlayerAnim* GrenadeAnim = Data->FirstPersonAnims.Find(Grenade->WeaponType);
	if (TestNotNull(TEXT("Explicit Flash animation row persisted"), FlashAnim)
		&& TestNotNull(TEXT("Grenade animation row exists"), GrenadeAnim))
	{
		TestEqual(TEXT("Same idle as working grenade"), FlashAnim->IdleAnim.Get(), GrenadeAnim->IdleAnim.Get());
		if (FlashAnim->IdleAnim)
		{
			TestTrue(TEXT("Idle comes from requested Grenade folder"), FlashAnim->IdleAnim->GetPathName().StartsWith(TEXT("/Game/FPS_Controller/Animations/FPP_Anims/Grenade/")));
		}
	}
	URangeLoadoutWidget* Widget = NewObject<URangeLoadoutWidget>();
	Widget->Init(nullptr);
	const TArray<FRangeWeaponChoice> Choices = Widget->GetThrowableChoices();
	TestTrue(TEXT("Grenade is available in warehouse"), Choices.ContainsByPredicate([GrenadeClass](const FRangeWeaponChoice& Choice) { return Choice.WeaponClass == GrenadeClass; }));
	TestTrue(TEXT("Flash is available alongside grenade"), Choices.ContainsByPredicate([FlashClass](const FRangeWeaponChoice& Choice) { return Choice.WeaponClass == FlashClass; }));
	TestTrue(TEXT("Can add grenade to pouch"), Widget->AdjustThrowableCount(GrenadeClass, 1));
	TestTrue(TEXT("Can also add flash to pouch"), Widget->AdjustThrowableCount(FlashClass, 1));
	TestEqual(TEXT("Both comparison throwables equipped"), Widget->GetThrowableCount(), 2);
	return true;
}

#endif
