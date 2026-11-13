#include "World/GlowStick.h"

#include "FPSTraceChannels.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AGlowStick::AGlowStick()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	InitialLifeSpan = 180.f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(5.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionObjectType(FPSTraceChannels::ECC_Projectile);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Collision->SetNotifyRigidBodyCollision(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Mesh->SetStaticMesh(Cylinder.Object);
		Mesh->SetRelativeScale3D(FVector(0.025f, 0.025f, 0.16f));
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Emissive(TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
	if (Emissive.Succeeded())
	{
		Mesh->SetMaterial(0, Emissive.Object);
	}

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Collision);
	Glow->SetIntensity(80.f);
	Glow->SetAttenuationRadius(550.f);
	Glow->SetLightColor(FLinearColor(0.35f, 1.f, 0.28f));
	Glow->SetCastShadows(false);
	Glow->SetMobility(EComponentMobility::Movable);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = Collision;
	ProjectileMovement->InitialSpeed = 0.f;
	ProjectileMovement->MaxSpeed = 1200.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 1.f;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.18f;
	ProjectileMovement->Friction = 0.55f;
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = 35.f;
	ProjectileMovement->bInterpMovement = true;
	ProjectileMovement->bInterpRotation = true;
}

void AGlowStick::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Dropper = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(Dropper, true);
	}
	if (IsValid(ProjectileMovement))
	{
		ProjectileMovement->OnProjectileStop.AddDynamic(this, &AGlowStick::OnStopped);
	}
}

void AGlowStick::Launch(const FVector& Direction, float Speed)
{
	const FVector Dir = Direction.GetSafeNormal();
	const float LaunchSpeed = FMath::Max(0.f, Speed);
	if (!IsValid(ProjectileMovement)) return;
	ProjectileMovement->InitialSpeed = LaunchSpeed;
	ProjectileMovement->Velocity = Dir * LaunchSpeed;
	ProjectileMovement->SetUpdatedComponent(Collision);
	if (!Dir.IsNearlyZero())
	{
		SetActorRotation(Dir.Rotation());
	}
}

void AGlowStick::OnStopped(const FHitResult& Hit)
{
	(void)Hit;
	if (!IsValid(ProjectileMovement)) return;
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->bRotationFollowsVelocity = false;
}
