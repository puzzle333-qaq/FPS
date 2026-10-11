#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RecoilModifierInterface.generated.h"

// Multipliers stacked on the weapon's authored recoil. 1 leaves it unchanged.
USTRUCT(BlueprintType)
struct FRecoilScale
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Recoil", meta = (ClampMin = "0"))
	float Pitch = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Recoil", meta = (ClampMin = "0"))
	float Yaw = 1.f;

	// Scales how fast the view gives the kick back after the trigger is released.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Recoil", meta = (ClampMin = "0"))
	float Recovery = 1.f;
};

UINTERFACE(MinimalAPI, Blueprintable, BlueprintType)
class URecoilModifier : public UInterface
{
	GENERATED_BODY()
};

// Put this on a weapon or character component. A grip returns Pitch 0.8 to cut vertical kick.
class FPS_API IRecoilModifier
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FPS|Recoil")
	FRecoilScale GetAttachmentRecoilScale() const;
};
