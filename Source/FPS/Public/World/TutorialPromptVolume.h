#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TutorialPromptVolume.generated.h"

class UBoxComponent;

/**
 * Local-only tutorial hint trigger. Overlap on the owning client shows the
 * text once per player (tracked on the local player controller).
 */
UCLASS()
class FPS_API ATutorialPromptVolume : public AActor
{
	GENERATED_BODY()

public:
	ATutorialPromptVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Tutorial")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Tutorial")
	FText PromptText;

	/** If set, this hint is shown at most once per local player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Tutorial")
	FName PromptId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Tutorial", meta = (ClampMin = "1"))
	float DisplayDuration = 6.f;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
