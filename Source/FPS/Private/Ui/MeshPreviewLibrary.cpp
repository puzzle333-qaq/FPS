#include "Ui/MeshPreviewLibrary.h"

#include "Ui/MeshPreviewCapture.h"
#include "Components/Image.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Engine/StreamableRenderAsset.h"

AMeshPreviewCapture* UMeshPreviewLibrary::SpawnMeshPreviewCapture(UObject* WorldContextObject, int32 Width, int32 Height)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!IsValid(World)) return nullptr;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;

	// 放到很远的地方，避免进画面；不要 HiddenInGame，否则 SceneCapture 拍不到
	AMeshPreviewCapture* Capture = World->SpawnActor<AMeshPreviewCapture>(
		AMeshPreviewCapture::StaticClass(),
		FVector(0.f, 0.f, -50000.f),
		FRotator::ZeroRotator,
		Params);
	if (!IsValid(Capture)) return nullptr;

	Capture->RenderTargetWidth = FMath::Max(Width, 16);
	Capture->RenderTargetHeight = FMath::Max(Height, 16);
	return Capture;
}

void UMeshPreviewLibrary::ApplyRenderTargetToImage(UImage* Image, UTextureRenderTarget2D* RenderTarget)
{
	if (!IsValid(Image) || !IsValid(RenderTarget)) return;

	FSlateBrush Brush = Image->GetBrush();
	Brush.SetResourceObject(RenderTarget);
	Brush.ImageSize = FVector2D(
		RenderTarget->SizeX > 0 ? RenderTarget->SizeX : 256.f,
		RenderTarget->SizeY > 0 ? RenderTarget->SizeY : 256.f);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Image->SetBrush(Brush);
}

AMeshPreviewCapture* UMeshPreviewLibrary::PreviewSoftPathToImage(
	UObject* WorldContextObject,
	UImage* Image,
	const FSoftObjectPath& MeshPath,
	AMeshPreviewCapture* ReuseCapture,
	FRotator RotationOffset,
	float Scale,
	int32 Width,
	int32 Height)
{
	AMeshPreviewCapture* Capture = IsValid(ReuseCapture)
		? ReuseCapture
		: SpawnMeshPreviewCapture(WorldContextObject, Width, Height);
	if (!IsValid(Capture)) return nullptr;

	Capture->RenderTargetWidth = FMath::Max(Width, 16);
	Capture->RenderTargetHeight = FMath::Max(Height, 16);
	if (!Capture->ShowSoftPath(MeshPath, RotationOffset, Scale))
	{
		return Capture;
	}
	ApplyRenderTargetToImage(Image, Capture->GetRenderTarget());
	return Capture;
}
