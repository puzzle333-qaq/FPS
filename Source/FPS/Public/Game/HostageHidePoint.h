#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HostageHidePoint.generated.h"

/** Root is the floor position below cover; Approach is the standing navigation destination. */
UCLASS()
class FPS_API AHostageHidePoint : public AActor
{
 GENERATED_BODY()
public:
 AHostageHidePoint();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class USceneComponent> Approach;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Hostage) bool bUnderTable=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Hostage) float SearchRadius=1800.f;
 UPROPERTY(Transient) TWeakObjectPtr<class AHostageActor> Occupant;
};
