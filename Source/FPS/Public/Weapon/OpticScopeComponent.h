#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OpticScopeComponent.generated.h"

class UAttachmentData;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USceneCaptureComponent2D;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

// 倍镜画中画：只在“本地玩家 + 开镜 + 瞄具是 Magnified”时工作。
// 原理：在倍镜模型的 LensSocket 上挂一个 SceneCapture，用很小的 FOV 拍一张放大画面到 RenderTarget，
// 再把 RenderTarget 塞进镜片材质。镜片外面还是正常画面，所以叫“画中画”。
// 现在所有武器（包括狙击）都按瞄具类型走这里：Variable / 配了镜片的 Magnified 才开画中画。
UCLASS()
class FPS_API UOpticScopeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOpticScopeComponent();

	// 角色每帧调用（AShooterCharacter::Tick）
	void UpdateView();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 关掉拍摄，并把镜片材质还原
	void Deactivate();
	// 倍镜模型或配件数据变了：重新挂 SceneCapture、换镜片材质
	bool BindToOptic(UStaticMeshComponent* OpticMesh, const UAttachmentData* Optic);
	void RestoreLensMaterial();
	float ComputeCaptureFOV(const UAttachmentData* Optic) const;

	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> LensMID;

	// 当前绑定的倍镜模型，以及它镜片槽原来的材质（关镜时还原）
	TWeakObjectPtr<UStaticMeshComponent> BoundMesh;
	TWeakObjectPtr<const UAttachmentData> BoundData;
	TWeakObjectPtr<UMaterialInterface> OriginalLensMaterial;
	int32 BoundMaterialIndex = INDEX_NONE;

	bool bActive = false;
	float SmoothedCaptureFOV = 0.f;
};