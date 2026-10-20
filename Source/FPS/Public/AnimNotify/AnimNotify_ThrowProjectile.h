#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ThrowProjectile.generated.h"

// Place this on the frame the grenade leaves the hand.
UCLASS(meta = (DisplayName = "Throw Projectile"))
class FPS_API UAnimNotify_ThrowProjectile : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	virtual FString GetNotifyName_Implementation() const override;
};
