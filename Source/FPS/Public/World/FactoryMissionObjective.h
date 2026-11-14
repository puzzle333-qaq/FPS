#pragma once
#include "CoreMinimal.h"
#include "World/MissionObjective.h"
#include "FactoryMissionObjective.generated.h"

UENUM(BlueprintType)
enum class EFactoryTask : uint8 { Ledger, Shipping, Crates, Breaker, Extraction };

UCLASS()
class FPS_API AFactoryMissionObjective : public AMissionObjective
{
 GENERATED_BODY()
public:
 AFactoryMissionObjective();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual bool CanInteract_Implementation(APawn* Pawn) const override;
 virtual FText GetInteractPrompt_Implementation(APawn* Pawn) const override;
 virtual void OnRep_Completed() override;
 virtual void CompleteObjective() override;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Factory") EFactoryTask Task=EFactoryTask::Ledger;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Factory") TObjectPtr<class UBoxComponent> InteractionBox;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Factory") float ExtractionRadius=300.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Factory") FName PowerGroupTag=TEXT("Factory.SwitchablePower");
 UFUNCTION(BlueprintPure) bool IsLedgerCollected() const;
 UFUNCTION(BlueprintPure) int32 GetOperatorsAtExtraction(int32& LivingOperators) const;
};
