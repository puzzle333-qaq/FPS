#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Weapon/Weapon.h"
#include "Character/PoliceAppearanceTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuthoredThirdPersonGripTest,"FPS.Attachments.AuthoredThirdPersonGrips",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAuthoredThirdPersonGripTest::RunTest(const FString&){
 auto* Data=LoadObject<UPoliceAppearanceData>(nullptr,TEXT("/Game/FPS/Characters/Police/DA_PoliceAppearance.DA_PoliceAppearance"));if(!TestNotNull(TEXT("Police rig configuration"),Data))return false;
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);auto* Actor=World->SpawnActor<AActor>();auto* Body=NewObject<USkeletalMeshComponent>(Actor);Actor->SetRootComponent(Body);Body->RegisterComponent();Body->SetSkeletalMesh(Data->BaseMesh.LoadSynchronous());Body->SetWorldLocationAndRotation(FVector(250,-70,30),FRotator(0,37,0));
 for(FString Name:{TEXT("AK105"),TEXT("TR15"),TEXT("Mk14"),TEXT("MKR9"),TEXT("MX68"),TEXT("R08"),TEXT("SRM12"),TEXT("Herrington"),TEXT("Viper"),TEXT("X24")}){
  FString Path=Name==TEXT("X24")?TEXT("/Game/FPS/Weapon/BP_Weapon_X24.BP_Weapon_X24_C"):FString::Printf(TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_%s.BP_Weapon_%s_C"),*Name,*Name);auto* Class=LoadClass<AWeapon>(nullptr,*Path);if(!TestNotNull(*Name,Class))continue;
  auto* Weapon=World->SpawnActor<AWeapon>(Class);Weapon->SetReplicates(false);TestTrue(TEXT("Authored left hand is configured"),Weapon->bHasThirdPersonLeftHandGrip);TestEqual(TEXT("Calibrated mount follows right hand bone"),Weapon->Mesh3PAttachSocket,FName(TEXT("hand_r")));
  Weapon->AttachCosmeticThirdPerson(Body,TEXT("weapon_r"),FTransform::Identity);TestTrue(TEXT("3P mesh directly attaches to character body"),Weapon->GetMesh3P()->GetAttachParent()==Body);TestEqual(TEXT("Missing preview socket never leaves gun at root"),Weapon->GetMesh3P()->GetAttachSocketName(),FName(TEXT("hand_r")));
  TestTrue(TEXT("Preview preserves the actual calibrated third-person mount"),Weapon->GetMesh3P()->GetRelativeTransform().Equals(Weapon->Mesh3PAttachTransform,.001f));FTransform Grip,AuthoredSocketGrip;Weapon->bHasThirdPersonLeftHandGrip=false;const bool HasSocket=Weapon->GetFabrikWorldTransform(false,AuthoredSocketGrip);Weapon->bHasThirdPersonLeftHandGrip=true;
  TestTrue(TEXT("Left-hand target exists without a weapon socket"),Weapon->GetFabrikWorldTransform(false,Grip));TestTrue(TEXT("Authored left-hand target maps through actual weapon transform"),Grip.Equals(HasSocket?AuthoredSocketGrip:Weapon->ThirdPersonLeftHandGrip*Weapon->GetMesh3P()->GetComponentTransform(),.001f));
 }
 World->DestroyWorld(false);return true;
}
#endif
