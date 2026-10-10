#include "Weapon/SniperScopeComponent.h"

#include "Camera/CameraComponent.h"
#include "CombatComponent.h"
#include "Components/Image.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ShooterCharacter.h"
#include "ShooterGamePlayTags.h"
#include "Weapon/Weapon.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

namespace
{
	// Fixed 3x until magnification switching exists. MaskRadius is in vertical
	// screen UV, after the material corrects for aspect, so 0.5 is a circle
	// whose diameter matches the screen height.
	constexpr float SniperMagnification = 3.f;
	constexpr float SniperMaskRadius = 0.5f;
	constexpr float SniperRingWidth = 0.022f;
	constexpr float SniperReticleThickness = 0.0014f;
	constexpr int32 SniperCaptureLongEdge = 1280;

	const TCHAR* SniperScopeMaterialPath = TEXT("/Game/FPS/UI/M_SniperScope.M_SniperScope");
}

void USniperScopeWidget::SetScopeMaterial(UMaterialInstanceDynamic* Material)
{
	ScopeMaterial = Material;
	if (IsValid(ScopeImage) && IsValid(ScopeMaterial))
	{
		ScopeImage->SetBrushFromMaterial(ScopeMaterial);
	}
}

void USniperScopeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ScopeCanvas"));
	WidgetTree->RootWidget = Canvas;

	ScopeImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ScopeImage"));
	if (UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(ScopeImage))
	{
		PanelSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		PanelSlot->SetOffsets(FMargin(0.f));
	}

	ScopeImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (IsValid(ScopeMaterial))
	{
		ScopeImage->SetBrushFromMaterial(ScopeMaterial);
	}
}

USniperScopeComponent::USniperScopeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USniperScopeComponent::EnsureResources()
{
	if (bResourcesReady)
	{
		return;
	}

	AShooterCharacter* Character = Cast<AShooterCharacter>(GetOwner());
	UCameraComponent* Camera = IsValid(Character) ? Character->GetFirstPersonCamera() : nullptr;
	if (!IsValid(Camera))
	{
		return;
	}

	UMaterialInterface* ScopeMaterialParent = LoadObject<UMaterialInterface>(nullptr, SniperScopeMaterialPath);
	if (!IsValid(ScopeMaterialParent))
	{
		UE_LOG(LogTemp, Warning, TEXT("Sniper scope material is missing: %s"), SniperScopeMaterialPath);
		return;
	}

	RenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("SniperScopeTarget"));
	RenderTarget->RenderTargetFormat = RTF_RGBA8;
	RenderTarget->ClearColor = FLinearColor::Black;
	RenderTarget->bAutoGenerateMips = false;
	RenderTarget->InitAutoFormat(SniperCaptureLongEdge, FMath::RoundToInt(SniperCaptureLongEdge * 9.f / 16.f));
	RenderTarget->UpdateResourceImmediate(true);

	Capture = NewObject<USceneCaptureComponent2D>(this, TEXT("SniperScopeCapture"));
	Capture->SetupAttachment(Camera);
	Capture->SetRelativeLocation(FVector::ZeroVector);
	Capture->SetRelativeRotation(FRotator::ZeroRotator);
	Capture->FOVAngle = ComputeScopeFieldOfView(Camera->FieldOfView);
	Capture->TextureTarget = RenderTarget;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	Capture->PostProcessBlendWeight = 0.f;
	Capture->PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	Capture->RegisterComponent();

	ScopeMaterial = UMaterialInstanceDynamic::Create(ScopeMaterialParent, this);
	ScopeMaterial->SetTextureParameterValue(TEXT("ScopeView"), RenderTarget);
	ScopeMaterial->SetScalarParameterValue(TEXT("MaskRadius"), SniperMaskRadius);
	ScopeMaterial->SetScalarParameterValue(TEXT("RingWidth"), SniperRingWidth);
	ScopeMaterial->SetScalarParameterValue(TEXT("ReticleThickness"), SniperReticleThickness);
	ScopeMaterial->SetScalarParameterValue(TEXT("Aspect"), 16.f / 9.f);

	if (APlayerController* Controller = Cast<APlayerController>(Character->GetController()))
	{
		Overlay = CreateWidget<USniperScopeWidget>(Controller, USniperScopeWidget::StaticClass());
		if (IsValid(Overlay))
		{
			Overlay->SetScopeMaterial(ScopeMaterial);
			Overlay->SetVisibility(ESlateVisibility::Collapsed);
			// Below the HUD so ammo and health stay visible.
			Overlay->AddToViewport(-1);
		}
	}

	bResourcesReady = IsValid(Capture) && IsValid(RenderTarget) && IsValid(ScopeMaterial) && IsValid(Overlay);
}

void USniperScopeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(Overlay))
	{
		Overlay->RemoveFromParent();
		Overlay = nullptr;
	}
	if (IsValid(Capture))
	{
		Capture->DestroyComponent();
		Capture = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void USniperScopeComponent::UpdateView()
{
	AShooterCharacter* Character = Cast<AShooterCharacter>(GetOwner());
	UCombatComponent* Combat = IsValid(Character) ? Character->GetCombatComponent() : nullptr;
	const AWeapon* Weapon = IsValid(Combat) ? Combat->CurrentWeapon.Get() : nullptr;
	const bool bWantScope = IsValid(Character)
		&& Character->IsLocallyControlled()
		&& IsValid(Combat)
		&& Combat->bAiming
		&& IsValid(Weapon)
		&& Weapon->WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Sniper);

	if (!bWantScope)
	{
		SetScopeActive(false);
		return;
	}

	EnsureResources();
	if (!bResourcesReady)
	{
		return;
	}

	SetScopeActive(true);
	ApplyScopeFrame();
}

void USniperScopeComponent::SetScopeActive(bool bActive)
{
	if (bScopeActive == bActive)
	{
		return;
	}
	bScopeActive = bActive;

	if (IsValid(Capture))
	{
		Capture->bCaptureEveryFrame = bActive;
		Capture->SetVisibility(bActive);
		Capture->SetComponentTickEnabled(bActive);
	}
	if (IsValid(Overlay))
	{
		Overlay->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void USniperScopeComponent::ApplyScopeFrame()
{
	AShooterCharacter* Character = Cast<AShooterCharacter>(GetOwner());
	UCameraComponent* Camera = IsValid(Character) ? Character->GetFirstPersonCamera() : nullptr;
	UCombatComponent* Combat = IsValid(Character) ? Character->GetCombatComponent() : nullptr;
	AWeapon* Weapon = IsValid(Combat) ? Combat->CurrentWeapon.Get() : nullptr;
	if (!IsValid(Camera) || !IsValid(Capture) || !IsValid(ScopeMaterial))
	{
		return;
	}

	// The blueprint aim timeline would otherwise zoom the whole screen.
	Camera->SetFieldOfView(Character->GetDefaultFieldOfView());
	Capture->FOVAngle = ComputeScopeFieldOfView(Character->GetDefaultFieldOfView());

	Capture->HiddenActors.Reset();
	Capture->HiddenActors.Add(Character);
	if (IsValid(Weapon))
	{
		Capture->HiddenActors.Add(Weapon);
	}

	FVector2D ViewportSize(16.f, 9.f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	const float Aspect = ViewportSize.Y > 1.f ? ViewportSize.X / ViewportSize.Y : (16.f / 9.f);
	ScopeMaterial->SetScalarParameterValue(TEXT("Aspect"), Aspect);

	if (IsValid(RenderTarget) && ViewportSize.X > 1.f && ViewportSize.Y > 1.f)
	{
		const float Scale = SniperCaptureLongEdge / FMath::Max(ViewportSize.X, ViewportSize.Y);
		const int32 TargetX = FMath::Max(16, FMath::RoundToInt(ViewportSize.X * Scale));
		const int32 TargetY = FMath::Max(16, FMath::RoundToInt(ViewportSize.Y * Scale));
		if (RenderTarget->SizeX != TargetX || RenderTarget->SizeY != TargetY)
		{
			RenderTarget->ResizeTarget(TargetX, TargetY);
		}
	}
}

float USniperScopeComponent::ComputeScopeFieldOfView(float CameraFieldOfView) const
{
	const float HalfCamera = FMath::DegreesToRadians(CameraFieldOfView * 0.5f);
	const float HalfScope = FMath::Atan(FMath::Tan(HalfCamera) / SniperMagnification);
	return FMath::RadiansToDegrees(HalfScope * 2.f);
}
