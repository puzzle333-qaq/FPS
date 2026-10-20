#include "Ui/LoadoutPreviewRig.h"

#include "Weapon/Weapon.h"
#include "Weapon/AttachmentData.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SkeletalMesh.h"

ALoadoutPreviewRig::ALoadoutPreviewRig()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	CaptureComponent->SetupAttachment(Root);
	// 枪身沿 +Y。相机在 +X 上看回去，正好是侧面。
	CaptureComponent->SetRelativeLocation(FVector(400.f, 0.f, 0.f));
	CaptureComponent->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	CaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
	// 竖图 512x768，正交宽度 180 时画面高度是 270，三把枪各占 90。
	CaptureComponent->OrthoWidth = 180.f;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureComponent->PostProcessBlendWeight = 0.f;
	CaptureComponent->ShowFlags.SetAtmosphere(false);
	CaptureComponent->ShowFlags.SetFog(false);
	CaptureComponent->ShowFlags.SetPostProcessing(false);
	// 没有天空时按勒克斯打光会全黑，整备预览用无光照，保证模型看得到。
	CaptureComponent->ShowFlags.SetLighting(false);

	KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	KeyLight->SetMobility(EComponentMobility::Movable);
	KeyLight->SetRelativeRotation(FRotator(-40.f, -30.f, 0.f));
	KeyLight->SetIntensity(20.f);
}

void ALoadoutPreviewRig::BeginPlay()
{
	Super::BeginPlay();

	SetActorEnableCollision(false);

	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->ClearColor = FLinearColor(0.06f, 0.07f, 0.09f, 1.f);
	RenderTarget->InitCustomFormat(FMath::Max(RenderTargetWidth, 16), FMath::Max(RenderTargetHeight, 16), PF_B8G8R8A8, false);
	RenderTarget->UpdateResourceImmediate(true);

	CaptureComponent->OrthoWidth = FMath::Max(OrthoWidth, 1.f);
	CaptureComponent->TextureTarget = RenderTarget;
	CaptureComponent->ShowOnlyActors.Add(this);
}

void ALoadoutPreviewRig::Capture()
{
	if (IsValid(CaptureComponent))
	{
		CaptureComponent->CaptureScene();
	}
}

void ALoadoutPreviewRig::ClearPreviews()
{
	for (int32 Index = PreviewComponents.Num() - 1; Index >= 0; --Index)
	{
		if (IsValid(PreviewComponents[Index]))
		{
			PreviewComponents[Index]->DestroyComponent();
		}
	}
	PreviewComponents.Reset();
}

void ALoadoutPreviewRig::ShowWeapons(const TArray<AWeapon*>& Weapons)
{
	ClearPreviews();

	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (!Weapons.IsValidIndex(Index)) continue;
		SpawnPreview(Weapons[Index], Index);
	}

	Capture();
}

void ALoadoutPreviewRig::SpawnPreview(AWeapon* Weapon, int32 Index)
{
	if (!IsValid(Weapon)) return;

	USkeletalMeshComponent* Source = Weapon->GetMesh1P();
	if (!IsValid(Source) || !Source->GetSkeletalMeshAsset())
	{
		Source = Weapon->GetMesh3P();
	}
	if (!IsValid(Source) || !Source->GetSkeletalMeshAsset()) return;

	USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(this);
	Mesh->SetSkeletalMeshAsset(Source->GetSkeletalMeshAsset());
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetForcedLOD(1);
	const int32 MaterialCount = Source->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		Mesh->SetMaterial(MaterialIndex, Source->GetMaterial(MaterialIndex));
	}

	// 原点偏在枪托一侧，往 -Y 挪一点让枪身落在格子中间。上中下各隔 90。
	const float Height = (1 - Index) * RowSpacing;
	Mesh->SetupAttachment(GetRootComponent());
	// Per-weapon preview transform (AWeapon::PreviewRotationOffset / PreviewLocationOffset / PreviewScale).
	Mesh->SetRelativeLocation(Weapon->PreviewLocationOffset + FVector(0.f, 0.f, Height));
	Mesh->SetRelativeRotation(Weapon->PreviewRotationOffset);
	Mesh->SetRelativeScale3D(FVector(FMath::Max(Weapon->PreviewScale, 0.01f)));
	Mesh->RegisterComponent();
	PreviewComponents.Add(Mesh);

	struct FPreviewPiece
	{
		UAttachmentData* Data = nullptr;
		FName SocketName = NAME_None;
	};

	TArray<FPreviewPiece> Pending;
	for (UAttachmentData* Data : Weapon->GetEquippedAttachments())
	{
		if (!IsValid(Data) || !IsValid(Weapon->ResolveAttachmentVisualMesh(Data))) continue;

		FPreviewPiece Piece;
		Piece.Data = Data;
		Piece.SocketName = Weapon->ResolveAttachmentSocket(Data);
		Pending.Add(Piece);
	}

	TArray<UStaticMeshComponent*> SpawnedAttachments;
	auto FindMount = [&Mesh, &SpawnedAttachments](FName SocketName) -> USceneComponent*
	{
		if (SocketName.IsNone() || Mesh->DoesSocketExist(SocketName))
		{
			return Mesh;
		}
		for (UStaticMeshComponent* Existing : SpawnedAttachments)
		{
			if (IsValid(Existing) && Existing->DoesSocketExist(SocketName))
			{
				return Existing;
			}
		}
		return nullptr;
	};

	auto SpawnPiece = [this, Weapon, &SpawnedAttachments](const FPreviewPiece& Piece, USceneComponent* Mount)
	{
		UStaticMesh* VisualMesh = Weapon->ResolveAttachmentVisualMesh(Piece.Data);
		if (!IsValid(VisualMesh)) return;

		UStaticMeshComponent* AttachmentMesh = NewObject<UStaticMeshComponent>(this);
		AttachmentMesh->SetStaticMesh(VisualMesh);
		AttachmentMesh->SetMobility(EComponentMobility::Movable);
		AttachmentMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AttachmentMesh->SetCastShadow(false);
		AttachmentMesh->SetupAttachment(Mount, Piece.SocketName);
		AttachmentMesh->SetRelativeTransform(Piece.Data->MeshOffset);
		AttachmentMesh->RegisterComponent();
		PreviewComponents.Add(AttachmentMesh);
		SpawnedAttachments.Add(AttachmentMesh);
	};

	// 和枪上的生成顺序一样：先挂枪，再挂护木这类已经生成的配件。
	bool bProgress = true;
	while (Pending.Num() > 0 && bProgress)
	{
		bProgress = false;
		for (int32 PieceIndex = 0; PieceIndex < Pending.Num();)
		{
			USceneComponent* Mount = FindMount(Pending[PieceIndex].SocketName);
			if (!IsValid(Mount))
			{
				++PieceIndex;
				continue;
			}
			SpawnPiece(Pending[PieceIndex], Mount);
			Pending.RemoveAt(PieceIndex);
			bProgress = true;
		}
	}

	for (const FPreviewPiece& Piece : Pending)
	{
		SpawnPiece(Piece, Mesh);
	}
}
