#include "World/BreakableDoor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "FPSTraceChannels.h"
#include "TimerManager.h"
#include "Game/AINoiseEvent.h"

ABreakableDoor::ABreakableDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
	SetReplicateMovement(false);
	ActiveInterpSpeed = PushInterpSpeed;

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
		DoorLeafMesh = DoorMeshFinder.Object;
		DoorLeaf->SetStaticMesh(DoorLeafMesh);
		DoorLeaf->SetRelativeLocation(DoorLeafRelativeLocation);
	}

	InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
	InteractBox->SetupAttachment(DoorLeaf);
	InteractBox->InitBoxExtent(InteractBoxExtent);
	InteractBox->SetRelativeLocation(InteractBoxRelativeLocation);
	InteractBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractBox->SetCollisionResponseToChannel(FPSTraceChannels::ECC_Weapon, ECR_Block);
	InteractBox->SetCanEverAffectNavigation(false);
}

void ABreakableDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyDoorMeshAndBox();
}

void ABreakableDoor::ApplyDoorMeshAndBox()
{
	if (!IsValid(DoorLeaf)) return;

	if (IsValid(DoorLeafMesh))
	{
		if (DoorLeaf->GetStaticMesh() != DoorLeafMesh)
		{
			DoorLeaf->SetStaticMesh(DoorLeafMesh);
		}
	}

	DoorLeaf->SetRelativeLocation(DoorLeafRelativeLocation);
	DoorLeaf->SetRelativeRotation(DoorLeafRelativeRotation);
	DoorLeaf->SetRelativeScale3D(DoorLeafRelativeScale);

	if (!IsValid(InteractBox)) return;

	if (bAutoFitInteractBox && IsValid(DoorLeaf->GetStaticMesh()))
	{
		const FBoxSphereBounds B = DoorLeaf->GetStaticMesh()->GetBounds();
		const FVector Ext = B.BoxExtent * DoorLeafRelativeScale.GetAbs();
		// Thin depth so traces hit the leaf face; keep at least a few cm.
		const FVector Fitted(
			FMath::Max(4.f, Ext.X),
			FMath::Max(10.f, Ext.Y),
			FMath::Max(40.f, Ext.Z));
		InteractBoxExtent = Fitted;
		InteractBoxRelativeLocation = B.Origin * DoorLeafRelativeScale;
	}

	InteractBox->SetBoxExtent(InteractBoxExtent);
	InteractBox->SetRelativeLocation(InteractBoxRelativeLocation);
}

void ABreakableDoor::BeginPlay()
{
	Super::BeginPlay();
	ApplyDoorMeshAndBox();
	if (HasAuthority())
	{
		Health = MaxHealth;
		bDestroyed = false;
		bOpen = false;
		TargetOpenDegrees = 0.f;
	}
	CurrentYaw = 0.f;
	TargetYaw = 0.f;
	ActiveInterpSpeed = PushInterpSpeed;
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
	CurrentYaw = FMath::FInterpTo(CurrentYaw, TargetYaw, DeltaSeconds, ActiveInterpSpeed);
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
	DOREPLIFETIME(ABreakableDoor, TargetOpenDegrees);
}

EDoorOpenMode ABreakableDoor::ResolveOpenMode(APawn* InteractingPawn) const
{
	if (!IsValid(InteractingPawn)) return EDoorOpenMode::Push;

	FVector ViewLoc;
	FRotator ViewRot;
	if (APlayerController* PC = Cast<APlayerController>(InteractingPawn->GetController()))
	{
		PC->GetPlayerViewPoint(ViewLoc, ViewRot);
	}
	else
	{
		InteractingPawn->GetActorEyesViewPoint(ViewLoc, ViewRot);
	}

	const float Pitch = ViewRot.Pitch;
	if (Pitch <= -LookDownPitchThreshold)
	{
		return EDoorOpenMode::Kick;
	}
	// Look up OR level → push (quiet default for teaching / CQB).
	return EDoorOpenMode::Push;
}

float ABreakableDoor::ResolveOpenDirectionAwayFrom(APawn* InteractingPawn) const
{
	if (!bAutoOpenAwayFromPlayer || !IsValid(InteractingPawn))
	{
		return FMath::Sign(OpenDirection) >= 0.f ? 1.f : -1.f;
	}

	// Actor forward = door face normal when closed. Player on that side → swing the other way.
	const FVector DoorOrigin = IsValid(Hinge) ? Hinge->GetComponentLocation() : GetActorLocation();
	const FVector ToPlayer = InteractingPawn->GetActorLocation() - DoorOrigin;
	const FVector Flat = FVector(ToPlayer.X, ToPlayer.Y, 0.f).GetSafeNormal();
	const FVector Face = FVector(GetActorForwardVector().X, GetActorForwardVector().Y, 0.f).GetSafeNormal();
	const float Side = FVector::DotProduct(Face, Flat);
	// Player in front of face → open negative yaw (away). Behind → positive.
	return (Side >= 0.f) ? -1.f : 1.f;
}

bool ABreakableDoor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	if (bDestroyed) return false;
	return IsValid(InteractingPawn);
}

FText ABreakableDoor::GetInteractPrompt_Implementation(APawn* InteractingPawn) const
{
	if (bDestroyed) return FText::GetEmpty();
	if (bOpen)
	{
		return NSLOCTEXT("Door", "Close", "F Close door");
	}
	const EDoorOpenMode Mode = ResolveOpenMode(InteractingPawn);
	if (Mode == EDoorOpenMode::Kick)
	{
		return NSLOCTEXT("Door", "Kick", "F Kick door");
	}
	return NSLOCTEXT("Door", "Push", "F Push door");
}

void ABreakableDoor::Interact_Implementation(APawn* InteractingPawn)
{
	if (!HasAuthority() || bDestroyed || !IsValid(InteractingPawn)) return;

	if (bOpen)
	{
		Auth_Close(InteractingPawn);
		return;
	}

	const EDoorOpenMode Mode = ResolveOpenMode(InteractingPawn);
	if (Mode == EDoorOpenMode::Kick)
	{
		Auth_KickOpen(InteractingPawn);
	}
	else
	{
		Auth_PushOpen(InteractingPawn);
	}
}

void ABreakableDoor::Auth_SetTargetOpenSigned(float SignedDegrees, float InterpSpeed, USoundBase* Sound, float VolumeMultiplier, float NoiseLoudness, AActor* NoiseInstigator, bool bKickNoise)
{
	if (!HasAuthority() || bDestroyed) return;
	ActiveInterpSpeed = InterpSpeed;
	TargetOpenDegrees = SignedDegrees;
	bOpen = FMath::Abs(SignedDegrees) > 1.f;
	ApplyOpenVisual(false);
	ForceNetUpdate();

	if (IsValid(Sound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), VolumeMultiplier);
	}

	if (NoiseLoudness > 0.01f)
	{
		AINoiseEvent::Report(
			GetWorld(),
			GetActorLocation(),
			NoiseLoudness,
			NoiseInstigator,
			bKickNoise ? AINoiseEvent::EKind::Gunshot : AINoiseEvent::EKind::Footstep);
	}
}

void ABreakableDoor::Auth_SetOpen(bool bNewOpen)
{
	if (!HasAuthority() || bDestroyed) return;
	if (bNewOpen)
	{
		const float Dir = FMath::Sign(OpenDirection) >= 0.f ? 1.f : -1.f;
		Auth_SetTargetOpenSigned(OpenAngleDegrees * Dir, PushInterpSpeed, PushSound.Get(), PushSoundVolume, PushNoiseLoudness, this, false);
	}
	else
	{
		Auth_Close(nullptr);
	}
}

void ABreakableDoor::Auth_PushOpen(APawn* InstigatorPawn)
{
	if (!HasAuthority() || bDestroyed) return;
	const float Dir = ResolveOpenDirectionAwayFrom(InstigatorPawn);
	Auth_SetTargetOpenSigned(OpenAngleDegrees * Dir, PushInterpSpeed, PushSound.Get(), PushSoundVolume, PushNoiseLoudness, InstigatorPawn, false);
	UE_LOG(LogTemp, Log, TEXT("[Door] Push open dir=%.0f"), Dir);
}

void ABreakableDoor::Auth_KickOpen(APawn* InstigatorPawn)
{
	if (!HasAuthority() || bDestroyed) return;
	const float Dir = ResolveOpenDirectionAwayFrom(InstigatorPawn);
	Auth_SetTargetOpenSigned(OpenAngleDegrees * Dir, KickInterpSpeed, KickSound.Get(), KickSoundVolume, KickNoiseLoudness, InstigatorPawn, true);
	UE_LOG(LogTemp, Log, TEXT("[Door] Kick open dir=%.0f"), Dir);
}

void ABreakableDoor::Auth_Close(APawn* InstigatorPawn)
{
	(void)InstigatorPawn;
	if (!HasAuthority() || bDestroyed) return;
	Auth_SetTargetOpenSigned(0.f, CloseInterpSpeed, CloseSound.Get(), PushSoundVolume, 0.f, InstigatorPawn, false);
	UE_LOG(LogTemp, Log, TEXT("[Door] Close"));
}

void ABreakableDoor::OnRep_Open()
{
	ApplyOpenVisual(false);
}

void ABreakableDoor::OnRep_TargetOpen()
{
	ApplyOpenVisual(false);
}

void ABreakableDoor::OnRep_Destroyed()
{
	ApplyDestroyedVisual(true);
}

void ABreakableDoor::ApplyOpenVisual(bool bSnap)
{
	TargetYaw = TargetOpenDegrees;
	ActiveInterpSpeed = FMath::Max(ActiveInterpSpeed, 0.5f);

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
	TargetOpenDegrees = 0.f;
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
