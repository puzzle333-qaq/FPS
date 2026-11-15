#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Game/HostileAICharacter.h"
#include "Game/CompliancePoseComponent.h"
#include "Game/DroppedHostileWeapon.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlashComplianceWeaponDropTest,"FPS.Compliance.FlashDropsWeaponOnce",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFlashComplianceWeaponDropTest::RunTest(const FString&)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL());World->GetWorldSettings()->NotifyBeginPlay();
 auto* Hostile=World->SpawnActor<AHostileAICharacter>(FVector(0,0,100),FRotator::ZeroRotator);
 auto Count=[World](){int32 N=0;for(TActorIterator<ADroppedHostileWeapon> It(World);It;++It)++N;return N;};
 TestEqual(TEXT("No dropped weapon before surrender"),Count(),0);
 Hostile->Auth_BeginFlashSurrenderWindow(5,10);
 TestEqual(TEXT("Effective flash immediately produces surrender"),Hostile->GetMorale(),EHostileMorale::Surrender);
 TestEqual(TEXT("Surrender starts hands-up then kneeling animation"),Hostile->CompliancePose->GetPose(),ECompliancePose::HandsUp);
 TestEqual(TEXT("Gun becomes one replicated physical world actor"),Count(),1);
 Hostile->Auth_BeginFlashSurrenderWindow(5,10);Hostile->SetMorale(EHostileMorale::Fighting);
 TestEqual(TEXT("Repeated effects never duplicate the dropped gun"),Count(),1);
 TestEqual(TEXT("A disarmed surrender cannot resume firing"),Hostile->GetMorale(),EHostileMorale::Surrender);
 TArray<AActor*> Actors;for(TActorIterator<AActor> It(World);It;++It)Actors.Add(*It);for(auto* A:Actors)A->Destroy();
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
