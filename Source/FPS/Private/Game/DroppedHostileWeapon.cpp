#include "Game/DroppedHostileWeapon.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
ADroppedHostileWeapon::ADroppedHostileWeapon()
{
 bReplicates=true;SetReplicateMovement(true);
 Collision=CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));SetRootComponent(Collision);Collision->InitSphereRadius(7.f);Collision->SetCollisionProfileName(TEXT("PhysicsActor"));Collision->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);Collision->SetCanEverAffectNavigation(false);
 Visual=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Visual"));Visual->SetupAttachment(Collision);Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ADroppedHostileWeapon::BeginPlay(){Super::BeginPlay();Collision->SetSimulatePhysics(HasAuthority());}
void ADroppedHostileWeapon::Initialize(USkeletalMeshComponent* Source,const FVector& Direction)
{
 if(!HasAuthority()||!Source)return;
 GunMesh=Source->GetSkeletalMeshAsset();VisualScale=Source->GetComponentScale();
 for(int32 I=0;I<Source->GetNumMaterials();++I)GunMaterials.Add(Source->GetMaterial(I));
 RefreshVisual();Collision->SetSimulatePhysics(true);Collision->SetPhysicsLinearVelocity(Direction.GetSafeNormal2D()*160.f+FVector(0,0,100));ForceNetUpdate();
}
void ADroppedHostileWeapon::RefreshVisual(){Visual->SetSkeletalMesh(GunMesh);Visual->SetRelativeScale3D(VisualScale);for(int32 I=0;I<GunMaterials.Num();++I)Visual->SetMaterial(I,GunMaterials[I]);}
void ADroppedHostileWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ADroppedHostileWeapon,GunMesh);DOREPLIFETIME(ADroppedHostileWeapon,GunMaterials);DOREPLIFETIME(ADroppedHostileWeapon,VisualScale);}
