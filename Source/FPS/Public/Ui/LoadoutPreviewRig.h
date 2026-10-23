#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
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

	// ---- 改枪台视角：拖拽旋转 / 缩放 / 对准某个槽位 ----
	void SetViewRotation(const FRotator& Rotation);
	FRotator GetViewRotation() const;
	void SetViewOrthoWidth(float Width);
	float GetViewOrthoWidth() const;
	// 相机平移到 Rig 局部 (Y,Z)。ResetView 回到正中。
	void SetViewPan(const FVector2D& PanYZ);
	FVector2D GetViewPan() const;
	// 世界坐标 -> 预览贴图 UV（0~1，左上为 0）。在画面外返回 false。
	bool ProjectWorldToUV(const FVector& World, FVector2D& OutUV) const;
	// 世界坐标 -> Rig 局部 (Y,Z)，给 SetViewPan 用
	FVector2D WorldToPan(const FVector& World) const;
	// 槽位在预览里的世界位置：装了配件取配件包围盒中心，否则取槽位 Socket。
	bool GetSlotWorldLocation(const AWeapon* Weapon, const FGameplayTag& SlotTag, FVector& OutWorld) const;

	// 渲染贴图尺寸（像素）。整备界面 PreviewImage 的笔刷尺寸跟着它
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Loadout", meta = (ClampMin = "16"))
	int32 RenderTargetWidth = 512;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Loadout", meta = (ClampMin = "16"))
	int32 RenderTargetHeight = 768;

	// 正交相机宽度（厘米）。越大枪越小
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Loadout", meta = (ClampMin = "1"))
	float OrthoWidth = 180.f;

	// 上下两把枪之间的距离（厘米）。第 i 把在 (1 - i) × RowSpacing 的高度
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Loadout")
	float RowSpacing = 90.f;

protected:
	virtual void BeginPlay() override;

private:
	void ClearPreviews();
	void SpawnPreview(AWeapon* Weapon, int32 Index);

	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> CaptureComponent;

	UPROPERTY()
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<USceneComponent>> SlotComponents;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> MountComponents;

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> KeyLight;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> PreviewComponents;
};
