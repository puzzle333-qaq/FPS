#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MeshPreviewCapture.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UDirectionalLightComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class USkeletalMesh;
class UStaticMesh;
class UStreamableRenderAsset;

/**
 * [DEPRECATED for loadout UI] 准备界面请用预烘焙 Texture，见 BakeMeshThumbs_HOWTO。
 * 本 Actor 仅保留给编辑器偶尔拍一张用。
 *
 * 通用缩略图拍照台：任意 StaticMesh / SkeletalMesh（枪、背心、弹匣、头盔…）
 * → SceneCapture → RenderTarget。不进关卡逻辑，UI 打开时临时 Spawn。
 */
UCLASS(Blueprintable, BlueprintType)
class FPS_API AMeshPreviewCapture : public AActor
{
	GENERATED_BODY()

public:
	AMeshPreviewCapture();

	UFUNCTION(BlueprintPure, Category = "FPS|Preview")
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

	/** 显示骨骼网格（枪、衣服等）。会清掉上一件。 */
	UFUNCTION(BlueprintCallable, Category = "FPS|Preview")
	void ShowSkeletalMesh(USkeletalMesh* Mesh, FRotator RotationOffset = FRotator::ZeroRotator, float Scale = 1.f);

	/** 显示静态网格（弹匣、配件等）。会清掉上一件。 */
	UFUNCTION(BlueprintCallable, Category = "FPS|Preview")
	void ShowStaticMesh(UStaticMesh* Mesh, FRotator RotationOffset = FRotator::ZeroRotator, float Scale = 1.f);

	/**
	 * Soft path 自动识别 Static / Skeletal。
	 * 例："/Game/InfimaGames/RealisticAssaultRifle/Art/Weapon/Meshes/SK_RAR_AssaultRifle.SK_RAR_AssaultRifle"
	 */
	UFUNCTION(BlueprintCallable, Category = "FPS|Preview")
	bool ShowSoftPath(const FSoftObjectPath& MeshPath, FRotator RotationOffset = FRotator::ZeroRotator, float Scale = 1.f);

	UFUNCTION(BlueprintCallable, Category = "FPS|Preview")
	bool ShowSoftAsset(TSoftObjectPtr<UStreamableRenderAsset> SoftMesh, FRotator RotationOffset = FRotator::ZeroRotator, float Scale = 1.f);

	UFUNCTION(BlueprintCallable, Category = "FPS|Preview")
	void ClearMesh();

	UFUNCTION(BlueprintCallable, Category = "FPS|Preview")
	void Capture();

	/** 手动改视角（拖拽预览时用） */
	UFUNCTION(BlueprintCallable, Category = "FPS|Preview")
	void SetViewRotation(FRotator Rotation);

	UFUNCTION(BlueprintPure, Category = "FPS|Preview")
	FRotator GetViewRotation() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Preview", meta = (ClampMin = "16"))
	int32 RenderTargetWidth = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Preview", meta = (ClampMin = "16"))
	int32 RenderTargetHeight = 512;

	/** 正交宽度；<=0 时按包围盒自动适配 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Preview")
	float OrthoWidth = 0.f;

	/** 自动适配时，包围球半径再乘这个系数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Preview", meta = (ClampMin = "1"))
	float AutoFitPadding = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Preview")
	FLinearColor ClearColor = FLinearColor(0.06f, 0.07f, 0.09f, 1.f);

protected:
	virtual void BeginPlay() override;

private:
	void EnsureRenderTarget();
	void FitCameraToComponent(USceneComponent* Comp);
	void FinishShow();

	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> CaptureComponent;

	UPROPERTY()
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> KeyLight;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> SkelMeshComp;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> StaticMeshComp;
};

