#pragma once
#include "CoreMinimal.h"
#include "MissionTypes.generated.h"

USTRUCT(BlueprintType)
struct FPS_API FMissionScoreRules
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float SThreshold = .95f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float AThreshold = .85f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float BThreshold = .70f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float CThreshold = .50f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Primary = 1000;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Marked = 250;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Neutralized = 100;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Arrested = 300;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Hostage = 500;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Survivor = 200;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Hidden = 250;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 HostageLostPenalty = 500;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SurrenderedKilledPenalty = 250;
};

/** A single replicated snapshot keeps the debrief and tablet consistent. */
USTRUCT(BlueprintType)
struct FPS_API FMissionReport
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) int32 SuspectsTotal = 0;
 UPROPERTY(BlueprintReadOnly) int32 Neutralized = 0;
 UPROPERTY(BlueprintReadOnly) int32 Arrested = 0;
 UPROPERTY(BlueprintReadOnly) int32 HostagesTotal = 0;
 UPROPERTY(BlueprintReadOnly) int32 Rescued = 0;
 UPROPERTY(BlueprintReadOnly) int32 HostagesLost = 0;
 UPROPERTY(BlueprintReadOnly) int32 OperatorsTotal = 0;
 UPROPERTY(BlueprintReadOnly) int32 Survivors = 0;
 UPROPERTY(BlueprintReadOnly) int32 MarkedTotal = 0;
 UPROPERTY(BlueprintReadOnly) int32 MarkedCompleted = 0;
 UPROPERTY(BlueprintReadOnly) int32 HiddenCompleted = 0;
 UPROPERTY(BlueprintReadOnly) int32 SurrenderedKilled = 0;
 UPROPERTY(BlueprintReadOnly) bool bPrimaryComplete = false;
 UPROPERTY(BlueprintReadOnly) int32 PrimaryPoints = 0;
 UPROPERTY(BlueprintReadOnly) int32 MarkedPoints = 0;
 UPROPERTY(BlueprintReadOnly) int32 NeutralizedPoints = 0;
 UPROPERTY(BlueprintReadOnly) int32 ArrestedPoints = 0;
 UPROPERTY(BlueprintReadOnly) int32 HostagePoints = 0;
 UPROPERTY(BlueprintReadOnly) int32 SurvivalPoints = 0;
 UPROPERTY(BlueprintReadOnly) int32 HiddenPoints = 0;
 UPROPERTY(BlueprintReadOnly) int32 Penalties = 0;
 UPROPERTY(BlueprintReadOnly) int32 Score = 0;
 UPROPERTY(BlueprintReadOnly) int32 MaximumScore = 0;
 UPROPERTY(BlueprintReadOnly) FString Grade = TEXT("—");
 void Calculate(const FMissionScoreRules& Rules, bool bWon, bool bFinal);
};
