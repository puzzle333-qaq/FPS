#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MeshPreviewLibrary.generated.h"

class AMeshPreviewCapture;
class UImage;
class UTextureRenderTarget2D;
class UStreamableRenderAsset;

UCLASS()
class FPS_API UMeshPreviewLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 在当前 World 生成一个拍照台（Hidden）。记得不用时 Destroy。 */
	UFUNCTION(BlueprintCallable, Category = "FPS|Preview", meta = (WorldContext = "WorldContextObject"))
	static AMeshPreviewCapture* SpawnMeshPreviewCapture(UObject* WorldContextObject, int32 Width = 512, int32 Height = 512);

	/** 把 RenderTarget 赋给 Image 的 Brush */
	UFUNCTION(BlueprintCallable, Category = "FPS|Preview")
	static void ApplyRenderTargetToImage(UImage* Image, UTextureRenderTarget2D* RenderTarget);

	/** Soft path → 拍照 → 赋给 Image。返回 Capture（可缓存复用）。 */
	UFUNCTION(BlueprintCallable, Category = "FPS|Preview", meta = (WorldContext = "WorldContextObject"))
	static AMeshPreviewCapture* PreviewSoftPathToImage(
		UObject* WorldContextObject,
		UImage* Image,
		const FSoftObjectPath& MeshPath,
		AMeshPreviewCapture* ReuseCapture = nullptr,
		FRotator RotationOffset = FRotator(0.f, 90.f, 0.f),
		float Scale = 1.f,
		int32 Width = 512,
		int32 Height = 512);
};
