#include "Game/HostileSpawner.h"

#include "Game/HostileAICharacter.h"
#include "Game/HostileSquadSubsystem.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

AHostileSpawner::AHostileSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

int32 AHostileSpawner::Auth_SpawnHostiles(TSubclassOf<AHostileAICharacter> DefaultClass)
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority()) return 0;

	TSubclassOf<AHostileAICharacter> ClassToUse = OverrideClass ? OverrideClass : DefaultClass;
	if (!ClassToUse) ClassToUse = AHostileAICharacter::StaticClass();

	int32 Spawned = 0;
	const FVector Base = GetActorLocation();
	const FRotator Rot = GetActorRotation();
	const int32 SpawnCount = FMath::Max(1, Count);
	UHostileSquadSubsystem* Squad = World->GetSubsystem<UHostileSquadSubsystem>();
	for (int32 i = 0; i < SpawnCount; ++i)
	{
		const FVector Loc = Base + GetActorRightVector() * (SpacingCm * i);
		const FTransform Xform(Rot, Loc);
		AHostileAICharacter* H = World->SpawnActorDeferred<AHostileAICharacter>(
			ClassToUse, Xform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!H) continue;
		if (bMixSquadRoles && Squad)
		{
			H->SquadRole = Squad->TakeNextRole();
			H->bAutoSquadRole = true;
		}
		H->FinishSpawning(Xform);
		++Spawned;
	}
	UE_LOG(LogTemp, Log, TEXT("[HostileSpawner] %s spawned %d"), *GetNameSafe(this), Spawned);
	return Spawned;
}
