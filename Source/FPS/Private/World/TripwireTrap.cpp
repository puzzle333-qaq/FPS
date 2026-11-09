#include "World/TripwireTrap.h"
#include "World/TrapPlacement.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interface/PlayerInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "ShooterGameModeBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"

ATripwireTrap::ATripwireTrap()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));

	PostLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PostLeft"));
	PostLeft->SetupAttachment(SceneRoot);
	PostLeft->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PostLeft->SetCollisionObjectType(ECC_WorldDynamic);
	PostLeft->SetCollisionResponseToAllChannels(ECR_Block);
	PostLeft->SetCanEverAffectNavigation(false);
	if (CubeFinder.Succeeded())
	{
		PostLeft->SetStaticMesh(CubeFinder.Object);
	}
	PostLeft->SetWorldScale3D(FVector(0.08f, 0.08f, 0.9f));

	PostRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PostRight"));
	PostRight->SetupAttachment(SceneRoot);
	PostRight->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PostRight->SetCollisionObjectType(ECC_WorldDynamic);
	PostRight->SetCollisionResponseToAllChannels(ECR_Block);
	PostRight->SetCanEverAffectNavigation(false);
	if (CubeFinder.Succeeded())
	{
		PostRight->SetStaticMesh(CubeFinder.Object);
	}
	PostRight->SetWorldScale3D(FVector(0.08f, 0.08f, 0.9f));

	WireMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WireMesh"));
	WireMesh->SetupAttachment(SceneRoot);
	WireMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WireMesh->SetCanEverAffectNavigation(false);
	if (CubeFinder.Succeeded())
	{
		WireMesh->SetStaticMesh(CubeFinder.Object);
	}
	WireMesh->SetWorldScale3D(FVector(0.02f, 2.4f, 0.02f));

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(SceneRoot);
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetCanEverAffectNavigation(false);
	TriggerBox->InitBoxExtent(FVector(20.f, HalfSpan, 40.f));
}

void ATripwireTrap::BeginPlay()
{
	Super::BeginPlay();
	LayoutGeometry();
	ApplyVisualState();
	if (IsValid(TriggerBox))
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATripwireTrap::OnTriggerBeginOverlap);
	}
}

void ATripwireTrap::LayoutGeometry()
{
	const float PostZ = WireHeight * 0.5f;
	if (IsValid(PostLeft))
	{
		PostLeft->SetRelativeLocation(FVector(0.f, -HalfSpan, PostZ));
	}
	if (IsValid(PostRight))
	{
		PostRight->SetRelativeLocation(FVector(0.f, HalfSpan, PostZ));
	}
	if (IsValid(WireMesh))
	{
		WireMesh->SetRelativeLocation(FVector(0.f, 0.f, WireHeight));
		const float SpanUnits = (HalfSpan * 2.f) / 100.f; // cube default 100cm
		WireMesh->SetRelativeScale3D(FVector(0.02f, FMath::Max(0.2f, SpanUnits), 0.02f));
	}
	if (IsValid(TriggerBox))
	{
		TriggerBox->SetRelativeLocation(FVector(0.f, 0.f, WireHeight));
		TriggerBox->SetBoxExtent(FVector(25.f, HalfSpan + 10.f, 35.f));
	}
}

void ATripwireTrap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATripwireTrap, bArmed);
	DOREPLIFETIME(ATripwireTrap, bTriggered);
	DOREPLIFETIME(ATripwireTrap, bDisarmed);
	DOREPLIFETIME(ATripwireTrap, PlacerController);
}

void ATripwireTrap::Auth_SetPlacer(AController* Placer)
{
	if (!HasAuthority()) return;
	PlacerController = Placer;
}

int32 ATripwireTrap::CountPlacedBy(UWorld* World, AController* Placer)
{
	if (!World || !Placer) return 0;
	int32 Count = 0;
	for (TActorIterator<ATripwireTrap> It(World); It; ++It)
	{
		ATripwireTrap* Trap = *It;
		if (!IsValid(Trap) || Trap->bTriggered || Trap->bDisarmed) continue;
		if (Trap->GetPlacer() == Placer)
		{
			++Count;
		}
	}
	return Count;
}

ATripwireTrap* ATripwireTrap::Auth_TryPlace(APawn* Pawn, bool bIgnoreSetup, FString* OutError)
{
	auto Fail = [&](const FString& Msg) -> ATripwireTrap*
	{
		if (OutError) *OutError = Msg;
		return nullptr;
	};

	if (!FPSTrapPlacement::CanPlace(Pawn, bIgnoreSetup, OutError))
	{
		return nullptr;
	}

	UWorld* World = Pawn->GetWorld();
	AController* Placer = FPSTrapPlacement::GetController(Pawn);
	const int32 MaxAllowed = GetDefault<ATripwireTrap>()->MaxPerPlayer;
	if (Placer && CountPlacedBy(World, Placer) >= MaxAllowed)
	{
		return Fail(FString::Printf(TEXT("绊雷已达上限 %d"), MaxAllowed));
	}

	const FVector Loc = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 160.f;
	const FRotator Rot(0.f, Pawn->GetActorRotation().Yaw, 0.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Instigator = Pawn;
	Params.Owner = Placer ? Placer : static_cast<AActor*>(Pawn);

	ATripwireTrap* Trap = World->SpawnActor<ATripwireTrap>(ATripwireTrap::StaticClass(), Loc, Rot, Params);
	if (!IsValid(Trap))
	{
		return Fail(TEXT("生成绊雷失败"));
	}
	Trap->Auth_SetPlacer(Placer);
	Trap->LayoutGeometry();
	UE_LOG(LogTemp, Log, TEXT("[Trap] Tripwire placed by %s at %s"), *GetNameSafe(Placer), *Loc.ToString());
	return Trap;
}

bool ATripwireTrap::IsPolicePawn(APawn* Pawn) const
{
	return IsValid(Pawn);
}

bool ATripwireTrap::CanInteract_Implementation(APawn* InteractingPawn) const
{
	if (!IsArmed()) return false;
	return IsPolicePawn(InteractingPawn);
}

FText ATripwireTrap::GetInteractPrompt_Implementation(APawn* InteractingPawn) const
{
	(void)InteractingPawn;
	if (!IsArmed()) return FText::GetEmpty();
	return NSLOCTEXT("Trap", "DisarmTripwire", "按 F 拆除绊雷");
}

void ATripwireTrap::Interact_Implementation(APawn* InteractingPawn)
{
	if (!HasAuthority() || !IsArmed() || !IsValid(InteractingPawn)) return;
	if (!IsPolicePawn(InteractingPawn)) return;
	Auth_Disarm(InteractingPawn);
}

void ATripwireTrap::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	(void)OverlappedComp; (void)OtherComp; (void)OtherBodyIndex; (void)bFromSweep; (void)SweepResult;
	if (!HasAuthority() || !IsArmed()) return;
	APawn* OtherPawn = Cast<APawn>(OtherActor);
	if (!IsValid(OtherPawn)) return;
	// Don't trigger on placer's own pawn during setup (still placing nearby).
	if (IsValid(PlacerController) && OtherPawn->GetController() == PlacerController.Get())
	{
		return;
	}
	Auth_Trigger(OtherActor);
}

void ATripwireTrap::Auth_Trigger(AActor* Victim)
{
	if (!HasAuthority() || !IsArmed()) return;
	bTriggered = true;
	bArmed = false;
	ApplyVisualState();
	ForceNetUpdate();

	if (IsValid(TriggerSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, TriggerSound, GetActorLocation());
	}
	ApplyExplosionDamage(Victim);
	UE_LOG(LogTemp, Log, TEXT("[Trap] Tripwire triggered by %s"), *GetNameSafe(Victim));
}

void ATripwireTrap::Auth_Disarm(APawn* InteractingPawn)
{
	if (!HasAuthority() || !IsArmed()) return;
	bDisarmed = true;
	bArmed = false;
	ApplyVisualState();
	ForceNetUpdate();
	if (IsValid(DisarmSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, DisarmSound, GetActorLocation());
	}
	UE_LOG(LogTemp, Log, TEXT("[Trap] Tripwire disarmed by %s"), *GetNameSafe(InteractingPawn));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("绊雷已拆除"));
	}
}

void ATripwireTrap::ApplyExplosionDamage(AActor* PreferredVictim)
{
	UWorld* World = GetWorld();
	if (!World || ExplosionDamage <= 0.f || DamageRadius <= 0.f) return;

	APawn* InstigatorPawn = nullptr;
	if (IsValid(PlacerController))
	{
		InstigatorPawn = PlacerController->GetPawn();
	}
	AActor* DamageInstigator = IsValid(InstigatorPawn) ? static_cast<AActor*>(InstigatorPawn) : this;

	const FVector Origin = GetActorLocation() + FVector(0.f, 0.f, WireHeight);
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TripwireBoom), false, this);
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);

	World->OverlapMultiByObjectType(
		Overlaps, Origin, FQuat::Identity, ObjectQuery,
		FCollisionShape::MakeSphere(DamageRadius), QueryParams);

	TSet<AActor*> Damaged;
	auto ApplyTo = [&](AActor* Actor)
	{
		if (!IsValid(Actor) || Damaged.Contains(Actor)) return;
		if (!Actor->Implements<UPlayerInterface>()) return;
		Damaged.Add(Actor);

		const float Dist = FVector::Dist(Origin, Actor->GetActorLocation());
		if (Dist > DamageRadius) return;
		float Scale = 1.f;
		if (Dist > InnerRadius)
		{
			const float Range = FMath::Max(1.f, DamageRadius - InnerRadius);
			Scale = 1.f - (Dist - InnerRadius) / Range;
		}
		Scale = FMath::Clamp(Scale, 0.f, 1.f);
		const float Applied = ExplosionDamage * Scale;
		if (Applied < 1.f) return;

		// Direct DoDamage — bypasses ScaleDamageByArmor (ballistic-only).
		IPlayerInterface::Execute_DoDamage(Actor, Applied, DamageInstigator);
	};

	if (IsValid(PreferredVictim))
	{
		ApplyTo(PreferredVictim);
	}
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ApplyTo(Overlap.GetActor());
	}
}

void ATripwireTrap::OnRep_State()
{
	ApplyVisualState();
}

void ATripwireTrap::ApplyVisualState()
{
	const bool bShowWire = IsArmed();
	if (IsValid(WireMesh))
	{
		WireMesh->SetVisibility(bShowWire, true);
	}
	if (IsValid(TriggerBox))
	{
		TriggerBox->SetCollisionEnabled(bShowWire ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
	if (bTriggered || bDisarmed)
	{
		if (IsValid(PostLeft)) PostLeft->SetVisibility(true);
		if (IsValid(PostRight)) PostRight->SetVisibility(true);
	}
}
