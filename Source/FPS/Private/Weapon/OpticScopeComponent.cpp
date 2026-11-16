#include "Weapon/OpticScopeComponent.h"

#include "Camera/CameraComponent.h"
#include "CombatComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
	const bool bBodyView=IsValid(Character)&&Character->IsSingleBodyPilot()&&!Character->IsDebugThirdPerson();
 UStaticMeshComponent* OpticMesh = IsValid(Weapon) ? (bBodyView?Weapon->GetOpticMesh3P():Weapon->GetOpticMesh1P()) : nullptr;

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
	// COD 式真实倍率：镜片在屏幕上占的角度 / 倍率，才是镜片里该拍的 FOV。
	// 否则按 90 度换算的 FOV 远大于镜片张角，镜里的东西反而比镜外还小。
	float TargetFOV = ComputeCaptureFOV(Optic);
	{
		const UCameraComponent* Cam = Character->GetFirstPersonCamera();
		const FName LensName = OpticMesh->DoesSocketExist(Optic->LensSocketName) ? Optic->LensSocketName : Optic->SightSocketName;
		if (IsValid(Cam) && IsValid(OpticMesh->GetStaticMesh()))
		{
			const FVector Ext = OpticMesh->GetStaticMesh()->GetBounds().BoxExtent * OpticMesh->GetComponentScale().GetAbs();
			float LensRadius = FMath::Min3(Ext.X, Ext.Y, Ext.Z) * 0.85f;
			// 模型上有 <镜片插槽>_Rim（镜片边缘一点）就用真实镜片半径，倍率才准。没有就退回整体包围盒估算
			const FName RimName(*(LensName.ToString() + TEXT("_Rim")));
			if (OpticMesh->DoesSocketExist(RimName))
			{
				const float RimRadius = FVector::Dist(OpticMesh->GetSocketLocation(RimName), OpticMesh->GetSocketLocation(LensName));
				if (RimRadius > 0.1f)
				{
					LensRadius = RimRadius;
				}
			}
			const float Dist = FVector::Dist(Cam->GetComponentLocation(), OpticMesh->GetSocketLocation(LensName));
			if (LensRadius > 0.1f && Dist > 0.1f)
			{
				const float Base = Optic->BaseFOVForMagnification > 0.f ? Optic->BaseFOVForMagnification : Character->GetDefaultFieldOfView();
				const float Mag = FMath::Max(1.f, Optic->Magnification);
				const float TanLens = LensRadius / Dist;
				const float TanBase = FMath::Tan(FMath::DegreesToRadians(Base * 0.5f));
				const float TanMain = FMath::Tan(FMath::DegreesToRadians(FMath::Max(5.f, Cam->FieldOfView) * 0.5f));
				const float Half = FMath::Atan(TanLens * TanBase / (TanMain * Mag));
				TargetFOV = FMath::Clamp(FMath::RadiansToDegrees(2.f * Half), 1.f, TargetFOV);
			}
		}
	}
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
		HideGlassSlots(OpticMesh);
	}
	else if (IsValid(LensMID) && BoundMaterialIndex != INDEX_NONE && OpticMesh->GetMaterial(BoundMaterialIndex) != LensMID)
	{
		// 每帧保险：画面槽材质被别处改掉时立刻换回画中画，不让镜片和准心掉线
		OpticMesh->SetMaterial(BoundMaterialIndex, LensMID);
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
	RestoreGlassSlots();

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
	// Texture param applied below after BoundMaterialIndex is chosen (multi-name probe).

	// Prefer RenderTarget slot index; fall back to DA index / Lense slot.
	BoundMaterialIndex = Optic->LensMaterialIndex;
	// 先找靠眼睛的目镜槽 RenderTargetRear（COD 画面在目镜上），没有再找挂着 M_RenderTarget 的槽
	{
		const TArray<FName> MeshSlotNames = OpticMesh->GetMaterialSlotNames();
		int32 RearIdx = INDEX_NONE, RtIdx = INDEX_NONE;
		for (int32 i = 0; i < OpticMesh->GetNumMaterials(); ++i)
		{
			const FString SName = MeshSlotNames.IsValidIndex(i) ? MeshSlotNames[i].ToString() : FString();
			const FString MName = GetNameSafe(OpticMesh->GetMaterial(i));
			if (RearIdx == INDEX_NONE && SName.Contains(TEXT("Rear"), ESearchCase::IgnoreCase)) RearIdx = i;
			if (RtIdx == INDEX_NONE && MName.Contains(TEXT("RenderTarget")) && !MName.Contains(TEXT("Placeholder"))) RtIdx = i;
		}
		if (RearIdx != INDEX_NONE) BoundMaterialIndex = RearIdx;
		else if (RtIdx != INDEX_NONE) BoundMaterialIndex = RtIdx;
	}
	if (BoundMaterialIndex < 0 || BoundMaterialIndex >= OpticMesh->GetNumMaterials())
	{
		BoundMaterialIndex = 0;
	}
	OriginalLensMaterial = OpticMesh->GetMaterial(BoundMaterialIndex);

	// Pack M_RenderTarget uses "RenderTarget"; project M_SniperScope uses "ScopeView".
	if (IsValid(LensMID) && IsValid(RenderTarget))
	{
		TArray<FName, TInlineAllocator<5>> CandidateParams;
		CandidateParams.Add(Optic->LensTextureParameterName);
		CandidateParams.Add(FName(TEXT("RenderTarget")));
		CandidateParams.Add(FName(TEXT("ScopeView")));
		CandidateParams.Add(FName(TEXT("Texture")));
		CandidateParams.Add(FName(TEXT("RT")));
		FName Chosen = NAME_None;
		for (const FName& Cand : CandidateParams)
		{
			if (Cand.IsNone()) continue;
			LensMID->SetTextureParameterValue(Cand, RenderTarget);
			UTexture* AppliedTexture = nullptr;
			if (LensMID->GetTextureParameterValue(FHashedMaterialParameterInfo(Cand), AppliedTexture)
				&& AppliedTexture == RenderTarget)
			{
				Chosen = Cand;
				break;
			}
		}
		if (Chosen.IsNone())
		{
			// Still set DA name so MID at least has something if param exists under another path.
			LensMID->SetTextureParameterValue(Optic->LensTextureParameterName, RenderTarget);
			UE_LOG(LogTemp, Warning, TEXT("[OpticScope] %s: lens texture param probe inconclusive"),
				*GetNameSafe(Optic));
		}
	}

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

	const float Bias = IsValid(Optic) ? Optic->PiPExposureBias : 0.f;
	const bool bUseLumen = !IsValid(Optic) || Optic->bPiPUseLumen;
	const bool bTemporalAA = IsValid(Optic) && Optic->bPiPTemporalAA;

	Capture->bAlwaysPersistRenderingState = true;
	Capture->PostProcessBlendWeight = 1.f;
	Capture->bInheritMainViewCameraPostProcessSettings = false;

	// 拍摄自己做 Histogram 测光，不跟项目/主视角的自动曝光开关绑死
	Capture->ShowFlags.SetEyeAdaptation(true);
	Capture->ShowFlags.SetBloom(false);
	Capture->ShowFlags.SetLocalExposure(false);
	Capture->ShowFlags.SetGrain(false);
	Capture->ShowFlags.SetAntiAliasing(true);
	Capture->ShowFlags.SetTemporalAA(bTemporalAA);   // 默认 FXAA；勾了才用 TAA

	FPostProcessSettings& PP = Capture->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = AEM_Histogram;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = Bias;
	// 始终关掉物理相机曝光叠加，避免 Manual/物理路径把亮度拉飞
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PP.AutoExposureApplyPhysicalCameraExposure = 0;
	// 合理亮度区间：暗部不塌成纯黑，亮部也不轻易过曝
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.AutoExposureMinBrightness = 0.03f;
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.AutoExposureMaxBrightness = 8.f;
	PP.bOverride_AutoExposureSpeedUp = true;
	PP.AutoExposureSpeedUp = 3.f;
	PP.bOverride_AutoExposureSpeedDown = true;
	PP.AutoExposureSpeedDown = 1.f;
	PP.bOverride_AutoExposureLowPercent = true;
	PP.AutoExposureLowPercent = 10.f;
	PP.bOverride_AutoExposureHighPercent = true;
	PP.AutoExposureHighPercent = 90.f;

	// 局部曝光中性化，胶片颗粒关掉（二者都会放大噪点或压死暗部）
	PP.bOverride_LocalExposureHighlightContrastScale = true;
	PP.LocalExposureHighlightContrastScale = 1.f;
	PP.bOverride_LocalExposureShadowContrastScale = true;
	PP.LocalExposureShadowContrastScale = 1.f;
	PP.bOverride_LocalExposureDetailStrength = true;
	PP.LocalExposureDetailStrength = 1.f;
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = 0.f;

	// Lumen：默认开，室内阴影才有间接光
	PP.bOverride_DynamicGlobalIlluminationMethod = true;
	PP.DynamicGlobalIlluminationMethod = bUseLumen
		? EDynamicGlobalIlluminationMethod::Lumen
		: EDynamicGlobalIlluminationMethod::None;
	PP.bOverride_ReflectionMethod = true;
	PP.ReflectionMethod = bUseLumen
		? EReflectionMethod::Lumen
		: EReflectionMethod::ScreenSpace;
	Capture->ShowFlags.SetLumenGlobalIllumination(bUseLumen);
	Capture->ShowFlags.SetLumenReflections(bUseLumen);
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
	// PSR_OPTIC_PIP_FIX: pack MI_*_Lense uses MF_FadeByCameraDistance (invisible in ADS).
	// Prefer slot that already has M_RenderTarget, else load pack PiP material / project scope mat.
	if (!IsValid(OpticMesh) || !IsValid(Optic))
	{
		return nullptr;
	}

	auto IsFadeGlass = [](const UMaterialInterface* Mat) -> bool
	{
		if (!IsValid(Mat)) return false;
		const FString N = Mat->GetName();
		return N.Contains(TEXT("Lense"), ESearchCase::IgnoreCase)
			|| N.Contains(TEXT("BaseOptical"), ESearchCase::IgnoreCase);
	};

	auto IsPipMat = [](const UMaterialInterface* Mat) -> bool
	{
		if (!IsValid(Mat)) return false;
		const FString N = Mat->GetName();
		return N.Contains(TEXT("RenderTarget")) && !N.Contains(TEXT("Placeholder"));
	};

	// DA 里填了画面材质（例如开了准心的 MI_PSR_ScopePiP）优先
	if (IsValid(Optic->LensMaterial) && !IsFadeGlass(Optic->LensMaterial))
	{
		return Optic->LensMaterial;
	}
	// Prefer an existing RenderTarget slot on the mesh (SM_OpticalSight_L slot 1).
	for (int32 i = 0; i < OpticMesh->GetNumMaterials(); ++i)
	{
		if (IsPipMat(OpticMesh->GetMaterial(i)))
		{
			return OpticMesh->GetMaterial(i);
		}
	}

	if (IsValid(Optic->LensMaterial) && !IsFadeGlass(Optic->LensMaterial))
	{
		return Optic->LensMaterial;
	}

	static const TCHAR* PsrPipPath = TEXT("/Game/PrecisionSniperRifle/Weapons/OpticalSight_L/Materials/M_RenderTarget.M_RenderTarget");
	static const TCHAR* ProjectPipPath = TEXT("/Game/FPS/UI/M_SniperScope.M_SniperScope");
	if (UMaterialInterface* PsrPip = LoadObject<UMaterialInterface>(nullptr, PsrPipPath))
	{
		return PsrPip;
	}
	if (UMaterialInterface* ProjectPip = LoadObject<UMaterialInterface>(nullptr, ProjectPipPath))
	{
		return ProjectPip;
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
	RestoreGlassSlots();
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

void UOpticScopeComponent::HideGlassSlots(UStaticMeshComponent* OpticMesh)
{
	RestoreGlassSlots();
	if (!IsValid(OpticMesh))
	{
		return;
	}
	static const TCHAR* InvisiblePath = TEXT("/Game/cgARMod/Weapons/Materials/M_Invisible.M_Invisible");
	UMaterialInterface* Invisible = LoadObject<UMaterialInterface>(nullptr, InvisiblePath);
	if (!IsValid(Invisible))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OpticScope] M_Invisible not found, lens glass stays visible"));
		return;
	}
	const TArray<FName> SlotNames = OpticMesh->GetMaterialSlotNames();
	for (int32 i = 0; i < OpticMesh->GetNumMaterials(); ++i)
	{
		if (i == BoundMaterialIndex)
		{
			continue;   // 画中画画面槽
		}
		UMaterialInterface* Mat = OpticMesh->GetMaterial(i);
		const FString MatName = GetNameSafe(Mat);
		const FString SlotName = SlotNames.IsValidIndex(i) ? SlotNames[i].ToString() : FString();
		if (Mat == LensMID || (MatName.Contains(TEXT("RenderTarget")) && !SlotName.Contains(TEXT("RenderTarget"), ESearchCase::IgnoreCase)))
		{
			continue;
		}
		const bool bOtherPipFace = SlotName.Contains(TEXT("RenderTarget"), ESearchCase::IgnoreCase);   // 物镜那面，开镜时藏掉
		if (!bOtherPipFace && !SlotName.Contains(TEXT("Lens"), ESearchCase::IgnoreCase)
			&& !MatName.Contains(TEXT("Lens"), ESearchCase::IgnoreCase))
		{
			continue;
		}
		HiddenGlassIndices.Add(i);
		HiddenGlassOriginals.Add(Mat);
		OpticMesh->SetMaterial(i, Invisible);
	}
	GlassMesh = OpticMesh;
}

void UOpticScopeComponent::RestoreGlassSlots()
{
	if (UStaticMeshComponent* Mesh = GlassMesh.Get())
	{
		for (int32 k = 0; k < HiddenGlassIndices.Num(); ++k)
		{
			Mesh->SetMaterial(HiddenGlassIndices[k], HiddenGlassOriginals[k].Get());
		}
	}
	HiddenGlassIndices.Reset();
	HiddenGlassOriginals.Reset();
	GlassMesh = nullptr;
}
void UOpticScopeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Deactivate();
	RestoreLensMaterial();
	RestoreGlassSlots();
	if (IsValid(Capture))
	{
		Capture->DestroyComponent();
		Capture = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}