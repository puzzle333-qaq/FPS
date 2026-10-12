


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
#include "Engine/StaticMesh.h"
#include "ShooterGamePlayTags.h"

AWeapon::AWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
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
	// 装了瞄具且瞄具模型上有 SightSocket：开镜对准红点中心
	if (const FAttachmentVisual* Optic = FindVisual(ShooterTags::Tag_Attachment_Slot_Optic))
	{
		if (IsValid(Optic->Mesh1P) && !Optic->Data->SightSocketName.IsNone() && Optic->Mesh1P->DoesSocketExist(Optic->Data->SightSocketName))
		{
			OutLocation = Optic->Mesh1P->GetSocketLocation(Optic->Data->SightSocketName);
			return true;
		}
	}

	// 否则用枪自己的 AimSocket（机瞄）
	if (!IsValid(Mesh1P) || AimSocketName.IsNone() || !Mesh1P->DoesSocketExist(AimSocketName))
	{
		return false;
	}

	OutLocation = Mesh1P->GetSocketLocation(AimSocketName);
	return true;
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

	// Snap onto this weapon's grip socket, then put back the offset authored on the mesh.
	// KeepRelativeTransform leaves a holstered gun parented to the previous socket.
	const FAttachmentTransformRules SnapRules = FAttachmentTransformRules::SnapToTargetNotIncludingScale;
	Mesh1P->AttachToComponent(PawnMesh1P, SnapRules, AttachPoint);
	Mesh1P->SetRelativeTransform(AuthoredMesh1PRelative);

	Mesh3P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh3P->AttachToComponent(PawnMesh3P, SnapRules, AttachPoint);
	Mesh3P->SetRelativeTransform(AuthoredMesh3PRelative);
	Mesh3P->RefreshBoneTransforms();

	// 现在知道是不是本地玩家了，把手电挂到正确的模型上
	UpdateFlashlightAttachment();
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

	FVector EyesWorldLocation;
	FRotator EyesWorldRotation;
	PC->GetActorEyesViewPoint(EyesWorldLocation, EyesWorldRotation);
	const FVector EyesWorldDirection = UKismetMathLibrary::GetForwardVector(EyesWorldRotation);

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

void AWeapon::Local_Fire(const FVector& ImpactPoint, const FVector& ImpactNormal, TEnumAsByte<EPhysicalSurface> ImpactSurfaceType, bool bIsFirstPerson)
{
	FireEffect(ImpactPoint, ImpactNormal, ImpactSurfaceType, bIsFirstPerson);

	// Predicted mag decrement happens only on the owning first-person fire path.
	// Third-person multicast is FX only; otherwise listen-server ammo is deducted twice.
	if (!bIsFirstPerson) return;

	APawn* InstigatorPawn = GetInstigator();
	if (IsValid(InstigatorPawn) && InstigatorPawn->IsLocallyControlled())
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
	Ammo = FMath::Clamp(Ammo - 1, 0, MagCapacity);
}

void AWeapon::Rep_Fire(int32 AuthAmmo)
{
	APawn* InstigatorPawn = GetInstigator();
	if (!IsValid(InstigatorPawn) || !InstigatorPawn->IsLocallyControlled() && !GetInstigator()->HasAuthority()) return;

	Sequence = FMath::Max(Sequence - 1, 0);
	Ammo = FMath::Clamp(AuthAmmo - Sequence, 0, MagCapacity);
}

void AWeapon::BeginPlay()
{
	Super::BeginPlay();

	// 默认配件只由服务器装，客户端通过同步拿到
	if (HasAuthority())
	{
		// 每个槽位装上自己的默认配件
		for (const FWeaponAttachmentSlot& Slot : AttachmentSlots)
		{
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
}

void AWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWeapon, EquippedAttachments);
	DOREPLIFETIME(AWeapon, bFlashlightOn);
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

	OnRep_EquippedAttachments();
	ForceNetUpdate();
	return true;
}

void AWeapon::ToggleFlashlight()
{
	if (!HasAuthority() || !HasFlashlight()) return;

	bFlashlightOn = !bFlashlightOn;
	OnRep_FlashlightOn();   // 服务器自己也要亮灯
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

float AWeapon::GetEffectiveAimFOV() const
{
	const UAttachmentData* Optic = GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Optic);
	if (IsValid(Optic) && Optic->bOverridesAimFOV)
	{
		return Optic->AimFOV;
	}
	return AimFeildOfView;
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
	RebuildAttachmentVisuals();
	OnAttachmentsChanged.Broadcast(this);
}

void AWeapon::OnRep_FlashlightOn()
{
	ApplyFlashlightState();
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
	if (!IsValid(Parent) || !IsValid(Data) || !IsValid(Data->Mesh)) return nullptr;

	// Socket 来自枪的槽位表，而不是配件本身
	const FWeaponAttachmentSlot* Slot = FindSlot(Data->SlotTag);
	const FName SocketName = Slot ? Slot->SocketName : NAME_None;
	if (!SocketName.IsNone() && !Parent->DoesSocketExist(SocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("Attachment %s: socket '%s' is missing on %s."), *GetNameSafe(Data), *SocketName.ToString(), *GetNameSafe(Parent->GetSkeletalMeshAsset()));
	}

	// 运行时创建组件：NewObject + 挂到 Socket + RegisterComponent
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
	Comp->SetStaticMesh(Data->Mesh);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->bReceivesDecals = false;
	Comp->CastShadow = !bFirstPerson;   // 和枪一样：1P 不投影，3P 投影
	Comp->SetupAttachment(Parent, SocketName);
	Comp->SetRelativeTransform(Data->MeshOffset);
	Comp->RegisterComponent();
	return Comp;
}

void AWeapon::RebuildAttachmentVisuals()
{
	// 专用服务器不需要画面
	if (GetNetMode() == NM_DedicatedServer) return;

	// 1. 清掉旧模型
	for (FAttachmentVisual& Visual : AttachmentVisuals)
	{
		if (IsValid(Visual.Mesh1P)) Visual.Mesh1P->DestroyComponent();
		if (IsValid(Visual.Mesh3P)) Visual.Mesh3P->DestroyComponent();
	}
	AttachmentVisuals.Reset();

	// 2. 按当前配件重新生成
	for (UAttachmentData* Attachment : EquippedAttachments)
	{
		if (!IsValid(Attachment)) continue;

		FAttachmentVisual& Visual = AttachmentVisuals.AddDefaulted_GetRef();
		Visual.Data = Attachment;
		Visual.Mesh1P = SpawnAttachmentMesh(Mesh1P, Attachment, true);
		Visual.Mesh3P = SpawnAttachmentMesh(Mesh3P, Attachment, false);
	}

	// 3. 手电：有就创建，没有就销毁
	if (HasFlashlight())
	{
		if (!IsValid(Flashlight))
		{
			Flashlight = NewObject<USpotLightComponent>(this);
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

	// 新模型要跟枪一样：本地玩家只显示 1P，其他人只显示 3P
	if (IsValid(GetInstigator()) && Mesh1P->GetAttachParent())
	{
		SetMeshVisibility(GetInstigator());
	}
	ApplyFlashlightState();
}

void AWeapon::UpdateFlashlightAttachment()
{
	if (!IsValid(Flashlight)) return;

	const FAttachmentVisual* Visual = AttachmentVisuals.FindByPredicate([](const FAttachmentVisual& V)
	{
		return IsValid(V.Data) && V.Data->bProvidesFlashlight;
	});
	if (!Visual) return;

	// 本地玩家看到的是 1P 模型，其他人看到的是 3P 模型，灯要跟着看得见的那个
	const APawn* OwningPawn = GetInstigator();
	const bool bLocal = IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	USceneComponent* Parent = bLocal ? Visual->Mesh1P.Get() : Visual->Mesh3P.Get();
	if (!IsValid(Parent))
	{
		Parent = bLocal ? Mesh1P.Get() : Mesh3P.Get();
	}

	const FName Socket = Parent->DoesSocketExist(Visual->Data->LightSocketName) ? Visual->Data->LightSocketName : NAME_None;
	Flashlight->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
}

void AWeapon::ApplyFlashlightState()
{
	if (IsValid(Flashlight))
	{
		Flashlight->SetVisibility(bFlashlightOn);
	}
}

void AWeapon::SetMeshVisibility(APawn* OwningPawn) const
{
	if (!IsValid(OwningPawn)) return;

	const bool bLocal = OwningPawn->IsLocallyControlled();
	Mesh1P->SetHiddenInGame(!bLocal, true);
	Mesh3P->SetHiddenInGame(bLocal, true);
}

