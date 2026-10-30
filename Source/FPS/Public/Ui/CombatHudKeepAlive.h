#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatHudKeepAlive.generated.h"

/**
 * Invisible always-ticking widget. Undoes WBP_ShooterOverlay property-binding that
 * Collapses the whole combat HUD (ammo/health) while ADS. Also re-ensures CombatStatusHud.
 */
UCLASS()
class FPS_API UCombatHudKeepAlive : public UUserWidget
{
	GENERATED_BODY()

public:
	UCombatHudKeepAlive(const FObjectInitializer& ObjectInitializer);
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UUserWidget> CachedOverlay;

	float ReensureAccum = 0.f;
};
