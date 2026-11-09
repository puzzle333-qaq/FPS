#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "Interface/Interactable.h"
#include "MissionDeployPoint.generated.h"

USTRUCT(BlueprintType)
struct FPS_API FMissionDestination
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(MultiLine=true)) FText Briefing;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UWorld> Map;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftClassPtr<AGameModeBase> GameMode;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDeploymentUpdated);

class UBoxComponent;
class UStaticMeshComponent;

/** Range mission selection and server-owned squad countdown. */
UCLASS()
class FPS_API AMissionDeployPoint : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AMissionDeployPoint();
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 UPROPERTY(EditAnywhere, Replicated, Category="FPS|Deploy") TArray<FMissionDestination> Missions;
 UPROPERTY(EditAnywhere, Category="FPS|Deploy", meta=(ClampMin="1")) float CountdownDuration = 5.f;
 UPROPERTY(ReplicatedUsing=OnRep_Deployment, BlueprintReadOnly, Category="FPS|Deploy") float CountdownEndServerTime = -1.f;
 UPROPERTY(ReplicatedUsing=OnRep_Deployment, BlueprintReadOnly, Category="FPS|Deploy") int32 SelectedMission = INDEX_NONE;
 UPROPERTY(ReplicatedUsing=OnRep_Deployment, BlueprintReadOnly, Category="FPS|Deploy") FString DeploymentError;
 UPROPERTY(BlueprintAssignable) FDeploymentUpdated OnDeploymentUpdated;
 UFUNCTION() void OnRep_Deployment();
 FMissionDestination GetDestination(int32 Index) const;
 int32 GetDestinationCount() const;
 bool Auth_SelectMission(APlayerController* PC, int32 Index);
 void Auth_Cancel(APlayerController* PC);
 UFUNCTION(NetMulticast, Reliable) void Multicast_ShowCountdown();
private:
 void DeploySquad();
 FTimerHandle DeployTimer;
 TWeakObjectPtr<APlayerController> DeploymentLeader;
public:

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	UPROPERTY(VisibleAnywhere, Category = "FPS|Deploy")
	TObjectPtr<UBoxComponent> InteractBox;

	UPROPERTY(VisibleAnywhere, Category = "FPS|Deploy")
	TObjectPtr<UStaticMeshComponent> TableMesh;

	UPROPERTY(EditAnywhere, Category = "FPS|Deploy")
	FText Prompt;

	UPROPERTY(EditAnywhere, Category = "FPS|Deploy")
	TSoftObjectPtr<UWorld> MissionMap;

	UPROPERTY(EditAnywhere, Category = "FPS|Deploy")
	TSoftClassPtr<AGameModeBase> MissionGameMode;
};
