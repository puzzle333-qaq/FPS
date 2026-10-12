#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AttachmentData.generated.h"

class UStaticMesh;
class UTexture2D;

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
