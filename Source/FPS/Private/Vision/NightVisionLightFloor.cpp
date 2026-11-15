#include "Vision/NightVisionLightFloor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
ANightVisionLightFloor::ANightVisionLightFloor()
{
 PrimaryActorTick.bCanEverTick=false;
 SceneRoot=CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));SetRootComponent(SceneRoot);
 Moonlight=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moonlight"));Moonlight->SetupAttachment(SceneRoot);
 Moonlight->SetMobility(EComponentMobility::Movable);Moonlight->SetIntensity(.25f);
 Moonlight->SetLightColor(FLinearColor(.62f,.74f,1.f));Moonlight->SetRelativeRotation(FRotator(-25,-35,0));Moonlight->SetCastShadows(true);
 Moonlight->SetVolumetricScatteringIntensity(.1f);
 AmbientFloor=CreateDefaultSubobject<USkyLightComponent>(TEXT("AmbientFloor"));AmbientFloor->SetupAttachment(SceneRoot);
 AmbientFloor->SetMobility(EComponentMobility::Movable);AmbientFloor->SetIntensity(.04f);
 AmbientFloor->SetLightColor(FLinearColor(.62f,.69f,.82f));
 AmbientFloor->SourceType=ESkyLightSourceType::SLS_SpecifiedCubemap;
 static ConstructorHelpers::FObjectFinder<UTextureCube> Cube(TEXT("/Engine/EngineResources/GrayTextureCube.GrayTextureCube"));
 if(Cube.Succeeded())AmbientFloor->Cubemap=Cube.Object;
 // Shadowed sky keeps sealed interiors darker than rooms with a light path.
 AmbientFloor->SetCastShadows(true);
 // Very weak diffuse-only hemisphere approximation for the factory's sealed
 // blockout rooms. Upward hemispheres keep this approximation out of the
 // sealed basement; non-inverse falloff avoids camera/hand hot spots.
 auto* West=CreateDefaultSubobject<USpotLightComponent>(TEXT("InteriorSignalWest"));
 auto* East=CreateDefaultSubobject<USpotLightComponent>(TEXT("InteriorSignalEast"));
 for(auto* Hemisphere:{West,East})
 {
  Hemisphere->SetInnerConeAngle(80.f);Hemisphere->SetOuterConeAngle(89.f);
  Hemisphere->SetRelativeRotation(FRotator(90,0,0));
 }
 InteriorSignal={West,East};
 for(int32 I=0;I<InteriorSignal.Num();++I)
 {
  auto* Light=InteriorSignal[I].Get();Light->SetupAttachment(SceneRoot);Light->SetMobility(EComponentMobility::Movable);
  Light->SetRelativeLocation(FVector(I?4200.f:1800.f,-1800.f,-430.f));
  Light->bUseInverseSquaredFalloff=false;Light->IntensityUnits=ELightUnits::Unitless;Light->LightFalloffExponent=2.f;
  Light->SetIntensity(.05f);Light->SetAttenuationRadius(9000.f);Light->SetCastShadows(false);
  Light->SetSpecularScale(0.f);Light->SetIndirectLightingIntensity(0.f);Light->SetVolumetricScatteringIntensity(0.f);
  Light->SetLightColor(FLinearColor(.62f,.74f,1.f));
 }
 Tags.Add(TEXT("NVGPersistentLight"));
}
void ANightVisionLightFloor::PrepareInfraredReceiver(AActor* Actor)
{
 if(!IsValid(Actor))return;
 TInlineComponentArray<UMeshComponent*> Meshes(Actor);
 for(auto* Mesh:Meshes)
 {
  bool FirstPerson=false;
  for(auto* Parent=static_cast<USceneComponent*>(Mesh);Parent;Parent=Parent->GetAttachParent())
  {
   if(auto* Primitive=Cast<UPrimitiveComponent>(Parent);Primitive&&Primitive->bOnlyOwnerSee){FirstPerson=true;break;}
   if(Parent->GetFName()==TEXT("Mesh1P")||Parent->GetFName()==TEXT("Mesh1P_Sleeves")){FirstPerson=true;break;}
  }
  const auto Existing=Mesh->LightingChannels;
  if(Existing.bChannel1==!FirstPerson)continue;
  Mesh->SetLightingChannels(Existing.bChannel0,!FirstPerson,Existing.bChannel2);
 }
}
void ANightVisionLightFloor::HandleSpawned(AActor* Actor)
{
 TWeakObjectPtr<AActor> Weak=Actor;
 GetWorld()->GetTimerManager().SetTimerForNextTick([Weak](){if(Weak.IsValid())PrepareInfraredReceiver(Weak.Get());});
}
void ANightVisionLightFloor::BeginPlay()
{
 Super::BeginPlay();
 for(TActorIterator<AActor> It(GetWorld());It;++It)PrepareInfraredReceiver(*It);
 SpawnHandle=GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this,&ThisClass::HandleSpawned));
}
void ANightVisionLightFloor::EndPlay(const EEndPlayReason::Type Reason)
{
 if(GetWorld()&&SpawnHandle.IsValid())GetWorld()->RemoveOnActorSpawnedHandler(SpawnHandle);
 Super::EndPlay(Reason);
}
