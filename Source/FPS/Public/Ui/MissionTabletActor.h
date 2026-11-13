#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MissionTabletActor.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UAnimSequence;
class UPrimitiveComponent;
class AShooterPlayerController;

/** Local-only first-person device. Mission state continues to come from MissionFlow. */
UCLASS()
class FPS_API AMissionTabletActor : public AActor
{
 GENERATED_BODY()
public:
 AMissionTabletActor();
 bool OpenFor(AShooterPlayerController* Controller);
 void Close(bool bAnimate=true);
 void RefreshMission();
 void ScrollBriefing(float Amount);
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Tablet") TObjectPtr<USkeletalMeshComponent> Hands;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Tablet") TObjectPtr<UStaticMeshComponent> TabletMesh;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Tablet") TObjectPtr<UWidgetComponent> Screen;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TObjectPtr<UAnimSequence> RaiseAnimation;
 UPROPERTY() TObjectPtr<UAnimSequence> HideAnimation;
 UPROPERTY() TObjectPtr<UAnimSequence> IdleAnimation;
 TArray<TPair<TWeakObjectPtr<UPrimitiveComponent>,bool>> HiddenMeshes;
 FTimerHandle AnimationTimer;
 bool bClosing=false;
 void PlayIdle();
 void RestoreEquipment();
};
