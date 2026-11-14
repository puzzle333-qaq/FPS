#include "Game/HostageActor.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Game/ShooterPlayerState.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Net/UnrealNetwork.h"
#include "Game/CoopCqbGameMode.h"
#include "Game/CoopCqbGameState.h"
#include "Game/MissionRadioComponent.h"

AHostageActor::AHostageActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SetRootComponent(Capsule);
	Capsule->InitCapsuleSize(34.f, 88.f);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
 Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
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

	CivilianMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CivilianMesh"));
 CivilianMesh->SetupAttachment(Capsule);
 CivilianMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 CivilianMesh->SetRelativeLocation(FVector(0, 0, -88));
 CivilianMesh->SetRelativeRotation(FRotator(0, -90, 0));
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
 if (Manny.Succeeded()) { CivilianMesh->SetSkeletalMesh(Manny.Object); BodyMesh->SetVisibility(false); }
 RescuedCrouchAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/ThirdPerson/Animations/Locomotion/Pistol/MM_Pistol_Crouch_Idle.MM_Pistol_Crouch_Idle")));
 Tags.AddUnique(FName(TEXT("Hostage")));
	Health = MaxHealth;
}

void AHostageActor::BeginPlay()
{
	Super::BeginPlay();
	Tags.AddUnique(FName(TEXT("Hostage")));
	if (HasAuthority()) { Health = MaxHealth; bAlive = true; }
 OnRep_RescueState();
}

bool AHostageActor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	const auto* GS = GetWorld()->GetGameState<ACoopCqbGameState>();
 return bAlive && !bRescued && IsValid(InteractingPawn) && GS && GS->IsMissionLive() && !GS->bRangeHub;
}

FText AHostageActor::GetInteractPrompt_Implementation(APawn* InteractingPawn) const
{
 return bAlive && !bRescued ? NSLOCTEXT("Hostage", "Rescue", "按 F 解救人质") : FText::GetEmpty();
}
void AHostageActor::Interact_Implementation(APawn* InteractingPawn)
{
 if (!HasAuthority() || !CanInteract_Implementation(InteractingPawn)) return;
 bRescued = true;
 bFollowing = false;
 FollowTarget = nullptr;
 OnRep_RescueState();
 ForceNetUpdate();
 if(auto* GS=GetWorld()->GetGameState<ACoopCqbGameState>();GS&&GS->MissionRadio)GS->MissionRadio->QueueCue(EMissionVoiceCue::CivilianSecured);
 if (auto* GM = GetWorld()->GetAuthGameMode<ACoopCqbGameMode>()) GM->Auth_RefreshObjectives();
}
void AHostageActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(AHostageActor, bRescued);
 DOREPLIFETIME(AHostageActor, bAlive);
 DOREPLIFETIME(AHostageActor, Health);
}
void AHostageActor::OnRep_RescueState()
{
 SetActorHiddenInGame(!bAlive);
 if (!bAlive) Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 if (bRescued && bAlive)
 {
  // Only change pose. Never move the actor or attach it to the rescuing operator.
  if (auto* Anim = RescuedCrouchAnimation.LoadSynchronous()) CivilianMesh->PlayAnimation(Anim, true);
  else BodyMesh->SetRelativeScale3D(FVector(.45f, .45f, .8f));
 }
}

bool AHostageActor::DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator)
{
	if (!HasAuthority() || !bAlive || DamageAmout <= 0.f) return false;
	if (const auto* GS = GetWorld()->GetGameState<ACoopCqbGameState>())
 {
  if (!GS->IsMissionLive() || GS->bRangeHub) return false;
 }
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
 ForceNetUpdate();
 if (auto* GM = GetWorld()->GetAuthGameMode<ACoopCqbGameMode>()) GM->Auth_RefreshObjectives();
	UE_LOG(LogTemp, Warning, TEXT("[Hostage] %s killed by %s (team=%d)"), *GetName(), *GetNameSafe(Killer), LastDamageTeam);
}
