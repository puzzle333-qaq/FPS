#include "Weapon/OpticScopeComponent.h"

#include "Camera/CameraComponent.h"
#include "CombatComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ShooterCharacter.h"
#include "ShooterGamePlayTags.h"
#include "Weapon/AttachmentData.h"
#include "Weapon/Weapon.h"

namespace
{
	// 编辑器里的蓝色摄影机是 SceneCapture 的预览模型。别让它出现在游戏里。
	void HideCaptureDecoration(USceneCaptureComponent2D* Capture)
	{
		if (!IsValid(Capture))
		{
			return;
		}

		TArray<USceneComponent*> Children;
		Capture->GetChildrenComponents(true, Children);
		for (USceneComponent* Child : Children)
		{
			Child->SetUsingAbsoluteScale(true);
			Child->SetWorldScale3D(FVector::OneVector);
			Child->SetHiddenInGame(true);
			Child->SetVisibility(false);
		}
	}
}

UOpticScopeComponent::UOpticScopeComponent()
{
	// 不自己 Tick，由角色 Tick 里调用 UpdateView，保证在开镜对准之后执行
	PrimaryComponentTick.bCanEverTick = false;
}

void UOpticScopeComponent::UpdateView()
{
	AShooterCharacter* Character = Cast<AShooterCharacter>(GetOwner());
	UCombatComponent* Combat = IsValid(Character) ? Character->GetCombatComponent() : nullptr;
	AWeapon* Weapon = IsValid(Combat) ? Combat->CurrentWeapon.Get() : nullptr;
	const UAttachmentData* Optic = IsValid(Weapon) ? Weapon->GetOpticAttachment() : nullptr;
	UStaticMeshComponent* OpticMesh = IsValid(Weapon) ? Weapon->GetOpticMesh1P() : nullptr;

	// 条件全部满足才开：本地玩家、正在开镜、瞄具要画中画（Variable / 配了镜片的 Magnified）。
	// 按瞄具类型判断，不再看武器是不是狙击
	const bool bWant = IsValid(Character)
		&& Character->IsLocallyControlled()
		&& IsValid(Combat) && Combat->bAiming
		&& !Character->IsCantedAiming()   // 第 10 步：侧瞄时主镜不拍
		&& IsValid(Weapon) && Weapon->UsesLensPiP()
		&& IsValid(Optic) && IsValid(OpticMesh);

	if (!bWant)
	{
		Deactivate();
		return;
	}

	if (!BindToOptic(OpticMesh, Optic))
	{
		Deactivate();
		return;
	}

	// 每帧更新：倍率可能在编辑器里改，隐藏列表要跟着当前武器
	// 拍摄 FOV 来自瞄具（可变倍率 = 当前档）。切档时平滑过去，刚开镜时直接对齐
	const float TargetFOV = ComputeCaptureFOV(Optic);
	if (!bActive || SmoothedCaptureFOV <= 0.f)
	{
		SmoothedCaptureFOV = TargetFOV;
	}
	else
	{
		const float Speed = Character->GetAimFOVInterpSpeed();
		SmoothedCaptureFOV = Speed > 0.f
			? FMath::FInterpTo(SmoothedCaptureFOV, TargetFOV, GetWorld()->GetDeltaSeconds(), Speed)
			: TargetFOV;
	}
	Capture->FOVAngle = SmoothedCaptureFOV;
	ApplyCaptureExposure(Optic);
	PlaceCapture(Character->GetFirstPersonCamera());
	Capture->HiddenActors.Reset();
	Capture->HiddenActors.Add(Character);   // 别拍到自己的手臂
	Capture->HiddenActors.Add(Weapon);      // 别拍到枪和倍镜自己（配件模型是枪的组件）

	if (!bActive)
	{
		bActive = true;
		Capture->bCaptureEveryFrame = true;
		Capture->SetComponentTickEnabled(true);
		Capture->SetVisibility(true);
		HideCaptureDecoration(Capture);
		// 只换画面槽。准心槽保持模型上原来的材质。
		if (IsValid(LensMID) && BoundMaterialIndex != INDEX_NONE)
		{
			OpticMesh->SetMaterial(BoundMaterialIndex, LensMID);
		}
	}
}

bool UOpticScopeComponent::BindToOptic(UStaticMeshComponent* OpticMesh, const UAttachmentData* Optic)
{
	if (BoundMesh.Get() == OpticMesh && BoundData.Get() == Optic)
	{
		// 绑定失败时 LensMID 是空的，下一帧直接返回，避免每帧刷日志
		return IsValid(Capture) && IsValid(LensMID);
	}

	// 换了倍镜：先把旧的还原
	Deactivate();
	RestoreLensMaterial();

	const bool bHasLensSocket = !Optic->LensSocketName.IsNone() && OpticMesh->DoesSocketExist(Optic->LensSocketName);
	const bool bHasSightSocket = !Optic->SightSocketName.IsNone() && OpticMesh->DoesSocketExist(Optic->SightSocketName);
	if (!bHasLensSocket && !bHasSightSocket)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OpticScope] %s 没有可用的 '%s' 或 '%s'，倍镜画中画不会显示"),
			*GetNameSafe(Optic), *Optic->LensSocketName.ToString(), *Optic->SightSocketName.ToString());
		LensMID = nullptr;
		BoundMesh = OpticMesh;
		BoundData = Optic;
		return false;
	}
	UMaterialInterface* PictureMaterial = ResolvePictureMaterial(OpticMesh, Optic);
	if (!IsValid(PictureMaterial))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OpticScope] %s 没有画面材质。填 LensMaterial，或把 LensMaterialIndex 指到已有画面的材质槽"),
			*GetNameSafe(Optic));
		LensMID = nullptr;
		BoundMesh = OpticMesh;
		BoundData = Optic;
		return false;
	}
	if (Optic->LensMaterialIndex < 0 || Optic->LensMaterialIndex >= OpticMesh->GetNumMaterials())
	{
		UE_LOG(LogTemp, Warning, TEXT("[OpticScope] %s 的 LensMaterialIndex=%d 超出模型材质槽数量"),
			*GetNameSafe(Optic), Optic->LensMaterialIndex);
		LensMID = nullptr;
		BoundMesh = OpticMesh;
		BoundData = Optic;
		return false;
	}

	// RenderTarget：正方形，按配件要求的分辨率
	const int32 Size = FMath::Clamp(Optic->RenderTargetSize, 64, 2048);
	if (!IsValid(RenderTarget))
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("OpticScopeTarget"));
		RenderTarget->RenderTargetFormat = RTF_RGBA8;
		RenderTarget->ClearColor = FLinearColor::Black;
		RenderTarget->bAutoGenerateMips = false;
		RenderTarget->InitAutoFormat(Size, Size);
		RenderTarget->UpdateResourceImmediate(true);
	}
	else if (RenderTarget->SizeX != Size)
	{
		RenderTarget->ResizeTarget(Size, Size);
	}

	// SceneCapture 不挂到镜子上。挂上去时，预览模型会跟着镜子变大。
	if (!IsValid(Capture))
	{
		Capture = NewObject<USceneCaptureComponent2D>(GetOwner(), TEXT("OpticScopeCapture"));
		Capture->TextureTarget = RenderTarget;
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		Capture->bAlwaysPersistRenderingState = true;
		Capture->PrimaryComponentTick.TickGroup = TG_PostUpdateWork;   // 等枪和相机都动完再拍
		Capture->RegisterComponent();
		Capture->SetComponentTickEnabled(false);
		ApplyCaptureExposure(Optic);
	}
	else
	{
		ApplyCaptureExposure(Optic);
	}
	Capture->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	Capture->SetUsingAbsoluteLocation(true);
	Capture->SetUsingAbsoluteRotation(true);
	Capture->SetUsingAbsoluteScale(true);
	Capture->SetWorldScale3D(FVector::OneVector);
	HideCaptureDecoration(Capture);

	// 只替换画面槽。准心槽保持模型上原来的材质。
	LensMID = UMaterialInstanceDynamic::Create(PictureMaterial, this);
	LensMID->SetTextureParameterValue(Optic->LensTextureParameterName, RenderTarget);
	UTexture* AppliedTexture = nullptr;
	if (!LensMID->GetTextureParameterValue(FHashedMaterialParameterInfo(Optic->LensTextureParameterName), AppliedTexture))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OpticScope] %s 的画面材质没有贴图参数 '%s'，放大画面塞不进去"),
			*GetNameSafe(Optic), *Optic->LensTextureParameterName.ToString());
	}

	BoundMaterialIndex = Optic->LensMaterialIndex;
	OriginalLensMaterial = OpticMesh->GetMaterial(BoundMaterialIndex);

	BoundMesh = OpticMesh;
	BoundData = Optic;
	return true;
}

void UOpticScopeComponent::ApplyCaptureExposure(const UAttachmentData* Optic)
{
	if (!IsValid(Capture))
	{
		return;
	}

	const float Bias = IsValid(Optic) ? Optic->PiPExposureBias : -1.5f;

	// FinalColorLDR 仍走色调映射；关掉自动曝光（眼球适应），改用固定补偿
	Capture->ShowFlags.SetEyeAdaptation(false);
	Capture->ShowFlags.SetBloom(false);   // 小分辨率 RT 上 Bloom 容易发白
	// Tonemapper 保持默认开着（FinalColorLDR 需要）

	Capture->PostProcessBlendWeight = 1.f;
	Capture->bInheritMainViewCameraPostProcessSettings = false;
	FPostProcessSettings& PP = Capture->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = AEM_Manual;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = Bias;
	// Manual 下把亮度锁死，避免残留的 auto 区间把画面拉亮
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.AutoExposureMinBrightness = 1.f;
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.AutoExposureMaxBrightness = 1.f;
}

void UOpticScopeComponent::PlaceCapture(const UCameraComponent* Camera)
{
	if (!IsValid(Capture))
	{
		return;
	}

	// 跟玩家镜头重合。枪和镜子已经从拍摄里隐藏，画面中心就是子弹射线。
	Capture->SetUsingAbsoluteScale(true);
	Capture->SetWorldScale3D(FVector::OneVector);
	if (IsValid(Camera))
	{
		Capture->SetWorldLocation(Camera->GetComponentLocation());
		Capture->SetWorldRotation(Camera->GetComponentRotation());
	}
	HideCaptureDecoration(Capture);
}

UMaterialInterface* UOpticScopeComponent::ResolvePictureMaterial(const UStaticMeshComponent* OpticMesh, const UAttachmentData* Optic) const
{
	if (!IsValid(OpticMesh) || !IsValid(Optic))
	{
		return nullptr;
	}
	if (IsValid(Optic->LensMaterial))
	{
		return Optic->LensMaterial;
	}
	if (Optic->LensMaterialIndex >= 0 && Optic->LensMaterialIndex < OpticMesh->GetNumMaterials())
	{
		return OpticMesh->GetMaterial(Optic->LensMaterialIndex);
	}
	return nullptr;
}

void UOpticScopeComponent::Deactivate()
{
	if (!bActive)
	{
		return;
	}
	bActive = false;

	// 不开镜就不拍，省性能
	if (IsValid(Capture))
	{
		Capture->bCaptureEveryFrame = false;
		Capture->SetComponentTickEnabled(false);
		Capture->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Capture->SetUsingAbsoluteScale(true);
		Capture->SetWorldScale3D(FVector::OneVector);
		Capture->SetVisibility(false);
		HideCaptureDecoration(Capture);
	}
	RestoreLensMaterial();
}

void UOpticScopeComponent::RestoreLensMaterial()
{
	UStaticMeshComponent* Mesh = BoundMesh.Get();
	if (IsValid(Mesh) && BoundMaterialIndex != INDEX_NONE && Mesh->GetMaterial(BoundMaterialIndex) == LensMID)
	{
		// 还原成镜片原来的材质（例如普通玻璃）
		Mesh->SetMaterial(BoundMaterialIndex, OriginalLensMaterial.Get());
	}
}

float UOpticScopeComponent::ComputeCaptureFOV(const UAttachmentData* /*Optic*/) const
{
	// 镜片内视野：Variable = 当前档，Magnified = 按 Magnification 换算。镜外镜头不在这里。
	const AShooterCharacter* Character = Cast<AShooterCharacter>(GetOwner());
	const UCombatComponent* Combat = IsValid(Character) ? Character->GetCombatComponent() : nullptr;
	const AWeapon* Weapon = IsValid(Combat) ? Combat->CurrentWeapon.Get() : nullptr;
	const float BaseFOV = IsValid(Character) ? Character->GetDefaultFieldOfView() : 90.f;
	return IsValid(Weapon) ? Weapon->GetPiPCaptureFOV(BaseFOV) : BaseFOV;
}

void UOpticScopeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Deactivate();
	RestoreLensMaterial();
	if (IsValid(Capture))
	{
		Capture->DestroyComponent();
		Capture = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}