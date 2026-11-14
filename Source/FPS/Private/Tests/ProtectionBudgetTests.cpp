#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ShooterCharacter.h"
#include "Character/PoliceAppearanceComponent.h"
#include "Loadout/PouchInventoryComponent.h"
#include "Weapon/WeaponFlash.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProtectionBudgetTest,"FPS.PoliceAppearance.ProtectionBudget",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProtectionBudgetTest::RunTest(const FString&){
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);auto* C=W->SpawnActor<AShooterCharacter>();auto* P=C->GetPouch();
 C->SetBodyArmorLocal(EBodyArmorType::Light);C->PoliceAppearance->FaceProtection=EPoliceFaceProtection::FlashGoggles;
 TestEqual(TEXT("Light armor and goggles keep all thirteen slots"),P->GetSlotCount(),13);
 for(int32 I=0;I<13;++I)TestTrue(TEXT("Can fill baseline capacity"),P->TryAddThrowable(AWeaponFlash::StaticClass())!=INDEX_NONE);
 C->SetBodyArmorLocal(EBodyArmorType::Heavy);TestEqual(TEXT("Heavy armor reserves one carrying slot"),P->GetSlotCount(),12);P->ReconcileCapacity();TestEqual(TEXT("Old overflow cannot remain usable"),P->CountUsed(),12);
 C->PoliceAppearance->FaceProtection=EPoliceFaceProtection::GogglesAndMask;P->ReconcileCapacity();TestEqual(TEXT("Mask penalty stacks with armor"),P->GetSlotCount(),11);TestEqual(TEXT("Combined occupancy bounded"),P->CountUsed(),11);TestEqual(TEXT("Cannot add beyond equipment-adjusted capacity"),P->TryAddThrowable(AWeaponFlash::StaticClass()),INDEX_NONE);
 TestTrue(TEXT("Movement penalties multiply"),FMath::IsNearlyEqual(C->GetArmorSpeedMultiplier(),.78f*.95f));
 C->SetBodyArmorLocal(EBodyArmorType::Light);TestEqual(TEXT("Mask alone reserves exactly one slot"),P->GetSlotCount(),12);TestTrue(TEXT("Mask alone slows movement"),FMath::IsNearlyEqual(C->GetArmorSpeedMultiplier(),.95f));
 C->PoliceAppearance->FaceProtection=EPoliceFaceProtection::GasMask;TestEqual(TEXT("Gas mask does not inherit ballistic mask penalty"),P->GetSlotCount(),13);TestTrue(TEXT("Light plus gas mask keeps base speed"),FMath::IsNearlyEqual(C->GetArmorSpeedMultiplier(),1.f));
 W->DestroyWorld(false);return true;
}
#endif
