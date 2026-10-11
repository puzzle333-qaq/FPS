#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_WeaponGrip.generated.h"

// Place this on the frame where the other hand takes the gun.
UCLASS(meta = (DisplayName = "Weapon Grip"))
class FPS_API UAnimNotify_WeaponGrip : public UAnimNotify
{
	GENERATED_BODY()

public:
	// Socket or bone on the character mesh that is holding the gun on this frame.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	FName GripSocket;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	virtual FString GetNotifyName_Implementation() const override;
};
