#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ActorComponent.h"
#include "SniperScopeComponent.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

// Full-screen overlay. The material draws the magnified view only inside the scope disc.
UCLASS()
class FPS_API USniperScopeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetScopeMaterial(UMaterialInstanceDynamic* Material);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ScopeMaterial;

private:
	UPROPERTY()
	TObjectPtr<UImage> ScopeImage;
};

// Renders a zoomed copy of the player view and shows it only inside the scope disc.
// The player camera itself stays at its normal field of view.
UCLASS()
class FPS_API USniperScopeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USniperScopeComponent();

	void UpdateView();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void EnsureResources();
	void SetScopeActive(bool bActive);
	void ApplyScopeFrame();
	float ComputeScopeFieldOfView(float CameraFieldOfView) const;

	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ScopeMaterial;

	UPROPERTY()
	TObjectPtr<USniperScopeWidget> Overlay;

	bool bResourcesReady = false;
	bool bScopeActive = false;
};
