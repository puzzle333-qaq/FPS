#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "ShooterCharacter.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSingleBodyCameraFollowTest,"FPS.SingleBody.CameraFollowsEvaluatedAnchor",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSingleBodyCameraFollowTest::RunTest(const FString&)
{
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL());
 ON_SCOPE_EXIT { World->DestroyWorld(false);GEngine->DestroyWorldContext(World); };
 auto* Character=World->SpawnActor<AShooterCharacter>();
 auto* PC=World->SpawnActor<APlayerController>();PC->SetAsLocalPlayerController();PC->Possess(Character);
 ON_SCOPE_EXIT { PC->UnPossess();PC->Destroy();Character->Destroy(); };
 auto* Body=Character->GetMesh();
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
 if(!TestNotNull(TEXT("Pilot body exists"),Mesh))return false;
 Body->SetSkeletalMesh(Mesh);Body->RefreshBoneTransforms();
 TestTrue(TEXT("Hidden neck still provides the camera anchor"),Body->DoesSocketExist(Character->SingleBodyCameraAnchorBone));
 Body->HideBoneByName(TEXT("neck_01"),EPhysBodyOp::PBO_None);Body->RefreshBoneTransforms();
 auto* Camera=Character->GetFirstPersonCamera();
 const FVector OriginalRelative=Camera->GetRelativeLocation();
 const FQuat OriginalRotation=Camera->GetComponentQuat();
 Character->bUseSingleBodyPilot=true;
 FMinimalViewInfo View;
 Character->CalcCamera(.016f,View);
 auto Eye=[&](){return Body->GetSocketLocation(Character->SingleBodyCameraAnchorBone)+Character->GetActorQuat().RotateVector(Character->SingleBodyCameraEyeOffset);};
 TestTrue(TEXT("Camera follows the evaluated anchor with hidden head scale"),View.Location.Equals(Eye(),.01f));
 TestTrue(TEXT("Gameplay traces use the same camera origin as the view"),Camera->GetComponentLocation().Equals(View.Location,.01f));
 const FVector Before=View.Location;
 const FVector AnchorDelta=Body->GetComponentQuat().RotateVector(FVector(25,0,-15));
 Body->AddLocalOffset(FVector(25,0,-15));
 Character->CalcCamera(.016f,View);
 TestTrue(TEXT("Camera follows anchor translation in all axes"),(View.Location-Before).Equals(AnchorDelta,.01f));
 Body->AddLocalRotation(FRotator(0,35,0));
 Character->CalcCamera(.016f,View);
 TestTrue(TEXT("Body animation rotation does not rotate mouse-controlled view"),Camera->GetComponentQuat().Equals(OriginalRotation,.001f));
 Character->SetDebugThirdPerson(true);Character->CalcCamera(.016f,View);
 TestTrue(TEXT("Debug third person restores original camera offset"),Camera->GetRelativeLocation().Equals(OriginalRelative,.01f));
 Character->SetDebugThirdPerson(false);Character->CalcCamera(.016f,View);
 Character->bUseSingleBodyPilot=false;Character->CalcCamera(.016f,View);
 TestTrue(TEXT("Disabling the pilot restores legacy first person"),Camera->GetRelativeLocation().Equals(OriginalRelative,.01f));
 Character->bUseSingleBodyPilot=true;Character->CalcCamera(.016f,View);
 auto* Wall=World->SpawnActor<AActor>();
 auto* Box=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(Box);Wall->AddInstanceComponent(Box);
 Box->SetBoxExtent(FVector(100,100,3));Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Ignore);Box->SetCollisionResponseToChannel(ECC_Camera,ECR_Block);Box->RegisterComponent();
 const FVector Origin=Character->GetActorLocation();Wall->SetActorLocation(FMath::Lerp(Origin,Eye(),.5f));
 Character->CalcCamera(.016f,View);
 TestTrue(TEXT("Head follow cannot push the camera through blocking geometry"),FVector::Dist(Origin,View.Location)<FVector::Dist(Origin,Eye())-5.f);
 Wall->Destroy();
 Camera->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);Camera->SetUsingAbsoluteLocation(true);
 const FVector Detached=Camera->GetComponentLocation();Body->AddLocalOffset(FVector(50,0,0));Character->CalcCamera(.016f,View);
 TestTrue(TEXT("Detached death camera is never moved by head follow"),Camera->GetComponentLocation().Equals(Detached,.01f));
 return true;
}
#endif
