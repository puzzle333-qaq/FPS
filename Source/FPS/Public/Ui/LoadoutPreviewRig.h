#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LoadoutPreviewRig.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UDirectionalLightComponent;
class AWeapon;
class USceneComponent;

// 把最多三把枪摆在镜头前，画进一张贴图，给整备界面左侧用。不进关卡，打开界面时临时生成。
UCLASS()
class FPS_API ALoadoutPreviewRig : public AActor
{
	GENERATED_BODY()

public:
	ALoadoutPreviewRig();

	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

	// Weapons 按界面从上到下的顺序，最多三把。空位不生成模型。
	void ShowWeapons(const TArray<AWeapon*>& Weapons);
	void Capture();

protected:
	virtual void BeginPlay() override;

private:
	void ClearPreviews();
	void SpawnPreview(AWeapon* Weapon, int32 Index);

	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> CaptureComponent;

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> KeyLight;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> PreviewComponents;
};
