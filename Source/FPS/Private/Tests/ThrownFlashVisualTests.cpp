#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Weapon/ThrownProjectile.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThrownFlashVisualTest, "FPS.Throwables.FlashVisual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FThrownFlashVisualTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world created"), World)) return false;
	World->InitializeActorsForPlay(FURL());
	AThrownFlash* Flash = World->SpawnActor<AThrownFlash>();
	if (!TestNotNull(TEXT("Flash projectile spawned"), Flash))
	{
		World->DestroyWorld(false);
		return false;
	}
	USkeletalMeshComponent* Body = Flash->FindComponentByClass<USkeletalMeshComponent>();
	UStaticMeshComponent* Placeholder = Flash->FindComponentByClass<UStaticMeshComponent>();
	if (TestNotNull(TEXT("Released body component exists"), Body))
	{
		if (TestNotNull(TEXT("Flash body asset loaded"), Body->GetSkeletalMeshAsset()))
		{
			TestTrue(TEXT("Correct released flashbang model"), Body->GetSkeletalMeshAsset()->GetPathName().EndsWith(TEXT("SK_FlashGrenadeWithoutFuse.SK_FlashGrenadeWithoutFuse")));
			TestTrue(TEXT("Flash body has materials"), Body->GetNumMaterials() > 0);
		}
		TestFalse(TEXT("Body visible before detonation"), Body->bHiddenInGame);
		TestEqual(TEXT("Body uses authored size rather than sphere scale"), Body->GetRelativeScale3D(), FVector::OneVector);
		TestEqual(TEXT("Body does not add a second collision shape"), Body->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	}
	if (TestNotNull(TEXT("Inherited component exists"), Placeholder))
	{
		TestNull(TEXT("Gray sphere removed"), Placeholder->GetStaticMesh());
	}
	Flash->Launch(FVector::ForwardVector, 900.f);
	UProjectileMovementComponent* Movement = Flash->FindComponentByClass<UProjectileMovementComponent>();
	if (TestNotNull(TEXT("Movement retained"), Movement))
	{
		TestTrue(TEXT("Projectile launches"), Movement->Velocity.Equals(FVector(900.f, 0.f, 0.f)));
	}
	TestEqual(TEXT("Flash effect retained"), Flash->EffectType, EThrownEffect::Flash);
	Flash->Detonate();
	if (Body) TestTrue(TEXT("Body hidden after multicast detonation"), Body->bHiddenInGame);
	World->DestroyWorld(false);
	return true;
}

#endif
