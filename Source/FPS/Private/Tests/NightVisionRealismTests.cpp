#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Vision/NightVisionProfile.h"
#include "Vision/NightVisionLightFloor.h"
#include "ShooterCharacter.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Scene.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNVGTubeProfileTest,"FPS.NVG.TubeProfile",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNVGTubeProfileTest::RunTest(const FString&)
{
 FPostProcessSettings P;NightVisionProfile::Apply(P,1,0);
 TestEqual(TEXT("Passive gain uses the agreed exposure"),P.AutoExposureBias,1.5f);
 TestEqual(TEXT("Low-light adaptation range"),P.AutoExposureMinBrightness,-3.f);
 TestEqual(TEXT("Highlight adaptation range"),P.AutoExposureMaxBrightness,2.5f);
 TestEqual(TEXT("Auto-gating uses bilateral local exposure"),P.LocalExposureMethod,ELocalExposureMethod::Bilateral);
 TestEqual(TEXT("No duplicate uniform grain"),P.FilmGrainIntensity,0.f);
 const float BaseBloom=P.BloomIntensity;NightVisionProfile::Apply(P,.2f,1.f);
 TestTrue(TEXT("Flash wash still raises exposure and bloom"),P.AutoExposureBias>1.5f&&P.BloomIntensity>BaseBloom);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNVGIRIsolationTest,"FPS.NVG.IRIsolationAndHousingDrain",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNVGIRIsolationTest::RunTest(const FString&)
{
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);
 auto* C=W->SpawnActor<AShooterCharacter>();
 TestFalse(TEXT("IR does not replicate"),C->NVGIlluminator->GetIsReplicated());
 TestFalse(TEXT("IR defaults off"),C->IsNVGInfraredEnabled());
 TestFalse(TEXT("IR casts no close-body shadows"),C->NVGIlluminator->CastShadows);
 TestTrue(TEXT("IR emitter is in front of viewmodel"),C->NVGIlluminator->GetRelativeLocation().Equals(FVector(70,0,-5)));
 TestTrue(TEXT("IR has its own lighting channel"),!C->NVGIlluminator->LightingChannels.bChannel0&&C->NVGIlluminator->LightingChannels.bChannel1);
 C->TacticalVisionMode=ETacticalVisionMode::NVG;C->NVGHousing=ENVGHousing::Dual;
 TestEqual(TEXT("Dual consumes the base battery rate"),C->GetVisionBatteryDrainMultiplier(),1.f);
 C->NVGHousing=ENVGHousing::Quad;TestEqual(TEXT("Quad consumes 1.5 times the battery"),C->GetVisionBatteryDrainMultiplier(),1.5f);
 C->TacticalVisionMode=ETacticalVisionMode::Thermal;TestEqual(TEXT("Thermal is independent of NVG housing"),C->GetVisionBatteryDrainMultiplier(),1.f);
 auto* A=W->SpawnActor<AActor>();
 auto* Body=NewObject<UStaticMeshComponent>(A);A->SetRootComponent(Body);Body->RegisterComponent();
 auto* Hands=NewObject<UStaticMeshComponent>(A);Hands->SetupAttachment(Body);Hands->SetOnlyOwnerSee(true);Hands->RegisterComponent();
 auto* Attachment=NewObject<UStaticMeshComponent>(A);Attachment->SetupAttachment(Hands);Attachment->RegisterComponent();
 ANightVisionLightFloor::PrepareInfraredReceiver(A);
 TestTrue(TEXT("World/body geometry retains normal light and receives IR"),Body->LightingChannels.bChannel0&&Body->LightingChannels.bChannel1);
 TestTrue(TEXT("First-person hands keep normal lighting but reject IR"),Hands->LightingChannels.bChannel0&&!Hands->LightingChannels.bChannel1);
 TestFalse(TEXT("First-person attachments also reject IR through their parent"),Attachment->LightingChannels.bChannel1);
 A->Destroy();C->Destroy();W->DestroyWorld(false);return true;
}
#endif
