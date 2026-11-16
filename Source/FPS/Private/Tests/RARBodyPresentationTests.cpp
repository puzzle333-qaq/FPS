#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Script.h"
#include "ShooterCharacter.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "WeaponData.h"
#include "ShooterGamePlayTags.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRARBodyPresentationTest,"FPS.SingleBody.RARBodyPresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRARBodyPresentationTest::RunTest(const FString&)
{
 FEditorScriptExecutionGuard Guard;
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 ON_SCOPE_EXIT {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
 auto* C=World->SpawnActor<AShooterCharacter>();auto* PC=World->SpawnActor<APlayerController>();PC->SetAsLocalPlayerController();PC->Possess(C);
 auto* Gun=World->SpawnActor<AWeapon>(LoadClass<AWeapon>(nullptr,TEXT("/Game/FPS/Weapon/RAR/BP_FPS_RAR.BP_FPS_RAR_C")));
 ON_SCOPE_EXIT {Gun->Destroy();PC->UnPossess();PC->Destroy();C->Destroy();};
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
 C->GetMesh()->SetSkeletalMesh(Mesh);C->GetMesh()->SetAnimInstanceClass(LoadClass<UAnimInstance>(nullptr,TEXT("/Game/FPS/Character/ABP_ThirdPerson.ABP_ThirdPerson_C")));
 C->bUseSingleBodyPilot=true;C->SingleBodyCameraProbeRadius=0;
 auto* Combat=C->GetCombatComponent();Combat->WeaponData=LoadObject<UWeaponData>(nullptr,TEXT("/Game/FPS/Data/DA_WeaponData.DA_WeaponData"));Gun->SetOwner(C);Gun->SetInstigator(C);Gun->DispatchBeginPlay();Combat->Equip(Gun);Gun->WeaponStatus=EWeaponStatus::Idle;
 FTransform Sight;
 TestTrue(TEXT("Visible third-person weapon exposes its own sight socket"),Gun->GetAimSocketWorldTransform(Sight,false));
 TestEqual(TEXT("Gun stays attached to physical right hand"),Gun->GetMesh3P()->GetAttachSocketName(),FName(TEXT("hand_r")));
 const auto* Row=Combat->WeaponData->ThirdPersonMontages.Find(Gun->WeaponType);
 if(!TestNotNull(TEXT("RAR body action row exists"),Row))return false;
 for(auto* Montage:{Row->EquipMontage.Get(),Row->FirstEquipMontage.Get(),Row->FireMontage.Get(),Row->EmptyReloadMontage.Get(),Row->TacticalReloadMontage.Get(),Row->InspectMontage1P.Get()})
 {
  if(!TestNotNull(TEXT("Body action is populated"),Montage))continue;
  TestTrue(TEXT("Body action is a dedicated retargeted asset"),Montage->GetPathName().Contains(TEXT("/RARBody/")));
  TestEqual(TEXT("Body action uses the actual Manny skeleton"),Montage->GetSkeleton(),Mesh->GetSkeleton());
  TestEqual(TEXT("Single upper-body track"),Montage->SlotAnimTracks.Num(),1);
  TestEqual(TEXT("Action slot is consumed by the body graph"),Montage->SlotAnimTracks[0].SlotName,FName(TEXT("BodyUpperAction")));
  TestTrue(TEXT("Body instance accepts the montage"),C->GetMesh()->GetAnimInstance()->Montage_Play(Montage)>0.f);
  C->GetMesh()->GetAnimInstance()->Montage_Stop(0);
 }
 Gun->WeaponStatus=EWeaponStatus::Idle;Combat->Initiate_Aim_Pressed();TestTrue(TEXT("ADS input enters aiming"),Combat->bAiming);
 C->Tick(.5f);TestTrue(TEXT("Visible body receives ADS IK alpha"),C->BodyAdsAlpha>.95f);
 TestFalse(TEXT("ADS right hand target is finite"),C->BodyAdsRightHand.ContainsNaN());
 // Solve through the actual body graph, then measure the visible weapon socket.
 // Each manual pose update must have a distinct engine frame; AnimInstance caches per-frame inputs.
 for(int I=0;I<8;++I){++GFrameCounter;C->Tick(.2f);C->GetMesh()->TickAnimation(.2f,false);C->GetMesh()->RefreshBoneTransforms();Gun->GetMesh3P()->UpdateComponentToWorld();}
 FMinimalViewInfo View;C->CalcCamera(.016f,View);Gun->GetAimSocketWorldTransform(Sight,false);
 const FVector InView=FTransform(View.Rotation,View.Location).InverseTransformPosition(Sight.GetLocation());
 AddInfo(FString::Printf(TEXT("Right hand target error: %.4f cm"),FVector::Dist(C->GetMesh()->GetSocketTransform(TEXT("hand_r"),RTS_Component).GetLocation(),C->BodyAdsRightHand.GetLocation())));
 AddInfo(FString::Printf(TEXT("Visible sight in camera space: %s"),*InView.ToString()));
 TestTrue(TEXT("Sight is brought in front of the eye"),InView.X>0.f);
 TestTrue(TEXT("Visible sight is centered by the body IK"),FVector2D(InView.Y,InView.Z).Size()<.1f);
 FTransform Grip;
 if(TestTrue(TEXT("Body support hand has an authored anchor"),Gun->GetFabrikWorldTransform(false,Grip)))
 {
  const float Error=FVector::Dist(Grip.GetLocation(),C->GetMesh()->GetSocketLocation(TEXT("hand_l")));
  AddInfo(FString::Printf(TEXT("Support hand anchor error: %.4f cm"),Error));
  TestTrue(TEXT("Support hand stays on grip after ADS"),Error<.1f);
 }
 Gun->WeaponStatus=EWeaponStatus::Firing;C->UpdateBodyAdsTarget();TestTrue(TEXT("Firing keeps body ADS active"),C->BodyAdsAlpha>.95f);
 Gun->WeaponStatus=EWeaponStatus::Idle;
 Combat->Initiate_Aim_Pressed();Gun->WeaponStatus=EWeaponStatus::Idle;Combat->Initiate_VersionWeapon();
 TestTrue(TEXT("Hold-R inspection actually plays on the body"),Combat->IsInspecting());
 Combat->CancelInspect();TestFalse(TEXT("Combat interruption stops body inspection"),Combat->IsInspecting());
 Combat->Initiate_InspectWeapon();TestTrue(TEXT("Ordinary inspection plays on body"),Combat->IsInspecting());Combat->CancelInspect();
 C->bUseSingleBodyPilot=false;C->UpdateBodyAdsTarget();TestEqual(TEXT("Legacy arms mode disables body IK"),C->BodyAdsAlpha,0.f);
 return true;
}
#endif

