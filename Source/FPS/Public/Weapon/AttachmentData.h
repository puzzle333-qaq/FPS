#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AttachmentData.generated.h"

class UStaticMesh;
class UTexture2D;
class UMaterialInterface;

// 瞄具类型：决定开镜时怎么显示
UENUM(BlueprintType)
enum class EOpticType : uint8
{
	None         UMETA(DisplayName = "None"),
	RedDot       UMETA(DisplayName = "Red Dot"),
	Holographic  UMETA(DisplayName = "Holographic"),
	Magnified    UMETA(DisplayName = "Magnified (PiP Scope)"),
	// 可变倍率：开镜时按键在 ZoomLevelsFOV 的几档之间切换（整屏变焦，例如 1x/4x）
	Variable     UMETA(DisplayName = "Variable Zoom")
};

// 一个配件 = 一份数据资产（在内容浏览器里右键 -> Miscellaneous -> Data Asset -> AttachmentData）
UCLASS(BlueprintType)
class FPS_API UAttachmentData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---------- 基本信息 ----------
	// 这个配件属于哪种槽位（枪管/后托/握把...）。槽位类型是 GameplayTag，
	// 想加新类型不用改 C++：Project Settings -> GameplayTags 里加一个 Attachment.Slot.xxx 就行
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment", meta = (Categories = "Attachment.Slot"))
	FGameplayTag SlotTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment")
	FText DisplayName;

	// 选择界面用的图标（可选）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment")
	TObjectPtr<UTexture2D> Icon;

	// ---------- 外观 ----------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Visual")
	TObjectPtr<UStaticMesh> Mesh;

	// 注意：挂在枪的哪个 Socket 不在这里配，而是由“枪的槽位表”决定（FWeaponAttachmentSlot::SocketName）
	// 这样同一个红点装在不同枪上，可以挂到各自不同的位置

	// 相对 Socket 的微调（位置/旋转/缩放）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Visual")
	FTransform MeshOffset = FTransform::Identity;

	// 握把模型上的扶手点。装上后，第一人称和第三人称的左手 FABRIK 都跟这个 Socket。
	// 握把没挂住，或模型上没有这个 Socket 时，用枪骨骼上的 FABRIK。
	// 在握把 StaticMesh 的 Socket Manager 里加同名 Socket，放在手应该握住的位置。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Grip")
	FName FabrikSocketName = TEXT("FABRIK");

	// 枪管或枪口配件模型上的枪口。开火闪光和弹道视觉起点用它。
	// 枪口配件（消音器）优先于枪管。都没有可用 Socket 时，用枪蓝图上的 MuzzleSocketName。
	// 在配件 StaticMesh 上加同名 Socket，X 轴朝枪口前方。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Muzzle")
	FName MuzzleSocketName = TEXT("Muzzle");

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

	// 瞄具类型。红点/全息：相机对准 SightSocket 即可。
	// 倍镜：除了对准 SightSocket，还会在镜片上用 SceneCapture 渲染放大画面（画中画）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic")
	EOpticType OpticType = EOpticType::None;

	// ---------- 倍镜（只有 OpticType = Magnified 才用） ----------
	// 倍率：2 / 4 / 8。会换算成 SceneCapture 的 FOV
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic|Magnified", meta = (EditCondition = "OpticType == EOpticType::Magnified", EditConditionHides, ClampMin = "1", ClampMax = "30"))
	float Magnification = 4.f;

	// 换算倍率时的“基准视野”。0 = 用角色相机的默认 FOV
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic|Magnified", meta = (EditCondition = "OpticType == EOpticType::Magnified", EditConditionHides, ClampMin = "0", ClampMax = "170"))
	float BaseFOVForMagnification = 0.f;

	// 镜片位置的 Socket（倍镜模型上，放在物镜前方，X 轴朝枪口）。SceneCapture 挂在这里
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic|Magnified", meta = (EditCondition = "OpticType == EOpticType::Magnified || OpticType == EOpticType::Variable", EditConditionHides))
	FName LensSocketName = TEXT("LensSocket");

	// 镜片材质。必须有一个 Texture 参数（名字见下面），代码会把放大画面塞进去
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic|Magnified", meta = (EditCondition = "OpticType == EOpticType::Magnified || OpticType == EOpticType::Variable", EditConditionHides))
	TObjectPtr<UMaterialInterface> LensMaterial;

	// 镜片材质里贴图参数的名字
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic|Magnified", meta = (EditCondition = "OpticType == EOpticType::Magnified || OpticType == EOpticType::Variable", EditConditionHides))
	FName LensTextureParameterName = TEXT("ScopeView");

	// 倍镜模型上镜片那块的材质槽序号（Static Mesh 编辑器 -> Material Slots 里看）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic|Magnified", meta = (EditCondition = "OpticType == EOpticType::Magnified || OpticType == EOpticType::Variable", EditConditionHides, ClampMin = "0"))
	int32 LensMaterialIndex = 0;

	// 渲染目标分辨率（正方形）。越大越清晰也越耗性能，512 一般够用
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic|Magnified", meta = (EditCondition = "OpticType == EOpticType::Magnified || OpticType == EOpticType::Variable", EditConditionHides, ClampMin = "64", ClampMax = "2048"))
	int32 RenderTargetSize = 512;

	bool IsMagnifiedOptic() const { return OpticType == EOpticType::Magnified; }

	// ---------- 可变倍率（只有 OpticType = Variable 才用） ----------
	// 每一档的开镜 FOV，第 0 档是默认档。例如 1x/4x 填 [55, 23]，1x/2x/4x 填 [55, 45, 23]
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic|Variable", meta = (EditCondition = "OpticType == EOpticType::Variable", EditConditionHides))
	TArray<float> ZoomLevelsFOV;

	bool IsVariableOptic() const { return OpticType == EOpticType::Variable && ZoomLevelsFOV.Num() > 0; }

	// ---------- 开镜速度 ----------
	// 开镜时间倍率：1.1 = 开镜慢 10%，0.9 = 快 10%。多个配件相乘（和后坐倍率一样）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|ADS", meta = (ClampMin = "0.05"))
	float AdsTimeMultiplier = 1.f;

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
