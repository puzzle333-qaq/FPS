#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/Actor.h"
#include "ShooterCharacter.h"
#include "UObject/UnrealType.h"
#include "Weapon/WeaponObstacle.h"
#include "Weapon/Weapon.h"
#include "CombatComponent.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponObstacleProbeTest, "FPS.WeaponObstacle.Detection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeaponObstacleProbeTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world exists"), World)) return false;
	AActor* Obstacle = World->SpawnActor<AActor>();
	UBoxComponent* Box = NewObject<UBoxComponent>(Obstacle);
	Obstacle->SetRootComponent(Box);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Box->InitBoxExtent(FVector(5.f, 100.f, 200.f));
	Box->SetWorldLocation(FVector(55.f, 0.f, 0.f));
	Box->RegisterComponent();
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponObstacleTest), false);
	auto Probe = [&](float Length = 70.f, FQuat Rotation = FQuat::Identity)
	{
		return WeaponObstacle::Probe(*World, FVector::ZeroVector, Rotation, Length, Params).Alpha;
	};
	TestTrue(TEXT("A full-height wall ahead raises the rifle"), Probe() > 0.55f);
	TestEqual(TEXT("A shorter pistol is not blocked by a wall beyond its barrel"), Probe(22.f), 0.f);
	TestEqual(TEXT("Facing away from a wall does not raise the weapon"), Probe(70.f, FRotator(0.f, 180.f, 0.f).Quaternion()), 0.f);
	Box->SetWorldLocation(FVector(-25.f, 0.f, 0.f));
	TestEqual(TEXT("Back against a wall stays clear"), Probe(), 0.f);
	Box->SetBoxExtent(FVector(15.f, 100.f, 200.f));
	Box->SetWorldLocation(FVector(-10.f, 0.f, 0.f));
	TestEqual(TEXT("Rear wall touching a probe's start does not false-trigger"), Probe(), 0.f);

	Box->SetBoxExtent(FVector(12.f, 40.f, 6.f));
	Box->SetWorldLocation(FVector(55.f, 0.f, -20.f));
	FHitResult EyeHit;
	TestFalse(TEXT("Eye-height line alone misses the low table"), World->LineTraceSingleByChannel(EyeHit,
		FVector::ZeroVector, FVector(78.f, 0.f, 0.f), ECC_Pawn, Params));
	TestTrue(TEXT("Barrel-height sweep detects the table"), Probe() > 0.55f);

	Box->SetBoxExtent(FVector(5.f, 100.f, 200.f));
	Box->SetWorldLocation(FVector(55.f, 0.f, 0.f));
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TestEqual(TEXT("Non-solid trigger volumes do not raise the gun"), Probe(), 0.f);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Box->SetBoxExtent(FVector(100.f, 4.f, 200.f));
	Box->SetWorldLocation(FVector(20.f, 14.f, 0.f));
	TestEqual(TEXT("Grazing a parallel side wall does not raise the gun"), Probe(), 0.f);
	AActor* FrontWall = World->SpawnActor<AActor>();
	UBoxComponent* FrontBox = NewObject<UBoxComponent>(FrontWall);
	FrontWall->SetRootComponent(FrontBox);
	FrontBox->InitBoxExtent(FVector(12.f, 40.f, 6.f));
	FrontBox->SetWorldLocation(FVector(55.f, 0.f, -20.f));
	FrontBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	FrontBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	FrontBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	FrontBox->RegisterComponent();
	TestTrue(TEXT("A side-wall overlap cannot hide low cover ahead in a corner"), Probe() > 0.55f);
	FrontBox->DestroyComponent();
	FrontWall->Destroy();
	Box->SetWorldLocation(FVector(150.f, 0.f, 0.f));
	Box->SetBoxExtent(FVector(5.f, 100.f, 200.f));
	TestEqual(TEXT("A distant obstacle stays clear"), Probe(), 0.f);
	TestEqual(TEXT("Non-gun zero-length weapons have no obstacle pose"), Probe(0.f), 0.f);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponObstaclePivotTest, "FPS.WeaponObstacle.HighReadyPivot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeaponObstaclePivotTest::RunTest(const FString& Parameters)
{
	// Shared arms have a root far below the camera; rotating that root used to pull
	// the entire gun backwards. The grip must stay fixed while the barrel goes up.
	const FVector Root(0.f, 0.f, -160.f);
	const FVector Grip(25.f, 12.f, -18.f);
	const FQuat Raise = FRotator(85.f, 0.f, 0.f).Quaternion();
	const FVector RaisedRoot = Root + WeaponObstacle::PivotCorrection(Root, Grip, Raise);
	const FVector RaisedGrip = RaisedRoot + Raise.RotateVector(Grip - Root);
	const FVector Muzzle = Grip + FVector(50.f, 0.f, 0.f);
	const FVector RaisedMuzzle = RaisedRoot + Raise.RotateVector(Muzzle - Root);
	TestTrue(TEXT("Raise rotates around the grip, not the mesh root"), RaisedGrip.Equals(Grip, 0.001f));
	TestTrue(TEXT("Muzzle rises substantially"), RaisedMuzzle.Z > Muzzle.Z + 40.f);
	TestTrue(TEXT("Raised muzzle needs less forward space"), RaisedMuzzle.X < Muzzle.X - 25.f);
	TestTrue(TEXT("No residual offset when clear"), WeaponObstacle::PivotCorrection(Root, Grip, FQuat::Identity).IsNearlyZero());
 // A nearby protected person dips the muzzle before barrel obstruction starts.
 // That transition must not lunge the grip forward either.
 for(float PitchValue : {-12.f,-6.f,25.f,55.f,85.f})
 {
  const FQuat Pose=FRotator(PitchValue,0.f,-30.f*FMath::Max(0.f,PitchValue)/85.f).Quaternion();
  const FVector PoseRoot=Root+WeaponObstacle::PivotCorrection(Root,Grip,Pose);
  TestTrue(TEXT("Safety dip and compact high-ready keep the grip anchored"),(PoseRoot+Pose.RotateVector(Grip-Root)).Equals(Grip,.001f));
 }
	UClass* PlayerClass = LoadClass<AShooterCharacter>(nullptr, TEXT("/Game/FPS/Character/BP_ShooterCharacter.BP_ShooterCharacter_C"));
	if (TestNotNull(TEXT("Player blueprint loads with the new pose settings"), PlayerClass))
	{
		const UObject* Defaults = PlayerClass->GetDefaultObject();
		const FFloatProperty* Pitch = FindFProperty<FFloatProperty>(PlayerClass, TEXT("WeaponBlockRaisePitch"));
		const FFloatProperty* Retract = FindFProperty<FFloatProperty>(PlayerClass, TEXT("WeaponBlockRetract"));
		if (Pitch && Retract)
		{
			TestTrue(TEXT("Blueprint uses the higher high-ready angle"), Pitch->GetPropertyValue_InContainer(Defaults) >= 80.f);
			TestTrue(TEXT("Blueprint preserves the reviewed viewmodel depth"), FMath::IsNearlyEqual(Retract->GetPropertyValue_InContainer(Defaults),-36.f));
		}
		else AddError(TEXT("Obstacle pose properties missing from player class"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponObstacleFireGateTest, "FPS.WeaponObstacle.FireGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeaponObstacleFireGateTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Fire test world exists"), World)) return false;
	World->InitializeActorsForPlay(FURL());
	AShooterCharacter* Shooter = World->SpawnActor<AShooterCharacter>();
	AWeapon* Weapon = World->SpawnActor<AWeapon>(FVector(0.f, 0.f, 1000.f), FRotator::ZeroRotator);
	if (!Shooter || !Weapon)
	{
		AddError(TEXT("Could not create fire test actors"));
		World->DestroyWorld(false);
		return false;
	}
	Weapon->SetInstigator(Shooter);
	Weapon->WeaponKind = EWeaponKind::LongGun;
	Weapon->WeaponLength = 70.f;
	Weapon->WeaponStatus = EWeaponStatus::Idle;
	Weapon->MagCapacity = 10;
	Weapon->Ammo = 10;
	UCombatComponent* Combat = Shooter->FindComponentByClass<UCombatComponent>();
	Combat->CurrentWeapon = Weapon;
	Combat->WeaponData = NewObject<UWeaponData>(Combat);
	const FVector Eye = Shooter->GetFirstPersonCamera()->GetComponentLocation();
	AActor* Obstacle = World->SpawnActor<AActor>();
	UBoxComponent* Wall = NewObject<UBoxComponent>(Obstacle);
	Obstacle->SetRootComponent(Wall);
	Wall->InitBoxExtent(FVector(5.f, 100.f, 200.f));
	Wall->SetWorldLocation(Eye + FVector(55.f, 0.f, 0.f));
	Wall->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Wall->SetCollisionResponseToAllChannels(ECR_Ignore);
	Wall->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Wall->RegisterComponent();
	TestTrue(TEXT("Block fire immediately before the pose has ticked"), Shooter->IsWeaponFireBlocked());
	Combat->Initiate_FireWeapon_Pressed();
	TestEqual(TEXT("Pressing fire against a wall spends no ammo"), Weapon->Ammo, 10);
	Combat->Local_FireWeapon();
	TestEqual(TEXT("Local prediction cannot bypass the obstruction"), Weapon->Ammo, 10);
	Combat->Auth_FireWeapon({});
	TestEqual(TEXT("Authority independently rejects an obstructed shot"), Weapon->Ammo, 10);
	Weapon->FireType = EFireType::Burst;
	Combat->bTriggerPressed = true;
	Combat->BurstShotsRemaining = 2;
	World->GetTimerManager().SetTimer(Combat->FireTimer, [] {}, 1.f, false);
	World->GetTimerManager().SetTimer(Combat->SprintFireBufferTimer, [] {}, 1.f, false);
	Combat->FireTimerFinished();
	TestEqual(TEXT("Remaining burst shots cancelled"), Combat->BurstShotsRemaining, 0);
	TestFalse(TEXT("Blocked fire releases the trigger latch"), Combat->bTriggerPressed);
	TestFalse(TEXT("Automatic/burst timer stopped"), World->GetTimerManager().IsTimerActive(Combat->FireTimer));
	TestFalse(TEXT("Sprint-to-fire buffer cancelled"), World->GetTimerManager().IsTimerActive(Combat->SprintFireBufferTimer));
	Wall->SetWorldLocation(Eye - FVector(55.f, 0.f, 0.f));
	Shooter->WeaponBlockAlpha = 0.3f;
	TestTrue(TEXT("Still cannot fire while the gun is lowering"), Shooter->IsWeaponFireBlocked());
	Combat->Auth_FireWeapon({});
	TestEqual(TEXT("Lowering pose does not spend ammo"), Weapon->Ammo, 10);
	Shooter->WeaponBlockAlpha = 0.f;
	TestFalse(TEXT("Fire unlocked once the gun is down and the path clear"), Shooter->IsWeaponFireBlocked());
	Combat->Auth_FireWeapon({});
	TestEqual(TEXT("A fresh valid shot works after lowering"), Weapon->Ammo, 9);
	World->DestroyWorld(false);
	return true;
}

#endif
