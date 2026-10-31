#include "Ui/MeshPreviewCapture.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableRenderAsset.h"
#include "UObject/SoftObjectPath.h"

AMeshPreviewCapture::AMeshPreviewCapture()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Root);

	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	CaptureComponent->SetupAttachment(Root);
	CaptureComponent->SetRelativeLocation(FVector(400.f, 0.f, 0.f));
	CaptureComponent->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	CaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
	CaptureComponent->OrthoWidth = 120.f;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureComponent->PostProcessBlendWeight = 0.f;
	CaptureComponent->ShowFlags.SetAtmosphere(false);
	CaptureComponent->ShowFlags.SetFog(false);
	CaptureComponent->ShowFlags.SetPostProcessing(false);
	// 无天空时用无光照，避免全黑
	CaptureComponent->ShowFlags.SetLighting(false);

	KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	KeyLight->SetMobility(EComponentMobility::Movable);
	KeyLight->SetRelativeRotation(FRotator(-40.f, -30.f, 0.f));
	KeyLight->SetIntensity(20.f);

	SkelMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkelMesh"));
	SkelMeshComp->SetupAttachment(Pivot);
	SkelMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkelMeshComp->SetCastShadow(false);
	SkelMeshComp->SetVisibility(false);

	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	StaticMeshComp->SetupAttachment(Pivot);
	StaticMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMeshComp->SetCastShadow(false);
	StaticMeshComp->SetVisibility(false);
}

void AMeshPreviewCapture::BeginPlay()
{
	Super::BeginPlay();
	SetActorEnableCollision(false);
	EnsureRenderTarget();
	CaptureComponent->ShowOnlyActors.Reset();
	CaptureComponent->ShowOnlyActors.Add(this);
	CaptureComponent->ShowOnlyComponents.Reset();
}

void AMeshPreviewCapture::EnsureRenderTarget()
{
	if (!IsValid(RenderTarget))
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	}
	RenderTarget->ClearColor = ClearColor;
	RenderTarget->InitCustomFormat(
		FMath::Max(RenderTargetWidth, 16),
		FMath::Max(RenderTargetHeight, 16),
		PF_B8G8R8A8,
		false);
	RenderTarget->UpdateResourceImmediate(true);
	CaptureComponent->TextureTarget = RenderTarget;
}

void AMeshPreviewCapture::ClearMesh()
{
	if (IsValid(SkelMeshComp))
	{
		SkelMeshComp->SetSkeletalMeshAsset(nullptr);
		SkelMeshComp->SetVisibility(false);
		SkelMeshComp->SetRelativeLocation(FVector::ZeroVector);
		SkelMeshComp->SetRelativeRotation(FRotator::ZeroRotator);
		SkelMeshComp->SetRelativeScale3D(FVector::OneVector);
	}
	if (IsValid(StaticMeshComp))
	{
		StaticMeshComp->SetStaticMesh(nullptr);
		StaticMeshComp->SetVisibility(false);
		StaticMeshComp->SetRelativeLocation(FVector::ZeroVector);
		StaticMeshComp->SetRelativeRotation(FRotator::ZeroRotator);
		StaticMeshComp->SetRelativeScale3D(FVector::OneVector);
	}
	if (IsValid(Pivot))
	{
		Pivot->SetRelativeLocation(FVector::ZeroVector);
	}
}

void AMeshPreviewCapture::Capture()
{
	EnsureRenderTarget();
	if (IsValid(CaptureComponent))
	{
		CaptureComponent->CaptureScene();
	}
}

void AMeshPreviewCapture::FitCameraToComponent(USceneComponent* Comp)
{
	if (!IsValid(Comp) || !IsValid(CaptureComponent) || !IsValid(Pivot)) return;

	// 先复位，再按世界包围盒把中心拉回 Actor 原点
	Comp->SetRelativeLocation(FVector::ZeroVector);
	Comp->MarkRenderStateDirty();
	Comp->UpdateBounds();

	const FVector WorldOrigin = Comp->Bounds.Origin;
	const FVector ActorOrigin = GetActorLocation();
	Pivot->SetWorldLocation(Pivot->GetComponentLocation() + (ActorOrigin - WorldOrigin));

	Comp->UpdateBounds();
	const float Radius = FMath::Max(Comp->Bounds.SphereRadius, 1.f);
	const float Fitted = (OrthoWidth > 0.f) ? OrthoWidth : (Radius * AutoFitPadding);
	CaptureComponent->OrthoWidth = FMath::Max(Fitted, 5.f);
	CaptureComponent->SetRelativeLocation(FVector(FMath::Max(Radius * 4.f, 200.f), 0.f, 0.f));
	CaptureComponent->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
}

void AMeshPreviewCapture::FinishShow()
{
	EnsureRenderTarget();
	Capture();
}

void AMeshPreviewCapture::ShowSkeletalMesh(USkeletalMesh* Mesh, FRotator RotationOffset, float Scale)
{
	ClearMesh();
	if (!IsValid(Mesh) || !IsValid(SkelMeshComp)) return;

	SkelMeshComp->SetSkeletalMeshAsset(Mesh);
	SkelMeshComp->SetRelativeRotation(RotationOffset);
	SkelMeshComp->SetRelativeScale3D(FVector(FMath::Max(Scale, 0.01f)));
	SkelMeshComp->SetVisibility(true);
	SkelMeshComp->SetForcedLOD(0);

	FitCameraToComponent(SkelMeshComp);
	FinishShow();
}

void AMeshPreviewCapture::ShowStaticMesh(UStaticMesh* Mesh, FRotator RotationOffset, float Scale)
{
	ClearMesh();
	if (!IsValid(Mesh) || !IsValid(StaticMeshComp)) return;

	StaticMeshComp->SetStaticMesh(Mesh);
	StaticMeshComp->SetRelativeRotation(RotationOffset);
	StaticMeshComp->SetRelativeScale3D(FVector(FMath::Max(Scale, 0.01f)));
	StaticMeshComp->SetVisibility(true);

	FitCameraToComponent(StaticMeshComp);
	FinishShow();
}

bool AMeshPreviewCapture::ShowSoftPath(const FSoftObjectPath& MeshPath, FRotator RotationOffset, float Scale)
{
	if (!MeshPath.IsValid()) return false;
	UObject* Loaded = MeshPath.TryLoad();
	if (!IsValid(Loaded)) return false;

	if (USkeletalMesh* Skel = Cast<USkeletalMesh>(Loaded))
	{
		ShowSkeletalMesh(Skel, RotationOffset, Scale);
		return true;
	}
	if (UStaticMesh* Stat = Cast<UStaticMesh>(Loaded))
	{
		ShowStaticMesh(Stat, RotationOffset, Scale);
		return true;
	}
	return false;
}

bool AMeshPreviewCapture::ShowSoftAsset(TSoftObjectPtr<UStreamableRenderAsset> SoftMesh, FRotator RotationOffset, float Scale)
{
	return ShowSoftPath(SoftMesh.ToSoftObjectPath(), RotationOffset, Scale);
}

void AMeshPreviewCapture::SetViewRotation(FRotator Rotation)
{
	if (!IsValid(Pivot)) return;
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch, -60.f, 60.f);
	Rotation.Roll = 0.f;
	Pivot->SetRelativeRotation(Rotation);
	Capture();
}

FRotator AMeshPreviewCapture::GetViewRotation() const
{
	return IsValid(Pivot) ? Pivot->GetRelativeRotation() : FRotator::ZeroRotator;
}
