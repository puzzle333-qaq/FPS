#include "Game/HostileAICharacter.h"

#include "Game/CoopCqbGameMode.h"
#include "Game/HostileAIController.h"
#include "Stats/StatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"

FName AHostileAICharacter::HostileTag(TEXT("FPS_Hostile"));

AHostileAICharacter::AHostileAICharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AHostileAIController::StaticClass();

	Tags.AddUnique(HostileTag);

	Stats = CreateDefaultSubobject<UStatsComponent>(TEXT("Stats"));

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 480.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 320.f;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 160.f;
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(60.f);
	bUseControllerRotationYaw = false;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshFinder(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (MeshFinder.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MeshFinder.Object);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}
}

void AHostileAICharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHostileAICharacter, bDead);
}

void AHostileAICharacter::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && Stats)
	{
		Stats->MaxHealth = MaxHealth;
		Stats->Health = MaxHealth;
		Stats->DeathState = EDeathState::NotDead;
	}
	if (!BodyMesh.IsNull())
	{
		if (USkeletalMesh* Loaded = BodyMesh.LoadSynchronous())
		{
			GetMesh()->SetSkeletalMesh(Loaded);
		}
	}
}

bool AHostileAICharacter::IsHostileAlive() const
{
	if (bDead) return false;
	if (Stats && Stats->DeathState != EDeathState::NotDead) return false;
	return true;
}

bool AHostileAICharacter::DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator)
{
	if (!HasAuthority() || !IsHostileAlive() || DamageAmout <= 0.f) return false;
	if (!Stats) return false;

	Stats->ChangeHealthByAmout(-DamageAmout, DamageInstigator);
	if (AHostileAIController* Brain = Cast<AHostileAIController>(GetController()))
	{
		const FVector From = IsValid(DamageInstigator) ? DamageInstigator->GetActorLocation() : GetActorLocation();
		Brain->NotifyTookDamage(DamageInstigator, From);
	}
	if (Stats->Health <= 0.f || Stats->DeathState != EDeathState::NotDead)
	{
		Auth_Die(DamageInstigator);
		return true;
	}
	return false;
}

float AHostileAICharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Actual = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	AActor* Inst = DamageCauser ? DamageCauser : (EventInstigator ? EventInstigator->GetPawn() : nullptr);
	DoDamage_Implementation(Actual > 0.f ? Actual : DamageAmount, Inst);
	return Actual;
}

void AHostileAICharacter::Auth_Die(AActor* Killer)
{
	if (!HasAuthority() || bDead) return;
	bDead = true;
	if (Stats)
	{
		Stats->DeathState = EDeathState::DeadFinished;
		Stats->Health = 0.f;
	}
	ApplyDeadVisual();
	ForceNetUpdate();

	if (AController* C = GetController())
	{
		C->UnPossess();
	}

	if (ACoopCqbGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACoopCqbGameMode>() : nullptr)
	{
		GM->NotifyHostileKilled(this);
	}
	UE_LOG(LogTemp, Log, TEXT("[Hostile] %s died (killer=%s)"), *GetNameSafe(this), *GetNameSafe(Killer));
}

void AHostileAICharacter::OnRep_Dead()
{
	ApplyDeadVisual();
}

void AHostileAICharacter::ApplyDeadVisual()
{
	if (!bDead) return;
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->StopMovementImmediately();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		Skel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Skel->SetSimulatePhysics(true);
	}
}

