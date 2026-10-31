#include "Game/HostileSpawner.h"

#include "Game/HostileAICharacter.h"
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
	for (int32 i = 0; i < FMath::Max(1, Count); ++i)
	{
		const FVector Loc = Base + GetActorRightVector() * (SpacingCm * i);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (AHostileAICharacter* H = World->SpawnActor<AHostileAICharacter>(ClassToUse, Loc, Rot, Params))
		{
			++Spawned;
			(void)H;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[HostileSpawner] %s spawned %d"), *GetNameSafe(this), Spawned);
	return Spawned;
}
