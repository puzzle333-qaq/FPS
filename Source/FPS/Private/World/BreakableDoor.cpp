#include "World/BreakableDoor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "FPSTraceChannels.h"
#include "TimerManager.h"

ABreakableDoor::ABreakableDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(SceneRoot);

	DoorLeaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorLeaf"));
	DoorLeaf->SetupAttachment(Hinge);
	DoorLeaf->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DoorLeaf->SetCollisionObjectType(ECC_WorldDynamic);
	DoorLeaf->SetCollisionResponseToAllChannels(ECR_Block);
	DoorLeaf->SetCollisionResponseToChannel(FPSTraceChannels::ECC_Weapon, ECR_Block);
	DoorLeaf->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DoorMeshFinder(
		TEXT("/Game/FPS_Controller/Environment/Meshes/SM_Door.SM_Door"));
	if (DoorMeshFinder.Succeeded())
	{
		DoorLeaf->SetStaticMesh(DoorMeshFinder.Object);
		// SM_Door pivot is typically at center; offset so hinge is on the -Y edge.
		DoorLeaf->SetRelativeLocation(FVector(0.f, 50.f, 0.f));
	}

	InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
	InteractBox->SetupAttachment(DoorLeaf);
	InteractBox->InitBoxExtent(FVector(6.f, 50.f, 110.f));
	InteractBox->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
	InteractBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractBox->SetCollisionResponseToChannel(FPSTraceChannels::ECC_Weapon, ECR_Block);
	InteractBox->SetCanEverAffectNavigation(false);
}

void ABreakableDoor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Health = MaxHealth;
		bDestroyed = false;
		bOpen = false;
	}
	CurrentYaw = 0.f;
	TargetYaw = 0.f;
	ApplyOpenVisual(true);
	ApplyDestroyedVisual(false);
}

void ABreakableDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDestroyed) return;
	if (FMath::IsNearlyEqual(CurrentYaw, TargetYaw, 0.05f))
	{
		CurrentYaw = TargetYaw;
		if (IsValid(Hinge))
		{
			Hinge->SetRelativeRotation(FRotator(0.f, CurrentYaw, 0.f));
		}
		return;
	}
	CurrentYaw = FMath::FInterpTo(CurrentYaw, TargetYaw, DeltaSeconds, OpenInterpSpeed);
	if (IsValid(Hinge))
	{
		Hinge->SetRelativeRotation(FRotator(0.f, CurrentYaw, 0.f));
	}
}

void ABreakableDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABreakableDoor, bOpen);
	DOREPLIFETIME(ABreakableDoor, bDestroyed);
	DOREPLIFETIME(ABreakableDoor, Health);
}

bool ABreakableDoor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	if (bDestroyed) return false;
	return IsValid(InteractingPawn);
}

FText ABreakableDoor::GetInteractPrompt_Implementation(APawn* InteractingPawn) const
{
	(void)InteractingPawn;
	if (bDestroyed)
	{
		return NSLOCTEXT("Door", "Destroyed", "");
	}
	return bOpen
		? NSLOCTEXT("Door", "Close", "按 F 关门")
		: NSLOCTEXT("Door", "Open", "按 F 开门");
}

void ABreakableDoor::Interact_Implementation(APawn* InteractingPawn)
{
	if (!HasAuthority() || bDestroyed || !IsValid(InteractingPawn)) return;
	Auth_SetOpen(!bOpen);
}

void ABreakableDoor::Auth_SetOpen(bool bNewOpen)
{
	if (!HasAuthority() || bDestroyed) return;
	if (bOpen == bNewOpen) return;
	bOpen = bNewOpen;
	ApplyOpenVisual(false);
	ForceNetUpdate();

	USoundBase* Snd = bOpen ? OpenSound.Get() : CloseSound.Get();
	if (IsValid(Snd))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Snd, GetActorLocation());
	}
}

void ABreakableDoor::OnRep_Open()
{
	ApplyOpenVisual(false);
}

void ABreakableDoor::OnRep_Destroyed()
{
	ApplyDestroyedVisual(true);
}

void ABreakableDoor::ApplyOpenVisual(bool bSnap)
{
	TargetYaw = bOpen ? (OpenAngleDegrees * OpenDirection) : 0.f;
	if (bSnap)
	{
		CurrentYaw = TargetYaw;
		if (IsValid(Hinge))
		{
			Hinge->SetRelativeRotation(FRotator(0.f, CurrentYaw, 0.f));
		}
	}
}

bool ABreakableDoor::DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator)
{
	if (!HasAuthority() || bDestroyed || !bCanBreak || DamageAmout <= 0.f) return false;

	Health = FMath::Max(0.f, Health - DamageAmout);
	UE_LOG(LogTemp, Log, TEXT("[Door] %s took %.1f dmg, HP=%.1f"), *GetNameSafe(this), DamageAmout, Health);
	if (Health <= 0.f)
	{
		Auth_DestroyDoor(DamageInstigator);
		return true;
	}
	ForceNetUpdate();
	return false;
}

void ABreakableDoor::Auth_DestroyDoor(AActor* InstigatorActor)
{
	(void)InstigatorActor;
	if (!HasAuthority() || bDestroyed) return;
	bDestroyed = true;
	bOpen = false;
	Health = 0.f;
	ApplyDestroyedVisual(true);
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[Door] %s destroyed"), *GetNameSafe(this));
}

void ABreakableDoor::ApplyDestroyedVisual(bool bSpawnDebris)
{
	if (!bDestroyed)
	{
		if (IsValid(DoorLeaf))
		{
			DoorLeaf->SetVisibility(true);
			DoorLeaf->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		if (IsValid(InteractBox))
		{
			InteractBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
		return;
	}

	if (IsValid(DoorLeaf))
	{
		DoorLeaf->SetVisibility(false);
		DoorLeaf->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (IsValid(InteractBox))
	{
		InteractBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (IsValid(BreakSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, BreakSound, GetActorLocation());
	}

	if (bSpawnDebris)
	{
		SpawnDebrisLocal();
	}
}

void ABreakableDoor::SpawnDebrisLocal()
{
	UWorld* World = GetWorld();
	if (!World) return;

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube) return;

	const FVector Origin = IsValid(DoorLeaf) ? DoorLeaf->GetComponentLocation() : GetActorLocation();
	for (int32 i = 0; i < FMath::Clamp(DebrisCount, 0, 12); ++i)
	{
		const FVector Offset(
			FMath::FRandRange(-30.f, 30.f),
			FMath::FRandRange(-40.f, 40.f),
			FMath::FRandRange(40.f, 160.f));
		UStaticMeshComponent* Chunk = NewObject<UStaticMeshComponent>(this);
		if (!Chunk) continue;
		Chunk->SetStaticMesh(Cube);
		Chunk->SetMobility(EComponentMobility::Movable);
		Chunk->SetWorldScale3D(FVector(
			FMath::FRandRange(0.08f, 0.18f),
			FMath::FRandRange(0.15f, 0.35f),
			FMath::FRandRange(0.08f, 0.2f)));
		Chunk->SetWorldLocation(Origin + Offset);
		Chunk->SetWorldRotation(FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f));
		Chunk->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Chunk->SetCollisionObjectType(ECC_PhysicsBody);
		Chunk->SetCollisionResponseToAllChannels(ECR_Block);
		Chunk->RegisterComponent();
		Chunk->SetSimulatePhysics(true);
		Chunk->AddImpulse(FVector(
			FMath::FRandRange(-200.f, 200.f),
			FMath::FRandRange(-200.f, 200.f),
			FMath::FRandRange(100.f, 400.f)), NAME_None, true);

		FTimerHandle Junk;
		TWeakObjectPtr<UStaticMeshComponent> WeakChunk(Chunk);
		World->GetTimerManager().SetTimer(Junk, [WeakChunk]()
		{
			if (UStaticMeshComponent* C = WeakChunk.Get())
			{
				C->DestroyComponent();
			}
		}, FMath::Max(1.f, DebrisLifeSeconds), false);
	}
}

ABreakableDoor* ABreakableDoor::SpawnInFrontOf(APawn* Pawn, float DistCm)
{
	if (!IsValid(Pawn)) return nullptr;
	UWorld* World = Pawn->GetWorld();
	if (!World) return nullptr;

	const FVector Loc = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * DistCm;
	const FRotator Rot = FRotator(0.f, Pawn->GetActorRotation().Yaw + 90.f, 0.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABreakableDoor* Door = World->SpawnActor<ABreakableDoor>(ABreakableDoor::StaticClass(), Loc, Rot, Params);
	if (IsValid(Door))
	{
		UE_LOG(LogTemp, Log, TEXT("[Door] Spawned test door at %s"), *Loc.ToString());
	}
	return Door;
}
