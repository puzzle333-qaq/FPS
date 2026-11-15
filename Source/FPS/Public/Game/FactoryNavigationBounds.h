#pragma once
#include "CoreMinimal.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "FactoryNavigationBounds.generated.h"
/** A navigation volume whose editable bounds do not require an editor BSP brush. */
UCLASS()
class FPS_API AFactoryNavigationBounds : public ANavMeshBoundsVolume
{
 GENERATED_BODY()
public:
 AFactoryNavigationBounds();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UBoxComponent> NavigationBounds;
 UFUNCTION(BlueprintCallable,CallInEditor,Category=Navigation) void RebuildNavigation();
};
