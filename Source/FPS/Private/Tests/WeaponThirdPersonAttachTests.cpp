#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Interface/PlayerInterface.h"
#include "ShooterCharacter.h"
#include "Weapon/Weapon.h"
#include "WeaponData.h"
#include "UObject/Script.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponThirdPersonAttachTest, "FPS.Weapons.ThirdPersonGrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeaponThirdPersonAttachTest::RunTest(const FString& Parameters)
{
	// The fixture does not start gameplay. Interface Execute_ calls still need
	// ProcessEvent enabled, just as editor scripting does for placed actors.
	FEditorScriptExecutionGuard ScriptGuard;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	AShooterCharacter* Pawn = World->SpawnActor<AShooterCharacter>();
	// Supply only grip data; loading DA_WeaponData pulls unrelated legacy guns into this test.
	Pawn->GetCombatComponent()->WeaponData = NewObject<UWeaponData>(Pawn);
	USkeletalMesh* Arms = LoadObject<USkeletalMesh>(nullptr,
		TEXT("/Game/FirstPersonRifleAnimations/Demo/FirstPersonArms/Character/Mesh/SK_Mannequin_Arms.SK_Mannequin_Arms"));
	Pawn->GetMesh1P_Implementation()->SetSkeletalMesh(Arms);
	USkeletalMesh* Bodies[] = {
		LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")),
		LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Bandits/Mesh/Preset/SKM_Bandit_Preset_13.SKM_Bandit_Preset_13"))
	};
	for (const TCHAR* Name : { TEXT("AK105"), TEXT("TR15"), TEXT("Mk14"), TEXT("MKR9"), TEXT("MX68"),
		TEXT("R08"), TEXT("SRM12"), TEXT("Herrington"), TEXT("Viper") })
	{
		const FString Path = FString::Printf(TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_%s.BP_Weapon_%s_C"), Name, Name);
		UClass* Class = LoadClass<AWeapon>(nullptr, *Path);
		if (!TestNotNull(Path, Class)) continue;
		FActorSpawnParameters SpawnParams;
		SpawnParams.Instigator = Pawn;
		AWeapon* Weapon = World->SpawnActor<AWeapon>(Class, FTransform::Identity, SpawnParams);
		if (!TestNotNull(TEXT("Weapon spawned"), Weapon)) continue;
		Weapon->SetInstigator(Pawn);
		TestEqual(TEXT("Instigator is the test character"), Weapon->GetInstigator(), static_cast<APawn*>(Pawn));
		TestTrue(TEXT("Character implements the player interface"), Pawn->Implements<UPlayerInterface>());
		Pawn->GetCombatComponent()->WeaponData->GripPoints.Add(Weapon->WeaponType, TEXT("RifleSocket"));
		TestEqual(TEXT("Calibrated mount follows the physical right hand"), Weapon->Mesh3PAttachSocket, FName(TEXT("hand_r")));
		TestFalse(TEXT("Grip includes the authored orientation"), Weapon->Mesh3PAttachTransform.GetRotation().Equals(FQuat::Identity, 0.01f));
		for (USkeletalMesh* Body : Bodies)
		{
			if (!TestNotNull(TEXT("Body mesh exists"), Body)) continue;
			Pawn->GetMesh()->SetSkeletalMesh(Body);
			Weapon->AttachToOwingPawn();
			USkeletalMeshComponent* Gun3P = Weapon->GetMesh3P();
			TestEqual(TEXT("Third-person parent is the body"), Gun3P->GetAttachParent(), static_cast<USceneComponent*>(Pawn->GetMesh()));
			TestEqual(TEXT("Third-person socket is hand_r"), Gun3P->GetAttachSocketName(), FName(TEXT("hand_r")));
			TestTrue(TEXT("Full calibrated transform survives attachment"), Gun3P->GetRelativeTransform().Equals(Weapon->Mesh3PAttachTransform, 0.001f));
			const FTransform FirstPersonBefore = Weapon->GetMesh1P()->GetRelativeTransform();
			const FName DefaultGrip = Pawn->GetWeaponAttachPoint_Implementation(Weapon->WeaponType);
			Weapon->AttachToPawnSocket(Pawn->GetMesh(), DefaultGrip);
			TestEqual(TEXT("Grip notify restores the calibrated right hand"), Gun3P->GetAttachSocketName(), FName(TEXT("hand_r")));
			TestTrue(TEXT("Grip notify does not accumulate offsets"), Gun3P->GetRelativeTransform().Equals(Weapon->Mesh3PAttachTransform, 0.001f));
			Weapon->AttachToPawnSocket(Pawn->GetMesh(), TEXT("hand_l"));
			TestEqual(TEXT("Left-hand transfer is respected"), Gun3P->GetAttachSocketName(), FName(TEXT("hand_l")));
			Weapon->AttachToPawnSocket(Pawn->GetMesh(), TEXT("hand_r"));
			TestTrue(TEXT("Right-hand return restores the full mount"), Gun3P->GetRelativeTransform().Equals(Weapon->Mesh3PAttachTransform, 0.001f));
			Weapon->AttachToPawnSocket(Pawn->GetMesh(), TEXT("ik_hand_gun"));
			TestEqual(TEXT("Legacy gun IK notify uses the physical hand mount"), Gun3P->GetAttachSocketName(), FName(TEXT("hand_r")));
			TestTrue(TEXT("Legacy gun IK notify restores the full mount"), Gun3P->GetRelativeTransform().Equals(Weapon->Mesh3PAttachTransform, 0.001f));
			TestTrue(TEXT("Third-person re-grips preserve first person"), Weapon->GetMesh1P()->GetRelativeTransform().Equals(FirstPersonBefore, 0.001f));
			Weapon->DetachFromPawn();
			Weapon->AttachToOwingPawn();
			TestTrue(TEXT("Re-equipping preserves the mount"), Gun3P->GetRelativeTransform().Equals(Weapon->Mesh3PAttachTransform, 0.001f));
		}
		Weapon->Destroy();
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
