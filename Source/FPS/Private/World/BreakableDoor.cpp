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
#include "CollisionQueryParams.h"

ABreakableDoor::ABreakableDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
	SetReplicateMovement(false);
	ActiveInterpSpeed = OpenInterpSpeed;

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
		TargetOpenDegrees = 0.f;
	}
	CurrentYaw = 0.f;
	TargetYaw = 0.f;
	ActiveInterpSpeed = OpenInterpSpeed;
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

EDoorAimZone ABreakableDoor::ResolveAimZone(APawn* InteractingPawn) const
{
	if (!IsValid(InteractingPawn) || !IsValid(DoorLeaf)) return EDoorAimZone::Handle;

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

	const FVector End = ViewLoc + ViewRot.Vector() * 350.f;
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(DoorAimZone), true, InteractingPawn);
	Q.AddIgnoredActor(InteractingPawn);
	UWorld* World = GetWorld();
	if (!World) return EDoorAimZone::Handle;

	bool bHit = World->LineTraceSingleByChannel(Hit, ViewLoc, End, ECC_Visibility, Q);
	if (!bHit || (Hit.GetActor() != this && Hit.GetComponent() != DoorLeaf && Hit.GetComponent() != InteractBox))
	{
		// Fallback: project aim ray onto door leaf plane approx using closest point on leaf bounds
		const FVector LeafLoc = DoorLeaf->GetComponentLocation();
		const FVector Closest = FMath::ClosestPointOnSegment(LeafLoc, ViewLoc, End);
		Hit.ImpactPoint = Closest;
		Hit.Component = DoorLeaf;
	}

	const FVector Local = DoorLeaf->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
	const float Z = Local.Z;
	if (Z < BottomZoneMaxZ) return EDoorAimZone::Bottom;
	if (Z > UpperZoneMinZ) return EDoorAimZone::Upper;
	return EDoorAimZone::Handle;
}

bool ABreakableDoor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	if (bDestroyed) return false;
	return IsValid(InteractingPawn);
}

FText ABreakableDoor::GetInteractPrompt_Implementation(APawn* InteractingPawn) const
{
	if (bDestroyed) return FText::GetEmpty();
	const EDoorAimZone Zone = ResolveAimZone(InteractingPawn);
	switch (Zone)
	{
	case EDoorAimZone::Upper:
		return NSLOCTEXT("Door", "Push", "按 F 轻推门");
	case EDoorAimZone::Bottom:
		return NSLOCTEXT("Door", "Kick", "按 F 踹门");
	default:
		return bOpen
			? NSLOCTEXT("Door", "Close", "按 F 关门")
			: NSLOCTEXT("Door", "Open", "按 F 开门");
	}
}

void ABreakableDoor::Interact_Implementation(APawn* InteractingPawn)
{
	if (!HasAuthority() || bDestroyed || !IsValid(InteractingPawn)) return;
	const EDoorAimZone Zone = ResolveAimZone(InteractingPawn);
	switch (Zone)
	{
	case EDoorAimZone::Upper:
		Auth_GentlePush(InteractingPawn);
		break;
	case EDoorAimZone::Bottom:
		Auth_KickOpen(InteractingPawn);
		break;
	default:
		Auth_SetOpen(!bOpen);
		break;
	}
}

void ABreakableDoor::Auth_SetTargetOpen(float DegreesAbs, float InterpSpeed)
{
	if (!HasAuthority() || bDestroyed) return;
	ActiveInterpSpeed = InterpSpeed;
	TargetOpenDegrees = DegreesAbs;
	const bool bNowOpen = FMath::Abs(DegreesAbs) > 1.f;
	bOpen = bNowOpen;
	ApplyOpenVisual(false);
	ForceNetUpdate();

	USoundBase* Snd = bOpen ? OpenSound.Get() : CloseSound.Get();
	if (IsValid(Snd))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Snd, GetActorLocation());
	}
}

void ABreakableDoor::Auth_SetOpen(bool bNewOpen)
{
	if (!HasAuthority() || bDestroyed) return;
	Auth_SetTargetOpen(bNewOpen ? OpenAngleDegrees : 0.f, OpenInterpSpeed);
}

void ABreakableDoor::Auth_GentlePush(APawn* InstigatorPawn)
{
	(void)InstigatorPawn;
	if (!HasAuthority() || bDestroyed) return;
	// If already fully open, gentle push does nothing useful; nudge ajar if closed/partial.
	const float Desired = FMath::Min(GentlePushAngleDegrees, OpenAngleDegrees);
	if (FMath::Abs(TargetOpenDegrees) >= Desired - 1.f && bOpen)
	{
		return;
	}
	Auth_SetTargetOpen(Desired, GentleInterpSpeed);
	UE_LOG(LogTemp, Log, TEXT("[Door] Gentle push -> %.0f deg"), Desired);
}

void ABreakableDoor::Auth_KickOpen(APawn* InstigatorPawn)
{
	(void)InstigatorPawn;
	if (!HasAuthority() || bDestroyed) return;
	Auth_SetTargetOpen(OpenAngleDegrees, KickInterpSpeed);
	UE_LOG(LogTemp, Log, TEXT("[Door] Kick open"));
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
	// Prefer replicated TargetOpenDegrees when available; fall back to binary open.
	const float AbsDeg = (FMath::Abs(TargetOpenDegrees) > 0.01f || bOpen)
		? (FMath::Abs(TargetOpenDegrees) > 0.01f ? FMath::Abs(TargetOpenDegrees) : OpenAngleDegrees)
		: 0.f;
	const float Signed = (AbsDeg > 0.01f ? AbsDeg : 0.f) * OpenDirection;
	// When closing via Auth_SetOpen(false), TargetOpenDegrees is 0.
	TargetYaw = (FMath::Abs(TargetOpenDegrees) < 0.01f && !bOpen) ? 0.f : Signed;
	if (!bOpen && FMath::Abs(TargetOpenDegrees) < 0.01f)
	{
		TargetYaw = 0.f;
	}
	else
	{
		TargetYaw = FMath::Abs(TargetOpenDegrees) * OpenDirection;
	}

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
