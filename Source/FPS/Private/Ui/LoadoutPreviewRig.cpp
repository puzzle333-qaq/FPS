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

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Root);

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
	SlotComponents.Reset();
	MountComponents.Reset();
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
	Mesh->SetupAttachment(IsValid(Pivot) ? Pivot.Get() : GetRootComponent());
	// Per-weapon preview transform (AWeapon::PreviewRotationOffset / PreviewLocationOffset / PreviewScale).
	Mesh->SetRelativeLocation(Weapon->PreviewLocationOffset + FVector(0.f, 0.f, Height));
	Mesh->SetRelativeRotation(Weapon->PreviewRotationOffset);
	Mesh->SetRelativeScale3D(FVector(FMath::Max(Weapon->PreviewScale, 0.01f)));
	Mesh->RegisterComponent();
	PreviewComponents.Add(Mesh);
	MountComponents.Add(Mesh);

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
		MountComponents.Add(AttachmentMesh);
		SlotComponents.Add(Piece.Data->SlotTag, AttachmentMesh);
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

void ALoadoutPreviewRig::SetViewRotation(const FRotator& Rotation)
{
	if (!IsValid(Pivot)) return;
	FRotator R = Rotation;
	R.Pitch = FMath::Clamp(R.Pitch, -60.f, 60.f);
	R.Roll = 0.f;
	Pivot->SetRelativeRotation(R);
	Capture();
}

FRotator ALoadoutPreviewRig::GetViewRotation() const
{
	return IsValid(Pivot) ? Pivot->GetRelativeRotation() : FRotator::ZeroRotator;
}

void ALoadoutPreviewRig::SetViewOrthoWidth(float Width)
{
	if (!IsValid(CaptureComponent)) return;
	CaptureComponent->OrthoWidth = FMath::Max(Width, 5.f);
	Capture();
}

float ALoadoutPreviewRig::GetViewOrthoWidth() const
{
	return IsValid(CaptureComponent) ? CaptureComponent->OrthoWidth : OrthoWidth;
}

void ALoadoutPreviewRig::SetViewPan(const FVector2D& PanYZ)
{
	if (!IsValid(CaptureComponent)) return;
	CaptureComponent->SetRelativeLocation(FVector(400.f, PanYZ.X, PanYZ.Y));
	Capture();
}

FVector2D ALoadoutPreviewRig::GetViewPan() const
{
	if (!IsValid(CaptureComponent)) return FVector2D::ZeroVector;
	const FVector L = CaptureComponent->GetRelativeLocation();
	return FVector2D(L.Y, L.Z);
}

FVector2D ALoadoutPreviewRig::WorldToPan(const FVector& World) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(World);
	return FVector2D(Local.Y, Local.Z);
}

bool ALoadoutPreviewRig::ProjectWorldToUV(const FVector& World, FVector2D& OutUV) const
{
	if (!IsValid(CaptureComponent)) return false;
	// 正交相机：相机空间 X 朝前，Y 朝右，Z 朝上
	const FVector Cam = CaptureComponent->GetComponentTransform().InverseTransformPosition(World);
	const float W = FMath::Max(CaptureComponent->OrthoWidth, 1.f);
	const float Aspect = RenderTargetHeight > 0 ? float(RenderTargetWidth) / float(RenderTargetHeight) : 1.f;
	const float H = W / FMath::Max(Aspect, 0.01f);
	OutUV = FVector2D(0.5f + Cam.Y / W, 0.5f - Cam.Z / H);
	return OutUV.X >= 0.f && OutUV.X <= 1.f && OutUV.Y >= 0.f && OutUV.Y <= 1.f;
}

bool ALoadoutPreviewRig::GetSlotWorldLocation(const AWeapon* Weapon, const FGameplayTag& SlotTag, FVector& OutWorld) const
{
	if (const TObjectPtr<USceneComponent>* Found = SlotComponents.Find(SlotTag))
	{
		if (IsValid(*Found))
		{
			OutWorld = (*Found)->Bounds.Origin;
			return true;
		}
	}
	if (!IsValid(Weapon)) return false;
	FName SocketName = NAME_None;
	for (const FWeaponAttachmentSlot& Slot : Weapon->GetAttachmentSlots())
	{
		if (Slot.SlotTag.MatchesTagExact(SlotTag)) { SocketName = Slot.SocketName; break; }
	}
	if (SocketName.IsNone()) return false;
	for (const TObjectPtr<USceneComponent>& Comp : MountComponents)
	{
		if (IsValid(Comp) && Comp->DoesSocketExist(SocketName))
		{
			OutWorld = Comp->GetSocketLocation(SocketName);
			return true;
		}
	}
	return false;
}