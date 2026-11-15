#include "Animation/AnimSequence.h"
#include "Animation/FirstPersonAnimInstance.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "UObject/Script.h"
#include "ShooterCharacter.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "WeaponData.h"
#include "ShooterGamePlayTags.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHybridPresentationTest,"FPS.Hybrid.RAR_TR15",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHybridPresentationTest::RunTest(const FString&)
{
 FEditorScriptExecutionGuard Guard;
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 auto* Class=LoadClass<AShooterCharacter>(nullptr,TEXT("/Game/FPS/Character/BP_ShooterCharacter.BP_ShooterCharacter_C"));
 auto* Character=World->SpawnActor<AShooterCharacter>(Class);auto* PC=World->SpawnActor<APlayerController>();PC->SetAsLocalPlayerController();PC->Possess(Character);
 auto* Combat=Character->GetCombatComponent();Combat->WeaponData=LoadObject<UWeaponData>(nullptr,TEXT("/Game/FPS/Data/DA_WeaponData.DA_WeaponData"));
 auto* Arms=Character->GetMesh1P_Implementation();TestTrue(TEXT("First person arms are independent from the legs"),Arms&&Arms!=Character->GetMesh());
 TestTrue(TEXT("Entire local third-person body hidden"),Character->GetMesh()->bOwnerNoSee);TestFalse(TEXT("No lower-body bone mask remains"),Character->GetMesh()->IsBoneHiddenByName(TEXT("spine_01")));
 for(const TCHAR* Path:{TEXT("/Game/FPS/Weapon/RAR/BP_FPS_RAR.BP_FPS_RAR_C"),TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_TR15.BP_Weapon_TR15_C")})
 {
  auto* GunClass=LoadClass<AWeapon>(nullptr,Path);if(!TestNotNull(Path,GunClass))continue;
  auto* Gun=World->SpawnActor<AWeapon>(GunClass);Gun->SetInstigator(Character);Gun->DispatchBeginPlay();Combat->Equip(Gun);
  TestTrue(TEXT("Both rifles share one hold category"),Combat->ResolveHoldPoseTag(Gun).MatchesTagExact(ShooterTags::Tag_WeaponType_Rifle));
  TestTrue(TEXT("Gun uses first person arms for ADS"),Gun->GetMesh1P()->GetAttachParent()==Arms);
  const FPlayerAnim* Row=Combat->WeaponData->FirstPersonAnims.Find(Combat->ResolveHoldPoseTag(Gun));
  if(TestNotNull(TEXT("Shared pose row"),Row)) {TestTrue(TEXT("Idle uses InfimaGames"),Row->IdleAnim&&Row->IdleAnim->GetPathName().Contains(TEXT("InfimaGames")));TestTrue(TEXT("Aim uses InfimaGames"),Row->AimIdleAnim&&Row->AimIdleAnim->GetPathName().Contains(TEXT("InfimaGames")));}
  for(auto* Grip:Gun->GetAllowedAttachmentsForSlot(ShooterTags::Tag_Attachment_Slot_Grip))
  {
   if(!Grip||!Grip->Mesh)continue;
   TestTrue(TEXT("Grip has its own first person hand anchor"),!Grip->FirstPersonFabrikSocketName.IsNone()&&Grip->Mesh->FindSocket(Grip->FirstPersonFabrikSocketName));
   if(Gun->EquipAttachment(Grip)){FTransform Target;TestTrue(TEXT("Grip resolves an actual left-hand IK target"),Gun->GetFabrikWorldTransform(true,Target));}
  }
  if(Gun->GetClass()->GetName().Contains(TEXT("Weapon_TR15")))
  {
   Gun->RemoveAttachment(ShooterTags::Tag_Attachment_Slot_Grip);
   FTransform Target;TestTrue(TEXT("TR15 idle has a support-hand target without a foregrip"),Gun->GetFabrikWorldTransform(true,Target));
   TestTrue(TEXT("TR15 uses its authored handguard anchor"),Target.Equals(Gun->ThirdPersonLeftHandGrip*Gun->GetMesh1P()->GetComponentTransform(),.001f));
  }
  else
  {
   Character->FABRIK_Alpha1P=1.f;
   Combat->Initiate_VersionWeapon();
   TestTrue(TEXT("RAR inspection actually starts"),Combat->IsInspecting());
   Arms->TickAnimation(.016f,false);
   auto* Anim=Cast<UFirstPersonAnimInstance>(Arms->GetAnimInstance());
   if(TestNotNull(TEXT("Native first-person animation instance"),Anim))TestTrue(TEXT("Inspection releases the left hand immediately"),FMath::IsNearlyZero(Anim->FABRIK_Alpha1P));
   Combat->CancelInspect();
  }
  Gun->Destroy();Combat->CurrentWeapon=nullptr;
 }
 Character->SetDebugThirdPerson(true);TestFalse(TEXT("Third person restores the upper body"),Character->GetMesh()->IsBoneHiddenByName(TEXT("spine_01")));
 PC->UnPossess();PC->Destroy();Character->Destroy();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif

