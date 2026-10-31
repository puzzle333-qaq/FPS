#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HostileSpawner.generated.h"

class AHostileAICharacter;

/** Place in graybox. Coop GameMode calls Auth_SpawnHostiles at mission start. */
UCLASS()
class FPS_API AHostileSpawner : public AActor
{
	GENERATED_BODY()

public:
	AHostileSpawner();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Hostile")
	TObjectPtr<USceneComponent> SceneRoot;

	/** How many hostiles to spawn at this point (offsets applied). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile", meta = (ClampMin = "1"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile")
	float SpacingCm = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Hostile")
	TSubclassOf<AHostileAICharacter> OverrideClass;

	/** Server: spawn hostiles. Returns number spawned. */
	int32 Auth_SpawnHostiles(TSubclassOf<AHostileAICharacter> DefaultClass);
};
