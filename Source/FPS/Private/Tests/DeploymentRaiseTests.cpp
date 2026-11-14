#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CombatComponent.h"
#include "ShooterCharacter.h"
#include "WeaponData.h"
#include "Weapon/Weapon.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/WorldSettings.h"
#include "Components/SkeletalMeshComponent.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeploymentRaiseFallbackTest,"FPS.Mission.DeploymentRaiseFallback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeploymentRaiseFallbackTest::RunTest(const FString&){
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL());World->GetWorldSettings()->NotifyBeginPlay();
 auto* Character=World->SpawnActor<AShooterCharacter>();auto* Combat=Character->GetCombatComponent();
 TestFalse(TEXT("Wait for required weapon data instead of acknowledging an unspawned gun"),Combat->BeginDeploymentRaise());
 Combat->WeaponData=NewObject<UWeaponData>();Combat->CurrentWeapon=World->SpawnActor<AWeapon>();
 TestTrue(TEXT("Missing optional equip clip is accepted, preventing a permanent briefing lock"),Combat->BeginDeploymentRaise());
 TestFalse(TEXT("No montage means the next presentation tick can acknowledge completion"),Combat->IsDeploymentRaisePlaying());
 TestEqual(TEXT("Offscreen first-person arms continue animation during recon"),Character->GetMesh1P_Implementation()->VisibilityBasedAnimTickOption,EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones);
 Combat->CurrentWeapon->Destroy();Character->Destroy();
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
