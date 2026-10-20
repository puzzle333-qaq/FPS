


#include "Weapon/Weapon.h"
#include "CombatComponent.h"
#include "FPSTraceChannels.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interface/PlayerInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "KismetTraceUtils.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMeshSocket.h"
#include "ShooterCharacter.h"
#include "ShooterGamePlayTags.h"
#include "UObject/UnrealType.h"

namespace
{
	const FName ReserveMagazineSocket(TEXT("SOCKET_Magazine_Reserve"));

	// 换弹会同时动两根弹夹骨：枪里的 SOCKET_Magazine，和送进去的 SOCKET_Magazine_Reserve。
	// 备用那只平时藏着。SetMeshVisibility 会把隐藏标记传播到子组件，这里再盖回去。
	void HideReserveMagazines(const AActor* Weapon)
	{
		if (!IsValid(Weapon)) return;

		TArray<UStaticMeshComponent*> Meshes;
		Weapon->GetComponents<UStaticMeshComponent>(Meshes);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (!IsValid(Mesh) || Mesh->GetAttachSocketName() != ReserveMagazineSocket) continue;
			Mesh->SetHiddenInGame(true);
			Mesh->SetVisibility(false);
		}
	}

	void DestroyReserveMagazines(AActor* Weapon)
	{
		if (!IsValid(Weapon)) return;

		TArray<UStaticMeshComponent*> Meshes;
		Weapon->GetComponents<UStaticMeshComponent>(Meshes);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (IsValid(Mesh) && Mesh->GetAttachSocketName() == ReserveMagazineSocket)
			{
				Mesh->DestroyComponent();
			}
		}
	}

	bool ComponentHasSocket(const USceneComponent* Component, FName SocketName)
	{
		return IsValid(Component) && !SocketName.IsNone() && Component->DoesSocketExist(SocketName);
	}

	// X24 枪身没有能对上 SK_X24 的蒙太奇插槽。套筒和空仓换弹直接播序列，播完停在最后一帧。
	void PlayX24BodySequence(USkeletalMeshComponent* Mesh, const TCHAR* SequencePath)
	{
		if (!IsValid(Mesh) || !SequencePath) return;
		const UWorld* World = Mesh->GetWorld();
		if (World && World->GetNetMode() == NM_DedicatedServer) return;

		UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, SequencePath);
		if (!IsValid(Sequence)) return;
		Mesh->PlayAnimation(Sequence, false);
	}

	void PlayX24Shot(USkeletalMeshComponent* Mesh)
	{
		static const TCHAR* Shots[] = {
			TEXT("/Game/FPS_Controller/Animations/WeaponsAnims/Pistol_X24/A_X24_Body_Shot_01.A_X24_Body_Shot_01"),
			TEXT("/Game/FPS_Controller/Animations/WeaponsAnims/Pistol_X24/A_X24_Body_Shot_02.A_X24_Body_Shot_02"),
			TEXT("/Game/FPS_Controller/Animations/WeaponsAnims/Pistol_X24/A_X24_Body_Shot_03.A_X24_Body_Shot_03"),
		};
		PlayX24BodySequence(Mesh, Shots[FMath::RandRange(0, UE_ARRAY_COUNT(Shots) - 1)]);
	}

	// 静态网格优先。X24 消音器、手电、红点是骨骼模型，没有静态网格时用骨骼那只。
	USceneComponent* AttachmentViewComponent(const FAttachmentVisual& Visual, bool bFirstPerson)
	{
		if (bFirstPerson)
		{
			if (IsValid(Visual.Mesh1P)) return Visual.Mesh1P.Get();
			return Visual.SkelMesh1P.Get();
		}
		if (IsValid(Visual.Mesh3P)) return Visual.Mesh3P.Get();
		return Visual.SkelMesh3P.Get();
	}

	// 槽位 Socket 先挂枪。枪上没有时，挂到已经生成、并且自己带这个 Socket 的配件上。
	USceneComponent* FindAttachmentMount(USkeletalMeshComponent* WeaponMesh, const TArray<FAttachmentVisual>& Visuals, bool bFirstPerson, FName SocketName)
	{
		if (!IsValid(WeaponMesh))
		{
			return nullptr;
		}
		if (SocketName.IsNone() || WeaponMesh->DoesSocketExist(SocketName))
		{
			return WeaponMesh;
		}

		for (const FAttachmentVisual& Visual : Visuals)
		{
			UStaticMeshComponent* Mesh = bFirstPerson ? Visual.Mesh1P.Get() : Visual.Mesh3P.Get();
			if (ComponentHasSocket(Mesh, SocketName))
			{
				return Mesh;
			}
			USkeletalMeshComponent* Skel = bFirstPerson ? Visual.SkelMesh1P.Get() : Visual.SkelMesh3P.Get();
			if (IsValid(Skel) && (SocketName.IsNone() || Skel->DoesSocketExist(SocketName)))
			{
				return Skel;
			}
		}

		return nullptr;
	}
}

AWeapon::AWeapon()
{
	// 只有激光开着时才 Tick（第 11 步），放在动画和开镜对齐之后
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;

	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>("Mesh1P");
	Mesh1P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Mesh1P->bReceivesDecals = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetHiddenInGame(true);
	SetRootComponent(Mesh1P);

	Mesh3P = CreateDefaultSubobject<USkeletalMeshComponent>("Mesh3P");
	Mesh3P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Mesh3P->bReceivesDecals = false;
	Mesh3P->CastShadow = true;
	Mesh3P->SetupAttachment(Mesh1P);
	Mesh3P->SetHiddenInGame(true);

	AimFeildOfView = 60.f;
	AimDownSightTime = 0.2f;
	AimSocketName = TEXT("AimSocket");
	TraceRadius = 5.f;
	FireTime = 0.1f;
	// Sample spray: climb, drift right, then swing left, then back toward center.
	RecoilPattern = {
		FRecoilStep(0.48f,  0.00f),
		FRecoilStep(0.55f,  0.10f),
		FRecoilStep(0.52f,  0.22f),
		FRecoilStep(0.46f,  0.30f),
		FRecoilStep(0.40f,  0.16f),
		FRecoilStep(0.38f, -0.08f),
		FRecoilStep(0.36f, -0.26f),
		FRecoilStep(0.34f, -0.34f),
		FRecoilStep(0.32f, -0.12f),
		FRecoilStep(0.30f,  0.14f),
		FRecoilStep(0.28f,  0.22f),
		FRecoilStep(0.26f,  0.06f)
	};
	RecoilPitchScale = 1.f;
	RecoilYawScale = 1.f;
	AimRecoilMultiplier = 0.65f;
	RecoilRecoverySpeed = 18.f;
	RecoilRecoveryDelay = 0.12f;
	RecoilResetTime = 0.4f;
	VisualRecoilLocation = FVector(1.5f, 0.2f, 0.3f);
	VisualRecoilRotation = FRotator(3.f, 2.f, 1.5f);   // FRotator(Pitch, Yaw, Roll)
	VisualRecoilAimMultiplier = 0.4f;
	VisualRecoilKickSpeed = 30.f;
	VisualRecoilRecoverySpeed = 10.f;
	Damage = 15.f;
	HeadshotMultiplier = 2.f;
	LimbMultiplier = 0.75f;

	MagCapacity = 10;
	Ammo = 5;
	StartingCarriedAmmo = 10;
	Sequence = 0;
	WeaponStatus = EWeaponStatus::UnEquipped;
	WeaponKind = EWeaponKind::LongGun;

}

void AWeapon::OnRep_Instigator()
{
	Super::OnRep_Instigator();

	// Authority never runs this. Clients used to attach every replicated gun, so the
	// holstered rifle showed up beside the equipped pistol. Only the current weapon attaches.
	APawn* OwningPawn = GetInstigator();
	UCombatComponent* Combat = UCombatComponent::FindCombatComponent(OwningPawn);
	if (IsValid(Combat) && Combat->CurrentWeapon == this)
	{
		SetActorHiddenInGame(false);
		AttachToOwingPawn();
		return;
	}

	DetachFromPawn();
	SetActorHiddenInGame(true);
	WeaponStatus = EWeaponStatus::UnEquipped;
}

USkeletalMeshComponent* AWeapon::GetMesh1P() const
{
	return Mesh1P;
}

bool AWeapon::HasAimSocket() const
{
	FVector Unused;
	return GetAimSocketWorldLocation(Unused);
}

bool AWeapon::GetAimSocketWorldLocation(FVector& OutLocation) const
{
	// 1) 瞄具 SightSocket：开镜对准红点/全息中心
	if (const FAttachmentVisual* Optic = FindVisual(ShooterTags::Tag_Attachment_Slot_Optic))
	{
		USceneComponent* OpticComp =
			IsValid(Optic->Mesh1P)
			? static_cast<USceneComponent*>(Optic->Mesh1P.Get())
			: static_cast<USceneComponent*>(Optic->SkelMesh1P.Get());
		if (IsValid(OpticComp) && IsValid(Optic->Data)
			&& !Optic->Data->SightSocketName.IsNone()
			&& OpticComp->DoesSocketExist(Optic->Data->SightSocketName))
		{
			OutLocation = OpticComp->GetSocketLocation(Optic->Data->SightSocketName);
			return true;
		}
	}

	// 2) 机瞄 AimSocket 可能在配件上（常见：枪管）。先枪管，再其它配件的 1P 模型
	if (!AimSocketName.IsNone())
	{
		auto TryAttachmentAim = [this, &OutLocation](const FGameplayTag& SlotTag) -> bool
		{
			const FAttachmentVisual* Visual = FindVisual(SlotTag);
			USceneComponent* AimComp =
				Visual
				? (IsValid(Visual->Mesh1P)
					? static_cast<USceneComponent*>(Visual->Mesh1P.Get())
					: static_cast<USceneComponent*>(Visual->SkelMesh1P.Get()))
				: nullptr;
			if (!Visual || !IsValid(AimComp) || !AimComp->DoesSocketExist(AimSocketName))
			{
				return false;
			}
			OutLocation = AimComp->GetSocketLocation(AimSocketName);
			return true;
		};

		if (TryAttachmentAim(ShooterTags::Tag_Attachment_Slot_Barrel))
		{
			return true;
		}

		for (const FAttachmentVisual& Visual : AttachmentVisuals)
		{
			USceneComponent* AimComp =
				IsValid(Visual.Mesh1P)
				? static_cast<USceneComponent*>(Visual.Mesh1P.Get())
				: static_cast<USceneComponent*>(Visual.SkelMesh1P.Get());
			if (!IsValid(Visual.Data) || !IsValid(AimComp))
			{
				continue;
			}
			// 枪管已查过；瞄具走 SightSocket，这里跳过
			if (Visual.Data->SlotTag == ShooterTags::Tag_Attachment_Slot_Barrel
				|| Visual.Data->SlotTag == ShooterTags::Tag_Attachment_Slot_Optic)
			{
				continue;
			}
			if (AimComp->DoesSocketExist(AimSocketName))
			{
				OutLocation = AimComp->GetSocketLocation(AimSocketName);
				return true;
			}
		}
	}

	// 3) 退回枪 Mesh1P 上的 AimSocket
	if (!IsValid(Mesh1P) || AimSocketName.IsNone() || !Mesh1P->DoesSocketExist(AimSocketName))
	{
		return false;
	}

	OutLocation = Mesh1P->GetSocketLocation(AimSocketName);
	return true;
}

bool AWeapon::GetFabrikWorldTransform(bool bFirstPerson, FTransform& OutWorld) const
{
	USkeletalMeshComponent* WeaponMesh = bFirstPerson ? Mesh1P : Mesh3P;
	// 隐藏的枪不 Tick。读 Socket 前先刷新骨骼，握把才跟着枪走。
	if (IsValid(WeaponMesh))
	{
		WeaponMesh->RefreshBoneTransforms();
		WeaponMesh->UpdateComponentToWorld();
	}

	if (const FAttachmentVisual* Grip = FindVisual(ShooterTags::Tag_Attachment_Slot_Grip))
	{
		const FName SocketName = IsValid(Grip->Data) ? Grip->Data->FabrikSocketName : NAME_None;
		UStaticMeshComponent* GripMesh = bFirstPerson ? Grip->Mesh1P.Get() : Grip->Mesh3P.Get();
		const USceneComponent* GripParent = IsValid(GripMesh) ? GripMesh->GetAttachParent() : nullptr;
		const FName MountSocket = IsValid(GripMesh) ? GripMesh->GetAttachSocketName() : NAME_None;
		// 握把没挂到真实插槽上时，它的 FABRIK 在枪根，不能拿来改扶手。
		const bool bGripMounted = IsValid(GripParent) && (MountSocket.IsNone() || GripParent->DoesSocketExist(MountSocket));
		if (bGripMounted && ComponentHasSocket(GripMesh, SocketName))
		{
			GripMesh->UpdateComponentToWorld();
			OutWorld = GripMesh->GetSocketTransform(SocketName, RTS_World);
			return true;
		}
	}

	if (!ComponentHasSocket(WeaponMesh, TEXT("FABRIK")))
	{
		return false;
	}

	OutWorld = WeaponMesh->GetSocketTransform(TEXT("FABRIK"), RTS_World);
	return true;
}

FName AWeapon::GetFallbackMuzzleSocketName() const
{
	if (const FNameProperty* Property = FindFProperty<FNameProperty>(GetClass(), TEXT("MuzzleSocketName")))
	{
		const FName AuthoredName = Property->GetPropertyValue_InContainer(this);
		if (!AuthoredName.IsNone())
		{
			return AuthoredName;
		}
	}

	return TEXT("Muzzle");
}

bool AWeapon::TryAttachmentMuzzle(const FGameplayTag& SlotTag, bool bFirstPerson, USceneComponent*& OutComponent, FName& OutSocketName) const
{
	const FAttachmentVisual* Visual = FindVisual(SlotTag);
	if (!Visual || !IsValid(Visual->Data))
	{
		return false;
	}

	USceneComponent* Mesh = AttachmentViewComponent(*Visual, bFirstPerson);
	// 先用数据里填的名字，再试常见命名（Infima 枪管是 SOCKET_Muzzle，消音器是 SOCKET_Emitter）
	const FName Candidates[] = { Visual->Data->MuzzleSocketName, TEXT("SOCKET_Muzzle"), TEXT("SOCKET_Emitter"), TEXT("Muzzle"), TEXT("MuzzleFlash") };
	for (const FName& Candidate : Candidates)
	{
		if (!Candidate.IsNone() && ComponentHasSocket(Mesh, Candidate))
		{
			OutComponent = Mesh;
			OutSocketName = Candidate;
			return true;
		}
	}
	return false;
}

bool AWeapon::GetMuzzleAttachPoint(bool bFirstPerson, USceneComponent*& OutComponent, FName& OutSocketName) const
{
	OutComponent = nullptr;
	OutSocketName = NAME_None;

	// 消音器在枪管前面，所以枪口配件优先于枪管。
	if (TryAttachmentMuzzle(ShooterTags::Tag_Attachment_Slot_Muzzle, bFirstPerson, OutComponent, OutSocketName)
		|| TryAttachmentMuzzle(ShooterTags::Tag_Attachment_Slot_Barrel, bFirstPerson, OutComponent, OutSocketName))
	{
		return true;
	}

	USkeletalMeshComponent* WeaponMesh = bFirstPerson ? Mesh1P : Mesh3P;
	if (!IsValid(WeaponMesh))
	{
		return false;
	}

	OutComponent = WeaponMesh;
	OutSocketName = GetFallbackMuzzleSocketName();
	if (!ComponentHasSocket(WeaponMesh, OutSocketName))
	{
		for (const FName Candidate : { FName(TEXT("Muzzle")), FName(TEXT("SOCKET_Muzzle")), FName(TEXT("MuzzleFlash")) })
		{
			if (ComponentHasSocket(WeaponMesh, Candidate)) { OutSocketName = Candidate; break; }
		}
	}
	return !OutSocketName.IsNone();
}

USkeletalMeshComponent* AWeapon::GetMesh3P() const
{
	return Mesh3P;
}

UMaterialInstanceDynamic* AWeapon::GetReticleDynamicMaterialInstance() 
{
	if (!IsValid(DynMatInst_Reticle))
	{
		DynMatInst_Reticle = UMaterialInstanceDynamic::Create(ReticleMaterial, this);
	}
	return DynMatInst_Reticle;
}

void AWeapon::CaptureAuthoredMeshRelative()
{
	if (bCapturedAuthoredRelative) return;

	AuthoredMesh1PRelative = Mesh1P->GetRelativeTransform();
	AuthoredMesh3PRelative = Mesh3P->GetRelativeTransform();
	bCapturedAuthoredRelative = true;
}


namespace
{
	bool IsUsableAttachPoint(const USkeletalMeshComponent* Mesh, FName SocketOrBone)
	{
		if (!IsValid(Mesh) || SocketOrBone.IsNone())
		{
			return false;
		}
		// Prefer real sockets. Orphan bones (e.g. Simple weapon_r at root) are unusable.
		if (Mesh->GetSocketByName(SocketOrBone) != nullptr)
		{
			return true;
		}
		const int32 BoneIdx = Mesh->GetBoneIndex(SocketOrBone);
		if (BoneIdx == INDEX_NONE)
		{
			return false;
		}
		const FName Parent = Mesh->GetParentBone(SocketOrBone);
		if (Parent.IsNone() && BoneIdx != 0)
		{
			return false;
		}
		return true;
	}

	FName ResolveAttachPoint(USkeletalMeshComponent* Mesh, FName Requested)
	{
		if (IsUsableAttachPoint(Mesh, Requested))
		{
			return Requested;
		}
		static const FName Fallbacks[] = {
			FName(TEXT("RifleSocketak")), FName(TEXT("RifleSocket")),
			FName(TEXT("hand_r")), FName(TEXT("HandGrip_R"))
		};
		for (const FName& Candidate : Fallbacks)
		{
			if (Candidate != Requested && IsUsableAttachPoint(Mesh, Candidate))
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[WeaponAttach] '%s' unusable on %s. Falling back to '%s'."),
					*Requested.ToString(), *GetNameSafe(Mesh->GetSkeletalMeshAsset()), *Candidate.ToString());
				return Candidate;
			}
		}
		UE_LOG(LogTemp, Error,
			TEXT("[WeaponAttach] No usable attach point for '%s' on %s."),
			*Requested.ToString(), *GetNameSafe(Mesh->GetSkeletalMeshAsset()));
		return Requested;
	}
}

void AWeapon::AttachToOwingPawn()
{
	APawn* OwningPawn = GetInstigator();
	if (!IsValid(OwningPawn) || !OwningPawn->Implements<UPlayerInterface>()) return;

	CaptureAuthoredMeshRelative();
	SetMeshVisibility(OwningPawn);

	const FName AttachPoint = IPlayerInterface::Execute_GetWeaponAttachPoint(OwningPawn, WeaponType);
	USkeletalMeshComponent* PawnMesh1P = IPlayerInterface::Execute_GetMesh1P(OwningPawn);
	USkeletalMeshComponent* PawnMesh3P = IPlayerInterface::Execute_GetMesh3P(OwningPawn);
	if (!IsValid(PawnMesh1P) || !IsValid(PawnMesh3P)) return;

	// 1P: GripPoints (AK = weapon_r on arms). 3P AK: always RifleSocketak + FABRIK (not weapon_r).
	const FName Attach1P = ResolveAttachPoint(PawnMesh1P, AttachPoint);
	FName Attach3P = AttachPoint;
	if (WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_AK))
	{
		static const FName RifleSocketAkName(TEXT("RifleSocketak"));
		Attach3P = IsUsableAttachPoint(PawnMesh3P, RifleSocketAkName)
			? RifleSocketAkName
			: ResolveAttachPoint(PawnMesh3P, FName(TEXT("RifleSocket")));
	}
	else
	{
		Attach3P = ResolveAttachPoint(PawnMesh3P, AttachPoint);
	}

	const FAttachmentTransformRules SnapRules = FAttachmentTransformRules::SnapToTargetNotIncludingScale;
	Mesh1P->AttachToComponent(PawnMesh1P, SnapRules, Attach1P);
	Mesh1P->SetRelativeTransform(AuthoredMesh1PRelative);

	Mesh3P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh3P->AttachToComponent(PawnMesh3P, SnapRules, Attach3P);
	Mesh3P->SetRelativeTransform(AuthoredMesh3PRelative);
	Mesh3P->RefreshBoneTransforms();

	UE_LOG(LogTemp, Log,
		TEXT("[WeaponAttach] %s type=%s grip=%s -> 1P sock=%s | 3P sock=%s"),
		*GetName(), *WeaponType.ToString(), *AttachPoint.ToString(),
		*Mesh1P->GetAttachSocketName().ToString(),
		*Mesh3P->GetAttachSocketName().ToString());

	UpdateFlashlightAttachment();
	UpdateLaserAttachment();
	ForceChamberSocketBones(Mesh1P);
	ForceChamberSocketBones(Mesh3P);
	BindChamberShellNotifies();
	ApplyChamberShellVisibility();
}

void AWeapon::AttachToPawnSocket(USkeletalMeshComponent* PawnMesh, FName SocketName)
{
	if (!IsValid(PawnMesh) || SocketName.IsNone())
	{
		return;
	}

	APawn* OwningPawn = GetInstigator();
	if (!IsValid(OwningPawn) || !OwningPawn->Implements<UPlayerInterface>())
	{
		return;
	}

	USkeletalMeshComponent* WeaponMesh = nullptr;
	if (PawnMesh == IPlayerInterface::Execute_GetMesh1P(OwningPawn))
	{
		WeaponMesh = Mesh1P;
	}
	else if (PawnMesh == IPlayerInterface::Execute_GetMesh3P(OwningPawn))
	{
		WeaponMesh = Mesh3P;
	}
	if (!IsValid(WeaponMesh))
	{
		return;
	}

	const bool bHasSocket = PawnMesh->DoesSocketExist(SocketName);
	const bool bHasBone = PawnMesh->GetBoneIndex(SocketName) != INDEX_NONE;
	if (!bHasSocket && !bHasBone)
	{
		UE_LOG(LogTemp, Warning, TEXT("Grip '%s' is missing on %s."), *SocketName.ToString(), *GetNameSafe(PawnMesh));
		return;
	}

	// Keep the gun where it already is on this frame, then follow the new hand.
	const FAttachmentTransformRules KeepWorld(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);
	WeaponMesh->AttachToComponent(PawnMesh, KeepWorld, SocketName);
}

void AWeapon::SetHiddenForHeal(bool bHideWeapon)
{
	if (bHideWeapon)
	{
		if (IsValid(Mesh1P))
		{
			Mesh1P->SetHiddenInGame(true, true);
		}
		if (IsValid(Mesh3P))
		{
			Mesh3P->SetHiddenInGame(true, true);
		}
		return;
	}

	SetMeshVisibility(GetInstigator());
	// 父网格 SetHiddenInGame(propagate) 会改掉手电子组件的隐藏标记，这里按开关状态写回去
	ApplyFlashlightState();
	ApplyLaserState();
}

void AWeapon::DetachFromPawn()
{
	if (IsValid(Mesh3P))
	{
		Mesh3P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		if (Mesh3P->GetAttachParent())
		{
			Mesh3P->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		}
	}

	if (IsValid(Mesh1P) && Mesh1P->GetAttachParent())
	{
		Mesh1P->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	}
}

void AWeapon::WeaponTrace(FHitResult& OutHit, float TraceLength)
{
	APawn* InstigatorPawn = GetInstigator();
	APlayerController* PC = IsValid(InstigatorPawn) ? Cast<APlayerController>(InstigatorPawn->GetController()) : nullptr;
	if (!IsValid(PC))
	{
		return;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.bReturnPhysicalMaterial = true;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(InstigatorPawn);

	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse.SetAllChannels(ECR_Ignore);
	ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Block);
	ResponseParams.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);
	ResponseParams.CollisionResponse.SetResponse(ECC_WorldDynamic, ECR_Block);
	ResponseParams.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Block);

	// 用玩家实际看到的镜头（含探头横移/侧倾、开镜位置）做射线起点和方向。
	// 以前用 GetActorEyesViewPoint = 胶囊眼高 + 控制旋转，探头时子弹还从没探头的中心打出去。
	FVector EyesWorldLocation;
	FRotator EyesWorldRotation;
	const UCameraComponent* ViewCamera = InstigatorPawn->FindComponentByClass<UCameraComponent>();
	if (IsValid(ViewCamera) && ViewCamera->IsActive())
	{
		EyesWorldLocation = ViewCamera->GetComponentLocation();
		EyesWorldRotation = ViewCamera->GetComponentRotation();
	}
	else
	{
		PC->GetPlayerViewPoint(EyesWorldLocation, EyesWorldRotation);
	}
	FVector EyesWorldDirection = UKismetMathLibrary::GetForwardVector(EyesWorldRotation);
	const float SpreadDeg = GetHipFireSpread();
	if (SpreadDeg > 0.f)
	{
		EyesWorldDirection = FMath::VRandCone(EyesWorldDirection, FMath::DegreesToRadians(SpreadDeg));
	}

	const FVector Start = EyesWorldLocation;
	const FVector End = Start + EyesWorldDirection * TraceLength;

	const bool bHit = GetWorld()->SweepSingleByChannel(
		OutHit,
		Start,
		End,
		FQuat::Identity,
		FPSTraceChannels::ECC_Weapon,
		FCollisionShape::MakeSphere(TraceRadius),
		QueryParams,
		ResponseParams);

	// A miss leaves ImpactPoint at the origin. TraceEnd is the far point of the sweep.
	if (!bHit)
	{
		OutHit.Location = End;
		OutHit.ImpactPoint = End;
		OutHit.Normal = -EyesWorldDirection;
		OutHit.ImpactNormal = -EyesWorldDirection;
		OutHit.Distance = TraceLength;
		OutHit.Time = 1.f;
	}

}


void AWeapon::WeaponMeleeHit(FHitResult& OutHit)
{
	OutHit = FHitResult();
	APawn* InstigatorPawn = GetInstigator();
	if (!IsValid(InstigatorPawn) || !GetWorld())
	{
		return;
	}

	USkeletalMeshComponent* BladeMesh = nullptr;
	if (IsValid(Mesh1P) && !Mesh1P->bHiddenInGame)
	{
		BladeMesh = Mesh1P;
	}
	else if (IsValid(Mesh3P))
	{
		BladeMesh = Mesh3P;
	}
	if (!IsValid(BladeMesh))
	{
		return;
	}

	FVector Start = BladeMesh->GetComponentLocation();
	FVector Forward = BladeMesh->GetForwardVector();
	if (!MeleeSocketName.IsNone() && BladeMesh->DoesSocketExist(MeleeSocketName))
	{
		const FTransform SocketXform = BladeMesh->GetSocketTransform(MeleeSocketName, RTS_World);
		Start = SocketXform.GetLocation();
		Forward = SocketXform.GetRotation().GetForwardVector();
	}
	const FVector End = Start + Forward * MeleeHitReach;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponMeleeHit), false, this);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(InstigatorPawn);

	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse.SetAllChannels(ECR_Ignore);
	ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Block);
	ResponseParams.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);
	ResponseParams.CollisionResponse.SetResponse(ECC_WorldDynamic, ECR_Block);
	ResponseParams.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Block);

	const bool bHit = GetWorld()->SweepSingleByChannel(
		OutHit,
		Start,
		End,
		FQuat::Identity,
		FPSTraceChannels::ECC_Weapon,
		FCollisionShape::MakeSphere(MeleeHitRadius),
		QueryParams,
		ResponseParams);

	if (!bHit)
	{
		OutHit.Location = End;
		OutHit.ImpactPoint = End;
		OutHit.Normal = -Forward;
		OutHit.ImpactNormal = -Forward;
		OutHit.Distance = MeleeHitReach;
		OutHit.Time = 1.f;
	}
}

void AWeapon::ComputeThrowLaunch(bool bHighThrow, FVector& OutLocation, FVector& OutDirection, float& OutSpeed) const
{
	OutLocation = GetActorLocation();
	OutDirection = GetActorForwardVector();
	OutSpeed = bHighThrow ? HighThrowSpeed : LowThrowSpeed;

	APawn* OwningPawn = GetInstigator();
	if (!IsValid(OwningPawn))
	{
		return;
	}

	FVector EyeLoc;
	FRotator EyeRot;
	OwningPawn->GetActorEyesViewPoint(EyeLoc, EyeRot);
	EyeRot.Pitch += bHighThrow ? HighThrowPitchOffset : LowThrowPitchOffset;
	OutDirection = EyeRot.Vector();
	OutLocation = EyeLoc + OutDirection * ThrowSpawnForward;
	if (!bHighThrow)
	{
		OutLocation.Z -= 20.f;
	}
	OutSpeed = bHighThrow ? HighThrowSpeed : LowThrowSpeed;
}

void AWeapon::Local_Fire(const FVector& ImpactPoint, const FVector& ImpactNormal, TEnumAsByte<EPhysicalSurface> ImpactSurfaceType, bool bIsFirstPerson)
{
	if (WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_X24))
	{
		PlayX24Shot(bIsFirstPerson ? Mesh1P : Mesh3P);
	}

	FireEffect(ImpactPoint, ImpactNormal, ImpactSurfaceType, bIsFirstPerson);

	// Predicted mag decrement happens only on the owning first-person fire path.
	// Third-person multicast is FX only; otherwise listen-server ammo is deducted twice.
	if (!bIsFirstPerson) return;

	APawn* InstigatorPawn = GetInstigator();
	if (IsValid(InstigatorPawn) && InstigatorPawn->IsLocallyControlled() && UsesAmmo())
	{
		Ammo = FMath::Clamp(Ammo - 1, 0, MagCapacity);
		if (!GetInstigator()->HasAuthority())
		{
			++Sequence;
		}
	}
}

void AWeapon::Auth_Fire()
{
	if (!UsesAmmo()) { return; }
	Ammo = FMath::Clamp(Ammo - 1, 0, MagCapacity);
}

void AWeapon::Rep_Fire(int32 AuthAmmo)
{
	APawn* InstigatorPawn = GetInstigator();
	if (!IsValid(InstigatorPawn) || !InstigatorPawn->IsLocallyControlled() && !GetInstigator()->HasAuthority()) return;

	Sequence = FMath::Max(Sequence - 1, 0);
	Ammo = FMath::Clamp(AuthAmmo - Sequence, 0, MagCapacity);
}

TArray<EFireType> AWeapon::GetAllowedFireModes() const
{
	if (AllowedFireModes.Num() > 0)
	{
		return AllowedFireModes;
	}
	if (WeaponKind == EWeaponKind::Pistol)
	{
		return { EFireType::Single, EFireType::Burst };
	}
	if (bCanSwitchFireMode)
	{
		return { EFireType::Single, EFireType::Automatic };
	}
	return { FireType };
}

bool AWeapon::AllowsFireType(EFireType Type) const
{
	return GetAllowedFireModes().Contains(Type);
}

EFireType AWeapon::GetNextFireMode() const
{
	const TArray<EFireType> Modes = GetAllowedFireModes();
	if (Modes.Num() == 0)
	{
		return FireType;
	}
	const int32 Index = Modes.IndexOfByKey(FireType);
	if (Index == INDEX_NONE)
	{
		return Modes[0];
	}
	return Modes[(Index + 1) % Modes.Num()];
}

float AWeapon::GetBurstShotInterval() const
{
	return BurstShotInterval > KINDA_SMALL_NUMBER ? BurstShotInterval : FireTime;
}

void AWeapon::SanitizeFireType()
{
	const TArray<EFireType> Modes = GetAllowedFireModes();
	if (Modes.Num() > 0 && !Modes.Contains(FireType))
	{
		FireType = Modes[0];
	}
}

void AWeapon::ForceChamberSocketBones(USkeletalMeshComponent* Mesh) const
{
	if (!IsValid(Mesh) || !Mesh->DoesSocketExist(TEXT("Bullet1Socket"))) return;
	USkeletalMesh* SkelMesh = Mesh->GetSkeletalMeshAsset();
	if (!IsValid(SkelMesh)) return;

	// 动画预览会更新全部骨骼。运行时没蒙皮的 Bullet 骨会被剔掉，插槽停在绑定姿势。
	for (USkeletalMeshSocket* Socket : SkelMesh->GetActiveSocketList())
	{
		if (IsValid(Socket))
		{
			Socket->bForceAlwaysAnimated = true;
		}
	}
	Mesh->RecalcRequiredBones(0);
	Mesh->RefreshBoneTransforms();
}

void AWeapon::SetupChamberShells()
{
	if (bChamberShellsReady || GetNetMode() == NM_DedicatedServer) return;
	if (!IsValid(Mesh1P) || !Mesh1P->DoesSocketExist(TEXT("Bullet1Socket"))) return;

	ForceChamberSocketBones(Mesh1P);
	ForceChamberSocketBones(Mesh3P);

	UStaticMesh* ShellMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/FPSTP/Content/Weapons/Weapons/WeaponList/WPN_Shotgun/StaticMesh/SM_ShotgunBullet.SM_ShotgunBullet"));
	if (!IsValid(ShellMesh))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Shotgun] %s could not load SM_ShotgunBullet."), *GetName());
		return;
	}

	const FName SocketNames[2] = { TEXT("Bullet1Socket"), TEXT("Bullet2Socket") };
	bChamberShellOn[0] = Ammo >= 1;
	bChamberShellOn[1] = Ammo >= 2 && Mesh1P->DoesSocketExist(SocketNames[1]);

	auto MakeShell = [this, ShellMesh](USkeletalMeshComponent* Parent, FName SocketName) -> UStaticMeshComponent*
	{
		if (!IsValid(Parent) || !Parent->DoesSocketExist(SocketName)) return nullptr;

		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->SetStaticMesh(ShellMesh);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->bReceivesDecals = false;
		Comp->CastShadow = false;
		// 相对插槽为单位变换。位置跟插槽走，不另加偏移。
		Comp->SetupAttachment(Parent, SocketName);
		Comp->SetRelativeTransform(FTransform::Identity);
		Comp->RegisterComponent();
		return Comp;
	};

	ChamberShells1P.Reset();
	ChamberShells3P.Reset();
	for (int32 Index = 0; Index < 2; ++Index)
	{
		ChamberShells1P.Add(MakeShell(Mesh1P, SocketNames[Index]));
		ChamberShells3P.Add(MakeShell(Mesh3P, SocketNames[Index]));
	}

	bChamberShellsReady = true;
	ApplyChamberShellVisibility();
	UE_LOG(LogTemp, Log, TEXT("[Shotgun] %s chamber shells on Bullet1Socket/Bullet2Socket. ammo=%d"), *GetName(), Ammo);
}

void AWeapon::ApplyChamberShellVisibility() const
{
	if (!bChamberShellsReady) return;

	const APawn* Pawn = GetInstigator();
	const bool bLocal = IsValid(Pawn) && Pawn->IsLocallyControlled();

	auto ApplyOne = [](const TObjectPtr<UStaticMeshComponent>& Mesh, bool bShow)
	{
		if (!IsValid(Mesh)) return;
		Mesh->SetHiddenInGame(!bShow);
		Mesh->SetVisibility(bShow);
	};

	for (int32 Index = 0; Index < 2; ++Index)
	{
		const bool bOn = bChamberShellOn[Index] != 0;
		if (ChamberShells1P.IsValidIndex(Index))
		{
			ApplyOne(ChamberShells1P[Index], bLocal && bOn);
		}
		if (ChamberShells3P.IsValidIndex(Index))
		{
			ApplyOne(ChamberShells3P[Index], !bLocal && bOn);
		}
	}
}

void AWeapon::HandleChamberShellNotify(FName NotifyName)
{
	if (!bChamberShellsReady) return;

	bool bChanged = false;
	if (NotifyName == TEXT("Bullet1Spawn"))
	{
		bChamberShellOn[0] = 1;
		bChanged = true;
	}
	else if (NotifyName == TEXT("Bullet1Delete"))
	{
		bChamberShellOn[0] = 0;
		bChanged = true;
	}
	else if (NotifyName == TEXT("Bullet2Spawn"))
	{
		bChamberShellOn[1] = 1;
		bChanged = true;
	}
	else if (NotifyName == TEXT("Bullet2Delete"))
	{
		bChamberShellOn[1] = 0;
		bChanged = true;
	}
	else if (NotifyName == TEXT("DeleteAllBullets"))
	{
		bChamberShellOn[0] = 0;
		bChamberShellOn[1] = 0;
		bChanged = true;
	}
	if (!bChanged) return;

	if (IsValid(Mesh1P)) Mesh1P->RefreshBoneTransforms();
	if (IsValid(Mesh3P)) Mesh3P->RefreshBoneTransforms();
	ApplyChamberShellVisibility();
}

void AWeapon::OnChamberShellNotify(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	(void)Payload;
	if (const UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetInstigator()))
	{
		if (Combat->CurrentWeapon != this) return;
	}
	HandleChamberShellNotify(NotifyName);
}

void AWeapon::BindChamberShellNotifies()
{
	if (bChamberShellNotifyBound || !bChamberShellsReady) return;

	APawn* Pawn = GetInstigator();
	if (!IsValid(Pawn) || !Pawn->Implements<UPlayerInterface>()) return;

	auto BindMesh = [this](USkeletalMeshComponent* Mesh)
	{
		if (!IsValid(Mesh)) return;
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			Anim->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &AWeapon::OnChamberShellNotify);
		}
	};

	BindMesh(IPlayerInterface::Execute_GetMesh1P(Pawn));
	BindMesh(IPlayerInterface::Execute_GetMesh3P(Pawn));
	bChamberShellNotifyBound = true;
}

void AWeapon::BeginPlay()
{
	Super::BeginPlay();
	SanitizeFireType();

	if (AuthoredMagCapacity <= 0)
	{
		AuthoredMagCapacity = MagCapacity;
	}

	// 默认配件只由服务器装，客户端通过同步拿到
	if (HasAuthority())
	{
		TSet<FGameplayTag> SeenSlots;
		// 每个槽位装上自己的默认配件
		for (const FWeaponAttachmentSlot& Slot : AttachmentSlots)
		{
			if (!Slot.SlotTag.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("[Attachment] %s has a slot with an empty SlotTag."), *GetName());
				continue;
			}
			if (SeenSlots.Contains(Slot.SlotTag))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Attachment] %s has more than one slot tagged '%s'. Only the first is used."), *GetName(), *Slot.SlotTag.ToString());
			}
			SeenSlots.Add(Slot.SlotTag);

			if (IsValid(Slot.DefaultAttachment))
			{
				EquipAttachment(Slot.DefaultAttachment);
			}
		}
	}
	else if (EquippedAttachments.Num() > 0)
	{
		RebuildAttachmentVisuals();
	}

	ApplyMagazineCapacity();
	SetupChamberShells();
	BindChamberShellNotifies();
}

void AWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWeapon, EquippedAttachments);
	DOREPLIFETIME(AWeapon, bFlashlightOn);
	DOREPLIFETIME(AWeapon, bLaserOn);
	DOREPLIFETIME_CONDITION(AWeapon, FireType, COND_SkipOwner);
}

// ================= 配件系统 =================

bool AWeapon::EquipAttachment(UAttachmentData* Attachment)
{
	if (!HasAuthority() || !IsValid(Attachment)) return false;

	// 服务器校验：枪上有这个槽 且 配件在白名单里。客户端发来的请求不可信，必须在这里检查
	if (!CanEquipAttachment(Attachment))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Attachment] %s rejected %s: no slot '%s' or not in AllowedAttachments."),
			*GetName(), *GetNameSafe(Attachment), *Attachment->SlotTag.ToString());
		return false;
	}

	// 同一个槽位只能有一个：先把旧的去掉
	const FGameplayTag NewSlot = Attachment->SlotTag;
	EquippedAttachments.RemoveAll([NewSlot](const UAttachmentData* Existing)
	{
		return !IsValid(Existing) || Existing->SlotTag == NewSlot;
	});
	EquippedAttachments.Add(Attachment);

	if (!HasFlashlight())
	{
		bFlashlightOn = false;
	}
	if (!HasLaser())
	{
		bLaserOn = false;
	}

	// 服务器不会收到 OnRep，所以手动调一次（监听服务器也要看到模型）
	OnRep_EquippedAttachments();
	ForceNetUpdate();
	return true;
}

bool AWeapon::RemoveAttachment(FGameplayTag SlotTag)
{
	if (!HasAuthority()) return false;

	const FWeaponAttachmentSlot* Slot = FindSlot(SlotTag);
	if (Slot && Slot->bRequired)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Attachment] %s: slot '%s' is required and cannot be emptied."), *GetName(), *SlotTag.ToString());
		return false;
	}

	const int32 Removed = EquippedAttachments.RemoveAll([SlotTag](const UAttachmentData* Existing)
	{
		return !IsValid(Existing) || Existing->SlotTag == SlotTag;
	});
	if (Removed == 0) return false;

	if (!HasFlashlight())
	{
		bFlashlightOn = false;
	}
	if (!HasLaser())
	{
		bLaserOn = false;
	}

	OnRep_EquippedAttachments();
	ForceNetUpdate();
	return true;
}

void AWeapon::ToggleFlashlight()
{
	// 第 12 步：战术键同时开关手电和激光（只有激光时也能开关激光）
	const bool bHasLight = HasFlashlight();
	const bool bHasLaser = HasLaser();
	if (!HasAuthority() || (!bHasLight && !bHasLaser)) return;

	const bool bNewOn = !(bHasLight ? bFlashlightOn : bLaserOn);
	if (bHasLight)
	{
		bFlashlightOn = bNewOn;
		OnRep_FlashlightOn();   // 服务器自己也要亮灯
	}
	if (bHasLaser)
	{
		bLaserOn = bNewOn;
		OnRep_LaserOn();
	}
	ForceNetUpdate();
}

const FWeaponAttachmentSlot* AWeapon::FindSlot(const FGameplayTag& SlotTag) const
{
	if (!SlotTag.IsValid()) return nullptr;
	// MatchesTagExact：必须完全相同，Attachment.Slot.Barrel 不会匹配父标签 Attachment.Slot
	return AttachmentSlots.FindByPredicate([&SlotTag](const FWeaponAttachmentSlot& Slot)
	{
		return Slot.SlotTag.MatchesTagExact(SlotTag);
	});
}

FName AWeapon::ResolveAttachmentSocket(const UAttachmentData* Data) const
{
	if (!IsValid(Data))
	{
		return NAME_None;
	}
	if (!Data->MountSocketName.IsNone())
	{
		return Data->MountSocketName;
	}
	const FWeaponAttachmentSlot* Slot = FindSlot(Data->SlotTag);
	return Slot ? Slot->SocketName : NAME_None;
}

UStaticMesh* AWeapon::ResolveAttachmentVisualMesh(const UAttachmentData* Data) const
{
	if (!IsValid(Data))
	{
		return nullptr;
	}

	// EquippedAttachments 在 Rebuild 之前已经写好，不看 AttachmentVisuals。
	if (!IsValid(GetOpticAttachment()))
	{
		return Data->Mesh.Get();
	}

	if (IsValid(Data->FoldedMesh))
	{
		return Data->FoldedMesh.Get();
	}

	// Infima 机瞄：竖起网格旁边有同名 _Folded。后机瞄竖起是 Ironsight_B，倒下是 IronSight_B_Folded。
	if (IsValid(Data->Mesh))
	{
		const FString RaisedPath = Data->Mesh->GetPathName();
		const FString FoldedPath = RaisedPath
			.Replace(TEXT("Ironsight_B"), TEXT("IronSight_B_Folded"))
			.Replace(TEXT("IronSight_F"), TEXT("IronSight_F_Folded"));
		if (FoldedPath != RaisedPath)
		{
			if (UStaticMesh* Folded = LoadObject<UStaticMesh>(nullptr, *FoldedPath))
			{
				return Folded;
			}
		}
	}

	return Data->Mesh.Get();
}

TArray<UAttachmentData*> AWeapon::GetAllowedAttachmentsForSlot(FGameplayTag SlotTag) const
{
	TArray<UAttachmentData*> Result;
	if (const FWeaponAttachmentSlot* Slot = FindSlot(SlotTag))
	{
		for (UAttachmentData* Attachment : Slot->AllowedAttachments)
		{
			if (IsValid(Attachment))
			{
				Result.Add(Attachment);
			}
		}
	}
	return Result;
}

bool AWeapon::CanEquipAttachment(const UAttachmentData* Attachment) const
{
	if (!IsValid(Attachment)) return false;

	const FWeaponAttachmentSlot* Slot = FindSlot(Attachment->SlotTag);
	if (!Slot) return false;   // 这把枪没有这种槽位

	// 白名单里有它才行
	return Slot->AllowedAttachments.Contains(Attachment);
}


EGripHoldStyle AWeapon::GetGripHoldStyle() const
{
	if (const UAttachmentData* Grip = GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Grip))
	{
		return Grip->GripHoldStyle;
	}
	return EGripHoldStyle::Cradle;
}
UAttachmentData* AWeapon::GetAttachmentInSlot(FGameplayTag SlotTag) const
{
	for (UAttachmentData* Attachment : EquippedAttachments)
	{
		if (IsValid(Attachment) && Attachment->SlotTag == SlotTag)
		{
			return Attachment;
		}
	}
	return nullptr;
}

void AWeapon::ApplyMagazineCapacity()
{
	if (AuthoredMagCapacity <= 0)
	{
		AuthoredMagCapacity = MagCapacity;
	}

	int32 NewCapacity = AuthoredMagCapacity;
	if (const UAttachmentData* Mag = GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Magazine))
	{
		if (Mag->MagazineCapacity > 0)
		{
			NewCapacity = Mag->MagazineCapacity;
		}
	}

	MagCapacity = FMath::Max(0, NewCapacity);
	Ammo = FMath::Clamp(Ammo, 0, MagCapacity);

	if (APawn* Pawn = GetInstigator())
	{
		if (UCombatComponent* Combat = Pawn->FindComponentByClass<UCombatComponent>())
		{
			Combat->NotifyAmmoChanged();
		}
	}
}

float AWeapon::GetRecoilMultiplier() const
{
	float Multiplier = 1.f;
	for (const UAttachmentData* Attachment : EquippedAttachments)
	{
		if (IsValid(Attachment))
		{
			Multiplier *= FMath::Max(0.f, Attachment->RecoilMultiplier);   // 相乘叠加：0.8 * 0.9 = 0.72
		}
	}
	return Multiplier;
}

float AWeapon::GetHipFireSpread() const
{
	const UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetInstigator());
	if (IsValid(Combat) && Combat->bAiming) return 0.f;
	float Spread = FMath::Max(0.f, HipFireSpread);
	for (const UAttachmentData* Attachment : EquippedAttachments)
	{
		if (IsValid(Attachment)) Spread *= FMath::Max(0.f, Attachment->HipFireSpreadMultiplier);
	}
	return Spread;
}

float AWeapon::GetEffectiveAimFOV() const
{
	const UAttachmentData* Optic = GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Optic);

	// 可变倍率但没配画面：整屏按当前档变焦。画中画时切档只改镜片里的画面。
	if (IsValid(Optic) && Optic->IsVariableOptic() && !UsesLensPiP())
	{
		const float LevelFOV = Optic->ZoomLevelsFOV[FMath::Clamp(CurrentZoomIndex, 0, Optic->ZoomLevelsFOV.Num() - 1)];
		if (LevelFOV > 0.f)
		{
			return LevelFOV;
		}
	}
	// 镜外镜头：瞄具勾了覆盖且 AimFOV > 0 就用它，否则用枪自己的 AimFeildOfView。
	// 画中画的放大不走这里，镜片里的倍率由 GetPiPCaptureFOV 单独算。
	if (IsValid(Optic) && Optic->bOverridesAimFOV && Optic->AimFOV > 0.f)
	{
		return Optic->AimFOV;
	}
	return AimFeildOfView > 0.f ? AimFeildOfView : 0.f;
}

bool AWeapon::UsesLensPiP() const
{
	const UAttachmentData* Optic = GetOpticAttachment();
	if (!IsValid(Optic)) return false;
	const bool bPiPType = Optic->IsVariableOptic() || Optic->OpticType == EOpticType::Magnified;
	if (!bPiPType) return false;

	const UStaticMeshComponent* OpticMesh = GetOpticMesh1P();
	if (!IsValid(OpticMesh)) return false;

	const bool bHasLensSocket = !Optic->LensSocketName.IsNone() && OpticMesh->DoesSocketExist(Optic->LensSocketName);
	const bool bHasSightSocket = !Optic->SightSocketName.IsNone() && OpticMesh->DoesSocketExist(Optic->SightSocketName);
	if (!bHasLensSocket && !bHasSightSocket) return false;

	// 填了画面材质就用它。Magnified 没填时，用画面槽上现成的材质（准心在另一槽，不会被换掉）。
	// Variable 仍要 LensMaterial，否则继续走整屏 FOV。
	if (IsValid(Optic->LensMaterial)) return true;
	if (Optic->OpticType != EOpticType::Magnified) return false;
	return Optic->LensMaterialIndex >= 0
		&& Optic->LensMaterialIndex < OpticMesh->GetNumMaterials()
		&& OpticMesh->GetMaterial(Optic->LensMaterialIndex) != nullptr;
}

float AWeapon::GetPiPCaptureFOV(float BaseFOV) const
{
	const UAttachmentData* Optic = GetOpticAttachment();
	if (!IsValid(Optic)) return BaseFOV;

	if (Optic->IsVariableOptic())
	{
		const float LevelFOV = Optic->ZoomLevelsFOV[FMath::Clamp(CurrentZoomIndex, 0, Optic->ZoomLevelsFOV.Num() - 1)];
		if (LevelFOV > 0.f) return LevelFOV;
	}
	// 倍率 -> 镜片内 FOV。镜外镜头用瞄具 AimFOV，不进这里。
	// tan(拍摄半角) = tan(基准半角) / 倍率
	const float Base = Optic->BaseFOVForMagnification > 0.f ? Optic->BaseFOVForMagnification : BaseFOV;
	const float Mag = FMath::Max(1.f, Optic->Magnification);
	return FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Base * 0.5f)) / Mag));
}

float AWeapon::GetEffectiveAdsTime() const
{
	float Time = FMath::Max(0.f, AimDownSightTime);
	for (const UAttachmentData* Attachment : EquippedAttachments)
	{
		if (IsValid(Attachment))
		{
			Time *= FMath::Max(0.05f, Attachment->AdsTimeMultiplier);   // 相乘叠加：1.1 * 0.9 = 0.99
		}
	}
	return Time;
}

int32 AWeapon::GetZoomLevelCount() const
{
	const UAttachmentData* Optic = GetOpticAttachment();
	return (IsValid(Optic) && Optic->IsVariableOptic()) ? Optic->ZoomLevelsFOV.Num() : 0;
}

UAttachmentData* AWeapon::GetOpticAttachment() const
{
	return GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Optic);
}

UStaticMeshComponent* AWeapon::GetOpticMesh1P() const
{
	const FAttachmentVisual* Optic = FindVisual(ShooterTags::Tag_Attachment_Slot_Optic);
	return Optic ? Optic->Mesh1P.Get() : nullptr;
}

bool AWeapon::GetCantedSight(UStaticMeshComponent*& OutMesh, const UAttachmentData*& OutData) const
{
	OutMesh = nullptr;
	OutData = nullptr;
	for (const FAttachmentVisual& Visual : AttachmentVisuals)
	{
		if (IsValid(Visual.Data) && (Visual.Data->bProvidesCantedSight || Visual.Data->bProvidesLaser) && IsValid(Visual.Mesh1P)
			&& !Visual.Data->CantedSightSocketName.IsNone()
			&& Visual.Mesh1P->DoesSocketExist(Visual.Data->CantedSightSocketName))
		{
			OutMesh = Visual.Mesh1P;
			OutData = Visual.Data;
			return true;
		}
	}
	return false;
}

bool AWeapon::HasFlashlight() const
{
	for (const UAttachmentData* Attachment : EquippedAttachments)
	{
		if (IsValid(Attachment) && Attachment->bProvidesFlashlight)
		{
			return true;
		}
	}
	return false;
}

FRecoilScale AWeapon::GetAttachmentRecoilScale_Implementation() const
{
	// CombatComponent::ResolveRecoilScale 会把这里的值乘到后座上
	FRecoilScale Scale;
	const float Recoil = GetRecoilMultiplier();
	Scale.Pitch = Recoil;
	Scale.Yaw = Recoil;
	for (const UAttachmentData* Attachment : EquippedAttachments)
	{
		if (IsValid(Attachment))
		{
			Scale.Recovery *= FMath::Max(0.f, Attachment->RecoilRecoveryMultiplier);
		}
	}
	return Scale;
}

void AWeapon::OnRep_EquippedAttachments()
{
	ApplyMagazineCapacity();
	RebuildAttachmentVisuals();
	OnAttachmentsChanged.Broadcast(this);

	if (APawn* Pawn = GetInstigator())
	{
		if (UCombatComponent* Combat = Pawn->FindComponentByClass<UCombatComponent>())
		{
			// Re-apply CurrenrAnim with Cradle/Grab idle overrides (no FABRIK).
			Combat->RefreshLocomotionBlendSpaces(false);
		}
	}

	// 开镜 FOV 现在由 AShooterCharacter::UpdateAimFieldOfView 每帧插值，
	// 瞄具换了它下一帧自动追到新的 FOV，这里不用再手动设镜头。
}

// ================= 激光（第 11 步） =================

void AWeapon::ToggleLaser()
{
	if (!HasAuthority() || !HasLaser()) return;

	bLaserOn = !bLaserOn;
	OnRep_LaserOn();   // 服务器自己也要看到
	ForceNetUpdate();
}

void AWeapon::SetLaserOn(bool bOn)
{
	if (!HasAuthority() || !HasLaser()) return;
	if (bLaserOn == bOn) return;
	bLaserOn = bOn;
	OnRep_LaserOn();
	ForceNetUpdate();
}

void AWeapon::OnRep_LaserOn()
{
	ApplyLaserState();
}

const UAttachmentData* AWeapon::GetLaserData() const
{
	for (const UAttachmentData* Attachment : EquippedAttachments)
	{
		if (IsValid(Attachment) && Attachment->bProvidesLaser)
		{
			return Attachment;
		}
	}
	return nullptr;
}

void AWeapon::RebuildLaser()
{
	const UAttachmentData* Data = GetLaserData();
	if (!Data)
	{
		if (IsValid(LaserBeam)) { LaserBeam->DestroyComponent(); }
		if (IsValid(LaserDot)) { LaserDot->DestroyComponent(); }
		LaserBeam = nullptr;
		LaserDot = nullptr;
		ApplyLaserState();
		return;
	}

	if (!IsValid(LaserBeam))
	{
		LaserBeam = NewObject<UStaticMeshComponent>(this);
		LaserBeam->SetMobility(EComponentMobility::Movable);
		LaserBeam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LaserBeam->CastShadow = false;
		LaserBeam->bReceivesDecals = false;
		LaserBeam->SetupAttachment(Mesh1P);
		LaserBeam->RegisterComponent();
	}

	UStaticMesh* BeamMesh = Data->LaserBeamMesh.LoadSynchronous();
	LaserBeam->SetStaticMesh(BeamMesh);
	LaserBeamLength = 100.f;
	if (IsValid(BeamMesh))
	{
		LaserBeamLength = FMath::Max(1.f, static_cast<float>(BeamMesh->GetBoundingBox().GetSize().X));
	}
	UMaterialInterface* BeamMat = Data->LaserBeamMaterial.LoadSynchronous();
	if (!IsValid(BeamMat))
	{
		BeamMat = LaserBeam->GetMaterial(0);
	}
	if (IsValid(BeamMat))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BeamMat, this);
		MID->SetVectorParameterValue(TEXT("Color"), Data->LaserColor);
		MID->SetVectorParameterValue(TEXT("LaserColor"), Data->LaserColor);
		for (const TCHAR* Param : { TEXT("Intensity"), TEXT("Emissive"), TEXT("Brightness"), TEXT("EmissiveIntensity") })
		{
			MID->SetScalarParameterValue(Param, Data->LaserBrightness);
		}
		LaserBeam->SetMaterial(0, MID);
	}

	if (!IsValid(LaserDot))
	{
		LaserDot = NewObject<UDecalComponent>(this);
		LaserDot->SetupAttachment(Mesh1P);
		LaserDot->SetUsingAbsoluteLocation(true);
		LaserDot->SetUsingAbsoluteRotation(true);
		LaserDot->SetUsingAbsoluteScale(true);
		LaserDot->RegisterComponent();
	}
	LaserDot->DecalSize = FVector(Data->LaserDotSize * 2.f, Data->LaserDotSize, Data->LaserDotSize);
	if (UMaterialInterface* DotMat = Data->LaserDotDecalMaterial.LoadSynchronous())
	{
		UMaterialInstanceDynamic* DotMID = UMaterialInstanceDynamic::Create(DotMat, this);
		DotMID->SetVectorParameterValue(TEXT("Color"), Data->LaserColor);
		DotMID->SetVectorParameterValue(TEXT("LaserColor"), Data->LaserColor);
		for (const TCHAR* Param : { TEXT("Intensity"), TEXT("Emissive"), TEXT("Brightness"), TEXT("EmissiveIntensity") })
		{
			DotMID->SetScalarParameterValue(Param, Data->LaserBrightness);
		}
		LaserDot->SetDecalMaterial(DotMID);
	}
	LaserDot->SetFadeScreenSize(0.f);   // 远处不淡出
	LaserDot->MarkRenderStateDirty();

	UpdateLaserAttachment();
}

void AWeapon::UpdateLaserAttachment()
{
	if (!IsValid(LaserBeam)) return;

	const FAttachmentVisual* Visual = AttachmentVisuals.FindByPredicate([](const FAttachmentVisual& V)
	{
		return IsValid(V.Data) && V.Data->bProvidesLaser;
	});
	if (!Visual) return;

	// 和手电一样：本地玩家用 1P 模型，其他人用 3P 模型。骨骼配件没有静态网格。
	const APawn* OwningPawn = GetInstigator();
	const bool bLocal = IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	USceneComponent* Parent = AttachmentViewComponent(*Visual, bLocal);
	if (!IsValid(Parent))
	{
		Parent = bLocal ? Mesh1P.Get() : Mesh3P.Get();
	}
	if (!IsValid(Parent)) return;

	const FName Socket = Parent->DoesSocketExist(Visual->Data->LaserSocketName) ? Visual->Data->LaserSocketName : NAME_None;
	LaserBeam->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	LaserBeam->SetRelativeScale3D(FVector::OneVector);
	ApplyLaserState();
}

void AWeapon::ApplyLaserState()
{
	const bool bActive = bLaserOn && IsValid(LaserBeam) && GetNetMode() != NM_DedicatedServer;
	SetActorTickEnabled(bActive);
	if (bActive)
	{
		UpdateLaser();
		return;
	}
	if (IsValid(LaserBeam))
	{
		LaserBeam->SetHiddenInGame(true);
		LaserBeam->SetVisibility(false);
	}
	if (IsValid(LaserDot))
	{
		LaserDot->SetHiddenInGame(true);
		LaserDot->SetVisibility(false);
	}
}

void AWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLaser();
}

void AWeapon::UpdateLaser()
{
	auto HideAll = [this]()
	{
		if (IsValid(LaserBeam)) { LaserBeam->SetHiddenInGame(true); LaserBeam->SetVisibility(false); }
		if (IsValid(LaserDot)) { LaserDot->SetHiddenInGame(true); LaserDot->SetVisibility(false); }
	};

	const UAttachmentData* Data = GetLaserData();
	UWorld* World = GetWorld();
	USceneComponent* Parent = IsValid(LaserBeam) ? LaserBeam->GetAttachParent() : nullptr;
	// 收枪（Actor 隐藏）、治疗隐藏、别人看不到的 1P 模型上：都不画
	if (!bLaserOn || !Data || !World || IsHidden() || !IsValid(Parent) || !Parent->ShouldRender())
	{
		HideAll();
		return;
	}

	const float Range = FMath::Max(1500.f, Data->LaserRange);
	const FVector Start = LaserBeam->GetComponentLocation();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponLaser), true, this);
	Params.AddIgnoredActor(GetInstigator());
	Params.AddIgnoredActor(GetOwner());
	FHitResult Hit;
	bool bHit = false;
	FVector Target;

	// 第 13/14 步：只有本地玩家侧瞄开镜时，射线从镜头穿准星；腰射/奔跑等一律沿 LaserSocket 前方，避免光束飘在屏幕中心
	const APawn* OwningPawn = GetInstigator();
	const AShooterCharacter* Shooter = Cast<AShooterCharacter>(OwningPawn);
	const bool bCantedLocal = IsValid(Shooter) && Shooter->IsLocallyControlled() && Shooter->IsCantedAiming();
	AController* Controller = IsValid(OwningPawn) ? OwningPawn->GetController() : nullptr;
	if (bCantedLocal && IsValid(Controller))
	{
		FVector ViewLoc;
		FRotator ViewRot;
		Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
		const FVector ViewEnd = ViewLoc + ViewRot.Vector() * Range;
		bHit = World->LineTraceSingleByChannel(Hit, ViewLoc, ViewEnd, ECC_Visibility, Params);
		Target = bHit ? FVector(Hit.ImpactPoint) : ViewEnd;
		const FVector ToTarget = Target - Start;
		if (!ToTarget.IsNearlyZero())
		{
			LaserBeam->SetWorldRotation(ToTarget.Rotation());
		}
	}
	else
	{
		LaserBeam->SetRelativeRotation(FRotator::ZeroRotator);
		const FVector End = Start + LaserBeam->GetForwardVector() * Range;
		bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
		Target = bHit ? FVector(Hit.ImpactPoint) : End;
	}
	const float Distance = FVector::Dist(Start, Target);

	bool bHideBeam = false;
	if (Data->bHideBeamWhenAiming)
	{
		if (const UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetInstigator()))
		{
			bHideBeam = Combat->bAiming;
		}
	}

	const float ParentScaleX = FMath::Max(KINDA_SMALL_NUMBER, static_cast<float>(Parent->GetComponentScale().X));
	LaserBeam->SetRelativeScale3D(FVector(Distance / (LaserBeamLength * ParentScaleX), 1.f, 1.f));
	LaserBeam->SetHiddenInGame(bHideBeam);
	LaserBeam->SetVisibility(!bHideBeam);

	if (IsValid(LaserDot))
	{
		if (bHit)
		{
			// 光斑大小随距离变大，远处也看得见
			const float DotRadius = FMath::Max(Data->LaserDotSize, Hit.Distance * 0.01f * Data->LaserDotSizePerMeter);
			LaserDot->DecalSize = FVector(FMath::Max(DotRadius * 2.f, 10.f), DotRadius, DotRadius);
			LaserDot->SetWorldLocationAndRotation(Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
			LaserDot->MarkRenderStateDirty();
			LaserDot->SetHiddenInGame(false);
			LaserDot->SetVisibility(true);
		}
		else
		{
			LaserDot->SetHiddenInGame(true);
			LaserDot->SetVisibility(false);
		}
	}
}
void AWeapon::OnRep_FlashlightOn()
{
	ApplyFlashlightState();
	ApplyLaserState();
}

const FAttachmentVisual* AWeapon::FindVisual(const FGameplayTag& SlotTag) const
{
	return AttachmentVisuals.FindByPredicate([&SlotTag](const FAttachmentVisual& Visual)
	{
		return IsValid(Visual.Data) && Visual.Data->SlotTag == SlotTag;
	});
}

UStaticMeshComponent* AWeapon::SpawnAttachmentMesh(USkeletalMeshComponent* Parent, const UAttachmentData* Data, bool bFirstPerson)
{
	UStaticMesh* VisualMesh = ResolveAttachmentVisualMesh(Data);
	if (!IsValid(Parent) || !IsValid(Data) || !IsValid(VisualMesh)) return nullptr;

	// 配件自己的 MountSocketName 优先。枪骨骼上没有时，挂到已经装上的配件（护木上的握把插槽）。
	const FName SocketName = ResolveAttachmentSocket(Data);
	USceneComponent* Mount = FindAttachmentMount(Parent, AttachmentVisuals, bFirstPerson, SocketName);
	if (!IsValid(Mount))
	{
		Mount = Parent;
		if (!SocketName.IsNone())
		{
			UE_LOG(LogTemp, Warning, TEXT("Attachment %s: socket '%s' is missing on %s and on equipped attachments."), *GetNameSafe(Data), *SocketName.ToString(), *GetNameSafe(Parent->GetSkeletalMeshAsset()));
		}
	}

	// 运行时创建组件：NewObject + 挂到 Socket + RegisterComponent
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetStaticMesh(VisualMesh);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->bReceivesDecals = false;
	Comp->CastShadow = !bFirstPerson;   // 和枪一样：1P 不投影，3P 投影
	Comp->SetupAttachment(Mount, SocketName);
	Comp->SetRelativeTransform(Data->MeshOffset);
	Comp->RegisterComponent();

	// 第一人称换弹要两只弹夹。装回去的那只挂在备用骨上，动画把它送进弹仓。
	if (bFirstPerson
		&& Data->SlotTag == ShooterTags::Tag_Attachment_Slot_Magazine
		&& Parent->DoesSocketExist(ReserveMagazineSocket))
	{
		UStaticMeshComponent* Reserve = NewObject<UStaticMeshComponent>(this);
		Reserve->SetMobility(EComponentMobility::Movable);
		Reserve->SetStaticMesh(Data->Mesh);
		Reserve->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Reserve->bReceivesDecals = false;
		Reserve->CastShadow = false;
		Reserve->SetupAttachment(Parent, ReserveMagazineSocket);
		Reserve->SetRelativeTransform(Data->MeshOffset);
		Reserve->SetHiddenInGame(true);
		Reserve->SetVisibility(false);
		Reserve->RegisterComponent();
	}
	return Comp;
}

USkeletalMeshComponent* AWeapon::SpawnAttachmentSkelMesh(USkeletalMeshComponent* Parent, const UAttachmentData* Data, bool bFirstPerson)
{
	if (!IsValid(Parent) || !IsValid(Data) || !IsValid(Data->SkeletalMesh)) return nullptr;

	const FName SocketName = ResolveAttachmentSocket(Data);
	USceneComponent* Mount = FindAttachmentMount(Parent, AttachmentVisuals, bFirstPerson, SocketName);
	if (!IsValid(Mount))
	{
		Mount = Parent;
		if (!SocketName.IsNone())
		{
			UE_LOG(LogTemp, Warning, TEXT("Attachment %s: socket '%s' missing for skeletal mag on %s."), *GetNameSafe(Data), *SocketName.ToString(), *GetNameSafe(Parent->GetSkeletalMeshAsset()));
		}
	}

	USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(this);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetSkeletalMesh(Data->SkeletalMesh);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->bReceivesDecals = false;
	Comp->CastShadow = !bFirstPerson;
	Comp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Comp->SetupAttachment(Mount, SocketName);
	Comp->SetRelativeTransform(Data->MeshOffset);
	Comp->RegisterComponent();
	// Mag Armature rest pose is offset from root; park on MagReloadAnim frame 0.
	if (UAnimSequence* RestPose = Data->MagReloadAnim.Get())
	{
		Comp->PlayAnimation(RestPose, false);
		if (UAnimSingleNodeInstance* Node = Comp->GetSingleNodeInstance())
		{
			Node->SetPosition(0.f);
			Node->SetPlaying(false);
		}
	}
	// Hide spare mag_02 (SKM_AK_Mag) + any BonesToHide. Toggle: DA bHideSpareMagazineBone / BonesToHide.
	if (Data->bHideSpareMagazineBone)
	{
		static const FName SpareMagBone(TEXT("mag_02"));
		if (Comp->GetBoneIndex(SpareMagBone) != INDEX_NONE)
		{
			Comp->HideBoneByName(SpareMagBone, EPhysBodyOp::PBO_None);
		}
	}
	for (const FName& BoneName : Data->BonesToHide)
	{
		if (!BoneName.IsNone() && Comp->GetBoneIndex(BoneName) != INDEX_NONE)
		{
			Comp->HideBoneByName(BoneName, EPhysBodyOp::PBO_None);
		}
	}
	return Comp;
}

USkeletalMeshComponent* AWeapon::GetMagazineSkelMesh1P() const
{
	const FAttachmentVisual* Mag = FindVisual(ShooterTags::Tag_Attachment_Slot_Magazine);
	return Mag ? Mag->SkelMesh1P.Get() : nullptr;
}

void AWeapon::PlayMagazineReload(EReloadType ReloadType)
{
	// 空仓才拉套筒。战术换弹没有枪身动画，只动弹匣。不依赖弹匣模型是否生成成功。
	if (WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_X24) && ReloadType == EReloadType::Empty)
	{
		static const TCHAR* EmptyReload = TEXT("/Game/FPS_Controller/Animations/WeaponsAnims/Pistol_X24/A_X24_Body_EmptyReload.A_X24_Body_EmptyReload");
		PlayX24BodySequence(Mesh1P, EmptyReload);
		PlayX24BodySequence(Mesh3P, EmptyReload);
	}

	// 备用弹夹跟枪身换弹一起出现。结束时 SetMeshVisibility 会再把它藏起来。
	bool bHasReserve = false;
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh) || Mesh->GetAttachSocketName() != ReserveMagazineSocket) continue;
		Mesh->SetHiddenInGame(false);
		Mesh->SetVisibility(true);
		bHasReserve = true;
	}

	// 枪已经生成过时，补一只挂到备用骨上，这次换弹就能看见装回去的弹夹。
	if (!bHasReserve && IsValid(Mesh1P) && Mesh1P->DoesSocketExist(ReserveMagazineSocket))
	{
		if (const FAttachmentVisual* MagVisual = FindVisual(ShooterTags::Tag_Attachment_Slot_Magazine))
		{
			if (IsValid(MagVisual->Data) && IsValid(MagVisual->Data->Mesh))
			{
				UStaticMeshComponent* Reserve = NewObject<UStaticMeshComponent>(this);
				Reserve->SetMobility(EComponentMobility::Movable);
				Reserve->SetStaticMesh(MagVisual->Data->Mesh);
				Reserve->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Reserve->bReceivesDecals = false;
				Reserve->CastShadow = false;
				Reserve->SetupAttachment(Mesh1P, ReserveMagazineSocket);
				Reserve->SetRelativeTransform(MagVisual->Data->MeshOffset);
				Reserve->SetHiddenInGame(false);
				Reserve->SetVisibility(true);
				Reserve->RegisterComponent();
			}
		}
	}

	USkeletalMeshComponent* MagMesh = GetMagazineSkelMesh1P();
	if (!IsValid(MagMesh)) return;

	const FAttachmentVisual* Mag = FindVisual(ShooterTags::Tag_Attachment_Slot_Magazine);
	if (!Mag || !IsValid(Mag->Data)) return;

	UAnimSequence* Seq = Mag->Data->MagReloadAnim;
	if (ReloadType == EReloadType::Empty && Mag->Data->MagEmptyReloadAnim)
	{
		Seq = Mag->Data->MagEmptyReloadAnim;
	}
	else if (ReloadType == EReloadType::Tactical && Mag->Data->MagTacticalReloadAnim)
	{
		Seq = Mag->Data->MagTacticalReloadAnim;
	}
	else if (ReloadType == EReloadType::First && Mag->Data->MagEmptyReloadAnim)
	{
		// First equip often shares empty-style mag motion; fall back to MagReloadAnim
		Seq = Mag->Data->MagReloadAnim ? Mag->Data->MagReloadAnim.Get() : Mag->Data->MagEmptyReloadAnim.Get();
	}
	if (!IsValid(Seq)) return;

	MagMesh->PlayAnimation(Seq, false);
}


void AWeapon::RebuildAttachmentVisuals()
{
	// 专用服务器不需要画面
	if (GetNetMode() == NM_DedicatedServer) return;

	// 手电挂在配件模型上。先拆下来，避免销毁模型时被带到枪根上并留下隐藏标记。
	if (IsValid(Flashlight))
	{
		Flashlight->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	}

	if (IsValid(LaserBeam))
	{
		LaserBeam->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	}
	// 备用弹夹不在 AttachmentVisuals 里，先拆掉，避免每次重装多一只。
	DestroyReserveMagazines(this);

	// 1. 清掉旧模型
	for (FAttachmentVisual& Visual : AttachmentVisuals)
	{
		if (IsValid(Visual.Mesh1P)) Visual.Mesh1P->DestroyComponent();
		if (IsValid(Visual.Mesh3P)) Visual.Mesh3P->DestroyComponent();
		if (IsValid(Visual.SkelMesh1P)) Visual.SkelMesh1P->DestroyComponent();
		if (IsValid(Visual.SkelMesh3P)) Visual.SkelMesh3P->DestroyComponent();
	}
	AttachmentVisuals.Reset();

	// 2. 先装挂在枪上的配件，再装挂在那些配件上的（握把插槽在护木上时，护木必须先在）。
	TArray<UAttachmentData*> Pending;
	for (UAttachmentData* Attachment : EquippedAttachments)
	{
		if (IsValid(Attachment))
		{
			Pending.Add(Attachment);
		}
	}

	bool bProgress = true;
	while (Pending.Num() > 0 && bProgress)
	{
		bProgress = false;
		for (int32 Index = 0; Index < Pending.Num();)
		{
			UAttachmentData* Attachment = Pending[Index];
			const FName SocketName = ResolveAttachmentSocket(Attachment);
			const bool bMount1P = !IsValid(Mesh1P) || FindAttachmentMount(Mesh1P, AttachmentVisuals, true, SocketName) != nullptr;
			const bool bMount3P = !IsValid(Mesh3P) || FindAttachmentMount(Mesh3P, AttachmentVisuals, false, SocketName) != nullptr;
			if (!bMount1P || !bMount3P)
			{
				++Index;
				continue;
			}

			FAttachmentVisual& Visual = AttachmentVisuals.AddDefaulted_GetRef();
			Visual.Data = Attachment;
			if (IsValid(Attachment->SkeletalMesh))
			{
				Visual.SkelMesh1P = SpawnAttachmentSkelMesh(Mesh1P, Attachment, true);
				Visual.SkelMesh3P = SpawnAttachmentSkelMesh(Mesh3P, Attachment, false);
			}
			else
			{
				Visual.Mesh1P = SpawnAttachmentMesh(Mesh1P, Attachment, true);
				Visual.Mesh3P = SpawnAttachmentMesh(Mesh3P, Attachment, false);
			}
			Pending.RemoveAt(Index);
			bProgress = true;
		}
	}

	for (UAttachmentData* Attachment : Pending)
	{
		FAttachmentVisual& Visual = AttachmentVisuals.AddDefaulted_GetRef();
		Visual.Data = Attachment;
		if (IsValid(Attachment->SkeletalMesh))
		{
			Visual.SkelMesh1P = SpawnAttachmentSkelMesh(Mesh1P, Attachment, true);
			Visual.SkelMesh3P = SpawnAttachmentSkelMesh(Mesh3P, Attachment, false);
		}
		else
		{
			Visual.Mesh1P = SpawnAttachmentMesh(Mesh1P, Attachment, true);
			Visual.Mesh3P = SpawnAttachmentMesh(Mesh3P, Attachment, false);
		}
	}

	// 3. 手电：有就创建，没有就销毁
	if (HasFlashlight())
	{
		if (!IsValid(Flashlight))
		{
			Flashlight = NewObject<USpotLightComponent>(this);
			Flashlight->SetMobility(EComponentMobility::Movable);
			Flashlight->SetupAttachment(Mesh1P);
			Flashlight->RegisterComponent();
		}
		for (const UAttachmentData* Attachment : EquippedAttachments)
		{
			if (IsValid(Attachment) && Attachment->bProvidesFlashlight)
			{
				Flashlight->SetIntensity(Attachment->LightIntensity);
				Flashlight->SetLightColor(Attachment->LightColor);
				Flashlight->SetAttenuationRadius(Attachment->LightAttenuationRadius);
				Flashlight->SetInnerConeAngle(Attachment->LightInnerConeAngle);
				Flashlight->SetOuterConeAngle(Attachment->LightOuterConeAngle);
				break;
			}
		}
		UpdateFlashlightAttachment();
	}
	else if (IsValid(Flashlight))
	{
		Flashlight->DestroyComponent();
		Flashlight = nullptr;
	}

	// 第 13 步：配了侧瞄（或激光）但模型上没有侧瞄插槽时，提示一次
	for (const FAttachmentVisual& Visual : AttachmentVisuals)
	{
		if (IsValid(Visual.Data) && (Visual.Data->bProvidesCantedSight || Visual.Data->bProvidesLaser) && IsValid(Visual.Mesh1P)
			&& !Visual.Mesh1P->DoesSocketExist(Visual.Data->CantedSightSocketName))
		{
			static TSet<FString> Warned;
			const FString Key = Visual.Data->GetPathName();
			if (!Warned.Contains(Key))
			{
				Warned.Add(Key);
				UE_LOG(LogTemp, Warning, TEXT("[Canted] %s: mesh %s has no socket '%s', canted sight disabled."),
					*GetNameSafe(Visual.Data), *GetNameSafe(Visual.Mesh1P->GetStaticMesh()), *Visual.Data->CantedSightSocketName.ToString());
			}
		}
	}

	// 4. 激光：有就创建光束和光斑，没有就销毁
	RebuildLaser();
	// 新模型要跟枪一样：本地玩家只显示 1P，其他人只显示 3P
	if (IsValid(GetInstigator()) && Mesh1P->GetAttachParent())
	{
		SetMeshVisibility(GetInstigator());
	}
	ApplyFlashlightState();
	ApplyLaserState();
}

void AWeapon::UpdateFlashlightAttachment()
{
	if (!IsValid(Flashlight)) return;

	const FAttachmentVisual* Visual = AttachmentVisuals.FindByPredicate([](const FAttachmentVisual& V)
	{
		return IsValid(V.Data) && V.Data->bProvidesFlashlight;
	});
	if (!Visual) return;

	// 本地玩家看到的是 1P 模型，其他人看到的是 3P 模型，灯要跟着看得见的那个。骨骼配件没有静态网格。
	const APawn* OwningPawn = GetInstigator();
	const bool bLocal = IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	USceneComponent* Parent = AttachmentViewComponent(*Visual, bLocal);
	if (!IsValid(Parent))
	{
		Parent = bLocal ? Mesh1P.Get() : Mesh3P.Get();
	}

	const FName Socket = Parent->DoesSocketExist(Visual->Data->LightSocketName) ? Visual->Data->LightSocketName : NAME_None;
	Flashlight->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	// 从被隐藏的 3P 模型挪到 1P 后，父级传播留下的 bHiddenInGame 不会自己清掉
	ApplyFlashlightState();
	ApplyLaserState();
}

void AWeapon::ApplyFlashlightState()
{
	if (!IsValid(Flashlight)) return;

	Flashlight->SetHiddenInGame(!bFlashlightOn);
	Flashlight->SetVisibility(bFlashlightOn);
}

void AWeapon::SetMeshVisibility(APawn* OwningPawn) const
{
	if (!IsValid(OwningPawn)) return;

	const bool bLocal = OwningPawn->IsLocallyControlled();
	Mesh1P->SetHiddenInGame(!bLocal, true);
	Mesh3P->SetHiddenInGame(bLocal, true);

	// Keep mag/attachments from lingering visible on the wrong mesh (e.g. spare mag on back).
	for (const FAttachmentVisual& Visual : AttachmentVisuals)
	{
		if (IsValid(Visual.SkelMesh1P)) { Visual.SkelMesh1P->SetHiddenInGame(!bLocal, true); }
		if (IsValid(Visual.SkelMesh3P)) { Visual.SkelMesh3P->SetHiddenInGame(bLocal, true); }
		if (IsValid(Visual.Mesh1P)) { Visual.Mesh1P->SetHiddenInGame(!bLocal, true); }
		if (IsValid(Visual.Mesh3P)) { Visual.Mesh3P->SetHiddenInGame(bLocal, true); }
	}

	// 父网格传播会把备用弹夹重新打开。平时只留枪里那一只。
	HideReserveMagazines(this);
	ApplyChamberShellVisibility();
}

