#include "Game/BombWeapon.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ABombWeapon::ABombWeapon()
{
	WeaponKind = EWeaponKind::Bomb;
	bCanSwitchFireMode = false;
	MagCapacity = 0;
	Ammo = 0;
	StartingCarriedAmmo = 0;

	auto SetupVisual = [](UStaticMeshComponent* Visual)
	{
		if (!Visual) return;
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Visual->SetGenerateOverlapEvents(false);
		Visual->SetRelativeScale3D(FVector(0.22f, 0.14f, 0.09f));
		Visual->SetRelativeLocation(FVector(12.f, 4.f, 0.f));
	};

	Visual1P = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BombVisual1P"));
	Visual1P->SetupAttachment(Mesh1P);
	SetupVisual(Visual1P);

	Visual3P = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BombVisual3P"));
	Visual3P->SetupAttachment(Mesh3P);
	SetupVisual(Visual3P);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Visual1P->SetStaticMesh(CubeMesh.Object);
		Visual3P->SetStaticMesh(CubeMesh.Object);
	}
}
