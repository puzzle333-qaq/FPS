#include "World/BloodPool.h"

#include "Game/HostileAICharacter.h"
#include "ShooterCharacter.h"
#include "Components/DecalComponent.h"
#include "Components/SphereComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

TArray<TWeakObjectPtr<ABloodPool>> ABloodPool::ActivePools;

ABloodPool::ABloodPool()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	StepTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("StepTrigger"));
	SetRootComponent(StepTrigger);
	StepTrigger->InitSphereRadius(StartRadius);
	StepTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StepTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	StepTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	StepTrigger->SetGenerateOverlapEvents(true);

	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("PuddleDecal"));
	Decal->SetupAttachment(StepTrigger);
	Decal->DecalSize = FVector(8.f, StartRadius, StartRadius);
	Decal->SetFadeScreenSize(0.001f);

	PuddleMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/FPS/FX/Blood/MI_BloodPuddle_Wet.MI_BloodPuddle_Wet")));

	InitialLifeSpan = LifeSpanSeconds;
}

void ABloodPool::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* Mat = ResolveMaterial(this))
	{
		Decal->SetDecalMaterial(Mat);
	}
	CurrentRadius = FMath::Min(StartRadius * 0.35f, StartRadius);
	if (TargetRadius <= KINDA_SMALL_NUMBER) TargetRadius = StartRadius;
	UpdateDecalSize();
	SetLifeSpan(LifeSpanSeconds);
	ActivePools.Add(this);
	PrunePools();
}

void ABloodPool::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Alpha = SpreadTime > KINDA_SMALL_NUMBER ? FMath::Clamp(DeltaSeconds / SpreadTime, 0.f, 1.f) : 1.f;
	CurrentRadius = FMath::FInterpTo(CurrentRadius, TargetRadius, DeltaSeconds, SpreadTime > 0.f ? (1.f / SpreadTime) : 8.f);
	(void)Alpha;
	UpdateDecalSize();
	if (GetWorld())
	{
		TryMarkOverlaps(GetWorld()->GetTimeSeconds());
	}
}

void ABloodPool::AddVolume(float ExtraRadius)
{
	TargetRadius = FMath::Min(MaxRadius, TargetRadius + FMath::Max(0.f, ExtraRadius));
}

void ABloodPool::UpdateDecalSize()
{
	const float R = FMath::Max(8.f, CurrentRadius);
	if (Decal)
	{
		Decal->DecalSize = FVector(R * 0.28f, R, R);
		Decal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	}
	if (StepTrigger)
	{
		StepTrigger->SetSphereRadius(R, false);
	}
}

void ABloodPool::TryMarkOverlaps(float Now)
{
	if (!StepTrigger) return;
	TArray<AActor*> Overlapping;
	StepTrigger->GetOverlappingActors(Overlapping, APawn::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		ACharacter* Char = Cast<ACharacter>(Actor);
		if (!IsValid(Char)) continue;
		const UCharacterMovementComponent* Move = Char->GetCharacterMovement();
		if (Move && Move->Velocity.SizeSquared2D() < 20.f * 20.f) continue;

		float& Next = NextMarkTime.FindOrAdd(Actor);
		if (Now < Next) continue;
		Next = Now + StepMarkCooldown;

		if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(Char))
		{
			Shooter->MarkBloodyFeet(StepsOnEnter);
		}
		else if (AHostileAICharacter* Hostile = Cast<AHostileAICharacter>(Char))
		{
			Hostile->MarkBloodyFeet(StepsOnEnter);
		}
	}
}

UMaterialInterface* ABloodPool::ResolveMaterial(const ABloodPool* CDO)
{
	if (CDO && !CDO->PuddleMaterial.IsNull())
	{
		if (UMaterialInterface* Mat = CDO->PuddleMaterial.LoadSynchronous())
		{
			return Mat;
		}
	}
	if (UMaterialInterface* Ours = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/FPS/FX/Blood/MI_BloodPuddle_Wet.MI_BloodPuddle_Wet")))
	{
		return Ours;
	}
	return LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/FPS_Controller/Environment/Materials/MI_Oil_Puddle_Decal.MI_Oil_Puddle_Decal"));
}

void ABloodPool::PrunePools()
{
	for (int32 i = ActivePools.Num() - 1; i >= 0; --i)
	{
		if (!ActivePools[i].IsValid()) ActivePools.RemoveAtSwap(i);
	}
	const int32 Cap = 24;
	while (ActivePools.Num() > Cap)
	{
		if (ABloodPool* Oldest = ActivePools[0].Get())
		{
			Oldest->Destroy();
		}
		ActivePools.RemoveAt(0);
	}
}

void ABloodPool::NotifyFloorHit(UWorld* World, const FVector& FloorLocation, const FVector& FloorNormal)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer) return;
	PrunePools();

	ABloodPool* Merge = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	const float MergeDist = 85.f;
	for (const TWeakObjectPtr<ABloodPool>& Ptr : ActivePools)
	{
		ABloodPool* Pool = Ptr.Get();
		if (!IsValid(Pool)) continue;
		const float Dist = FVector::Dist(Pool->GetActorLocation(), FloorLocation);
		if (Dist < MergeDist && Dist < BestDist)
		{
			BestDist = Dist;
			Merge = Pool;
		}
	}
	if (Merge)
	{
		Merge->AddVolume(18.f);
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABloodPool* Pool = World->SpawnActor<ABloodPool>(FloorLocation, FRotationMatrix::MakeFromZ(FloorNormal.GetSafeNormal()).Rotator(), Params);
	if (Pool)
	{
		Pool->TargetRadius = Pool->StartRadius;
	}
}
