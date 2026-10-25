#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "PSRArmsAnimInstance.generated.h"

// Native graph: IdleSequence (loop) -> DefaultSlot -> Output. No casts, no character dependency.
USTRUCT()
struct FPSRArmsAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()
	FPSRArmsAnimInstanceProxy() {}
	FPSRArmsAnimInstanceProxy(UAnimInstance* In) : FAnimInstanceProxy(In) {}
	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual FAnimNode_Base* GetCustomRootNode() override { return &Slot; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override { OutNodes.Add(&Slot); OutNodes.Add(&IdlePlayer); }
	FAnimNode_SequencePlayer_Standalone IdlePlayer;
	FAnimNode_Slot Slot;
};

UCLASS(Transient, Blueprintable)
class FPS_API UPSRArmsAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	UPSRArmsAnimInstance();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PSR")
	TObjectPtr<UAnimSequenceBase> IdleAnim;
	/** Optional soft path; if set, loaded in Initialize and preferred over IdleAnim (Mann retarget pilot). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PSR")
	TSoftObjectPtr<UAnimSequenceBase> IdleAnimSoft;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="PSR")
	FName SlotName = TEXT("DefaultSlot");
protected:
	UPROPERTY(Transient)
	FPSRArmsAnimInstanceProxy Proxy;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return &Proxy; }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy*) override {}
	friend struct FPSRArmsAnimInstanceProxy;
};
