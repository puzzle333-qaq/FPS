#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NightVisionLightFloor.generated.h"
/** Persistent night signal, independent of mains power. Also separates world IR receivers from viewmodels. */
UCLASS()
class FPS_API ANightVisionLightFloor : public AActor
{
 GENERATED_BODY()
public:
 ANightVisionLightFloor();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class USceneComponent> SceneRoot;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UDirectionalLightComponent> Moonlight;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class USkyLightComponent> AmbientFloor;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<class UPointLightComponent>> InteriorSignal;
 static void PrepareInfraredReceiver(AActor* Actor);
protected:
 virtual void BeginPlay()override;
 virtual void EndPlay(const EEndPlayReason::Type Reason)override;
private:
 FDelegateHandle SpawnHandle;
 void HandleSpawned(AActor* Actor);
};
