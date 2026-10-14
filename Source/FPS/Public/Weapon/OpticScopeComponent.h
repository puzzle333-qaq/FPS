#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OpticScopeComponent.generated.h"

class UAttachmentData;
class UCameraComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USceneCaptureComponent2D;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

// 倍镜画中画：只在“本地玩家 + 开镜 + 瞄具要画中画”时工作。
// 玩家镜头用瞄具的 AimFOV（勾了覆盖），否则用枪的 AimFeildOfView。放大只发生在 SceneCapture 里。
// 实时画面只写进画面槽（LensMaterialIndex）。准心槽保持镜子自带材质，这里不替换。
// 拍摄机跟玩家镜头重合，画面中心和子弹是同一条射线。
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
	// 关掉拍摄，并把画面槽材质还原
	void Deactivate();
	// 倍镜模型或配件数据变了：准备 RenderTarget，只改画面槽
	bool BindToOptic(UStaticMeshComponent* OpticMesh, const UAttachmentData* Optic);
	void PlaceCapture(const UCameraComponent* Camera);
	// 手动曝光 + 关掉眼球适应，避免倍镜画中画过曝
	void ApplyCaptureExposure(const UAttachmentData* Optic);
	void RestoreLensMaterial();
	float ComputeCaptureFOV(const UAttachmentData* Optic) const;
	UMaterialInterface* ResolvePictureMaterial(const UStaticMeshComponent* OpticMesh, const UAttachmentData* Optic) const;

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