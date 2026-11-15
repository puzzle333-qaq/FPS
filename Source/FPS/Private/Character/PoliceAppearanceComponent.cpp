#include "Character/PoliceAppearanceComponent.h"
#include "Character/PoliceAppearanceSaveGame.h"
#include "Loadout/PouchInventoryComponent.h"
#include "ShooterCharacter.h"
#include "ShooterPlayerController.h"
#include "CombatComponent.h"
#include "WeaponData.h"
#include "ShooterGamePlayTags.h"
#include "UObject/UnrealType.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace { const TCHAR* AppearanceSlot=TEXT("PoliceAppearance"); }

UPoliceAppearanceComponent::UPoliceAppearanceComponent()
{
 PrimaryComponentTick.bCanEverTick=false;
 SetIsReplicatedByDefault(true);
 AppearanceData=TSoftObjectPtr<UPoliceAppearanceData>(FSoftObjectPath(TEXT("/Game/FPS/Characters/Police/DA_PoliceAppearance.DA_PoliceAppearance")));
}
void UPoliceAppearanceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(UPoliceAppearanceComponent,FaceProtection);
}
void UPoliceAppearanceComponent::BeginPlay()
{
 Super::BeginPlay();RefreshAppearance();
 if(auto* Character=Cast<AShooterCharacter>(GetOwner()))
 {
  Character->GetMesh()->OnAnimInitialized.AddDynamic(this,&ThisClass::OnBodyAnimationInitialized);
  OnBodyAnimationInitialized();
  GetWorld()->GetTimerManager().SetTimer(LocalInitTimer,this,&ThisClass::InitializeLocalSelection,.25f,true);
 }
}
void UPoliceAppearanceComponent::OnBodyAnimationInitialized()
{
 if(GetWorld())GetWorld()->GetTimerManager().SetTimer(PoseInitTimer,this,&ThisClass::RefreshEquippedPose,.5f,false);
}
void UPoliceAppearanceComponent::RefreshEquippedPose()
{
 if(auto* Character=Cast<AShooterCharacter>(GetOwner()))
  if(auto* Combat=Character->GetCombatComponent())
  {
   Combat->RefreshAnimsAfterMesh3PSwap();
   if(auto* Anim=Character->GetMesh()->GetAnimInstance())
    if(auto* Property=FindFProperty<FStructProperty>(Anim->GetClass(),TEXT("CurrentAnim"));Property&&Property->Struct==FPlayerAnim::StaticStruct())
    {
     auto* Pose=Property->ContainerPtrToValuePtr<FPlayerAnim>(Anim);
     if(!Pose->IdleAnim&&Combat->WeaponData)
      if(const auto* Default=Combat->WeaponData->ThirdPersonAnims.Find(ShooterTags::Tag_WeaponType_AK))*Pose=*Default;
    }
  }
}
bool UPoliceAppearanceComponent::IsValidSelection(EPoliceFaceProtection Face,EBodyArmorType Armor)
{
 return uint8(Face)<=uint8(EPoliceFaceProtection::GasMask)&&uint8(Armor)<=uint8(EBodyArmorType::Heavy);
}
void UPoliceAppearanceComponent::InitializeLocalSelection()
{
 auto* Character=Cast<AShooterCharacter>(GetOwner());
 if(!Character)return;
 if(Character->GetLocalRole()==ROLE_SimulatedProxy||(Character->GetController()&&!Character->IsLocallyControlled()))
 {GetWorld()->GetTimerManager().ClearTimer(LocalInitTimer);return;}
 if(!Character->IsLocallyControlled()||!Character->GetController())return;
 GetWorld()->GetTimerManager().ClearTimer(LocalInitTimer);
 if(bLocalInitialized||Character->GetFactionLook()!=ECharacterLookFaction::Police)return;
 bLocalInitialized=true;
 EPoliceFaceProtection Face=FaceProtection;
 EBodyArmorType Armor=Character->GetBodyArmor()==EBodyArmorType::None?EBodyArmorType::Light:Character->GetBodyArmor();
 if(UGameplayStatics::DoesSaveGameExist(AppearanceSlot,0))
  if(auto* Saved=Cast<UPoliceAppearanceSaveGame>(UGameplayStatics::LoadGameFromSlot(AppearanceSlot,0));
   Saved&&Saved->Version==1&&IsValidSelection(Saved->Face,Saved->Armor))
  {Face=Saved->Face;Armor=Saved->Armor;}
 if(Character->HasAuthority())ApplySelection(Face,Armor);
 else if(auto* PC=Cast<AShooterPlayerController>(Character->GetController()))GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(PC,[PC,Face,Armor](){PC->Server_SetPoliceProtection(Face,Armor);}));
 else Server_SetProtection(Face,Armor);
}
void UPoliceAppearanceComponent::SaveLocalSelection(EPoliceFaceProtection Face,EBodyArmorType Armor)
{
 const auto* Character=Cast<AShooterCharacter>(GetOwner());
 if(!Character||!Character->IsLocallyControlled()||PreviewTarget)return;
 auto* Saved=NewObject<UPoliceAppearanceSaveGame>();Saved->Face=Face;Saved->Armor=Armor;
 if(!UGameplayStatics::SaveGameToSlot(Saved,AppearanceSlot,0))
  UE_LOG(LogTemp,Warning,TEXT("[PoliceAppearance] Could not save local selection"));
}
void UPoliceAppearanceComponent::SetProtection(EPoliceFaceProtection Face,EBodyArmorType Armor)
{
 if(!IsValidSelection(Face,Armor))return;
 auto* Character=Cast<AShooterCharacter>(GetOwner());
 if(!Character||(!Character->HasAuthority()&&!Character->IsLocallyControlled()))return;
 SaveLocalSelection(Face,Armor);
 if(Character->HasAuthority())ApplySelection(Face,Armor);
 else
 {
  FaceProtection=Face;Character->BodyArmor=Armor;
  Character->bHasFlashGoggles=Face==EPoliceFaceProtection::FlashGoggles||Face==EPoliceFaceProtection::GogglesAndMask;
  RefreshAppearance();
  if(auto* PC=Cast<AShooterPlayerController>(Character->GetController()))GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(PC,[PC,Face,Armor](){PC->Server_SetPoliceProtection(Face,Armor);}));
  else Server_SetProtection(Face,Armor);
 }
}
void UPoliceAppearanceComponent::Server_SetProtection_Implementation(EPoliceFaceProtection Face,EBodyArmorType Armor)
{
 if(IsValidSelection(Face,Armor))ApplySelection(Face,Armor);
}
void UPoliceAppearanceComponent::ApplySelection(EPoliceFaceProtection Face,EBodyArmorType Armor)
{
 auto* Character=Cast<AShooterCharacter>(GetOwner());
 if(!Character||!Character->HasAuthority()||!IsValidSelection(Face,Armor))return;
 TGuardValue<bool> Guard(bApplyingSelection,true);
 FaceProtection=Face;
 Character->SetBodyArmorLocal(Armor);
 Character->SetHasFlashGoggles(Face==EPoliceFaceProtection::FlashGoggles||Face==EPoliceFaceProtection::GogglesAndMask);
 RefreshAppearance();OnBodyAnimationInitialized();Character->ForceNetUpdate();
 UE_LOG(LogTemp,Log,TEXT("[PoliceAppearance] %s face=%d armor=%d"),*Character->GetName(),uint8(Face),uint8(Armor));
}
void UPoliceAppearanceComponent::SyncFlashGoggles(bool bEnabled)
{
 if(bApplyingSelection||!GetOwner()->HasAuthority())return;
 const bool HasGoggles=FaceProtection==EPoliceFaceProtection::FlashGoggles||FaceProtection==EPoliceFaceProtection::GogglesAndMask;
 if(bEnabled&&!HasGoggles)FaceProtection=EPoliceFaceProtection::FlashGoggles;
 else if(!bEnabled&&HasGoggles)FaceProtection=EPoliceFaceProtection::None;
 RefreshAppearance();GetOwner()->ForceNetUpdate();
}
void UPoliceAppearanceComponent::OnRep_Protection(){RefreshAppearance();}
void UPoliceAppearanceComponent::InitializePreview(USkeletalMeshComponent* Target,EPoliceFaceProtection Face,EBodyArmorType Armor)
{
 if(!IsValid(Target)||PreviewTarget||!IsValidSelection(Face,Armor))return;
 SetIsReplicated(false);PreviewTarget=Target;
 OriginalPreviewMesh=Target->GetSkeletalMeshAsset();OriginalPreviewAnim=Target->GetAnimClass();OriginalPreviewMaterials=Target->OverrideMaterials;
 FaceProtection=Face;PreviewArmor=Armor;RefreshAppearance();
 auto* Fill=NewObject<UPointLightComponent>(GetOwner());
 Fill->ComponentTags.Add(TEXT("PolicePreviewFill"));Fill->SetupAttachment(Target);
 Fill->SetCastShadows(false);Fill->SetIntensity(2500.f);Fill->SetAttenuationRadius(600.f);
 GetOwner()->AddInstanceComponent(Fill);Fill->RegisterComponent();
 Fill->SetWorldLocation(Target->GetComponentLocation()+Target->GetRightVector()*200.f+FVector(0,0,140));
}
void UPoliceAppearanceComponent::SetPreviewProtection(EPoliceFaceProtection Face,EBodyArmorType Armor)
{
 if(!PreviewTarget||!IsValidSelection(Face,Armor))return;
 FaceProtection=Face;PreviewArmor=Armor;RefreshAppearance();
}
void UPoliceAppearanceComponent::RefreshAppearance()
{
 if(auto* C=Cast<AShooterCharacter>(GetOwner()))
 {
  if(auto* P=C->GetPouch())P->ReconcileCapacity();
  if(auto* PC=Cast<AShooterPlayerController>(C->GetController()))PC->RefreshMovementSpeed();
 }
 if(!GetOwner()||GetNetMode()==NM_DedicatedServer)return;
 auto* Character=Cast<AShooterCharacter>(GetOwner());
 USkeletalMeshComponent* Body=PreviewTarget?PreviewTarget.Get():(Character?Character->GetMesh():nullptr);
 const bool Active=Body&&(PreviewTarget||(Character&&Character->GetFactionLook()==ECharacterLookFaction::Police));
 if(!Active)
 {
  for(USkeletalMeshComponent* Part:Parts)if(Part){Part->SetVisibility(false);Part->SetHiddenInGame(true);}
  return;
 }
 UPoliceAppearanceData* Data=AppearanceData.LoadSynchronous();
 if(!Data)return;
 if(PreviewTarget)
  if(auto* Mesh=Data->BaseMesh.LoadSynchronous();Mesh&&Body->GetSkeletalMeshAsset()!=Mesh)
  {
   Body->EmptyOverrideMaterials();Body->SetSkeletalMesh(Mesh,true);
   if(OriginalPreviewAnim)Body->SetAnimInstanceClass(OriginalPreviewAnim);
   else if(auto* Idle=LoadObject<UAnimationAsset>(nullptr,TEXT("/Game/FPS/Characters/Police/A_Police_LoadoutPose.A_Police_LoadoutPose")))Body->PlayAnimation(Idle,true);
  }
 if(!Body->GetSkeletalMeshAsset()||Body->GetSkeletalMeshAsset()!=Data->BaseMesh.Get())return;
 const EBodyArmorType Armor=PreviewTarget?PreviewArmor:Character->GetBodyArmor();
 const auto& Materials=Body->GetSkeletalMeshAsset()->GetMaterials();
 for(int32 LOD=0;LOD<Body->GetSkeletalMeshAsset()->GetLODNum();++LOD)
  for(int32 I=0;I<Materials.Num();++I)
  {
   const FName Slot=Materials[I].MaterialSlotName;
   const bool Hidden=Data->ReplacedMaterialSlots.Contains(Slot)||(Armor==EBodyArmorType::None&&Data->UnarmoredHiddenSlots.Contains(Slot));
   Body->ShowMaterialSection(I,INDEX_NONE,!Hidden,LOD);
  }
 TArray<USkeletalMesh*> Desired;
 if(Armor!=EBodyArmorType::None)
  for(const auto& Mesh:Armor==EBodyArmorType::Heavy?Data->HeavyArmor:Data->LightArmor)
   if(auto* Loaded=Mesh.LoadSynchronous())Desired.Add(Loaded);
 if(FaceProtection==EPoliceFaceProtection::FlashGoggles||FaceProtection==EPoliceFaceProtection::GogglesAndMask)
  if(auto* Mesh=Data->Goggles.LoadSynchronous())Desired.Add(Mesh);
 if(FaceProtection==EPoliceFaceProtection::GogglesAndMask)
 {if(auto* Mesh=Data->BallisticMask.LoadSynchronous())Desired.Add(Mesh);}
 else if(FaceProtection==EPoliceFaceProtection::GasMask)
 {if(auto* Mesh=Data->GasMask.LoadSynchronous())Desired.Add(Mesh);}
 while(Parts.Num()<Desired.Num())
 {
  auto* Part=NewObject<USkeletalMeshComponent>(GetOwner(),*FString::Printf(TEXT("PoliceModule_%d"),Parts.Num()));
  Part->SetupAttachment(Body);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetGenerateOverlapEvents(false);
  Part->SetCanEverAffectNavigation(false);Part->bUseAttachParentBound=true;Part->bReceivesDecals=false;
  GetOwner()->AddInstanceComponent(Part);Part->RegisterComponent();Parts.Add(Part);
 }
 for(int32 I=0;I<Parts.Num();++I)
 {
  auto* Part=Parts[I].Get();if(!Part)continue;
  USkeletalMesh* Mesh=Desired.IsValidIndex(I)?Desired[I]:nullptr;
  if(Part->GetSkeletalMeshAsset()!=Mesh){Part->EmptyOverrideMaterials();Part->SetSkeletalMesh(Mesh);}
  Part->SetLeaderPoseComponent(Mesh?Body:nullptr);
  Part->SetVisibility(Mesh&&Body->IsVisible());Part->SetHiddenInGame(!Mesh||Body->bHiddenInGame);
  Part->SetOwnerNoSee(Body->bOwnerNoSee);Part->SetOnlyOwnerSee(Body->bOnlyOwnerSee);
  Part->SetLightingChannels(Body->LightingChannels.bChannel0,Body->LightingChannels.bChannel1,Body->LightingChannels.bChannel2);
 }
}
void UPoliceAppearanceComponent::ClearParts()
{
 for(USkeletalMeshComponent* Part:Parts)if(IsValid(Part))Part->DestroyComponent();Parts.Reset();
 if(GetOwner())
 {
  TArray<UPointLightComponent*> Lights;GetOwner()->GetComponents(Lights);
  for(auto* Light:Lights)if(Light->ComponentHasTag(TEXT("PolicePreviewFill")))Light->DestroyComponent();
 }
}
void UPoliceAppearanceComponent::RestorePreview()
{
 if(IsValid(PreviewTarget))
 {
  if(auto* Mesh=PreviewTarget->GetSkeletalMeshAsset())
   for(int32 LOD=0;LOD<Mesh->GetLODNum();++LOD)PreviewTarget->ShowAllMaterialSections(LOD);
  PreviewTarget->SetSkeletalMesh(OriginalPreviewMesh);PreviewTarget->OverrideMaterials=OriginalPreviewMaterials;
  if(OriginalPreviewAnim)PreviewTarget->SetAnimInstanceClass(OriginalPreviewAnim);
 }
 PreviewTarget=nullptr;
}
void UPoliceAppearanceComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 if(UWorld* World=GetWorld())
 {World->GetTimerManager().ClearTimer(LocalInitTimer);World->GetTimerManager().ClearTimer(PoseInitTimer);}
 if(auto* Character=Cast<AShooterCharacter>(GetOwner()))
  if(auto* Mesh=Character->GetMesh();IsValid(Mesh))Mesh->OnAnimInitialized.RemoveDynamic(this,&ThisClass::OnBodyAnimationInitialized);
 RestorePreview();ClearParts();Super::EndPlay(Reason);
}
void UPoliceAppearanceComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
 RestorePreview();ClearParts();Super::OnComponentDestroyed(bDestroyingHierarchy);
}
