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
	Capture->HiddenActors.Reset();
	Capture->HiddenActors.Add(Character);   // 别拍到自己的手臂
	Capture->HiddenActors.Add(Weapon);      // 别拍到枪和倍镜自己（配件模型是枪的组件）

	if (!bActive)
	{
		bActive = true;
		Capture->bCaptureEveryFrame = true;
		Capture->SetComponentTickEnabled(true);
		Capture->SetVisibility(true);
		// 换上画中画镜片材质
		if (IsValid(LensMID) && BoundMaterialIndex != INDEX_NONE)
		{
			OpticMesh->SetMaterial(BoundMaterialIndex, LensMID);
		}
	}
}

bool UOpticScopeComponent::BindToOptic(UStaticMeshComponent* OpticMesh, const UAttachmentData* Optic)
{
	if (BoundMesh.Get() == OpticMesh && BoundData.Get() == Optic && IsValid(Capture))
	{
		return true;   // 没变，不用重新绑
	}

	// 换了倍镜：先把旧的还原
	Deactivate();
	RestoreLensMaterial();

	if (Optic->LensSocketName.IsNone() || !OpticMesh->DoesSocketExist(Optic->LensSocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OpticScope] %s 的模型上没有 Socket '%s'，倍镜画中画不会显示"),
			*GetNameSafe(Optic), *Optic->LensSocketName.ToString());
		BoundMesh = OpticMesh;   // 记住，避免每帧刷日志
		BoundData = Optic;
		return false;
	}
	if (!IsValid(Optic->LensMaterial))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OpticScope] %s 没有填 LensMaterial"), *GetNameSafe(Optic));
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

	// SceneCapture：只创建一次，换倍镜时重新挂到新模型的 LensSocket
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
	}
	Capture->AttachToComponent(OpticMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Optic->LensSocketName);

	// 镜片材质：动态材质实例，把 RenderTarget 塞进贴图参数
	LensMID = UMaterialInstanceDynamic::Create(Optic->LensMaterial, this);
	LensMID->SetTextureParameterValue(Optic->LensTextureParameterName, RenderTarget);

	BoundMaterialIndex = Optic->LensMaterialIndex < OpticMesh->GetNumMaterials() ? Optic->LensMaterialIndex : INDEX_NONE;
	if (BoundMaterialIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OpticScope] %s 的 LensMaterialIndex=%d 超出模型材质槽数量"), *GetNameSafe(Optic), Optic->LensMaterialIndex);
	}
	else
	{
		OriginalLensMaterial = OpticMesh->GetMaterial(BoundMaterialIndex);
	}

	BoundMesh = OpticMesh;
	BoundData = Optic;
	return true;
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
		Capture->SetVisibility(false);
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
	// 统一交给武器算：Variable = 当前档，Magnified = AimFOV 或按 Magnification 换算
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