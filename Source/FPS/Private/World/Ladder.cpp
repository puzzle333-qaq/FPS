#include "World/Ladder.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ALadder::ALadder()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // static level geometry, exists on server and clients

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->SetCanEverAffectNavigation(false);
}

void ALadder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Placeholder ladder: 10cm thick slab, Width x Height, front face at X=0.
	Mesh->SetRelativeLocation(FVector(5.f, 0.f, Height * 0.5f));
	Mesh->SetRelativeScale3D(FVector(0.1f, Width / 100.f, Height / 100.f));
	Mesh->SetVisibility(bShowPlaceholderMesh);
	Mesh->SetCollisionEnabled(bShowPlaceholderMesh ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

	// Trigger in front of the ladder (climber side), reaching a bit above the top.
	Trigger->SetRelativeLocation(FVector(-StandOff * 0.5f - 10.f, 0.f, Height * 0.5f + 20.f));
	Trigger->SetBoxExtent(FVector(StandOff * 0.5f + 35.f, Width * 0.5f + 20.f, Height * 0.5f + 20.f));
}

FVector ALadder::GetClimbForward() const
{
	FVector F = GetActorForwardVector();
	F.Z = 0.f;
	return F.GetSafeNormal();
}

FVector ALadder::GetClimbLocation(float FeetZ, float CapsuleHalfHeight) const
{
	const FVector Base = GetActorLocation() - GetClimbForward() * StandOff;
	return FVector(Base.X, Base.Y, FeetZ + CapsuleHalfHeight);
}

FVector ALadder::GetTopExitLocation(float CapsuleHalfHeight) const
{
	const FVector Top = GetActorLocation() + GetClimbForward() * TopExitForward;
	return FVector(Top.X, Top.Y, GetTopZ() + CapsuleHalfHeight + 3.f);
}
