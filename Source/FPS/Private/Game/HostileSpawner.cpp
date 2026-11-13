#include "Game/HostileSpawner.h"

#include "Game/HostileAICharacter.h"
#include "Game/HostileSquadSubsystem.h"
#include "Weapon/Weapon.h"
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
		if (!BodyMeshOverride.IsNull())
		{
			H->BodyMesh = BodyMeshOverride;
		}
		else if (LookPresets.Num() > 0)
		{
			TArray<TSoftObjectPtr<USkeletalMesh>> Pool = LookPresets;
			Pool.RemoveAll([](const TSoftObjectPtr<USkeletalMesh>& Mesh) { return Mesh.IsNull(); });
			if (Pool.Num() > 0)
			{
				H->BodyMesh = Pool[FMath::RandRange(0, Pool.Num() - 1)];
			}
		}
		if (!H->WeaponClass && WeaponClasses.Num() > 0)
		{
			TArray<TSubclassOf<AWeapon>> Guns;
			for (const TSoftClassPtr<AWeapon>& Soft : WeaponClasses)
			{
				if (UClass* GunClass = Soft.LoadSynchronous())
				{
					Guns.Add(GunClass);
				}
			}
			if (Guns.Num() > 0)
			{
				H->WeaponClass = Guns[FMath::RandRange(0, Guns.Num() - 1)];
			}
		}
		H->FinishSpawning(Xform);
		++Spawned;
	}
	UE_LOG(LogTemp, Log, TEXT("[HostileSpawner] %s spawned %d"), *GetNameSafe(this), Spawned);
	return Spawned;
}
