#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AttachmentData.generated.h"

class UStaticMesh;
class USkeletalMesh;
class UAnimSequence;
class UAnimMontage;
class UTexture2D;
class UMaterialInterface;

// 瞄具类型：决定开镜时怎么显示
// Grip hold style: Cradle = angled/triangular/default; Grab = vertical. Pose from anims, not FABRIK.
UENUM(BlueprintType)
enum class EGripHoldStyle : uint8
{
	Cradle UMETA(DisplayName = "Cradle / Angled"),
	Grab   UMETA(DisplayName = "Grab / Vertical")
};

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

	// 倒下形态。填了之后，瞄具槽有配件时用这个模型，没镜子时用上面的 Mesh（竖起）。
	// RAR 前后机瞄：Mesh = 竖起，FoldedMesh = 旁边那套倒下 StaticMesh。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Visual")
	TObjectPtr<UStaticMesh> FoldedMesh;

	// Optional skeletal mesh (e.g. AK magazine). When set, weapon spawns this instead of Mesh.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Visual")
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	// Played on the magazine skeletal component when the arms reload (sync with Mag30 body notifies).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Magazine")
	TObjectPtr<UAnimSequence> MagReloadAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Magazine")
	TObjectPtr<UAnimSequence> MagEmptyReloadAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Magazine")
	TObjectPtr<UAnimSequence> MagTacticalReloadAnim;

	// 0 = 不改枪上的 MagCapacity。AK 弹鼓填 95。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Magazine", meta = (ClampMin = "0"))
	int32 MagazineCapacity = 0;

	// 手臂换弹。填了就覆盖 DA_WeaponData 里这把枪的换弹。AK MAG30 留空，弹鼓填 DrumMag 那套。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Magazine|CharacterReload")
	TObjectPtr<UAnimMontage> MagCharacterReloadMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Magazine|CharacterReload")
	TObjectPtr<UAnimMontage> MagCharacterEmptyReloadMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Magazine|CharacterReload")
	TObjectPtr<UAnimMontage> MagCharacterTacticalReloadMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Magazine|CharacterReload")
	TObjectPtr<UAnimMontage> MagCharacterEmptyReloadMontage_Grab;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Magazine|CharacterReload")
	TObjectPtr<UAnimMontage> MagCharacterTacticalReloadMontage_Grab;

	// Hide bone mag_02 (spare mag on SKM_AK_Mag). Uncheck to show the second mag mesh.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Magazine")
	bool bHideSpareMagazineBone = false;

	// Extra bones to hide after spawn (optional).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Magazine")
	TArray<FName> BonesToHide;

	// 留空时用枪槽位表的 SocketName。同一个红点装在不同枪上，可以继续挂到各枪自己的槽位。
	// 填了之后，这个配件改挂自己的插槽。垂直握把和三角握把就能分开：
	// 护木上的 FVHGrip_sckt / AFGrip_sckt，或 RAR 护木上的 SOCKET_Grip。
	// 先在枪骨骼上找；没有时再在已经装上的配件上找（和 FPS Controller 的 Socket、RAR 的握把父级一样）。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Visual")
	FName MountSocketName;

	// 相对 Socket 的微调（位置/旋转/缩放）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Visual")
	FTransform MeshOffset = FTransform::Identity;

	// 握把模型上的扶手点。装上后，第一人称和第三人称的左手 FABRIK 都跟这个 Socket。
	// 握把没挂住，或模型上没有这个 Socket 时，用枪骨骼上的 FABRIK。
	// 在握把 StaticMesh 的 Socket Manager 里加同名 Socket，放在手应该握住的位置。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Grip")
	FName FabrikSocketName = TEXT("FABRIK");

	// Grip slot only. Cradle=angled/default; Grab=vertical.
	// 姿势只定手指和手腕。手的位置靠握把上的 FABRIK / grab 插槽。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Grip")
	EGripHoldStyle GripHoldStyle = EGripHoldStyle::Cradle;

	// 枪管或枪口配件模型上的枪口。开火闪光和弹道视觉起点用它。
	// 枪口配件（消音器）优先于枪管。都没有可用 Socket 时，用枪蓝图上的 MuzzleSocketName。
	// 在配件 StaticMesh 上加同名 Socket，X 轴朝枪口前方。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Muzzle")
	FName MuzzleSocketName = TEXT("Muzzle");

	// ---------- 改枪台属性（只用于界面显示和对比，不影响实际手感） ----------
	// 加减分（-100~100）。改枪台属性条 = 枪的基础值 + 所有配件的分数。后座/散布倍率也会自动折算进操控/精准。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Stats")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Stats", meta = (UIMin = "-100", UIMax = "100"))
	float StatDamage = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Stats", meta = (UIMin = "-100", UIMax = "100"))
	float StatAccuracy = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Stats", meta = (UIMin = "-100", UIMax = "100"))
	float StatControl = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Stats", meta = (UIMin = "-100", UIMax = "100"))
	float StatRange = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Stats", meta = (UIMin = "-100", UIMax = "100"))
	float StatMobility = 0.f;

	// ---------- 后座 ----------
	// 后座倍率：0.8 = 后座减少 20%。多个配件会相乘
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Recoil", meta = (ClampMin = "0"))
	float RecoilMultiplier = 1.f;

	// 松开扳机后回正速度倍率：1.2 = 回正快 20%
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Recoil", meta = (ClampMin = "0"))
	float RecoilRecoveryMultiplier = 1.f;

	// 腰射散布倍率，所有配件相乘（垂直握把可设 0.8）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Spread", meta = (ClampMin = "0"))
	float HipFireSpreadMultiplier = 1.f;

	// ---------- 互斥 ----------
	// 装上这个配件后被锁住的槽位（例：PCC 短护木填 Attachment.Slot.Grip）。
	// 已装在这些槽里的配件会被自动卸下；锁住期间服务器拒绝往这些槽装东西，改枪台显示为锁定。
	// 会锁住必选槽（bRequired）的配件本身不能装。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Compatibility", meta = (Categories = "Attachment.Slot"))
	FGameplayTagContainer BlockedSlots;

	// ---------- 瞄具（红点 / 倍镜） ----------
	// 勾上后，镜外的开镜 FOV 用下面的 AimFOV，而不是枪自己的 AimFeildOfView。
	// 画中画的镜片倍率另看 Magnification，不受这个角度影响。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic")
	bool bOverridesAimFOV = false;

	// 镜外开镜视野（玩家镜头）。画中画时这是看着镜子外壳的视野，镜片里的放大不看它。
	// 红点/全息勾上后，整屏就用这个角度。
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

	// 镜片位置的 Socket。模型和这个 Socket 或 SightSocket 至少有一个，画中画才会启用。
	// 拍摄机不挂在这里，它跟玩家镜头重合，避免预览模型跟着镜子放大。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic|Magnified", meta = (EditCondition = "OpticType == EOpticType::Magnified || OpticType == EOpticType::Variable", EditConditionHides))
	FName LensSocketName = TEXT("LensSocket");

	// 画面槽材质。必须有一个贴图参数（名字见下面），代码会把实时画面塞进去。
	// 留空时，Magnified 使用模型上 LensMaterialIndex 那一槽的现成材质。
	// 这一槽是放大画面，不是准心。准心留在模型的另一层材质上。
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

	// 画中画曝光补偿（EV）。0 = 不额外加减；正数更亮，负数更暗。拍摄用 Histogram 自动曝光，此项只是微调
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic|Magnified", meta = (EditCondition = "OpticType == EOpticType::Magnified || OpticType == EOpticType::Variable", EditConditionHides, UIMin = "-4", UIMax = "4"))
	float PiPExposureBias = 0.f;

	// 画中画是否用 Lumen GI/反射。默认开：室内阴影有间接光，不会纯黑。关可省性能，但暗部会塌
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic|Magnified", meta = (EditCondition = "OpticType == EOpticType::Magnified || OpticType == EOpticType::Variable", EditConditionHides))
	bool bPiPUseLumen = true;

	// 画中画时域抗锯齿。默认关 = FXAA（不依赖时域历史，不易出噪点）；开 = TAA/TSR
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic|Magnified", meta = (EditCondition = "OpticType == EOpticType::Magnified || OpticType == EOpticType::Variable", EditConditionHides))
	bool bPiPTemporalAA = false;

	// 第 8 步：这面瞄具要求的眼距。勾上后覆盖武器的 AdsEyeDistance（例如倍镜要离眼睛远一点）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic")
	bool bOverridesEyeDistance = false;

	// 眼距（厘米，瞄准点到镜头）。只有勾了 bOverridesEyeDistance 才用
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Optic", meta = (EditCondition = "bOverridesEyeDistance", ClampMin = "0"))
	float EyeDistance = 10.f;

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

	// ---------- 侧瞄（第 10 步，45 度战术红点） ----------
	// 第 12 步：激光配件（bProvidesLaser）也自动提供侧瞄，下面三项对它同样有效。
	// 勾上：这个配件是侧瞄。装在哪个槽都行（一般是战术导轨或瞄具旁边的导轨）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Canted")
	bool bProvidesCantedSight = false;

	// 侧瞄模型上的瞄准插槽。X 轴朝枪口，Z 轴朝侧瞄自己的“上方”（歪 45 度）。开镜时枪会转到让这个插槽正对屏幕中心、Z 朝上
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Canted", meta = (EditCondition = "bProvidesCantedSight || bProvidesLaser"))
	FName CantedSightSocketName = TEXT("CantedSightSocket");

	// 侧瞄时的镜头 FOV。0 = 用枪的 AimFeildOfView
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Canted", meta = (EditCondition = "bProvidesCantedSight || bProvidesLaser", ClampMin = "0", ClampMax = "170"))
	float CantedAimFOV = 0.f;

	// 侧瞄眼距（厘米，插槽到镜头）。0 = 用枪的 AdsEyeDistance；都为 0 时保持原距离
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Canted", meta = (EditCondition = "bProvidesCantedSight || bProvidesLaser", ClampMin = "0"))
	float CantedEyeDistance = 0.f;

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

	// ---------- 激光（第 11 步） ----------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Laser")
	bool bProvidesLaser = false;

	// 光束从配件模型的哪个 Socket 射出（X 轴 = 射出方向；没有就用模型原点）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Laser", meta = (EditCondition = "bProvidesLaser"))
	FName LaserSocketName = TEXT("LaserSocket");

	// 光束模型：轴心在起点、沿 +X 延伸。运行时按命中距离拉伸 X
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Laser", meta = (EditCondition = "bProvidesLaser"))
	TSoftObjectPtr<UStaticMesh> LaserBeamMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/FPSTP/Content/Weapons/Attachments/Param/StaticMesh/SM_LaserBeam.SM_LaserBeam")));

	// 光束材质（空 = 用模型自带材质）。会做成动态材质并写入 Color / LaserColor 参数
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Laser", meta = (EditCondition = "bProvidesLaser"))
	TSoftObjectPtr<UMaterialInterface> LaserBeamMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/FPSTP/Content/Weapons/Attachments/Param/Materials/M_LaserBeam.M_LaserBeam")));

	// 命中点光斑（Decal 材质）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Laser", meta = (EditCondition = "bProvidesLaser"))
	TSoftObjectPtr<UMaterialInterface> LaserDotDecalMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/FPSTP/Content/Weapons/Attachments/AttachmentsList/Tactical/TacticalLaser/Materials/M_LaserDecal.M_LaserDecal")));

	// 射线最远距离（厘米）。第 13 步：最少 1500（代码里也会把更小的值抬到 1500）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Laser", meta = (EditCondition = "bProvidesLaser", ClampMin = "1500"))
	float LaserRange = 10000.f;

	// 光束和光斑亮度：写入材质的 Intensity / Emissive / Brightness / EmissiveIntensity 标量参数
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Laser", meta = (EditCondition = "bProvidesLaser", ClampMin = "0"))
	float LaserBrightness = 50.f;

	// 远处光斑放大：每米距离光斑半径至少多少厘米（0.25 = 15 米处约 3.75 厘米），保证远处看得见
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Laser", meta = (EditCondition = "bProvidesLaser", ClampMin = "0"))
	float LaserDotSizePerMeter = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Laser", meta = (EditCondition = "bProvidesLaser"))
	FLinearColor LaserColor = FLinearColor(1.f, 0.f, 0.f);

	// 光斑大小（厘米，Decal 半径）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Laser", meta = (EditCondition = "bProvidesLaser", ClampMin = "0.1"))
	float LaserDotSize = 1.5f;

	// 开镜时隐藏光束（光斑保留）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Attachment|Laser", meta = (EditCondition = "bProvidesLaser"))
	bool bHideBeamWhenAiming = true;
	// 让 AssetManager 能按 "Attachment" 类型找到所有配件（以后做选择界面有用）
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
