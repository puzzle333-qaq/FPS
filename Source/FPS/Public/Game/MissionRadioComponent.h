#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/MissionVoiceTypes.h"
#include "MissionRadioComponent.generated.h"

class UAudioComponent;
class USoundAttenuation;

USTRUCT(BlueprintType)
struct FMissionTransmission
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) EMissionVoiceCue Cue=EMissionVoiceCue::None;
 UPROPERTY(BlueprintReadOnly) float StartServerTime=0;
 UPROPERTY(BlueprintReadOnly) int32 Serial=0;
};

/** Server schedules radio events; every client plays its own non-spatial 2D voice. */
UCLASS(ClassGroup=(Mission))
class FPS_API UMissionRadioComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UMissionRadioComponent();
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UFUNCTION(BlueprintCallable,BlueprintAuthorityOnly,Category="Mission|Voice") void QueueCue(EMissionVoiceCue Cue,bool bOnce=false);
 void PlayImmediate(EMissionVoiceCue Cue);
 void StopBriefing();
 float GetRemainingSeconds() const;
 UFUNCTION(NetMulticast,Reliable) void Multicast_OperatorWarning(APawn* Speaker,EMissionVoiceCue Cue);
 UFUNCTION(BlueprintPure,Category="Mission|Voice") bool IsRadioPlaying() const;
 UPROPERTY(ReplicatedUsing=OnRep_Transmission,BlueprintReadOnly,Category="Mission|Voice") FMissionTransmission Transmission;
 UPROPERTY(Transient,BlueprintReadOnly,Category="Mission|Voice") int32 LocalPlaybackCount=0;
 UPROPERTY(Transient,BlueprintReadOnly,Category="Mission|Voice") int32 LocalWarningCount=0;
 UPROPERTY(Transient,BlueprintReadOnly,Category="Mission|Voice") EMissionVoiceCue LastLocalCue=EMissionVoiceCue::None;
 UPROPERTY(Transient,BlueprintReadOnly,Category="Mission|Voice") TObjectPtr<APawn> LastWarningSpeaker;
private:
 void DispatchNext();
 void StartLocalPlayback();
 UFUNCTION() void OnRep_Transmission();
 UPROPERTY(Transient) TObjectPtr<UAudioComponent> LocalRadio;
 UPROPERTY(Transient) TObjectPtr<USoundAttenuation> ShoutAttenuation;
 TArray<EMissionVoiceCue> Pending;
 TSet<EMissionVoiceCue> OnceCues;
 FTimerHandle DispatchTimer,LocalStartTimer;
 float CurrentEndTime=0;
};
