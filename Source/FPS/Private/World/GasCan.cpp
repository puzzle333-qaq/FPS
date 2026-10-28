#include "World/GasCan.h"
#include "World/TrapPlacement.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FPSTraceChannels.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interface/PlayerInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"

AGasCan::AGasCan()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CanMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CanMesh"));
	CanMesh->SetupAttachment(SceneRoot);
	CanMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CanMesh->SetCollisionObjectType(ECC_WorldDynamic);
	CanMesh->SetCollisionResponseToAllChannels(ECR_Block);
	CanMesh->SetCollisionResponseToChannel(FPSTraceChannels::ECC_Weapon, ECR_Block);
	CanMesh->SetCanEverAffectNavigation(false);
	CanMesh->SetNotifyRigidBodyCollision(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylFinder.Succeeded())
	{
		CanMesh->SetStaticMesh(CylFinder.Object);
	}
	CanMesh->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.55f));
	CanMesh->SetRelativeLocation(FVector(0.f, 0.f, 55.f));
}

void AGasCan::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Health = MaxHealth;
		bExploded = false;
	}
	ApplyVisualState();
}

void AGasCan::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGasCan, Health);
	DOREPLIFETIME(AGasCan, bExploded);
	DOREPLIFETIME(AGasCan, PlacerController);
}

void AGasCan::Auth_SetPlacer(AController* Placer)
{
	if (!HasAuthority()) return;
	PlacerController = Placer;
}

int32 AGasCan::CountPlacedBy(UWorld* World, AController* Placer)
{
	if (!World || !Placer) return 0;
	int32 Count = 0;
	for (TActorIterator<AGasCan> It(World); It; ++It)
	{
		AGasCan* Can = *It;
		if (!IsValid(Can) || Can->bExploded) continue;
		if (Can->GetPlacer() == Placer)
		{
			++Count;
		}
	}
	return Count;
}

AGasCan* AGasCan::Auth_TryPlace(APawn* Pawn, bool bIgnoreSetup, FString* OutError)
{
	auto Fail = [&](const FString& Msg) -> AGasCan*
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
	const int32 MaxAllowed = GetDefault<AGasCan>()->MaxPerPlayer;
	if (Placer && CountPlacedBy(World, Placer) >= MaxAllowed)
	{
		return Fail(FString::Printf(TEXT("汽油桶已达上限 %d"), MaxAllowed));
	}

	const FVector Loc = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 140.f;
	const FRotator Rot(0.f, Pawn->GetActorRotation().Yaw, 0.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Instigator = Pawn;
	Params.Owner = Placer ? Placer : static_cast<AActor*>(Pawn);

	AGasCan* Can = World->SpawnActor<AGasCan>(AGasCan::StaticClass(), Loc, Rot, Params);
	if (!IsValid(Can))
	{
		return Fail(TEXT("生成汽油桶失败"));
	}
	Can->Auth_SetPlacer(Placer);
	UE_LOG(LogTemp, Log, TEXT("[Trap] GasCan placed by %s at %s"), *GetNameSafe(Placer), *Loc.ToString());
	return Can;
}

bool AGasCan::DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator)
{
	if (!HasAuthority() || bExploded || DamageAmout <= 0.f) return false;
	Health = FMath::Max(0.f, Health - DamageAmout);
	UE_LOG(LogTemp, Log, TEXT("[Trap] GasCan took %.1f dmg HP=%.1f"), DamageAmout, Health);
	if (Health <= 0.f)
	{
		Auth_Explode(DamageInstigator);
		return true;
	}
	ForceNetUpdate();
	return false;
}

void AGasCan::Auth_Explode(AActor* DamageInstigator)
{
	if (!HasAuthority() || bExploded) return;
	bExploded = true;
	Health = 0.f;
	ApplyVisualState();
	ForceNetUpdate();

	if (IsValid(ExplosionSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, GetActorLocation());
	}

	ApplyExplosionDamage(DamageInstigator);

	if (FireDuration > 0.f && FireTickDamage > 0.f)
	{
		FireTimeLeft = FireDuration;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(FireTimer, this, &AGasCan::Auth_FireTick, FireTickInterval, true);
		}
	}
	else
	{
		SetLifeSpan(2.f);
	}
	UE_LOG(LogTemp, Log, TEXT("[Trap] GasCan exploded"));
}

void AGasCan::ApplyExplosionDamage(AActor* DamageInstigator)
{
	UWorld* World = GetWorld();
	if (!World || ExplosionDamage <= 0.f || DamageRadius <= 0.f) return;

	APawn* PlacerPawn = IsValid(PlacerController) ? PlacerController->GetPawn() : nullptr;
	AActor* InstigatorActor = IsValid(DamageInstigator) ? DamageInstigator
		: (IsValid(PlacerPawn) ? static_cast<AActor*>(PlacerPawn) : this);

	const FVector Origin = GetActorLocation();
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GasCanBoom), false, this);
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);

	World->OverlapMultiByObjectType(
		Overlaps, Origin, FQuat::Identity, ObjectQuery,
		FCollisionShape::MakeSphere(DamageRadius), QueryParams);

	TSet<AActor*> Damaged;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!IsValid(Actor) || Damaged.Contains(Actor)) continue;
		if (!Actor->Implements<UPlayerInterface>()) continue;
		Damaged.Add(Actor);

		const float Dist = FVector::Dist(Origin, Actor->GetActorLocation());
		if (Dist > DamageRadius) continue;
		float Scale = 1.f;
		if (Dist > InnerRadius)
		{
			const float Range = FMath::Max(1.f, DamageRadius - InnerRadius);
			Scale = 1.f - (Dist - InnerRadius) / Range;
		}
		Scale = FMath::Clamp(Scale, 0.f, 1.f);
		const float Applied = ExplosionDamage * Scale;
		if (Applied < 1.f) continue;

		// Direct DoDamage — explosives ignore body armor.
		IPlayerInterface::Execute_DoDamage(Actor, Applied, InstigatorActor);
	}
}

void AGasCan::Auth_FireTick()
{
	if (!HasAuthority() || !bExploded) return;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FireTimeLeft -= FireTickInterval;
	APawn* PlacerPawn = IsValid(PlacerController) ? PlacerController->GetPawn() : nullptr;
	AActor* InstigatorActor = IsValid(PlacerPawn) ? static_cast<AActor*>(PlacerPawn) : this;

	const FVector Origin = GetActorLocation();
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GasCanFire), false, this);
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	World->OverlapMultiByObjectType(
		Overlaps, Origin, FQuat::Identity, ObjectQuery,
		FCollisionShape::MakeSphere(FireRadius), QueryParams);

	TSet<AActor*> Hit;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!IsValid(Actor) || Hit.Contains(Actor)) continue;
		if (!Actor->Implements<UPlayerInterface>()) continue;
		Hit.Add(Actor);
		IPlayerInterface::Execute_DoDamage(Actor, FireTickDamage, InstigatorActor);
	}

	if (FireTimeLeft <= 0.f)
	{
		World->GetTimerManager().ClearTimer(FireTimer);
		Destroy();
	}
}

void AGasCan::OnRep_Exploded()
{
	ApplyVisualState();
}

void AGasCan::ApplyVisualState()
{
	if (!IsValid(CanMesh)) return;
	if (bExploded)
	{
		CanMesh->SetVisibility(false, true);
		CanMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else
	{
		CanMesh->SetVisibility(true, true);
		CanMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}
