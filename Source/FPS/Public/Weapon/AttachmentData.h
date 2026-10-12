#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AttachmentData.generated.h"

class UStaticMesh;
class UTexture2D;

// 配件槽位：每把枪每个槽位最多装一个配件
UENUM(BlueprintType)
enum class EAttachmentSlot : uint8
{
	Muzzle UMETA(DisplayName = "Muzzle"),     // 枪口（消音器、制退器）
	Optic UMETA(DisplayName = "Optic"),       // 瞄具（红点、倍镜）
	Grip UMETA(DisplayName = "Grip"),         // 握把
	Tactical UMETA(DisplayName = "Tactical")  // 战术（手电筒）
};

// 一个配件 = 一份数据资产（在内容浏览器里右键 -> Miscellaneous -> Data Asset -> AttachmentData）
UCLASS(BlueprintType)
class FPS_API UAttachmentData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---------- 基本信息 ----------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment")
	EAttachmentSlot Slot = EAttachmentSlot::Muzzle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment")
	FText DisplayName;

	// 选择界面用的图标（可选）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment")
	TObjectPtr<UTexture2D> Icon;

	// ---------- 外观 ----------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Visual")
	TObjectPtr<UStaticMesh> Mesh;

	// 挂到枪的骨骼网格体上哪个 Socket（要在枪的 Skeletal Mesh 里先建好）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Visual")
	FName AttachSocketName = TEXT("Socket_Muzzle");

	// 相对 Socket 的微调（位置/旋转/缩放）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Visual")
	FTransform MeshOffset = FTransform::Identity;

	// ---------- 后座 ----------
	// 后座倍率：0.8 = 后座减少 20%。多个配件会相乘
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Recoil", meta = (ClampMin = "0"))
	float RecoilMultiplier = 1.f;

	// 松开扳机后回正速度倍率：1.2 = 回正快 20%
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Recoil", meta = (ClampMin = "0"))
	float RecoilRecoveryMultiplier = 1.f;

	// ---------- 瞄具（红点 / 倍镜） ----------
	// 勾上后，开镜 FOV 用下面的 AimFOV，而不是枪自己的 AimFeildOfView
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic")
	bool bOverridesAimFOV = false;

	// 开镜视野：越小放大倍率越高（红点约 60，4 倍镜约 20）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic", meta = (EditCondition = "bOverridesAimFOV", ClampMin = "5", ClampMax = "170"))
	float AimFOV = 55.f;

	// 配件模型上的瞄准点 Socket（红点镜片中心）。存在时开镜会对准它，而不是枪上的 AimSocket
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic")
	FName SightSocketName = TEXT("SightSocket");

	// ---------- 手电筒 ----------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Flashlight")
	bool bProvidesFlashlight = false;

	// 灯光挂在配件模型的哪个 Socket 上（没有就挂在模型原点）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Flashlight", meta = (EditCondition = "bProvidesFlashlight"))
	FName LightSocketName = TEXT("LightSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Flashlight", meta = (EditCondition = "bProvidesFlashlight"))
	float LightIntensity = 5000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Flashlight", meta = (EditCondition = "bProvidesFlashlight"))
	FLinearColor LightColor = FLinearColor(1.f, 0.95f, 0.85f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Flashlight", meta = (EditCondition = "bProvidesFlashlight"))
	float LightAttenuationRadius = 2500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Flashlight", meta = (EditCondition = "bProvidesFlashlight"))
	float LightInnerConeAngle = 12.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Flashlight", meta = (EditCondition = "bProvidesFlashlight"))
	float LightOuterConeAngle = 24.f;

	// 让 AssetManager 能按 "Attachment" 类型找到所有配件（以后做选择界面有用）
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
