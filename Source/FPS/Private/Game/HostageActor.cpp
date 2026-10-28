#include "Game/HostageActor.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Game/ShooterPlayerState.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

AHostageActor::AHostageActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SetRootComponent(Capsule);
	Capsule->InitCapsuleSize(34.f, 88.f);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	Capsule->SetCanEverAffectNavigation(false);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Capsule);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeMesh.Object);
		BodyMesh->SetRelativeScale3D(FVector(0.45f, 0.45f, 1.6f));
		BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	}

	Tags.AddUnique(FName(TEXT("Hostage")));
	Health = MaxHealth;
}

void AHostageActor::BeginPlay()
{
	Super::BeginPlay();
	Tags.AddUnique(FName(TEXT("Hostage")));
	Health = MaxHealth;
	bAlive = true;
}

bool AHostageActor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return bAlive && IsValid(InteractingPawn);
}

FText AHostageActor::GetInteractPrompt_Implementation(APawn* InteractingPawn) const
{
	if (!bAlive) return FText::GetEmpty();
	return bFollowing
		? NSLOCTEXT("Hostage", "StopFollow", "按 F 停止跟随")
		: NSLOCTEXT("Hostage", "Follow", "按 F 跟随（人质占位）");
}

void AHostageActor::Interact_Implementation(APawn* InteractingPawn)
{
	if (!HasAuthority() || !bAlive || !IsValid(InteractingPawn)) return;
	if (bFollowing && FollowTarget.Get() == InteractingPawn)
	{
		bFollowing = false;
		FollowTarget = nullptr;
		UE_LOG(LogTemp, Log, TEXT("[Hostage] %s stopped following"), *GetName());
		return;
	}
	bFollowing = true;
	FollowTarget = InteractingPawn;
	// Stub: teleport slightly behind interactor; full AI later.
	const FVector Behind = InteractingPawn->GetActorLocation() - InteractingPawn->GetActorForwardVector() * 80.f;
	SetActorLocation(Behind);
	UE_LOG(LogTemp, Log, TEXT("[Hostage] %s now following %s (stub)"), *GetName(), *GetNameSafe(InteractingPawn));
}

bool AHostageActor::DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator)
{
	if (!HasAuthority() || !bAlive || DamageAmout <= 0.f) return false;
	uint8 Team = 255;
	if (const APawn* P = Cast<APawn>(DamageInstigator))
	{
		if (const AShooterPlayerState* PS = P->GetPlayerState<AShooterPlayerState>())
		{
			Team = PS->GetTeamId();
		}
	}
	else if (const AController* C = Cast<AController>(DamageInstigator))
	{
		if (const AShooterPlayerState* PS = C->GetPlayerState<AShooterPlayerState>())
		{
			Team = PS->GetTeamId();
		}
	}
	LastDamageTeam = Team;
	Health = FMath::Max(0.f, Health - DamageAmout);
	UE_LOG(LogTemp, Log, TEXT("[Hostage] %s dmg=%.1f hp=%.1f team=%d"), *GetName(), DamageAmout, Health, Team);
	if (Health <= 0.f)
	{
		Die(DamageInstigator);
	}
	return true;
}

void AHostageActor::Die(AActor* Killer)
{
	if (!bAlive) return;
	bAlive = false;
	bFollowing = false;
	FollowTarget = nullptr;
	if (Capsule)
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetActorHiddenInGame(true);
	UE_LOG(LogTemp, Warning, TEXT("[Hostage] %s killed by %s (team=%d)"), *GetName(), *GetNameSafe(Killer), LastDamageTeam);
}
