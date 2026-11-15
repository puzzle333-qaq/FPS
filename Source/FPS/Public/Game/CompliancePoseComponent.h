#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CompliancePoseComponent.generated.h"

UENUM(BlueprintType)
enum class ECompliancePose : uint8 { None, Calm, Scared, Flee, HandsUp, Bound, TableHide, ProneHide, Crawl, Binding, Kneel };

USTRUCT()
struct FCompliancePoseState
{
 GENERATED_BODY()
 UPROPERTY() ECompliancePose Pose=ECompliancePose::None;
 UPROPERTY() float ServerStartTime=0.f;
 UPROPERTY() float PlayRate=1.f;
};

/** Replicated, server-timed animation sequence. Late viewers seek into the current phase. */
UCLASS(ClassGroup=NPC,meta=(BlueprintSpawnableComponent))
class FPS_API UCompliancePoseComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UCompliancePoseComponent();
 void SetMesh(class USkeletalMeshComponent* InMesh);
 void SetPose(ECompliancePose Pose,float PlayRate=1.f);
 ECompliancePose GetPose() const {return State.Pose;}
 float GetHandsUpDuration() const;
 float GetBindingDuration() const;
 bool IsHandsUpSettled() const;
 /** Setup utility for animation assets with the same verified UE5 bind pose. */
 UFUNCTION(BlueprintCallable,Category="NPC|Setup") static bool PrepareCompatibleAnimation(class UAnimSequence* Animation,class USkeleton* Skeleton);
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
 UPROPERTY(ReplicatedUsing=Refresh) FCompliancePoseState State;
 UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Mesh;
 UFUNCTION() void Refresh();
 FTimerHandle PhaseTimer;
};
