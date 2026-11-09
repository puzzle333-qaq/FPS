#include "Game/HostileAICharacter.h"

#include "Game/CoopCqbGameMode.h"
#include "Game/CoopCqbGameState.h"
#include "Game/HostileAIController.h"
#include "Stats/StatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
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
 GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
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
	DOREPLIFETIME(AHostileAICharacter, Morale);
 DOREPLIFETIME(AHostileAICharacter, bArrested);
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

void AHostileAICharacter::SetMorale(EHostileMorale NewMorale)
{
	if (!HasAuthority()) return;
	if (bArrested) return;
 Morale = NewMorale;
 if (Morale == EHostileMorale::Surrender)
 {
  Crouch();
  GetCharacterMovement()->StopMovementImmediately();
 }
 ForceNetUpdate();
}

void AHostileAICharacter::Multicast_HostileFired_Implementation(FVector Muzzle, FVector Impact)
{
	OnHostileFired(Muzzle, Impact);
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
 if (const auto* GS = GetWorld()->GetGameState<ACoopCqbGameState>())
 {
  if (!GS->IsMissionLive() || GS->bRangeHub) return false;
 }

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

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
		Move->SetComponentTickEnabled(false);
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	USkeletalMeshComponent* Skel = GetMesh();
	if (!Skel) return;

	if (!Skel->GetPhysicsAsset())
	{
		if (UPhysicsAsset* Asset = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin")))
		{
			Skel->SetPhysicsAsset(Asset);
		}
	}

	Skel->SetCollisionProfileName(TEXT("Ragdoll"));
	Skel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Skel->SetAllBodiesSimulatePhysics(true);
	Skel->SetSimulatePhysics(true);
	Skel->WakeAllRigidBodies();
	Skel->bBlendPhysics = true;
	Skel->bPauseAnims = true;
}


bool AHostileAICharacter::CanInteract_Implementation(APawn* Pawn) const
{
 const auto* GS = GetWorld()->GetGameState<ACoopCqbGameState>();
 return IsValid(Pawn) && IsHostileAlive() && !bArrested && Morale == EHostileMorale::Surrender
  && GS && GS->IsMissionLive() && !GS->bRangeHub;
}
FText AHostileAICharacter::GetInteractPrompt_Implementation(APawn* Pawn) const
{
 return NSLOCTEXT("Mission", "Arrest", "按 F 拘捕嫌疑人");
}
void AHostileAICharacter::Interact_Implementation(APawn* Pawn)
{
 if (!HasAuthority() || !CanInteract_Implementation(Pawn)) return;
 bArrested = true;
 Crouch();
 OnRep_Arrested();
 ForceNetUpdate();
 if (auto* GM = GetWorld()->GetAuthGameMode<ACoopCqbGameMode>()) GM->Auth_RefreshObjectives();
}
void AHostileAICharacter::OnRep_Arrested()
{
 if (!bArrested) return;
 GetCharacterMovement()->StopMovementImmediately();
 if (auto* Brain = Cast<AHostileAIController>(GetController()))
 {
  Brain->StopMovement();
  Brain->SetActorTickEnabled(false);
 }
 // Keep the pawn and its crouching animation in place after restraint.
}
