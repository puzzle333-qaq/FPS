#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Character/PoliceAppearanceComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoliceAppearanceTest,"FPS.PoliceAppearance.Combinations",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPoliceAppearanceTest::RunTest(const FString&)
{
 auto* Data=LoadObject<UPoliceAppearanceData>(nullptr,TEXT("/Game/FPS/Characters/Police/DA_PoliceAppearance.DA_PoliceAppearance"));
 if(!TestNotNull(TEXT("Authored appearance configuration"),Data))return false;
 auto* Base=Data->BaseMesh.LoadSynchronous();
 if(!TestNotNull(TEXT("Preset 07 base"),Base))return false;
 auto* Existing=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Bandits/Mesh/Preset/SKM_Bandit_Preset_13.SKM_Bandit_Preset_13"));
 TestTrue(TEXT("Uses the existing live skeleton"),Existing&&Existing->GetSkeleton()==Base->GetSkeleton());
 for(const auto& Mat:Base->GetMaterials())TestNotNull(TEXT("Every preset material is assigned"),Mat.MaterialInterface.Get());
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 auto* Owner=World->SpawnActor<AActor>();
 auto* Body=NewObject<USkeletalMeshComponent>(Owner);Owner->SetRootComponent(Body);Body->RegisterComponent();Body->SetSkeletalMesh(Base);
 auto* Appearance=NewObject<UPoliceAppearanceComponent>(Owner);Appearance->RegisterComponent();
 Appearance->InitializePreview(Body,EPoliceFaceProtection::FlashGoggles,EBodyArmorType::Light);
 for(uint8 Armor=0;Armor<=2;++Armor)for(uint8 Face=0;Face<=3;++Face)
 {
  Appearance->SetPreviewProtection(EPoliceFaceProtection(Face),EBodyArmorType(Armor));
  const int32 ExpectedArmor=Armor==0?0:(Armor==1?Data->LightArmor.Num():Data->HeavyArmor.Num());
  const int32 ExpectedFace=Face==0?0:(Face==2?2:1);int32 Active=0;
  for(USkeletalMeshComponent* Part:Appearance->Parts)if(Part->GetSkeletalMeshAsset())
  {
   ++Active;TestTrue(TEXT("Follower shares body pose"),Part->LeaderPoseComponent.Get()==Body);
   TestTrue(TEXT("Follower shares existing skeleton"),Part->GetSkeletalMeshAsset()->GetSkeleton()==Base->GetSkeleton());
   TestTrue(TEXT("Cosmetic pieces do not block interaction or bullets"),Part->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
   for(const auto& Mat:Part->GetSkeletalMeshAsset()->GetMaterials())TestNotNull(TEXT("Module material assigned"),Mat.MaterialInterface.Get());
  }
  TestEqual(TEXT("Exactly the selected armor and face pieces are visible"),Active,ExpectedArmor+ExpectedFace);
  for(int32 I=0;I<Base->GetMaterials().Num();++I)
   if(Data->ReplacedMaterialSlots.Contains(Base->GetMaterials()[I].MaterialSlotName))TestFalse(TEXT("Baked equipment is hidden, preventing double gear"),Body->IsMaterialSectionShown(I,0));
 }
 TestFalse(TEXT("Reject invalid face enum"),UPoliceAppearanceComponent::IsValidSelection(EPoliceFaceProtection(255),EBodyArmorType::Light));
 Appearance->DestroyComponent();World->DestroyWorld(false);return true;
}
#endif
