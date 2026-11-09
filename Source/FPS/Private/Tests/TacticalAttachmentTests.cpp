#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Loadout/WarehouseLoadoutSaveGame.h"
#include "Misc/Paths.h"
#include "ShooterGamePlayTags.h"
#include "Ui/RangeLoadoutWidget.h"
#include "Weapon/Weapon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombinedTacticalAttachmentTest, "FPS.Attachments.CombinedTactical",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombinedTacticalAttachmentTest::RunTest(const FString& Parameters)
{
	UAttachmentData* Device = LoadObject<UAttachmentData>(nullptr, TEXT("/Game/FPS/Data/Kinemation/DA_Kinemation_Laser.DA_Kinemation_Laser"));
	if (!TestNotNull(TEXT("Combined laser data exists"), Device)) return false;
	TestTrue(TEXT("One device provides both functions"), Device->bProvidesLaser && Device->bProvidesFlashlight);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	World->InitializeActorsForPlay(FURL());
	const TCHAR* Guns[] = { TEXT("TR15"), TEXT("Mk14"), TEXT("SRM12"), TEXT("MKR9"), TEXT("MX68") };
	for (const TCHAR* Name : Guns)
	{
		const FString Path = FString::Printf(TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_%s.BP_Weapon_%s_C"), Name, Name);
		UClass* Class = LoadClass<AWeapon>(nullptr, *Path);
		if (!TestNotNull(FString::Printf(TEXT("%s blueprint loads"), Name), Class)) continue;
		AWeapon* Weapon = World->SpawnActor<AWeapon>(Class, FVector::ZeroVector, FRotator::ZeroRotator);
		Weapon->EnsureDeviceSlots();
		TestFalse(FString::Printf(TEXT("%s has no separate flashlight slot"), Name), Weapon->HasAttachmentSlot(ShooterTags::Tag_Attachment_Slot_Flashlight));
		TestEqual(FString::Printf(TEXT("%s resolves the authored rail"), Name), Weapon->ResolveAttachmentSocket(Device), FName(TEXT("TacticleSocket")));
		TestTrue(FString::Printf(TEXT("%s equips the combined device"), Name), Weapon->EquipAttachment(Device));
		const FAttachmentVisual* Visual = Weapon->AttachmentVisuals.FindByPredicate([&](const FAttachmentVisual& V) { return V.Data == Device; });
		if (TestNotNull(FString::Printf(TEXT("%s device visual spawned"), Name), Visual))
		{
			if (TestNotNull(TEXT("First person device mesh exists"), Visual->Mesh1P.Get()))
			{
				TestEqual(TEXT("1P uses TacticleSocket"), Visual->Mesh1P->GetAttachSocketName(), FName(TEXT("TacticleSocket")));
				TestTrue(TEXT("1P rail actually exists"), Weapon->Mesh1P->DoesSocketExist(TEXT("TacticleSocket")));
				TestTrue(TEXT("1P snaps without legacy separation offset"), Visual->Mesh1P->GetRelativeTransform().Equals(FTransform::Identity));
			}
			if (TestNotNull(TEXT("Third person device mesh exists"), Visual->Mesh3P.Get()))
			{
				TestEqual(TEXT("3P uses TacticleSocket"), Visual->Mesh3P->GetAttachSocketName(), FName(TEXT("TacticleSocket")));
				TestTrue(TEXT("3P rail actually exists"), Weapon->Mesh3P->DoesSocketExist(TEXT("TacticleSocket")));
			}
		}
		TestTrue(TEXT("Combined device has a flashlight"), Weapon->HasFlashlight());
		TestTrue(TEXT("Combined device has a laser"), Weapon->HasLaser());
		TestNotNull(TEXT("1P spotlight created"), Weapon->Flashlight1P.Get());
		TestNotNull(TEXT("3P spotlight created"), Weapon->Flashlight3P.Get());
		if (Weapon->Flashlight1P) TestEqual(TEXT("Light uses its real emitter socket"), Weapon->Flashlight1P->GetAttachSocketName(), FName(TEXT("Light_Sckt")));
		if (TestNotNull(TEXT("Laser beam created"), Weapon->LaserBeam.Get())) TestEqual(TEXT("Laser uses its real emitter socket"), Weapon->LaserBeam->GetAttachSocketName(), FName(TEXT("Laser_Sckt")));
		Weapon->ToggleFlashlight();
		TestTrue(TEXT("Flashlight switches on"), Weapon->IsFlashlightOn());
		TestFalse(TEXT("Laser remains off until separately toggled"), Weapon->IsLaserOn());
		Weapon->ToggleLaser();
		TestTrue(TEXT("Both functions can be on together"), Weapon->IsLaserOn() && Weapon->IsFlashlightOn());
		Weapon->ToggleFlashlight();
		TestTrue(TEXT("Turning light off keeps the laser on"), Weapon->IsLaserOn() && !Weapon->IsFlashlightOn());
		Weapon->ToggleLaser();
		TestFalse(TEXT("Laser switches off independently"), Weapon->IsLaserOn());
		URangeLoadoutWidget* Widget = NewObject<URangeLoadoutWidget>();
		Widget->SelectPrimary(Class);
		TestTrue(TEXT("UI can equip the same device as runtime"), Widget->EquipAttachment(Class, Device));
		const TArray<FRangeAttachmentSlotView> Views = Widget->GetWeaponSlots(Class);
		TestEqual(TEXT("UI exposes exactly one combined rail"), Views.FilterByPredicate([](const FRangeAttachmentSlotView& V) { return V.SlotTag == ShooterTags::Tag_Attachment_Slot_Laser; }).Num(), 1);
		TestFalse(TEXT("UI has no independent flashlight row"), Views.ContainsByPredicate([](const FRangeAttachmentSlotView& V) { return V.SlotTag == ShooterTags::Tag_Attachment_Slot_Flashlight; }));
	}
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTacticalSlotCompatibilityTest, "FPS.Attachments.TacticalSlotCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTacticalSlotCompatibilityTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AWeapon* Weapon = World->SpawnActor<AWeapon>();
	UAttachmentData* Legacy = NewObject<UAttachmentData>(Weapon);
	Legacy->SlotTag = ShooterTags::Tag_Attachment_Slot_Tactical;
	Legacy->bProvidesLaser = true;
	FWeaponAttachmentSlot Slot;
	Slot.HipFireSpread = 0.f;
	Slot.SlotTag = ShooterTags::Tag_Attachment_Slot_Tactical;
	Slot.SocketName = TEXT("TacticleSocket");
	Slot.AllowedAttachments.Add(Legacy);
	Weapon->AttachmentSlots = { Slot };
	TestTrue(TEXT("Legacy Tactical-tagged attachment validates against combined rail"), Weapon->CanEquipAttachment(Legacy));
	TestEqual(TEXT("Legacy Tactical tag still resolves the blueprint socket"), Weapon->ResolveAttachmentSocket(Legacy), FName(TEXT("TacticleSocket")));
	const TArray<FWeaponAttachmentSlot> Once = Weapon->GetAttachmentSlots();
	const TArray<FWeaponAttachmentSlot> Twice = AWeapon::NormalizeDeviceSlots(Once);
	TestEqual(TEXT("Normalization keeps one rail"), Twice.Num(), 1);
	TestEqual(TEXT("Normalization is stable"), Twice[0].SocketName, Slot.SocketName);
	World->DestroyWorld(false);

	// A previous warehouse save may contain both old slots, in either order.
	UClass* Class = LoadClass<AWeapon>(nullptr, TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_TR15.BP_Weapon_TR15_C"));
	UAttachmentData* Combo = LoadObject<UAttachmentData>(nullptr, TEXT("/Game/FPS/Data/Kinemation/DA_Kinemation_Laser.DA_Kinemation_Laser"));
	UAttachmentData* Light = LoadObject<UAttachmentData>(nullptr, TEXT("/Game/FPS/Data/Kinemation/DA_Kinemation_LS3_Light.DA_Kinemation_LS3_Light"));
	if (!Class || !Combo || !Light) { AddError(TEXT("Compatibility assets missing")); return false; }
	FLoadoutPreset Loadout;
	FWeaponPreset Preset;
	Preset.WeaponClass = Class;
	Preset.Attachments = { Combo, Light };
	Loadout.Weapons.Add(Preset);
	const FString SaveSlot = TEXT("TacticalTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	TestTrue(TEXT("Write legacy two-device preset"), UWarehouseLoadoutSaveGame::SaveLoadout(Loadout, SaveSlot));
	FLoadoutPreset Restored;
	if (TestTrue(TEXT("Load legacy preset into combined rail"), UWarehouseLoadoutSaveGame::LoadLoadout(Restored, SaveSlot)))
	{
		TestEqual(TEXT("Only one physical device is restored"), Restored.Weapons[0].Attachments.Num(), 1);
		if (Restored.Weapons[0].Attachments.Num()) TestEqual(TEXT("Combined device wins over old standalone light"), Restored.Weapons[0].Attachments[0].Get(), Combo);
	}
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), SaveSlot + TEXT(".sav"));
	IFileManager::Get().Delete(*Path);
	IFileManager::Get().Delete(*(Path + TEXT(".bak")));
	return true;
}

#endif
