#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/Interactable.h"
#include "MissionObjective.generated.h"

/** Place in a mission for evidence/marked tasks; hidden tasks are optional. */
UCLASS()
class FPS_API AMissionObjective : public AActor, public IInteractable
{
 GENERATED_BODY()
public:
 AMissionObjective();
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 virtual bool CanInteract_Implementation(APawn* Pawn) const override;
 virtual FText GetInteractPrompt_Implementation(APawn* Pawn) const override;
 virtual void Interact_Implementation(APawn* Pawn) override;
 UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mission") void CompleteObjective();
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission") FText ObjectiveName;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mission") bool bHiddenObjective = false;
 UPROPERTY(Replicated, BlueprintReadOnly, Category="Mission") bool bCompleted = false;
};
