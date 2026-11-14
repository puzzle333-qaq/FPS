


#include "Weapon/Weapon.h"
#include "WeaponData.h"
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
#include "Materials/MaterialInterface.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarWeaponVisLog(
	TEXT("fps.WeaponVisLog"),
	0,
	TEXT("1 = emit [KinMag]/[RarMag]/[KinRT] diagnostic logs"));

#define UE_LOG_WEAPONVIS(Verbosity, Format, ...) \
	do { if (CVarWeaponVisLog.GetValueOnAnyThread() > 0) { UE_LOG(LogTemp, Verbosity, Format, ##__VA_ARGS__); } } while (0)
#include "Engine/World.h"
#include "TimerManager.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimMontage.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "BoneContainer.h"
#include "Engine/SkeletalMeshSocket.h"
#include "ShooterCharacter.h"
#include "ShooterGamePlayTags.h"
#include "UObject/UnrealType.h"

namespace
{
	// Dual-mag: primary mag socket + optional reserve socket (Infima SOCKET_Magazine_Reserve).
	// Reserve stays hidden until reload. SetMeshVisibility propagates hide; re-hide here.
	void HideReserveMagazines(const AActor* Weapon, FName ReserveSocket)
	{
		if (!IsValid(Weapon) || ReserveSocket.IsNone()) return;

		TArray<UStaticMeshComponent*> Meshes;
		Weapon->GetComponents<UStaticMeshComponent>(Meshes);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (!IsValid(Mesh) || Mesh->GetAttachSocketName() != ReserveSocket) continue;
			Mesh->SetHiddenInGame(true);
			Mesh->SetVisibility(false);
		}
	}

	void DestroyReserveMagazines(AActor* Weapon, FName ReserveSocket)
	{
		if (!IsValid(Weapon) || ReserveSocket.IsNone()) return;

		TArray<UStaticMeshComponent*> Meshes;
		Weapon->GetComponents<UStaticMeshComponent>(Meshes);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (IsValid(Mesh) && Mesh->GetAttachSocketName() == ReserveSocket)
			{
				Mesh->DestroyComponent();
			}
		}
	}

	bool ComponentHasSocket(const USceneComponent* Component, FName SocketName)
	{
		return IsValid(Component) && !SocketName.IsNone() && Component->DoesSocketExist(SocketName);
	}

	// 静态网格优先。骨骼配件（消音器、手电、红点）没有静态网格时用骨骼那只。
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

	// PCC 一类：枪身骨骼上有 Muzzle_Barrel_Long/Regular/Short 三个枪口，按当前枪管选。
	FName ResolveBarrelVariantSocket(const USkeletalMeshComponent* WeaponMesh, const UAttachmentData* Barrel, FName SocketName)
	{
		const FString Base = SocketName.ToString();
		static const FString Prefix = TEXT("Muzzle_Barrel_");
		if (!IsValid(WeaponMesh) || !Base.StartsWith(Prefix)) return SocketName;
		FString Variant = TEXT("Regular");
		if (IsValid(Barrel))
		{
			const FString Key = Barrel->GetName() + TEXT("|") + GetNameSafe(Barrel->Mesh.Get());
			if (Key.Contains(TEXT("Long"))) Variant = TEXT("Long");
			else if (Key.Contains(TEXT("Short"))) Variant = TEXT("Short");
		}
		const FName Candidate(*(Prefix + Variant));
		return WeaponMesh->DoesSocketExist(Candidate) ? Candidate : SocketName;
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


namespace OpticTrimT1025
{
	static bool IsKinLongGun(const AWeapon* W)
	{
		if (!IsValid(W) || !W->GetClass()) return false;
		const FString N = W->GetClass()->GetName();
		static const TCHAR* Keys[] = {
			TEXT("Weapon_AK105"), TEXT("Weapon_TR15"), TEXT("Weapon_Mk14"), TEXT("Weapon_MKR9"),
			TEXT("Weapon_MX68"), TEXT("Weapon_R08"), TEXT("Weapon_SRM12"), TEXT("Weapon_Herrington"), TEXT("Weapon_Viper")
		};
		for (const TCHAR* K : Keys) { if (N.Contains(K)) return true; }
		return false;
	}

	static bool IsKeptKinOptic(const UAttachmentData* A)
	{
		if (!IsValid(A)) return false;
		const FString N = A->GetName();
		return N.Contains(TEXT("MicroRedDot")) || N.Contains(TEXT("PhantomTAC"))
			|| N.Contains(TEXT("FPSTP_Holographic")) || N.Contains(TEXT("FPSTP_Reflex"));
	}

#if WITH_EDITOR
	static UAttachmentData* LoadDA(const TCHAR* Path)
	{
		return LoadObject<UAttachmentData>(nullptr, Path);
	}

	static bool SaveObj(UObject* Obj)
	{
		if (!IsValid(Obj)) return false;
		UPackage* Pkg = Obj->GetOutermost();
		if (!Pkg) return false;
		Pkg->FullyLoad();
		Obj->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Pkg->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const bool bOk = UPackage::SavePackage(Pkg, Obj, *Filename, Args);
		UE_LOG(LogTemp, Log, TEXT("[T1025] Save %s -> %d"), *Pkg->GetName(), bOk ? 1 : 0);
		return bOk;
	}

	static void ConfigureOpticsAndTrimAllowlists()
	{
		static bool bDone = false;
		if (bDone) return;
		bDone = true;

		UAttachmentData* Micro = LoadDA(TEXT("/Game/FPS/Data/Kinemation/DA_Kinemation_MicroRedDot.DA_Kinemation_MicroRedDot"));
		UAttachmentData* Phantom = LoadDA(TEXT("/Game/FPS/Data/Kinemation/DA_Kinemation_PhantomTAC.DA_Kinemation_PhantomTAC"));
		UAttachmentData* Holo = LoadDA(TEXT("/Game/FPS/Data/Kinemation/DA_FPSTP_Holographic.DA_FPSTP_Holographic"));
		UAttachmentData* Reflex = LoadDA(TEXT("/Game/FPS/Data/Kinemation/DA_FPSTP_Reflex.DA_FPSTP_Reflex"));

		auto SetupRedDot = [](UAttachmentData* DA, float FOV, const TCHAR* Sight)
		{
			if (!IsValid(DA)) return;
			DA->OpticType = EOpticType::RedDot;
			DA->bOverridesAimFOV = true;
			DA->AimFOV = FOV;
			DA->SightSocketName = Sight;
		};

		SetupRedDot(Micro, 55.f, TEXT("AimPoint"));
		// Phantom like PSR optical: camera/FOV path (NOT Magnified PiP)
		SetupRedDot(Phantom, 40.f, TEXT("AimPoint"));
		if (IsValid(Phantom))
		{
			Phantom->bOverridesEyeDistance = true;
			Phantom->EyeDistance = 12.f;
			Phantom->DisplayName = FText::FromString(TEXT("Phantom TAC Scope"));
		}
		if (IsValid(Holo))
		{
			Holo->OpticType = EOpticType::Holographic;
			Holo->bOverridesAimFOV = true;
			Holo->AimFOV = 55.f;
			if (Holo->SightSocketName.IsNone()) Holo->SightSocketName = TEXT("SightSocket");
		}
		SetupRedDot(Reflex, 55.f, TEXT("SightSocket"));
		if (IsValid(Reflex))
		{
			Reflex->OpticType = EOpticType::RedDot;
			Reflex->bOverridesAimFOV = true;
			Reflex->AimFOV = 55.f;
		}

#if WITH_EDITOR
		if (IsValid(Micro)) { Micro->MarkPackageDirty(); }
		if (IsValid(Phantom)) { Phantom->MarkPackageDirty(); }
		if (IsValid(Holo)) { Holo->MarkPackageDirty(); }
		if (IsValid(Reflex)) { Reflex->MarkPackageDirty(); }
#endif

		TArray<UAttachmentData*> Keep;
		if (IsValid(Micro)) Keep.Add(Micro);
		if (IsValid(Phantom)) Keep.Add(Phantom);
		if (IsValid(Holo)) Keep.Add(Holo);
		if (IsValid(Reflex)) Keep.Add(Reflex);

		static const TCHAR* GunPaths[] = {
			TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_AK105.BP_Weapon_AK105_C"),
			TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_TR15.BP_Weapon_TR15_C"),
			TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_Mk14.BP_Weapon_Mk14_C"),
			TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_MKR9.BP_Weapon_MKR9_C"),
			TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_MX68.BP_Weapon_MX68_C"),
			TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_R08.BP_Weapon_R08_C"),
			TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_SRM12.BP_Weapon_SRM12_C"),
			TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_Herrington.BP_Weapon_Herrington_C"),
			TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_Viper.BP_Weapon_Viper_C"),
		};
		for (const TCHAR* Path : GunPaths)
		{
			UClass* Cls = LoadClass<AWeapon>(nullptr, Path);
			if (!Cls) { UE_LOG(LogTemp, Warning, TEXT("[T1025] missing class %s"), Path); continue; }
			AWeapon* CDO = Cls->GetDefaultObject<AWeapon>();
			if (!CDO) continue;
			bool bChanged = false;
			for (FWeaponAttachmentSlot& Slot : CDO->AttachmentSlots)
			{
				if (!Slot.SlotTag.ToString().Contains(TEXT("Optic"))) continue;
				Slot.AllowedAttachments.Reset();
				for (UAttachmentData* KeepDA : Keep)
				{
					Slot.AllowedAttachments.Add(KeepDA);
				}
				if (Slot.DefaultAttachment && !IsKeptKinOptic(Slot.DefaultAttachment))
				{
					Slot.DefaultAttachment = nullptr;
				}
				bChanged = true;
			}
			if (bChanged)
			{
#if WITH_EDITOR
				CDO->MarkPackageDirty();
#endif
				UE_LOG(LogTemp, Log, TEXT("[T1025] trimmed optics on %s (dirty; Save All to persist)"), *Cls->GetName());
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[T1025] ConfigureOpticsAndTrimAllowlists done keep=%d"), Keep.Num());
	}

	static FAutoConsoleCommand Cmd(
		TEXT("fps.OpticTrimT1025"),
		TEXT("Trim KIN optics to MicroRedDot/Phantom/Holo/Reflex; Phantom FOV scope; configure DAs"),
		FConsoleCommandDelegate::CreateStatic(&ConfigureOpticsAndTrimAllowlists));
#endif
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
	// Original COD-ish feel: vertical primary, mild left/right (not vertical-only; not RoN spray).
	RecoilPattern = {
		FRecoilStep(0.50f,  0.00f),
		FRecoilStep(0.48f,  0.10f),
		FRecoilStep(0.45f,  0.16f),
		FRecoilStep(0.42f,  0.12f),
		FRecoilStep(0.38f, -0.08f),
		FRecoilStep(0.36f, -0.16f),
		FRecoilStep(0.34f, -0.20f),
		FRecoilStep(0.33f, -0.10f),
		FRecoilStep(0.32f,  0.10f),
		FRecoilStep(0.31f,  0.18f),
		FRecoilStep(0.30f,  0.14f),
		FRecoilStep(0.30f,  0.00f)
	};
	RecoilPitchScale = 1.f;
	RecoilYawScale = 1.15f;
	AimRecoilMultiplier = 0.65f;
	RecoilRecoverySpeed = 30.f;
	RecoilRecoveryDelay = 0.06f;
	RecoilResetTime = 0.4f;
	// Mesh Loc/Rot kick kept OFF (user request) — view recoil only
	VisualRecoilLocation = FVector::ZeroVector;
	VisualRecoilRotation = FRotator::ZeroRotator;
	VisualRecoilAimMultiplier = 0.4f;
	VisualRecoilKickSpeed = 50.f;
	VisualRecoilRecoverySpeed = 16.f;
	Damage = 40.f;              // short TTK: was 15 (body ~3 shots @100HP)
	HeadshotMultiplier = 2.5f;  // was 2.0 -> head ~100
	LimbMultiplier = 0.75f;
	WeaponLength = 70.f;
	BlockedHipFireExtra = 8.f;

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
		if (IsValid(OpticComp) && IsValid(Optic->Data))
		{
			// DA 填的名字优先；模型上没有时试常见命名（PCC 红点只有 Sight_ZeroPoint_*，SOP 有 Camera_sckt）
			const FName SightCandidates[] = { Optic->Data->SightSocketName, TEXT("SightSocket"), TEXT("AimPoint"), TEXT("Camera_sckt"),
				TEXT("Sight_ZeroPoint_Near"), TEXT("Sight_ZeroPoint_Mid"), TEXT("Sight_ZeroPoint_Far") };
			for (const FName& Candidate : SightCandidates)
			{
				if (!Candidate.IsNone() && OpticComp->DoesSocketExist(Candidate))
				{
					OutLocation = OpticComp->GetSocketLocation(Candidate);
					return true;
				}
			}
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

bool AWeapon::UsesCustomMesh1PAttach() const
{
	return !Mesh1PAttachSocket.IsNone();
}

FName AWeapon::ResolveMesh1PAttachPoint(const USkeletalMeshComponent* PawnMesh1P) const
{
	if (!IsValid(PawnMesh1P) || Mesh1PAttachSocket.IsNone())
	{
		return NAME_None;
	}

	auto HasBone = [](const USkeletalMeshComponent* Mesh, FName Name) -> bool
	{
		return IsValid(Mesh) && !Name.IsNone() && Mesh->GetBoneIndex(Name) != INDEX_NONE;
	};
	auto HasSocket = [](const USkeletalMeshComponent* Mesh, FName Name) -> bool
	{
		return IsValid(Mesh) && !Name.IsNone() && Mesh->GetSocketByName(Name) != nullptr;
	};
	// Usable for attach: real socket, OR bone that is NOT shadowed by a same-named mesh socket
	// parented elsewhere (AttachToComponent prefers the socket and would freeze the gun).
	auto UsableAnimated = [&](FName Name) -> bool
	{
		if (HasBone(PawnMesh1P, Name))
		{
			if (const USkeletalMeshSocket* Sock = PawnMesh1P->GetSocketByName(Name))
			{
				// Shadow socket on a different parent steals AttachToComponent from the bone.
				if (Sock->BoneName != Name)
				{
					return false;
				}
			}
			return true;
		}
		return HasSocket(PawnMesh1P, Name);
	};

	const FName Requested = Mesh1PAttachSocket;
	const bool bWantsGunBone =
		Requested == FName(TEXT("ik_hand_gun"))
		|| Requested == FName(TEXT("VB ik_hand_gun"))
		|| Requested == FName(TEXT("VB ik_hand_gun_pivot"));

	// RAR-proven path: SOCKET_Weapon is authored on the animated ik_hand_gun bone (identity).
	if (bWantsGunBone && HasSocket(PawnMesh1P, TEXT("SOCKET_Weapon")))
	{
		UE_LOG(LogTemp, Log,
			TEXT("[WeaponAttach] %s remapped '%s' -> SOCKET_Weapon (follows animated ik_hand_gun)."),
			*GetName(), *Requested.ToString());
		return FName(TEXT("SOCKET_Weapon"));
	}
	if (bWantsGunBone && HasSocket(PawnMesh1P, TEXT("WeaponSocket")))
	{
		UE_LOG(LogTemp, Log,
			TEXT("[WeaponAttach] %s remapped '%s' -> WeaponSocket."),
			*GetName(), *Requested.ToString());
		return FName(TEXT("WeaponSocket"));
	}

	if (UsableAnimated(Requested))
	{
		return Requested;
	}

	static const FName Fallbacks[] = {
		FName(TEXT("SOCKET_Weapon")),
		FName(TEXT("WeaponSocket")),
		FName(TEXT("ik_hand_gun")),
		FName(TEXT("weapon_r")),
		FName(TEXT("hand_r"))
	};
	for (const FName& Name : Fallbacks)
	{
		if (UsableAnimated(Name) || (Name != FName(TEXT("ik_hand_gun")) && HasSocket(PawnMesh1P, Name)))
		{
			return Name;
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[WeaponAttach] %s Mesh1PAttachSocket '%s' missing on %s."),
		*GetName(), *Mesh1PAttachSocket.ToString(), *GetNameSafe(PawnMesh1P->GetSkeletalMeshAsset()));
	return NAME_None;
}

namespace
{
	FTransform RefPoseComponentSpace(const FReferenceSkeleton& RefSkel, int32 BoneIndex)
	{
		if (BoneIndex < 0 || BoneIndex >= RefSkel.GetNum())
		{
			return FTransform::Identity;
		}
		FTransform CS = RefSkel.GetRefBonePose()[BoneIndex];
		for (int32 Parent = RefSkel.GetParentIndex(BoneIndex); Parent != INDEX_NONE; Parent = RefSkel.GetParentIndex(Parent))
		{
			CS = CS * RefSkel.GetRefBonePose()[Parent];
		}
		return CS;
	}

	// Gun meshes were authored so the actor origin sits on VB ik_hand_gun_pivot.
	// That pivot is not keyed. Keep its rest offset, but parent the gun to the
	// animated bone so MK14 reload moves it with the right hand.
	FTransform PivotOffsetRelativeToBone(const USkeletalMesh* ArmMesh, FName AnimatedBone)
	{
		if (!IsValid(ArmMesh) || AnimatedBone.IsNone())
		{
			return FTransform::Identity;
		}
		const FReferenceSkeleton& RefSkel = ArmMesh->GetRefSkeleton();
		const int32 GunIdx = RefSkel.FindBoneIndex(AnimatedBone);
		const int32 PivotIdx = RefSkel.FindBoneIndex(TEXT("VB ik_hand_gun_pivot"));
		if (GunIdx == INDEX_NONE || PivotIdx == INDEX_NONE)
		{
			return FTransform::Identity;
		}
		const FTransform GunCS = RefPoseComponentSpace(RefSkel, GunIdx);
		const FTransform PivotCS = RefPoseComponentSpace(RefSkel, PivotIdx);
		FTransform Rel = PivotCS.GetRelativeTransform(GunCS);
		Rel.SetScale3D(FVector::OneVector);
		Rel.NormalizeRotation();
		return Rel;
	}
}

UAnimSequence* AWeapon::GetGripPoseAnim() const
{
	if (const UAttachmentData* Grip = GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Grip))
	{
		if (IsValid(Grip->GripPoseAnim))
		{
			return Grip->GripPoseAnim.Get();
		}
	}
	return nullptr;
}

void AWeapon::ApplyAuthoredArmAnims(FPlayerAnim& InOut) const
{
	// Overlay BP/DA-authored sequences only — no hardcoded asset path tables.
	UAnimSequence* Idle = ArmsIdleSequence.Get();
	UAnimSequence* AimIdle = ArmsAimIdleSequence.Get();
	UAnimSequence* Sprint = ArmsSprintSequence.Get();

	if (IsValid(Idle))
	{
		InOut.IdleAnim = Idle;
	}
	if (IsValid(AimIdle))
	{
		InOut.AimIdleAnim = AimIdle;
	}
	else if (IsValid(Idle) && !IsValid(InOut.AimIdleAnim))
	{
		InOut.AimIdleAnim = Idle;
	}
	if (IsValid(Sprint))
	{
		InOut.SprintAnim = Sprint;
	}
	else if (IsValid(Idle) && !IsValid(InOut.SprintAnim))
	{
		InOut.SprintAnim = Idle;
	}
	if (UAnimSequence* GripPose = GetGripPoseAnim())
	{
		InOut.IdleAnim_Grab = GripPose;
		InOut.LeftHandHoldPose_Grab = GripPose;
	}
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

	auto TryGripComponent = [&OutWorld](USceneComponent* GripComp, const UAttachmentData* Data) -> bool
	{
		if (!IsValid(GripComp) || !IsValid(Data))
		{
			return false;
		}
		const USceneComponent* GripParent = GripComp->GetAttachParent();
		const FName MountSocket = GripComp->GetAttachSocketName();
		// 握把没挂到真实插槽上时，它的 FABRIK 在枪根，不能拿来改扶手。
		const bool bGripMounted = IsValid(GripParent) && (MountSocket.IsNone() || GripParent->DoesSocketExist(MountSocket));
		if (!bGripMounted)
		{
			return false;
		}

		static const FName GripHandNames[] = {
			NAME_None, // filled with Data->FabrikSocketName
			TEXT("GripSocket"), TEXT("FABRIK"), TEXT("ik_hand_l"), TEXT("LeftHandSocket")
		};
		for (FName SocketName : GripHandNames)
		{
			if (SocketName.IsNone())
			{
				SocketName = Data->FabrikSocketName;
			}
			if (ComponentHasSocket(GripComp, SocketName))
			{
				GripComp->UpdateComponentToWorld();
				OutWorld = GripComp->GetSocketTransform(SocketName, RTS_World);
				return true;
			}
		}
		return false;
	};

	if (const FAttachmentVisual* Grip = FindVisual(ShooterTags::Tag_Attachment_Slot_Grip))
	{
		USceneComponent* GripComp = AttachmentViewComponent(*Grip, bFirstPerson);
		if (TryGripComponent(GripComp, Grip->Data.Get()))
		{
			return true;
		}
	}

	// Authored left-hand sockets on the weapon body when no foregrip is mounted.
	const FName FabrikNames[] = {
		FabrikSocketName, TEXT("GripSocket"), TEXT("FABRIK"), TEXT("LeftHandSocket"), TEXT("lefthand"),
		TEXT("IK_Hand_L"), TEXT("hand_l_socket"), TEXT("ik_hand_l")
	};
	for (const FName Name : FabrikNames)
	{
		if (Name.IsNone())
		{
			continue;
		}
		if (ComponentHasSocket(WeaponMesh, Name))
		{
			OutWorld = WeaponMesh->GetSocketTransform(Name, RTS_World);
			UE_LOG(LogTemp, Verbose, TEXT("[FABRIK] %s using socket %s (1P=%d)"),
				*GetName(), *Name.ToString(), bFirstPerson ? 1 : 0);
			return true;
		}
	}

 if(!bFirstPerson&&bHasThirdPersonLeftHandGrip&&IsValid(WeaponMesh))
 {
  OutWorld=ThirdPersonLeftHandGrip*WeaponMesh->GetComponentTransform();
  return true;
 }
	return false;
}

FName AWeapon::GetFallbackMuzzleSocketName() const
{
	const FName AuthoredName = MuzzleSocketName.IsNone() ? FName(TEXT("Muzzle")) : MuzzleSocketName;
	return ResolveBarrelVariantSocket(Mesh1P, GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Barrel), AuthoredName);
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
	const FName Candidates[] = { Visual->Data->MuzzleSocketName, TEXT("MuzzleSocket"), TEXT("SOCKET_Muzzle"), TEXT("SOCKET_Emitter"), TEXT("Muzzle"), TEXT("MuzzleFlash") };
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
	if (TryAttachmentMuzzle(ShooterTags::Tag_Attachment_Slot_Muzzle, bFirstPerson, OutComponent, OutSocketName))
	{
		return true;
	}

	// 枪身自带按枪管区分的枪口（PCC 的 Muzzle_Barrel_Long/Regular/Short）时，优先用它，不用枪管模型上的 Muzzle。
	{
		USkeletalMeshComponent* BodyMesh = bFirstPerson ? Mesh1P : Mesh3P;
		const FName Variant = ResolveBarrelVariantSocket(BodyMesh,
			GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Barrel), GetFallbackMuzzleSocketName());
		if (Variant.ToString().StartsWith(TEXT("Muzzle_Barrel_")) && ComponentHasSocket(BodyMesh, Variant))
		{
			OutComponent = BodyMesh;
			OutSocketName = Variant;
			return true;
		}
	}

	if (TryAttachmentMuzzle(ShooterTags::Tag_Attachment_Slot_Barrel, bFirstPerson, OutComponent, OutSocketName))
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
		for (const FName Candidate : { FName(TEXT("MuzzleSocket")), FName(TEXT("Muzzle")), FName(TEXT("SOCKET_Muzzle")), FName(TEXT("MuzzleFlash")) })
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

	// Mesh1P is RootComponent: unattached RelativeTransform is actor spawn world pose,
	// not a socket-local authored offset. Always identity loc/rot; keep mesh scale only.
	if (IsValid(Mesh1P))
	{
		AuthoredMesh1PRelative = FTransform(FQuat::Identity, FVector::ZeroVector, Mesh1P->GetRelativeScale3D());
	}

	if (IsValid(Mesh3P) && Mesh3P->GetAttachParent() == Mesh1P)
	{
		AuthoredMesh3PRelative = Mesh3P->GetRelativeTransform();
	}
	else if (IsValid(Mesh3P))
	{
		AuthoredMesh3PRelative = FTransform(FQuat::Identity, FVector::ZeroVector, Mesh3P->GetRelativeScale3D());
	}

	bCapturedAuthoredRelative = true;
}

void AWeapon::ApplyMeshAttachRelative(USkeletalMeshComponent* WeaponMesh, bool bFirstPerson)
{
	if (!IsValid(WeaponMesh)) return;

	if (!bCapturedAuthoredRelative)
	{
		CaptureAuthoredMeshRelative();
	}

	const FTransform& Authored = bFirstPerson ? AuthoredMesh1PRelative : AuthoredMesh3PRelative;
	FVector Loc = Authored.GetLocation();
	FQuat Rot = Authored.GetRotation();
	if (!bFirstPerson && !Mesh3PAttachSocket.IsNone()
		&& WeaponMesh->GetAttachSocketName() == Mesh3PAttachSocket)
	{
		// This is a complete socket-local mount, not a correction to the preview
		// component transform. In particular, never substitute the Trigger bone.
		WeaponMesh->SetRelativeTransform(Mesh3PAttachTransform);
		return;
	}
	if (bUseMeshAttachRelativeRotation)
	{
		const FRotator RotOffset = bFirstPerson ? Mesh1PAttachRelativeRotation : Mesh3PAttachRelativeRotation;
		const FVector LocOffset = bFirstPerson ? Mesh1PAttachRelativeLocation : Mesh3PAttachRelativeLocation;
		Loc = Authored.GetLocation() + LocOffset;
		Rot = RotOffset.Quaternion();
	}

	// Bandits FP arms: Ironbelly-tuned rifle Mesh1P sits ~90° yaw left on Epic hand_r.
	// Custom Mesh1PAttachSocket mounts already carry their own correction — do not yaw them.
	if (bFirstPerson && !UsesCustomMesh1PAttach() && WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Rifle))
	{
		if (const USkeletalMeshComponent* ParentMesh = Cast<USkeletalMeshComponent>(WeaponMesh->GetAttachParent()))
		{
			if (const USkeletalMesh* ParentAsset = ParentMesh->GetSkeletalMeshAsset())
			{
				if (ParentAsset->GetPathName().Contains(TEXT("Bandit")))
				{
					Rot = FRotator(0.f, 90.f, 0.f).Quaternion() * Rot;
				}
			}
		}
	}

	WeaponMesh->SetRelativeLocationAndRotation(Loc, Rot);
	WeaponMesh->SetRelativeScale3D(Authored.GetScale3D());
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

	FName ResolveAttachPoint(const USkeletalMeshComponent* Mesh, FName Requested)
	{
		if (IsUsableAttachPoint(Mesh, Requested))
		{
			return Requested;
		}
		static const FName Fallbacks[] = {
			FName(TEXT("VB ik_hand_gun_pivot")), FName(TEXT("ik_hand_gun")),
			FName(TEXT("weapon_r")), FName(TEXT("WeaponSocket")),
			FName(TEXT("RifleSocketak")), FName(TEXT("RifleSocket")),
			FName(TEXT("PistolSocket")), FName(TEXT("PistolSocketX24")),
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

	// 3P 换弹蒙太奇结束也会进这里。战术换弹还在播时不能把备用匣藏掉。
	if (WeaponStatus != EWeaponStatus::Reloading)
	{
		FinishMagazineReloadVisuals();
	}

	CaptureAuthoredMeshRelative();
	SetMeshVisibility(OwningPawn);

	const FName AttachPoint = IPlayerInterface::Execute_GetWeaponAttachPoint(OwningPawn, WeaponType);
	USkeletalMeshComponent* PawnMesh1P = IPlayerInterface::Execute_GetMesh1P(OwningPawn);
	USkeletalMeshComponent* PawnMesh3P = IPlayerInterface::Execute_GetMesh3P(OwningPawn);
	if (!IsValid(PawnMesh1P) || !IsValid(PawnMesh3P)) return;

	// 1P custom: Mesh1PAttachSocket (e.g. ik_hand_gun) + optional pivot rest offset.
	// 1P default: GripPoints (AK = weapon_r on arms). 3P AK: always RifleSocketak + FABRIK (not weapon_r).
	const bool bCustom1P = UsesCustomMesh1PAttach();
	FName Attach1P = NAME_None;
	if (bCustom1P)
	{
		Attach1P = ResolveMesh1PAttachPoint(PawnMesh1P);
	}
	if (Attach1P.IsNone())
	{
		Attach1P = ResolveAttachPoint(PawnMesh1P, AttachPoint);
	}
	FName Attach3P = AttachPoint;
	const bool bCustom3P = !Mesh3PAttachSocket.IsNone() && IsUsableAttachPoint(PawnMesh3P, Mesh3PAttachSocket);
	if (bCustom3P)
	{
		Attach3P = Mesh3PAttachSocket;
	}
	else
	{
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
	}

	const FAttachmentTransformRules SnapRules = bCustom1P
		? FAttachmentTransformRules::SnapToTargetNotIncludingScale
		: FAttachmentTransformRules::SnapToTargetIncludingScale;
	Mesh1P->AttachToComponent(PawnMesh1P, SnapRules, Attach1P);
	if (bCustom1P)
	{
		FTransform Rel = FTransform::Identity;
		if (bMesh1PAttachUsePivotOffset)
		{
			// Pivot offset is only meaningful on KINEMATION pack arms (VB ik_hand_gun_pivot).
			// On Bandits / shared arms that bone is absent → Identity.
			Rel = PivotOffsetRelativeToBone(PawnMesh1P->GetSkeletalMeshAsset(), Attach1P);
		}
		FRotator RotOffset = Mesh1PAttachRotation;
		// SOCKET_Weapon / WeaponSocket are authored identity on the animated ik_hand_gun bone
		// (same mount RAR uses). Ignore the early KINEMATION pilot guess (0,90,-90).
		const bool bRarStyleSocket =
			Attach1P == FName(TEXT("SOCKET_Weapon"))
			|| Attach1P == FName(TEXT("WeaponSocket"));
		if (bRarStyleSocket && RotOffset.Equals(FRotator(0.f, 90.f, -90.f), 1.f))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[WeaponAttach] %s clearing legacy Mesh1PAttachRotation (0,90,-90) on %s"),
				*GetName(), *Attach1P.ToString());
			RotOffset = FRotator::ZeroRotator;
		}
		const FVector Loc = Rel.GetLocation() + Mesh1PAttachLocation;
		const FQuat Rot = RotOffset.Quaternion() * Rel.GetRotation();
		Mesh1P->SetRelativeLocationAndRotation(Loc, Rot);
		if (bCapturedAuthoredRelative)
		{
			Mesh1P->SetRelativeScale3D(AuthoredMesh1PRelative.GetScale3D());
		}
		const USkeletalMeshSocket* Sock = PawnMesh1P->GetSocketByName(Attach1P);
		UE_LOG(LogTemp, Log,
			TEXT("[WeaponAttach] %s target=%s kind=%s parentBone=%s customAttach loc=%s rot=%s"),
			*GetName(),
			*Attach1P.ToString(),
			Sock ? TEXT("socket") : TEXT("bone"),
			Sock ? *Sock->BoneName.ToString() : TEXT("n/a"),
			*Loc.ToString(),
			*Rot.Rotator().ToString());
	}
	else
	{
		ApplyMeshAttachRelative(Mesh1P, true);
	}

	Mesh3P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh3P->AttachToComponent(PawnMesh3P, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Attach3P);
	ApplyMeshAttachRelative(Mesh3P, false);
	if (bCustom3P)
	{
		UE_LOG(LogTemp, Log, TEXT("[WeaponAttach] %s calibrated 3P sock=%s loc=%s rot=%s"),
			*GetName(), *Attach3P.ToString(),
			*Mesh3P->GetRelativeLocation().ToString(),
			*Mesh3P->GetRelativeRotation().ToString());
	}


	Mesh3P->RefreshBoneTransforms();

	const bool b1POk = IsUsableAttachPoint(PawnMesh1P, Mesh1P->GetAttachSocketName());
	const bool b3POk = IsUsableAttachPoint(PawnMesh3P, Mesh3P->GetAttachSocketName());
	UE_LOG(LogTemp, Log,
		TEXT("[WeaponAttach] %s type=%s grip=%s -> 1P sock=%s parent=%s ok=%d | 3P sock=%s parent=%s ok=%d mesh3p=%s"),
		*GetName(), *WeaponType.ToString(), *AttachPoint.ToString(),
		*Mesh1P->GetAttachSocketName().ToString(),
		*GetNameSafe(Mesh1P->GetAttachParent()),
		b1POk ? 1 : 0,
		*Mesh3P->GetAttachSocketName().ToString(),
		*GetNameSafe(Mesh3P->GetAttachParent()),
		b3POk ? 1 : 0,
		*GetNameSafe(PawnMesh3P->GetSkeletalMeshAsset()));
	if (!b1POk || !b3POk)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[WeaponAttach] BAD attach %s 1P=%d 3P=%d (requested grip=%s resolved1P=%s resolved3P=%s)"),
			*GetName(), b1POk ? 1 : 0, b3POk ? 1 : 0,
			*AttachPoint.ToString(), *Attach1P.ToString(), *Attach3P.ToString());
	}

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

	const bool bFirstPerson = (WeaponMesh == Mesh1P);
	FName UseSocket = SocketName;
	if (!bFirstPerson && !Mesh3PAttachSocket.IsNone())
	{
		const FName DefaultGrip = IPlayerInterface::Execute_GetWeaponAttachPoint(OwningPawn, WeaponType);
		const USkeletalMeshSocket* RequestedSocket = PawnMesh->GetSocketByName(SocketName);
		const FName RequestedBone = RequestedSocket ? RequestedSocket->BoneName : SocketName;
		const bool bRightHandGrip = RequestedBone == FName(TEXT("hand_r"))
			|| RequestedBone == FName(TEXT("ik_hand_gun"))
			|| RequestedBone == FName(TEXT("ik_hand_r"))
			|| RequestedBone == FName(TEXT("weapon_r"));
		// Remap right-hand grip notifies only. Reloads may intentionally transfer
		// the weapon to hand_l or a holster; those mounts must remain available.
		if ((SocketName == DefaultGrip || bRightHandGrip)
			&& IsUsableAttachPoint(PawnMesh, Mesh3PAttachSocket))
		{
			UseSocket = Mesh3PAttachSocket;
		}
	}
	if (!IsUsableAttachPoint(PawnMesh, UseSocket))
	{
		UE_LOG(LogTemp, Warning, TEXT("Grip '%s' is missing on %s."), *UseSocket.ToString(), *GetNameSafe(PawnMesh));
		return;
	}

	// Reapply a deterministic local mount on every re-grip; never accumulate it.
	WeaponMesh->AttachToComponent(PawnMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, UseSocket);
	ApplyMeshAttachRelative(WeaponMesh, bFirstPerson);
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
	if (!bCapturedAuthoredRelative)
	{
		CaptureAuthoredMeshRelative();
	}

	if (IsValid(Mesh3P))
	{
		Mesh3P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		if (Mesh3P->GetAttachParent())
		{
			Mesh3P->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		}
		Mesh3P->SetRelativeTransform(AuthoredMesh3PRelative);
	}

	if (IsValid(Mesh1P) && Mesh1P->GetAttachParent())
	{
		Mesh1P->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	}
	if (IsValid(Mesh1P))
	{
		Mesh1P->SetRelativeTransform(AuthoredMesh1PRelative);
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

void AWeapon::Local_Fire(const FVector& ImpactPoint, const FVector& ImpactNormal, TEnumAsByte<EPhysicalSurface> ImpactSurfaceType, bool bIsFirstPerson, bool bConsumeAmmo)
{
	if (WeaponFireSequences.Num() > 0)
	{
		TArray<UAnimSequence*, TInlineAllocator<8>> Choices;
		for (const TObjectPtr<UAnimSequence>& Seq : WeaponFireSequences)
		{
			if (IsValid(Seq))
			{
				Choices.Add(Seq.Get());
			}
		}
		if (Choices.Num() > 0)
		{
			PlayBodySequence(bIsFirstPerson ? Mesh1P : Mesh3P, Choices[FMath::RandRange(0, Choices.Num() - 1)]);
		}
	}

	FireEffect(ImpactPoint, ImpactNormal, ImpactSurfaceType, bIsFirstPerson);

	// Predicted mag decrement happens only on the owning first-person fire path.
	// Third-person multicast is FX only; otherwise listen-server ammo is deducted twice.
	// Shotgun pellets: only the first pellet should consume ammo (bConsumeAmmo=false for the rest).
	if (!bIsFirstPerson || !bConsumeAmmo) return;

	APawn* InstigatorPawn = GetInstigator();
	if (IsValid(InstigatorPawn) && InstigatorPawn->IsLocallyControlled() && UsesAmmo())
	{
		Ammo = FMath::Clamp(Ammo - 1, 0, MagCapacity);
		if (!GetInstigator()->HasAuthority())
		{
			++Sequence;
		}
		UpdateMagazineAmmoVisuals();
	}
}

void AWeapon::AttachCosmeticThirdPerson(USceneComponent* Parent, FName SocketName, FTransform RelativeTransform)
{
	if (!IsValid(Parent)) return;
 CaptureAuthoredMeshRelative();

	SetActorEnableCollision(false);
	SetActorTickEnabled(false);

	// Mesh3P hangs off Mesh1P. Hiding the 1P body must not propagate, or the 3P gun goes with it.
	// Stock and magazine are separate 1P components, so hiding only Mesh1P leaves a second copy.
	if (IsValid(Mesh1P))
	{
		Mesh1P->SetHiddenInGame(true, false);
		Mesh1P->SetVisibility(false, false);
		TArray<USceneComponent*> FirstPersonParts = Mesh1P->GetAttachChildren();
		for (USceneComponent* Child : FirstPersonParts)
		{
			if (!IsValid(Child) || Child == Mesh3P) continue;
			Child->SetHiddenInGame(true, true);
			Child->SetVisibility(false, true);
		}
	}
	if (IsValid(Mesh3P))
	{
		Mesh3P->SetHiddenInGame(false, true);
		Mesh3P->SetVisibility(true, true);
	}
	HideReserveMagazines(this, ReserveMagazineSocketName);
	if (IsValid(Flashlight1P))
	{
		Flashlight1P->SetHiddenInGame(true);
		Flashlight1P->SetVisibility(false);
	}
	if (IsValid(Flashlight3P))
	{
		Flashlight3P->SetHiddenInGame(true);
		Flashlight3P->SetVisibility(false);
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

	AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
	SetActorRelativeTransform(RelativeTransform);
 if(auto* Body=Cast<USkeletalMeshComponent>(Parent);Body&&Mesh3P)
 {
  FName Mount=SocketName;
  if(!Mesh3PAttachSocket.IsNone()&&IsUsableAttachPoint(Body,Mesh3PAttachSocket))Mount=Mesh3PAttachSocket;
  else if(WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_AK))Mount=ResolveAttachPoint(Body,TEXT("RifleSocketak"));
  else Mount=ResolveAttachPoint(Body,Mount);
  Mesh3P->AttachToComponent(Body,FAttachmentTransformRules::SnapToTargetNotIncludingScale,Mount);
  ApplyMeshAttachRelative(Mesh3P,false);
  Mesh3P->SetRelativeTransform(Mesh3P->GetRelativeTransform()*RelativeTransform);
 }
}

void AWeapon::PlayCosmeticFire(const FVector& ImpactPoint)
{
	const FVector From = IsValid(Mesh3P) ? Mesh3P->GetComponentLocation() : GetActorLocation();
	FVector Normal = (ImpactPoint - From).GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		Normal = FVector::UpVector;
	}
	FireEffect(ImpactPoint, Normal, SurfaceType_Default, false);
}

void AWeapon::Auth_Fire()
{
	if (!UsesAmmo()) { return; }
	Ammo = FMath::Clamp(Ammo - 1, 0, MagCapacity);
	UpdateMagazineAmmoVisuals();
}

void AWeapon::Rep_Fire(int32 AuthAmmo)
{
	APawn* InstigatorPawn = GetInstigator();
	if (!IsValid(InstigatorPawn) || !InstigatorPawn->IsLocallyControlled() && !GetInstigator()->HasAuthority()) return;

	Sequence = FMath::Max(Sequence - 1, 0);
	Ammo = FMath::Clamp(AuthAmmo - Sequence, 0, MagCapacity);
	UpdateMagazineAmmoVisuals();
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
	if (!bChamberShellsReady) return;
	if (bChamberShellNotifyBound) return;

	APawn* Pawn = GetInstigator();
	if (!IsValid(Pawn) || !Pawn->Implements<UPlayerInterface>()) return;

	bool bBoundAny = false;
	auto BindMesh = [this, &bBoundAny](USkeletalMeshComponent* Mesh)
	{
		if (!IsValid(Mesh)) return;
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			Anim->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &AWeapon::OnChamberShellNotify);
			bBoundAny = true;
		}
	};

	BindMesh(IPlayerInterface::Execute_GetMesh1P(Pawn));
	BindMesh(IPlayerInterface::Execute_GetMesh3P(Pawn));
	// Only latch once an AnimInstance existed — BeginPlay often runs before arms ABP is ready.
	if (bBoundAny)
	{
		bChamberShellNotifyBound = true;
	}
}

namespace
{
	// RequiredBones drops bones that have no skin weights. HideBone drops them again.
	// A socket is not added back for these mag bones, so an attached mag stays in the well.
	// Merge the bone and its parents back in after every RecalcRequiredBones.
	void MergeBoneChain(TArray<FBoneIndexType>& Bones, const FReferenceSkeleton& Ref, FName BoneName)
	{
		if (BoneName.IsNone()) return;
		const int32 Start = Ref.FindBoneIndex(BoneName);
		if (Start == INDEX_NONE) return;

		TArray<FBoneIndexType> Extra;
		for (int32 Idx = Start; Idx != INDEX_NONE; Idx = Ref.GetParentIndex(Idx))
		{
			Extra.Add(static_cast<FBoneIndexType>(Idx));
		}
		Extra.Sort();

		TArray<FBoneIndexType> Merged;
		Merged.Reserve(Bones.Num() + Extra.Num());
		int32 A = 0;
		int32 B = 0;
		while (A < Bones.Num() && B < Extra.Num())
		{
			if (Bones[A] == Extra[B])
			{
				Merged.Add(Bones[A]);
				++A;
				++B;
			}
			else if (Bones[A] < Extra[B])
			{
				Merged.Add(Bones[A]);
				++A;
			}
			else
			{
				Merged.Add(Extra[B]);
				++B;
			}
		}
		while (A < Bones.Num())
		{
			Merged.Add(Bones[A]);
			++A;
		}
		while (B < Extra.Num())
		{
			Merged.Add(Extra[B]);
			++B;
		}
		Bones = MoveTemp(Merged);
	}

	void AppendKinEvaluatedBones(USkeletalMeshComponent* Mesh, const TArray<FName>& BoneNames)
	{
		if (!IsValid(Mesh) || BoneNames.Num() == 0 || !Mesh->GetSkeletalMeshAsset()) return;

		const FReferenceSkeleton& Ref = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
		const int32 ReqBefore = Mesh->RequiredBones.Num();
		const int32 FillBefore = Mesh->FillComponentSpaceTransformsRequiredBones.Num();
		for (const FName BoneName : BoneNames)
		{
			MergeBoneChain(Mesh->RequiredBones, Ref, BoneName);
			MergeBoneChain(Mesh->FillComponentSpaceTransformsRequiredBones, Ref, BoneName);
		}
		if (Mesh->RequiredBones.Num() == ReqBefore
			&& Mesh->FillComponentSpaceTransformsRequiredBones.Num() == FillBefore)
		{
			return;
		}

		if (TSharedPtr<FBoneContainer> Shared = Mesh->GetSharedRequiredBones())
		{
			Shared->Reset();
		}
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			Anim->RecalcRequiredBones();
		}
	}

	void CollectKinFollowBones(const FString& Cls, TArray<FName>& OutBones)
	{
		if (Cls.Contains(TEXT("Weapon_AK105")))
		{
			OutBones = { TEXT("Magazine_Parent") };
		}
		else if (Cls.Contains(TEXT("Weapon_Mk14")))
		{
			OutBones = { TEXT("Mag") };
		}
		else if (Cls.Contains(TEXT("Weapon_MKR9")))
		{
			OutBones = { TEXT("Mag_Primary"), TEXT("Mag_Secondary") };
		}
		else if (Cls.Contains(TEXT("Weapon_SRM12")))
		{
			OutBones = { TEXT("Magazine") };
		}
		else if (Cls.Contains(TEXT("Weapon_Viper")))
		{
			OutBones = { TEXT("Magazine"), TEXT("MagazineSocket"), TEXT("Mag") };
		}
		else if (Cls.Contains(TEXT("Weapon_R08")))
		{
			OutBones = {
				TEXT("Speedloader"), TEXT("Cylinder"), TEXT("CylinderHolder"), TEXT("CylinderRelease"),
				TEXT("Ammo_01"), TEXT("Ammo_02"), TEXT("Ammo_03"), TEXT("Ammo_04"),
				TEXT("Ammo_05"), TEXT("Ammo_06"), TEXT("Ammo_07"), TEXT("Ammo_08")
			};
		}
		else if (Cls.Contains(TEXT("Weapon_Herrington")))
		{
			OutBones = { TEXT("Cartridge"), TEXT("Feed"), TEXT("Reload"), TEXT("Ejector") };
		}
	}

	void ArmKinEvaluatedBones(USkeletalMeshComponent* Mesh, const TArray<FName>& BoneNames)
	{
		if (!IsValid(Mesh) || BoneNames.Num() == 0 || !Mesh->GetSkeletalMeshAsset()) return;
		// Recalc replaces RequiredBones. Only build the base set when it is still empty,
		// then merge the mag / speedloader / shell bones the clip has to move.
		if (Mesh->RequiredBones.Num() == 0)
		{
			Mesh->RecalcRequiredBones(FMath::Max(0, Mesh->GetPredictedLODLevel()));
		}
		AppendKinEvaluatedBones(Mesh, BoneNames);
	}

	void RefreshKinFollowBones(const AWeapon* Weapon, USkeletalMeshComponent* Mesh)
	{
		if (!IsValid(Weapon) || !IsValid(Mesh)) return;
		const FString Cls = Weapon->GetClass() ? Weapon->GetClass()->GetName() : FString();
		TArray<FName> Bones;
		CollectKinFollowBones(Cls, Bones);
		ArmKinEvaluatedBones(Mesh, Bones);
	}

	bool IsWeaponAnimPlaying(const USkeletalMeshComponent* WeaponMesh)
	{
		if (!IsValid(WeaponMesh)) return false;
		if (const UAnimSingleNodeInstance* Node = WeaponMesh->GetSingleNodeInstance())
		{
			return Node->IsPlaying();
		}
		if (const UAnimInstance* Anim = WeaponMesh->GetAnimInstance())
		{
			return Anim->IsAnyMontagePlaying();
		}
		return false;
	}

	void UpdateKinSecondaryMag(USkeletalMeshComponent* WeaponMesh, USceneComponent* Primary, USkeletalMeshComponent* Secondary)
	{
		if (!IsValid(Secondary)) return;
		bool bShow = false;
		if (IsValid(WeaponMesh) && IsValid(Primary) && IsWeaponAnimPlaying(WeaponMesh))
		{
			const float DistSq = FVector::DistSquared(Primary->GetComponentLocation(), Secondary->GetComponentLocation());
			bShow = DistSq > FMath::Square(8.f);
		}
		Secondary->SetHiddenInGame(!bShow);
		Secondary->SetVisibility(bShow);
	}

	bool ActorHasKinSecondaryMag(const AActor* Actor)
	{
		if (!IsValid(Actor)) return false;
		TArray<USkeletalMeshComponent*> Comps;
		Actor->GetComponents<USkeletalMeshComponent>(Comps);
		for (const USkeletalMeshComponent* Comp : Comps)
		{
			if (IsValid(Comp) && Comp->GetName().Contains(TEXT("KinMagSecondary")))
			{
				return true;
			}
		}
		return false;
	}

	// SRM12 mag-check keeps the same hand grip as reload, but the visible mag stays in the
	// well while the support hand lifts. Hold it on hand_l only for that lift, then put it back.
	bool Srm12MagCheckTime(const AWeapon* Weapon, float& OutTime)
	{
		OutTime = -1.f;
		if (!IsValid(Weapon) || !Weapon->GetClass() || !Weapon->GetClass()->GetName().Contains(TEXT("Weapon_SRM12")))
		{
			return false;
		}
		const USkeletalMeshComponent* Mesh = Weapon->GetMesh1P();
		if (!IsValid(Mesh)) return false;
		if (const UAnimSingleNodeInstance* Node = Mesh->GetSingleNodeInstance())
		{
			const UAnimationAsset* Asset = Node->GetAnimationAsset();
			if (!Node->IsPlaying() || !Asset || !Asset->GetName().Contains(TEXT("Mag_Check")))
			{
				return false;
			}
			OutTime = Node->GetCurrentTime();
			return true;
		}
		if (const UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			if (UAnimMontage* Active = Anim->GetCurrentActiveMontage())
			{
				if (Anim->Montage_IsPlaying(Active) && Active->GetName().Contains(TEXT("Mag_Check")))
				{
					OutTime = Anim->Montage_GetPosition(Active);
					return true;
				}
			}
		}
		return false;
	}

	bool UpdateSrm12MagCheckHand(AWeapon* Weapon)
	{
		static TWeakObjectPtr<USceneComponent> Held;
		static TWeakObjectPtr<USceneComponent> SavedParent;
		static FTransform SavedRel = FTransform::Identity;
		static FName SavedSocket = NAME_None;

		auto Restore = [&]()
		{
			USceneComponent* Mag = Held.Get();
			USceneComponent* Parent = SavedParent.Get();
			if (IsValid(Mag) && IsValid(Parent) && Mag->GetAttachSocketName() == FName(TEXT("hand_l")))
			{
				Mag->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SavedSocket);
				Mag->SetUsingAbsoluteScale(true);
				Mag->SetRelativeTransform(SavedRel);
				Mag->SetRelativeScale3D(FVector::OneVector);
			}
			Held = nullptr;
			SavedParent = nullptr;
			SavedSocket = NAME_None;
		};

		float Time = -1.f;
		const bool bPlaying = Srm12MagCheckTime(Weapon, Time);
		// Authored grip is locked from ~0.75s through ~2.5s. Outside that the mag is back in the well.
		const bool bHold = bPlaying && Time >= 0.70f && Time <= 2.52f;
		if (!bHold)
		{
			Restore();
			return bPlaying;
		}

		USkeletalMeshComponent* Mag = Weapon->GetMagazineSkelMesh1P();
		APawn* Pawn = Weapon->GetInstigator();
		if (!IsValid(Mag) || !IsValid(Pawn) || !Pawn->Implements<UPlayerInterface>())
		{
			return true;
		}
		USkeletalMeshComponent* Arms = IPlayerInterface::Execute_GetMesh1P(Pawn);
		static const FName HandBone(TEXT("hand_l"));
		if (!IsValid(Arms) || Arms->GetBoneIndex(HandBone) == INDEX_NONE)
		{
			return true;
		}

		if (Mag->GetAttachSocketName() != HandBone)
		{
			SavedRel = Mag->GetRelativeTransform();
			SavedParent = Mag->GetAttachParent();
			SavedSocket = Mag->GetAttachSocketName();
			Held = Mag;
			Mag->AttachToComponent(Arms, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandBone);
			Mag->SetUsingAbsoluteScale(true);
		}
		// Mag bone relative to hand_l while the mag-check clip is locked to the support hand.
		// Feed lips are mag local +Z. That grip points them at the ground, so tip +90° about local X.
		const FQuat Grip = FRotator(62.3f, -39.8f, -61.4f).Quaternion();
		const FQuat TipFeed = FQuat(FVector::ForwardVector, FMath::DegreesToRadians(90.f));
		Mag->SetRelativeLocationAndRotation(FVector(8.52f, -2.27f, -1.51f), (Grip * TipFeed).Rotator());
		Mag->SetRelativeScale3D(FVector::OneVector);
		return true;
	}
}

void AWeapon::EnsureKinemationSharedArmsAssets()
{
	const FString Cls = GetClass() ? GetClass()->GetName() : FString();
	const bool bKin =
		Cls.Contains(TEXT("Weapon_AK105")) || Cls.Contains(TEXT("Weapon_TR15")) || Cls.Contains(TEXT("Weapon_Mk14"))
		|| Cls.Contains(TEXT("Weapon_MKR9")) || Cls.Contains(TEXT("Weapon_MX68")) || Cls.Contains(TEXT("Weapon_R08"))
		|| Cls.Contains(TEXT("Weapon_SRM12")) || Cls.Contains(TEXT("Weapon_Herrington")) || Cls.Contains(TEXT("Weapon_Viper"));
	if (!bKin)
	{
		return;
	}

	// Dual-mag Infima reserve path: KIN meshes don't have SOCKET_Magazine_Reserve.
	ReserveMagazineSocketName = NAME_None;

	// Separate mag mesh: AK105 / Mk14 / MKR9 / SRM12 / Viper. Full SKM has the mag bone but the
	// visible mag is SKM_*Mag* on DA_KIN_Mag_*. TR15 and MX68 keep the baked body mag.
	// R08 (speedloader) and Herrington (shell) have no mag mesh; their body bones are forced later.
	struct FKinMagDA
	{
		const TCHAR* Match;
		const TCHAR* Path;
		const TCHAR* Socket;
		const TCHAR* AmmoPrefix;
	};
	static const FKinMagDA MagDAGuns[] = {
		{ TEXT("Weapon_AK105"), TEXT("/Game/FPS/Weapon/Kinemation/Data/DA_KIN_Mag_AK105.DA_KIN_Mag_AK105"), TEXT("Magazine_Parent"), TEXT("Ammo_") },
		{ TEXT("Weapon_Mk14"), TEXT("/Game/FPS/Weapon/Kinemation/Data/DA_KIN_Mag_Mk14.DA_KIN_Mag_Mk14"), TEXT("Mag"), TEXT("Mk14_Ammo_") },
		{ TEXT("Weapon_MKR9"), TEXT("/Game/FPS/Weapon/Kinemation/Data/DA_KIN_Mag_MKR9.DA_KIN_Mag_MKR9"), TEXT("Mag_Primary"), TEXT("Ammo_") },
		{ TEXT("Weapon_SRM12"), TEXT("/Game/FPS/Weapon/Kinemation/Data/DA_KIN_Mag_SRM12.DA_KIN_Mag_SRM12"), TEXT("Magazine"), TEXT("Bullet_") },
		{ TEXT("Weapon_Viper"), TEXT("/Game/FPS/Weapon/Kinemation/Data/DA_KIN_Mag_Viper.DA_KIN_Mag_Viper"), TEXT("Magazine"), TEXT("Ammo_") },
	};
	const FKinMagDA* MagRow = nullptr;
	for (const FKinMagDA& Row : MagDAGuns)
	{
		if (Cls.Contains(Row.Match))
		{
			MagRow = &Row;
			break;
		}
	}
	const bool bNeedsMagDA = MagRow != nullptr;

	struct FKinBodyMag { const TCHAR* Match; const TCHAR* FullMesh; };
	static const FKinBodyMag BodyMagGuns[] = {
		{ TEXT("Weapon_AK105"), TEXT("/Game/KINEMATION/TacticalShooterPack/Weapons/AK105/SKM_AK105.SKM_AK105") },
		{ TEXT("Weapon_TR15"), TEXT("/Game/KINEMATION/TacticalShooterPack/Weapons/TR15/SKM_TR-15.SKM_TR-15") },
		{ TEXT("Weapon_Mk14"), TEXT("/Game/KINEMATION/TacticalShooterPack/Weapons/Mk14EBR/SKM_Mk14EBR.SKM_Mk14EBR") },
		{ TEXT("Weapon_MKR9"), TEXT("/Game/KINEMATION/TacticalShooterPack/Weapons/MKR9/SKM_MKR9.SKM_MKR9") },
		{ TEXT("Weapon_MX68"), TEXT("/Game/KINEMATION/TacticalShooterPack/Weapons/MX68/SKM_MX68.SKM_MX68") },
		{ TEXT("Weapon_R08"), TEXT("/Game/KINEMATION/TacticalShooterPack/Weapons/R08/SKM_R08.SKM_R08") },
		{ TEXT("Weapon_SRM12"), TEXT("/Game/KINEMATION/TacticalShooterPack/Weapons/SRM-12/SKM_SRM-12.SKM_SRM-12") },
		{ TEXT("Weapon_Herrington"), TEXT("/Game/KINEMATION/TacticalShooterPack/Weapons/Herrington_11-87/SKM_Herrington_11-87.SKM_Herrington_11-87") },
		{ TEXT("Weapon_Viper"), TEXT("/Game/KINEMATION/TacticalShooterPack/Weapons/WK-11_Viper/SKM_WK-11_Viper.SKM_WK-11_Viper") },
	};
	const TCHAR* FullPath = nullptr;
	for (const FKinBodyMag& Row : BodyMagGuns)
	{
		if (Cls.Contains(Row.Match)) { FullPath = Row.FullMesh; break; }
	}
	if (FullPath)
	{
		if (USkeletalMesh* Full = LoadObject<USkeletalMesh>(nullptr, FullPath))
		{
			if (IsValid(Mesh1P) && !Mesh1P->GetSkeletalMeshAsset())
			{
				Mesh1P->SetSkeletalMesh(Full);
			}
			if (IsValid(Mesh3P) && !Mesh3P->GetSkeletalMeshAsset())
			{
				Mesh3P->SetSkeletalMesh(Full);
			}
			UE_LOG_WEAPONVIS(Log, TEXT("[KinMag] %s Full mesh fallback 1P=%s 3P=%s"),
				*Cls,
				*GetNameSafe(Mesh1P ? Mesh1P->GetSkeletalMeshAsset() : nullptr),
				*GetNameSafe(Mesh3P ? Mesh3P->GetSkeletalMeshAsset() : nullptr));
		}
	}

	if (bNeedsMagDA)
	{
		const TCHAR* MagDAPath = MagRow->Path;
		const FName MagSock = FName(MagRow->Socket);
		if (MagRow->AmmoPrefix && MagRow->AmmoPrefix[0] != TEXT('\0'))
		{
			const FName RowPrefix(MagRow->AmmoPrefix);
			// Mk14 / SRM12 use a non-default prefix; fill only if unset or still the class default.
			if (MagazineAmmoBonePrefix.IsNone()
				|| (MagazineAmmoBonePrefix == FName(TEXT("Ammo_")) && RowPrefix != FName(TEXT("Ammo_"))))
			{
				MagazineAmmoBonePrefix = RowPrefix;
			}
		}
		if (UAttachmentData* MagDA = LoadObject<UAttachmentData>(nullptr, MagDAPath))
		{
			bool bFoundSlot = false;
			for (FWeaponAttachmentSlot& Slot : AttachmentSlots)
			{
				if (Slot.SlotTag == ShooterTags::Tag_Attachment_Slot_Magazine)
				{
					if (!IsValid(Slot.DefaultAttachment))
					{
						Slot.DefaultAttachment = MagDA;
						Slot.bRequired = true;
					}
					const bool bSocketMissing = !Slot.SocketName.IsNone() && IsValid(Mesh1P)
						&& Mesh1P->GetBoneIndex(Slot.SocketName) == INDEX_NONE
						&& !Mesh1P->DoesSocketExist(Slot.SocketName);
					if (Slot.SocketName.IsNone() || bSocketMissing)
					{
						Slot.SocketName = MagSock;
					}
					if (!Slot.AllowedAttachments.Contains(MagDA))
					{
						Slot.AllowedAttachments.Add(MagDA);
					}
					bFoundSlot = true;
					UE_LOG_WEAPONVIS(Log, TEXT("[KinMag] %s Mag DA restored %s sock=%s ammoPrefix=%s"),
						*Cls, *MagDA->GetName(), *Slot.SocketName.ToString(), *MagazineAmmoBonePrefix.ToString());
				}
			}
			if (!bFoundSlot)
			{
				FWeaponAttachmentSlot NewSlot;
				NewSlot.SlotTag = ShooterTags::Tag_Attachment_Slot_Magazine;
				NewSlot.SocketName = MagSock;
				NewSlot.DefaultAttachment = MagDA;
				NewSlot.bRequired = true;
				NewSlot.AllowedAttachments.Add(MagDA);
				AttachmentSlots.Add(NewSlot);
				UE_LOG_WEAPONVIS(Warning, TEXT("[KinMag] %s Mag slot created with DA %s"), *Cls, *MagDA->GetName());
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[KinMag] %s failed to load Mag DA %s"), *Cls, MagDAPath);
		}
	}
	else if (FullPath)
	{
		// Baked body-mag guns (TR15 path): never spawn Mag DA.
		for (FWeaponAttachmentSlot& Slot : AttachmentSlots)
		{
			if (Slot.SlotTag == ShooterTags::Tag_Attachment_Slot_Magazine)
			{
				Slot.DefaultAttachment = nullptr;
				Slot.bRequired = false;
				UE_LOG_WEAPONVIS(Log, TEXT("[KinMag] %s Mag slot DefaultAttachment cleared (keep body mag)"), *Cls);
			}
		}
		RemoveAttachment(ShooterTags::Tag_Attachment_Slot_Magazine);
		auto UnhideMag = [](USkeletalMeshComponent* Mesh)
		{
			if (!IsValid(Mesh)) return;
			static const FName Bones[] = {
				TEXT("Magazine"), TEXT("Mag"), TEXT("Mag_Primary"), TEXT("Mag_Secondary"),
				TEXT("VB Magazine"), TEXT("Magazine_Parent"), TEXT("mag_01"), TEXT("mag_02"),
				TEXT("MagazineSocket")
			};
			for (const FName Bone : Bones)
			{
				if (Mesh->GetBoneIndex(Bone) != INDEX_NONE)
				{
					Mesh->UnHideBoneByName(Bone);
				}
			}
		};
		UnhideMag(Mesh1P);
		UnhideMag(Mesh3P);
	}

	// Do not clear Blueprint-authored OverrideArms*. If a BP still points at pack
	// ABP_FP_Arms / SK_Arms, set those fields to None on the asset.

	auto SoftLoad = [](const TCHAR* Path) -> UAnimSequence*
	{
		return LoadObject<UAnimSequence>(nullptr, Path);
	};

	struct FKinAnims {
		const TCHAR* Match; // substring of class name
		const TCHAR* Idle; const TCHAR* Equip; const TCHAR* ReloadEmpty; const TCHAR* ReloadTac; const TCHAR* Fire;
		const TCHAR* WFire; const TCHAR* WReloadEmpty; const TCHAR* WReloadTac; const TCHAR* WEquip;
	};
	static const FKinAnims Table[] = {
		{ TEXT("Weapon_AK105"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/AK105/RT_A_FP_AK105_Idle.RT_A_FP_AK105_Idle"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/AK105/RT_A_FP_AK105_Draw.RT_A_FP_AK105_Draw"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/AK105/RT_A_FP_AK105_Reload_Empty.RT_A_FP_AK105_Reload_Empty"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/AK105/RT_A_FP_AK105_Reload_Tac.RT_A_FP_AK105_Reload_Tac"),
		  TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Actions/A_RAR_FP_PCH_AssaultRifle_Fire.A_RAR_FP_PCH_AssaultRifle_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/AK105/Weapon/A_W_AK105_Fire.A_W_AK105_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/AK105/Weapon/A_W_AK105_Reload_Empty.A_W_AK105_Reload_Empty"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/AK105/Weapon/A_W_AK105_Reload_Tac.A_W_AK105_Reload_Tac"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/AK105/Weapon/A_W_AK105_Draw.A_W_AK105_Draw") },
		{ TEXT("Weapon_TR15"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/TR15/RT_A_FP_TR15_Idle_Pose_Non_Grip.RT_A_FP_TR15_Idle_Pose_Non_Grip"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/TR15/RT_A_FP_TR15_Draw.RT_A_FP_TR15_Draw"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/TR15/RT_A_FP_TR15_Reload_Empty.RT_A_FP_TR15_Reload_Empty"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/TR15/RT_A_FP_TR15_Reload_Tactical.RT_A_FP_TR15_Reload_Tactical"),
		  TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Actions/A_RAR_FP_PCH_AssaultRifle_Fire.A_RAR_FP_PCH_AssaultRifle_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/TR15/Weapon/A_W_TR15_Fire.A_W_TR15_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/TR15/Weapon/A_W_TR15_Reload_Empty.A_W_TR15_Reload_Empty"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/TR15/Weapon/A_W_TR15_Reload_Tac.A_W_TR15_Reload_Tac"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/TR15/Weapon/A_W_TR15_Draw.A_W_TR15_Draw") },
		{ TEXT("Weapon_Mk14"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Mk14EBR/RT_A_FP_Mk14EBR_Idle.RT_A_FP_Mk14EBR_Idle"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Mk14EBR/RT_A_FP_Mk14EBR_Draw.RT_A_FP_Mk14EBR_Draw"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Mk14EBR/RT_A_FP_Mk14EBR_Reload_Empty.RT_A_FP_Mk14EBR_Reload_Empty"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Mk14EBR/RT_A_FP_Mk14EBR_Reload_Tactical.RT_A_FP_Mk14EBR_Reload_Tactical"),
		  TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Actions/A_RAR_FP_PCH_AssaultRifle_Fire.A_RAR_FP_PCH_AssaultRifle_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/Mk14EBR/Weapon/A_W_Mk14EBR_Fire.A_W_Mk14EBR_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/Mk14EBR/Weapon/A_W_Mk14EBR_Reload_Empty.A_W_Mk14EBR_Reload_Empty"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/Mk14EBR/Weapon/A_W_Mk14EBR_Reload_Tactical.A_W_Mk14EBR_Reload_Tactical"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/Mk14EBR/Weapon/A_W_Mk14EBR_Draw.A_W_Mk14EBR_Draw") },
		{ TEXT("Weapon_MKR9"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/MKR9/RT_A_FP_MKR9_Idle.RT_A_FP_MKR9_Idle"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/MKR9/RT_A_FP_MKR9_Draw.RT_A_FP_MKR9_Draw"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/MKR9/RT_A_FP_MKR9_Reload_Empty.RT_A_FP_MKR9_Reload_Empty"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/MKR9/RT_A_FP_MKR9_Reload_Tac.RT_A_FP_MKR9_Reload_Tac"),
		  TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Actions/A_RAR_FP_PCH_AssaultRifle_Fire.A_RAR_FP_PCH_AssaultRifle_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/MKR9/Weapon/A_W_MKR9_Fire.A_W_MKR9_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/MKR9/Weapon/A_W_MKR9_Reload_Empty.A_W_MKR9_Reload_Empty"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/MKR9/Weapon/A_W_MKR9_Reload_Tac.A_W_MKR9_Reload_Tac"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/MKR9/Weapon/A_W_MKR9_Draw.A_W_MKR9_Draw") },
		{ TEXT("Weapon_MX68"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/MX68/RT_A_FP_MX68_Idle.RT_A_FP_MX68_Idle"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/MX68/RT_A_FP_MX68_Draw.RT_A_FP_MX68_Draw"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/MX68/RT_A_FP_MX68_Reload_01.RT_A_FP_MX68_Reload_01"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/MX68/RT_A_FP_MX68_Reload_02.RT_A_FP_MX68_Reload_02"),
		  TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Actions/A_RAR_FP_PCH_AssaultRifle_Fire.A_RAR_FP_PCH_AssaultRifle_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/MX68/Weapon/A_W_MX68_Fire_Belt.A_W_MX68_Fire_Belt"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/MX68/Weapon/A_W_MX68_Reload_01.A_W_MX68_Reload_01"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/MX68/Weapon/A_W_MX68_Reload_02.A_W_MX68_Reload_02"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/MX68/Weapon/A_W_MX68_Draw.A_W_MX68_Draw") },
		{ TEXT("Weapon_R08"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/R08/RT_A_FP_R08_Idle.RT_A_FP_R08_Idle"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/R08/RT_A_FP_R08_Draw.RT_A_FP_R08_Draw"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/R08/RT_A_FP_R08_Reload_01.RT_A_FP_R08_Reload_01"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/R08/RT_A_FP_R08_Reload_02.RT_A_FP_R08_Reload_02"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/R08/RT_A_FP_R08_Fire.RT_A_FP_R08_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/R08/Weapon/A_W_R08_Fire.A_W_R08_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/R08/Weapon/A_W_R08_Reload_01.A_W_R08_Reload_01"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/R08/Weapon/A_W_R08_Reload_02.A_W_R08_Reload_02"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/R08/Weapon/A_W_R08_Draw.A_W_R08_Draw") },
		{ TEXT("Weapon_SRM12"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/SRM12/RT_A_FP_SRM-12_Idle_Pose.RT_A_FP_SRM-12_Idle_Pose"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/SRM12/RT_A_FP_SRM-12_Draw.RT_A_FP_SRM-12_Draw"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/SRM12/RT_A_FP_SRM-12_Reload_Empty.RT_A_FP_SRM-12_Reload_Empty"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/SRM12/RT_A_FP_SRM-12_Reload_Tactical.RT_A_FP_SRM-12_Reload_Tactical"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/SRM12/RT_A_FP_SRM-12_Fire.RT_A_FP_SRM-12_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/SRM-12/Weapon/A_W_SRM-12_Fire.A_W_SRM-12_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/SRM-12/Weapon/A_W_SRM-12_Reload_Empty.A_W_SRM-12_Reload_Empty"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/SRM-12/Weapon/A_W_SRM-12_Reload_Tac.A_W_SRM-12_Reload_Tac"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/SRM-12/Weapon/A_W_SRM-12_Draw.A_W_SRM-12_Draw") },
		{ TEXT("Weapon_Herrington"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Herrington/RT_A_FP_Herrington_11-87_Idle.RT_A_FP_Herrington_11-87_Idle"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Herrington/RT_A_FP_Herrington_11-87_Draw.RT_A_FP_Herrington_11-87_Draw"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Herrington/RT_A_FP_Herrington_11-87_Reload_Empty_Start.RT_A_FP_Herrington_11-87_Reload_Empty_Start"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Herrington/RT_A_FP_Herrington_11-87_Reload_Tac_Start.RT_A_FP_Herrington_11-87_Reload_Tac_Start"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Herrington/RT_A_FP_Herrington_11-87_Fire.RT_A_FP_Herrington_11-87_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/Herrington_11-87/Weapon/A_W_Herrington_11-87_Fire.A_W_Herrington_11-87_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/Herrington_11-87/Weapon/A_W_Herrington_11-87_Reload_Empty_Start.A_W_Herrington_11-87_Reload_Empty_Start"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/Herrington_11-87/Weapon/A_W_Herrington_11-87_Reload_Tac_Start.A_W_Herrington_11-87_Reload_Tac_Start"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/Herrington_11-87/Weapon/A_W_Herrington_11-87_Draw.A_W_Herrington_11-87_Draw") },
		{ TEXT("Weapon_Viper"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Viper/RT_A_FP_WK-11_Viper_Idle_Pose.RT_A_FP_WK-11_Viper_Idle_Pose"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Viper/RT_A_FP_WK-11_Viper_Draw.RT_A_FP_WK-11_Viper_Draw"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Viper/RT_A_FP_WK-11_Viper_Reload_Empty.RT_A_FP_WK-11_Viper_Reload_Empty"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Viper/RT_A_FP_WK-11_Viper_Reload_Tac.RT_A_FP_WK-11_Viper_Reload_Tac"),
		  TEXT("/Game/FPS/Weapon/Kinemation/Anims/Viper/RT_A_FP_WK-11_Viper_Fire.RT_A_FP_WK-11_Viper_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/WK-11_Viper/Weapon/A_W_WK-11_Viper_Fire.A_W_WK-11_Viper_Fire"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/WK-11_Viper/Weapon/A_W_WK-11_Viper_Reload_Empty.A_W_WK-11_Viper_Reload_Empty"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/WK-11_Viper/Weapon/A_W_WK-11_Viper_Reload_Tac.A_W_WK-11_Viper_Reload_Tac"),
		  TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/WK-11_Viper/Weapon/A_W_WK-11_Viper_Draw.A_W_WK-11_Viper_Draw") },
	};

	for (const FKinAnims& Row : Table)
	{
		if (!Cls.Contains(Row.Match)) continue;

		auto AssignArms = [&](TObjectPtr<UAnimSequence>& Slot, const TCHAR* Path)
		{
			if (!Path) return;
			if (IsValid(Slot.Get())) return;
			if (UAnimSequence* Seq = SoftLoad(Path))
			{
				Slot = Seq;
			}
		};
		AssignArms(ArmsIdleSequence, Row.Idle);
		AssignArms(ArmsAimIdleSequence, Row.Idle);
		AssignArms(ArmsEquipSequence, Row.Equip);
		AssignArms(ArmsReloadEmptySequence, Row.ReloadEmpty);
		AssignArms(ArmsReloadTacticalSequence, Row.ReloadTac);
		AssignArms(ArmsFireSequence, Row.Fire);
		// Prefer aimed RAR fire when Row.Fire is the hip RAR fallback (rifles with no pack Character Fire).
		if (Row.Fire && FCString::Strstr(Row.Fire, TEXT("A_RAR_FP_PCH_AssaultRifle_Fire.")) && !FCString::Strstr(Row.Fire, TEXT("Aimed")))
		{
			AssignArms(ArmsFireAimSequence, TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Actions/A_RAR_FP_PCH_AssaultRifle_Fire_Aimed.A_RAR_FP_PCH_AssaultRifle_Fire_Aimed"));
			if (!IsValid(ArmsFireAimSequence.Get()))
			{
				AssignArms(ArmsFireAimSequence, Row.Fire);
			}
		}
		else
		{
			AssignArms(ArmsFireAimSequence, Row.Fire);
		}

		auto AssignWpn = [&](TObjectPtr<UAnimSequence>& Slot, const TCHAR* Path)
		{
			if (!Path) return;
			if (IsValid(Slot.Get())) return;
			if (UAnimSequence* Seq = SoftLoad(Path))
			{
				Slot = Seq;
			}
		};
		AssignWpn(WeaponFireSequence, Row.WFire);
		AssignWpn(WeaponReloadEmptySequence, Row.WReloadEmpty);
		AssignWpn(WeaponReloadTacticalSequence, Row.WReloadTac);
		AssignWpn(WeaponEquipSequence, Row.WEquip);
		if (WeaponFireSequences.Num() == 0 && Row.WFire)
		{
			if (UAnimSequence* Seq = SoftLoad(Row.WFire))
			{
				WeaponFireSequences.Add(Seq);
			}
		}
		UE_LOG_WEAPONVIS(Log, TEXT("[KinRT] %s arms fire=%s empty=%s"),
			*Cls,
			*GetNameSafe(ArmsFireSequence.Get()),
			*GetNameSafe(ArmsReloadEmptySequence.Get()));
		break;
	}
}

void AWeapon::BeginPlay()
{
	Super::BeginPlay();
	SanitizeFireType();
	EnsureKinemationSharedArmsAssets();

	// [NoHipSpread] Undo RosterHipfire intentional huge cones (RAR 12 / AK 14 / base 10, etc.).
	// User: no-crosshair already discourages casual hipfire — don't force a huge cone.
	// Restore pre-20261028 small defaults; only shrink, never enlarge a tighter authored value.
	{
		float Target = 3.f;
		const FString Cls = GetClass() ? GetClass()->GetName() : FString();
		if (WeaponKind == EWeaponKind::Melee || WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Melee)
			|| Cls.Contains(TEXT("Knife")))
		{
			Target = 0.f;
		}
		else if (PelletsPerShot > 1
			|| WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_ShotGun)
			|| WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_DoubleBarrel)
			|| WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Winchester)
			|| Cls.Contains(TEXT("Shotgun")) || Cls.Contains(TEXT("ShotGun")))
		{
			Target = 6.f; // original pump/base shotgun hip (~ADS still fans pellets at 0.35x)
		}
		else if (WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_AK) || Cls.Contains(TEXT("AK")))
		{
			Target = 5.f;
		}
		else if (WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_MCX) || Cls.Contains(TEXT("MCX")))
		{
			Target = 5.f;
		}
		else if (WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_PSR) || Cls.Contains(TEXT("PSR")))
		{
			Target = 6.f;
		}
		else if (WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_X24) || Cls.Contains(TEXT("X24")))
		{
			Target = 1.2f;
		}
		else if (WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Deagle) || Cls.Contains(TEXT("Deagle")))
		{
			Target = 1.f;
		}
		else if (WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Pistol)
			|| WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Colt1911)
			|| WeaponKind == EWeaponKind::Pistol
			|| Cls.Contains(TEXT("Pistol")) || Cls.Contains(TEXT("Glock")))
		{
			Target = 1.f;
		}
		else if (Cls.Contains(TEXT("PCC")))
		{
			Target = 3.5f;
		}
		else if (WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Rifle)
			|| Cls.Contains(TEXT("RAR")) || Cls.Contains(TEXT("AR10")) || Cls.Contains(TEXT("HXR")))
		{
			Target = 3.f; // RAR / rifle original
		}
		else if (WeaponKind == EWeaponKind::Throwable
			|| WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Grenade)
			|| WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Smoke)
			|| WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Molotov)
			|| WeaponType.MatchesTagExact(ShooterTags::Tag_WeaponType_Flash))
		{
			Target = 0.f;
		}

		if (HipFireSpread > Target)
		{
			HipFireSpread = Target;
		}
	}

	// [AdsHudYaw] Restore mild horizontal recoil after revert left yaw nearly invisible.
	// Keep pitch as authored; only lift yaw if BP/C++ pattern is effectively vertical-only.
	// Do NOT reintroduce RoN/RecoilFeel2 large scales.
	if (WeaponKind == EWeaponKind::LongGun && RecoilPattern.Num() > 0)
	{
		float MaxAbsYaw = 0.f;
		for (const FRecoilStep& Step : RecoilPattern)
		{
			MaxAbsYaw = FMath::Max(MaxAbsYaw, FMath::Abs(Step.Yaw));
		}
		if (MaxAbsYaw < 0.08f)
		{
			static const float MildYaw[12] = {
				0.00f, 0.10f, 0.16f, 0.12f, -0.08f, -0.16f,
				-0.20f, -0.10f, 0.10f, 0.18f, 0.14f, 0.00f
			};
			for (int32 i = 0; i < RecoilPattern.Num(); ++i)
			{
				RecoilPattern[i].Yaw = MildYaw[i % 12];
			}
		}
		if (RecoilYawScale < 1.f)
		{
			RecoilYawScale = 1.15f;
		}
	}

	if (AuthoredMagCapacity <= 0)
	{
		AuthoredMagCapacity = MagCapacity;
	}

	EnsureDeviceSlots();

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
		UE_LOG(LogTemp, Warning, TEXT("[Attachment] %s rejected %s: no slot '%s', not in AllowedAttachments, or slot locked by %s."),
			*GetName(), *GetNameSafe(Attachment), *Attachment->SlotTag.ToString(), *GetNameSafe(GetSlotBlocker(Attachment->SlotTag)));
		return false;
	}

	// One physical device per rail; light and laser switches remain independent.
	const FGameplayTag NewSlot = GetEquipSlot(Attachment);
	EquippedAttachments.RemoveAll([this, NewSlot](const UAttachmentData* Existing)
	{
		return !IsValid(Existing) || GetEquipSlot(Existing) == NewSlot;
	});
	// 新配件锁住的槽：已装的卸下（例：换短护木自动卸握把）
	if (!Attachment->BlockedSlots.IsEmpty())
	{
		EquippedAttachments.RemoveAll([this, Attachment](const UAttachmentData* Existing)
		{
			if (!IsValid(Existing)) return true;
			const FGameplayTag Slot = GetEquipSlot(Existing);
			return Attachment->BlockedSlots.HasTagExact(Existing->SlotTag) || Attachment->BlockedSlots.HasTagExact(Slot)
				|| (Slot == ShooterTags::Tag_Attachment_Slot_Laser &&
					(Attachment->BlockedSlots.HasTagExact(ShooterTags::Tag_Attachment_Slot_Tactical)
					|| Attachment->BlockedSlots.HasTagExact(ShooterTags::Tag_Attachment_Slot_Flashlight)));
		});
	}
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

	const int32 Removed = EquippedAttachments.RemoveAll([this, SlotTag](const UAttachmentData* Existing)
	{
		return !IsValid(Existing) || GetEquipSlot(Existing) == SlotTag;
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
	if (!HasAuthority() || !HasFlashlight()) return;

	bFlashlightOn = !bFlashlightOn;
	OnRep_FlashlightOn();
	ForceNetUpdate();
}

TArray<FWeaponAttachmentSlot> AWeapon::NormalizeDeviceSlots(const TArray<FWeaponAttachmentSlot>& Slots)
{
	const auto& LaserTag = ShooterTags::Tag_Attachment_Slot_Laser;
	const auto& LightTag = ShooterTags::Tag_Attachment_Slot_Flashlight;
	const auto& TacticalTag = ShooterTags::Tag_Attachment_Slot_Tactical;
	const FWeaponAttachmentSlot* Rail = Slots.FindByPredicate([&](const FWeaponAttachmentSlot& Slot) { return Slot.SlotTag == LaserTag; });
	if (!Rail) Rail = Slots.FindByPredicate([&](const FWeaponAttachmentSlot& Slot) { return Slot.SlotTag == TacticalTag; });
	if (!Rail) Rail = Slots.FindByPredicate([&](const FWeaponAttachmentSlot& Slot) { return Slot.SlotTag == LightTag; });
	if (!Rail) return Slots;
	FWeaponAttachmentSlot Device = *Rail;
	Device.SlotTag = LaserTag;
	Device.AllowedAttachments.Reset();
	Device.DefaultAttachment = nullptr;
	Device.bRequired = false;
	TArray<FWeaponAttachmentSlot> Result;
	int32 InsertIndex = INDEX_NONE;
	int32 DefaultPriority = -1;
	for (const FWeaponAttachmentSlot& Slot : Slots)
	{
		const bool bDedicated = Slot.SlotTag == LaserTag || Slot.SlotTag == LightTag;
		if (!bDedicated && Slot.SlotTag != TacticalTag)
		{
			Result.Add(Slot);
			continue;
		}
		FWeaponAttachmentSlot Other = Slot;
		Other.AllowedAttachments.Reset();
		Other.DefaultAttachment = nullptr;
		for (UAttachmentData* Data : Slot.AllowedAttachments)
		{
			if (!IsValid(Data)) continue;
			if (bDedicated || Data->bProvidesLaser || Data->bProvidesFlashlight) Device.AllowedAttachments.AddUnique(Data);
			else Other.AllowedAttachments.Add(Data);
		}
		UAttachmentData* Default = Slot.DefaultAttachment;
		const bool bDeviceDefault = IsValid(Default) && (bDedicated || Default->bProvidesLaser || Default->bProvidesFlashlight);
		if (bDeviceDefault)
		{
			const int32 Priority = Slot.SlotTag == LaserTag ? 2 : (Slot.SlotTag == TacticalTag ? 1 : 0);
			if (Priority > DefaultPriority)
			{
				Device.DefaultAttachment = Default;
				DefaultPriority = Priority;
			}
			Device.AllowedAttachments.AddUnique(Default);
		}
		else Other.DefaultAttachment = Default;
		if (bDedicated || Other.AllowedAttachments.IsEmpty()) Device.bRequired |= Slot.bRequired;
		if (InsertIndex == INDEX_NONE && (bDedicated || !Device.AllowedAttachments.IsEmpty())) InsertIndex = Result.Num();
		if (!bDedicated && (!Other.AllowedAttachments.IsEmpty() || Other.DefaultAttachment)) Result.Add(Other);
	}
	if (InsertIndex != INDEX_NONE) Result.Insert(Device, InsertIndex);
	return Result;
}

TArray<FWeaponAttachmentSlot> AWeapon::GetAttachmentSlots() const
{
	return NormalizeDeviceSlots(AttachmentSlots);
}

bool AWeapon::HasAttachmentSlot(FGameplayTag SlotTag) const
{
	return GetAttachmentSlots().ContainsByPredicate([&](const FWeaponAttachmentSlot& Slot) { return Slot.SlotTag == SlotTag; });
}

void AWeapon::EnsureDeviceSlots()
{
	// Optic allow-list mutation is editor-only: fps.OpticTrimT1025
	if (bDeviceSlotsReady) return;
	bDeviceSlotsReady = true;
	AttachmentSlots = NormalizeDeviceSlots(AttachmentSlots);
}

FGameplayTag AWeapon::GetEquipSlot(const UAttachmentData* Attachment) const
{
	if (!IsValid(Attachment)) return FGameplayTag();
	if (Attachment->bProvidesLaser || Attachment->bProvidesFlashlight
		|| Attachment->SlotTag == ShooterTags::Tag_Attachment_Slot_Laser
		|| Attachment->SlotTag == ShooterTags::Tag_Attachment_Slot_Flashlight)
	{
		return ShooterTags::Tag_Attachment_Slot_Laser;
	}
	return Attachment->SlotTag;
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
	const TArray<FWeaponAttachmentSlot> Slots = GetAttachmentSlots();
	const FGameplayTag EquipSlot = GetEquipSlot(Data);
	const FWeaponAttachmentSlot* Slot = Slots.FindByPredicate([&](const FWeaponAttachmentSlot& Entry) { return Entry.SlotTag == EquipSlot; });
	FName SlotSocket = Slot ? Slot->SocketName : NAME_None;
	if (SlotSocket.IsNone() && Data->SlotTag == ShooterTags::Tag_Attachment_Slot_Magazine && !MagazineSocketName.IsNone())
	{
		SlotSocket = MagazineSocketName;
	}
	if (Data->SlotTag == ShooterTags::Tag_Attachment_Slot_Grip)
	{
		const FName GripMountNames[] = {
			SlotSocket, FName(TEXT("GripSocket")), FName(TEXT("Attach_AR15_Grip"))
		};
		for (const FName& Name : GripMountNames)
		{
			if (!Name.IsNone() && IsValid(Mesh1P) && Mesh1P->DoesSocketExist(Name))
			{
				return Name;
			}
		}
	}
	return ResolveBarrelVariantSocket(Mesh1P, GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Barrel), SlotSocket);
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
	const TArray<FWeaponAttachmentSlot> Slots = GetAttachmentSlots();
	if (const FWeaponAttachmentSlot* Slot = Slots.FindByPredicate([&](const FWeaponAttachmentSlot& Entry) { return Entry.SlotTag == SlotTag; }))
	{
		const bool bFilterKinOptic = OpticTrimT1025::IsKinLongGun(this)
			&& SlotTag.ToString().Contains(TEXT("Optic"));
		for (UAttachmentData* Attachment : Slot->AllowedAttachments)
		{
			if (!IsValid(Attachment)) continue;
			if (bFilterKinOptic && !OpticTrimT1025::IsKeptKinOptic(Attachment)) continue;
			Result.Add(Attachment);
		}
	}
	return Result;
}

bool AWeapon::CanEquipAttachment(const UAttachmentData* Attachment) const
{
	if (!IsValid(Attachment)) return false;

	const FGameplayTag EquipSlot = GetEquipSlot(Attachment);
	const TArray<FWeaponAttachmentSlot> Slots = GetAttachmentSlots();
	const FWeaponAttachmentSlot* Slot = Slots.FindByPredicate([&](const FWeaponAttachmentSlot& Entry) { return Entry.SlotTag == EquipSlot; });
	if (!Slot) return false;   // 这把枪没有这种槽位

	// 白名单里有它才行
	if (!Slot->AllowedAttachments.Contains(Attachment)) return false;
	if (OpticTrimT1025::IsKinLongGun(this)
		&& EquipSlot.ToString().Contains(TEXT("Optic"))
		&& !OpticTrimT1025::IsKeptKinOptic(Attachment))
	{
		return false;
	}

	// 槽被别的已装配件锁住（例：短护木锁握把）
	if (GetSlotBlocker(EquipSlot)) return false;

	// 自己会锁住必选槽的配件不能装
	for (const FGameplayTag& Blocked : Attachment->BlockedSlots)
	{
		if (Slots.ContainsByPredicate([&](const FWeaponAttachmentSlot& Entry)
		{
			return Entry.bRequired && (Entry.SlotTag == Blocked || (Entry.SlotTag == ShooterTags::Tag_Attachment_Slot_Laser
				&& (Blocked == ShooterTags::Tag_Attachment_Slot_Tactical || Blocked == ShooterTags::Tag_Attachment_Slot_Flashlight)));
		})) return false;
	}
	return true;
}

UAttachmentData* AWeapon::GetSlotBlocker(FGameplayTag SlotTag) const
{
	if (!SlotTag.IsValid()) return nullptr;
	for (UAttachmentData* Equipped : EquippedAttachments)
	{
		if (!IsValid(Equipped) || GetEquipSlot(Equipped).MatchesTagExact(SlotTag)) continue;
		const bool bBlocksDevice = (SlotTag.MatchesTagExact(ShooterTags::Tag_Attachment_Slot_Laser)
				|| SlotTag.MatchesTagExact(ShooterTags::Tag_Attachment_Slot_Flashlight))
			&& (Equipped->BlockedSlots.HasTagExact(ShooterTags::Tag_Attachment_Slot_Tactical)
				|| Equipped->BlockedSlots.HasTagExact(ShooterTags::Tag_Attachment_Slot_Flashlight));
		if (Equipped->BlockedSlots.HasTagExact(SlotTag) || bBlocksDevice)
		{
			return Equipped;
		}
	}
	return nullptr;
}


EGripHoldStyle AWeapon::GetGripHoldStyle() const
{
	const UAttachmentData* Grip = GetAttachmentInSlot(ShooterTags::Tag_Attachment_Slot_Grip);
	if (!IsValid(Grip))
	{
		return EGripHoldStyle::None;
	}
	const FString GripName = Grip->GetName();
	if (GripName.Contains(TEXT("Vert")))
	{
		return EGripHoldStyle::Grab;
	}
	// Angled assets were saved as Grab. The name is the hold the mesh was built for.
	if (GripName.Contains(TEXT("Angl")) || GripName.Contains(TEXT("Cant")) || GripName.Contains(TEXT("45")))
	{
		return EGripHoldStyle::Cradle;
	}
	return Grip->GripHoldStyle == EGripHoldStyle::Grab ? EGripHoldStyle::Grab : EGripHoldStyle::Cradle;
}
UAttachmentData* AWeapon::GetAttachmentInSlot(FGameplayTag SlotTag) const
{
	for (UAttachmentData* Attachment : EquippedAttachments)
	{
		if (IsValid(Attachment) && GetEquipSlot(Attachment) == SlotTag)
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
	UpdateMagazineAmmoVisuals();

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
	float Spread = FMath::Max(0.f, HipFireSpread);
	for (const UAttachmentData* Attachment : EquippedAttachments)
	{
		if (IsValid(Attachment)) Spread *= FMath::Max(0.f, Attachment->HipFireSpreadMultiplier);
	}
	// ADS: single bullet -> zero cone; multi-pellet (shotgun) keeps a tighter cone so pellets still fan.
	if (IsValid(Combat) && Combat->bAiming)
	{
		return (PelletsPerShot > 1) ? Spread * 0.35f : 0.f;
	}
	// Barrel jammed into wall/body: add inaccuracy (longer guns already block ADS more often).
	if (const AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetInstigator()))
	{
		Spread += BlockedHipFireExtra * Shooter->GetWeaponBlockAlpha();
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
	OnDeviceHudChanged.Broadcast();
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
 OnDeviceHudChanged.Broadcast();
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
	float MagCheckTime = -1.f;
	SetActorTickEnabled(bActive || ActorHasKinSecondaryMag(this) || Srm12MagCheckTime(this, MagCheckTime));
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
	const bool bSrm12MagCheck = UpdateSrm12MagCheckHand(this);
	if (!bSrm12MagCheck && !ActorHasKinSecondaryMag(this) && !bLaserOn)
	{
		SetActorTickEnabled(false);
	}

	if (!ActorHasKinSecondaryMag(this)) return;
	const FAttachmentVisual* Mag = FindVisual(ShooterTags::Tag_Attachment_Slot_Magazine);
	TArray<USkeletalMeshComponent*> Comps;
	GetComponents<USkeletalMeshComponent>(Comps);
	for (USkeletalMeshComponent* Comp : Comps)
	{
		if (!IsValid(Comp) || !Comp->GetName().Contains(TEXT("KinMagSecondary"))) continue;
		USkeletalMeshComponent* WeaponMesh = Cast<USkeletalMeshComponent>(Comp->GetAttachParent());
		USceneComponent* Primary = nullptr;
		if (Mag)
		{
			Primary = (WeaponMesh == Mesh1P) ? static_cast<USceneComponent*>(Mag->SkelMesh1P.Get()) : static_cast<USceneComponent*>(Mag->SkelMesh3P.Get());
			if (!IsValid(Primary))
			{
				Primary = (WeaponMesh == Mesh1P) ? static_cast<USceneComponent*>(Mag->Mesh1P.Get()) : static_cast<USceneComponent*>(Mag->Mesh3P.Get());
			}
		}
		UpdateKinSecondaryMag(WeaponMesh, Primary, Comp);
	}
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
 OnDeviceHudChanged.Broadcast();
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
	// SnapToTarget loc/rot onto the rail socket (GripSocket). MeshOffset is an extra tweak.
	Comp->SetRelativeTransform(Data->bSnapToMountSocket ? FTransform::Identity : Data->MeshOffset);

	Comp->RegisterComponent();

	// FPSTP Holo/Reflex: pack BP mounts a separate Reticle plane; DA only spawns body mesh.
	{
		const FString AttName = Data->GetName();
		const bool bHolo = AttName.Contains(TEXT("FPSTP_Holographic"));
		const bool bReflex = AttName.Contains(TEXT("FPSTP_Reflex"));
		if (bFirstPerson && (bHolo || bReflex))
		{
			static UStaticMesh* ScopePlane = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/FPSTP/Content/Weapons/Attachments/Param/StaticMesh/SM_ScopePlane.SM_ScopePlane"));
			UMaterialInterface* ReticleMI = nullptr;
			if (bHolo)
			{
				ReticleMI = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FPSTP/Content/Weapons/Attachments/AttachmentsList/Sight/Holographic/Materials/MI_HolographicReticle_Var1.MI_HolographicReticle_Var1"));
			}
			else
			{
				ReticleMI = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FPSTP/Content/Weapons/Attachments/AttachmentsList/Sight/Reflex/Materials/MI_ReflexSightReticleVar1.MI_ReflexSightReticleVar1"));
				if (!IsValid(ReticleMI))
				{
					ReticleMI = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FPSTP/Content/Weapons/Attachments/AttachmentsList/Sight/Reflex/Materials/MI_ReflexSightReticleVar2.MI_ReflexSightReticleVar2"));
				}
			}
			if (IsValid(ScopePlane) && IsValid(ReticleMI))
			{
				FName PlaneSocket = NAME_None;
				const FName Cands[] = { Data->SightSocketName, FName(TEXT("AimPoint")), FName(TEXT("SightSocket")) };
				for (const FName Cand : Cands)
				{
					if (!Cand.IsNone() && Comp->DoesSocketExist(Cand)) { PlaneSocket = Cand; break; }
				}
				UStaticMeshComponent* Plane = NewObject<UStaticMeshComponent>(this);
				Plane->SetMobility(EComponentMobility::Movable);
				Plane->SetStaticMesh(ScopePlane);
				Plane->SetMaterial(0, ReticleMI);
				Plane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Plane->bReceivesDecals = false;
				Plane->CastShadow = false;
				if (!PlaneSocket.IsNone())
				{
					Plane->SetupAttachment(Comp, PlaneSocket);
				}
				else
				{
					Plane->SetupAttachment(Comp);
				}
				Plane->SetRelativeTransform(FTransform::Identity);
				Plane->SetRelativeScale3D(FVector(0.04f, 0.04f, 0.04f));
				Plane->RegisterComponent();
				UE_LOG(LogTemp, Log, TEXT("[OpticReticle] %s socket=%s"), *AttName, *PlaneSocket.ToString());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[OpticReticle] missing assets for %s"), *AttName);
			}
		}
	}

	// 第一人称换弹要两只弹夹。装回去的那只挂在备用骨上，动画把它送进弹仓。
	if (bFirstPerson
		&& Data->SlotTag == ShooterTags::Tag_Attachment_Slot_Magazine
		&& Parent->DoesSocketExist(ReserveMagazineSocketName))
	{
		UStaticMeshComponent* Reserve = NewObject<UStaticMeshComponent>(this);
		Reserve->SetMobility(EComponentMobility::Movable);
		Reserve->SetStaticMesh(Data->Mesh);
		Reserve->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Reserve->bReceivesDecals = false;
		Reserve->CastShadow = false;
		Reserve->SetupAttachment(Parent, ReserveMagazineSocketName);
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
	Comp->SetRelativeTransform(Data->bSnapToMountSocket ? FTransform::Identity : Data->MeshOffset);

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


void AWeapon::UpdateMagazineAmmoVisuals()
{
	if (GetNetMode() == NM_DedicatedServer) return;

	const int32 Rounds = UsesAmmo() ? FMath::Max(0, Ammo) : 0;

	auto ApplyAmmoBones = [this, Rounds](USkeletalMeshComponent* Comp)
	{
		if (!IsValid(Comp) || MagazineAmmoBonePrefix.IsNone()) return;
		USkeletalMesh* Skel = Comp->GetSkeletalMeshAsset();
		if (!IsValid(Skel)) return;

		const FReferenceSkeleton& RefSkel = Skel->GetRefSkeleton();
		const FString Prefix = MagazineAmmoBonePrefix.ToString();
		int32 Found = 0;
		for (int32 BoneIdx = 0; BoneIdx < RefSkel.GetNum(); ++BoneIdx)
		{
			const FName BoneName = RefSkel.GetBoneName(BoneIdx);
			const FString BoneStr = BoneName.ToString();
			if (!BoneStr.StartsWith(Prefix)) continue;
			const FString Suffix = BoneStr.Mid(Prefix.Len());
			if (Suffix.IsEmpty() || !Suffix.IsNumeric()) continue;
			const int32 Slot = FCString::Atoi(*Suffix); // Ammo_001 => 1
			if (Slot <= 0) continue;
			++Found;
			const bool bShow = Slot <= Rounds;
			if (bShow)
			{
				Comp->UnHideBoneByName(BoneName);
			}
			else
			{
				Comp->HideBoneByName(BoneName, EPhysBodyOp::PBO_None);
			}
		}
		if (Found > 0)
		{
			Comp->MarkRenderStateDirty();
		}
	};

	// Mag attachment skels (KINEMATION SK_Mag_* / SKM_Mag_*).
	if (const FAttachmentVisual* Mag = FindVisual(ShooterTags::Tag_Attachment_Slot_Magazine))
	{
		ApplyAmmoBones(Mag->SkelMesh1P.Get());
		ApplyAmmoBones(Mag->SkelMesh3P.Get());

		// RAR static Empty/Full swap when MagEmptyMesh set (or auto _Full -> _Empty sibling).
		auto ResolveEmpty = [](const UAttachmentData* Data) -> UStaticMesh*
		{
			if (!IsValid(Data)) return nullptr;
			if (IsValid(Data->MagEmptyMesh)) return Data->MagEmptyMesh.Get();
			if (!IsValid(Data->Mesh)) return nullptr;
			const FString PathName = Data->Mesh->GetPathName();
			if (!PathName.Contains(TEXT("_Full"))) return nullptr;
			const FString EmptyPath = PathName.Replace(TEXT("_Full"), TEXT("_Empty"));
			return LoadObject<UStaticMesh>(nullptr, *EmptyPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		};

		if (IsValid(Mag->Data) && IsValid(Mag->Data->Mesh))
		{
			UStaticMesh* FullMesh = Mag->Data->Mesh.Get();
			UStaticMesh* EmptyMesh = ResolveEmpty(Mag->Data);
			UStaticMesh* Want = (Rounds > 0 || !IsValid(EmptyMesh)) ? FullMesh : EmptyMesh;
			auto SwapSM = [Want](UStaticMeshComponent* Comp)
			{
				if (!IsValid(Comp) || !IsValid(Want)) return;
				if (Comp->GetStaticMesh() != Want)
				{
					Comp->SetStaticMesh(Want);
				}
			};
			SwapSM(Mag->Mesh1P.Get());
			SwapSM(Mag->Mesh3P.Get());
		}
	}

	// Also scan gun body in case Ammo_* live on the weapon skel.
	ApplyAmmoBones(Mesh1P);
	ApplyAmmoBones(Mesh3P);
}


void AWeapon::PlayMagazineReload(EReloadType ReloadType)
{
	// RAR-like: weapon body reload drives Mag socket/bone; Mag DA stays attached and follows.
	// Empty and Tactical both need the body sequence on KIN Mag-DA guns (no MagReloadAnim on Mag DA).
	UAnimSequence* BodySeq = nullptr;
	if (ReloadType == EReloadType::Empty)
	{
		BodySeq = WeaponReloadEmptySequence.Get();
	}
	else if (ReloadType == EReloadType::Tactical)
	{
		BodySeq = WeaponReloadTacticalSequence.Get();
	}
	else if (ReloadType == EReloadType::First)
	{
		BodySeq = WeaponReloadEmptySequence.Get() ? WeaponReloadEmptySequence.Get() : WeaponReloadTacticalSequence.Get();
	}
	if (!IsValid(BodySeq))
	{
		BodySeq = WeaponReloadEmptySequence.Get() ? WeaponReloadEmptySequence.Get() : WeaponReloadTacticalSequence.Get();
	}
	if (IsValid(BodySeq))
	{
		PlayBodySequence(Mesh1P, BodySeq);
		PlayBodySequence(Mesh3P, BodySeq);
		UE_LOG_WEAPONVIS(Log, TEXT("[KinMag] PlayMagazineReload body=%s type=%d"), *BodySeq->GetName(), (int32)ReloadType);
	}

	// Infima dual-hand: SOCKET_Magazine (old, pulled) + SOCKET_Magazine_Reserve (new, held)
	// are both on the GUN mesh; WEP reload moves them to the hands. Keep reserve visible
	// for the whole reload — do NOT hide via Infima notify timers (that regressed to 1 mag).
	bReloadReserveMagVisible = true;
	ShowReserveMagazineForReload();

	// Keep the chambered/main mag visible while the spare is out.
	if (const FAttachmentVisual* MagVis = FindVisual(ShooterTags::Tag_Attachment_Slot_Magazine))
	{
		if (IsValid(MagVis->Mesh1P))
		{
			MagVis->Mesh1P->SetHiddenInGame(false);
			MagVis->Mesh1P->SetVisibility(true);
		}
		if (IsValid(MagVis->SkelMesh1P))
		{
			MagVis->SkelMesh1P->SetHiddenInGame(false);
			MagVis->SkelMesh1P->SetVisibility(true);
		}
	}

	USkeletalMeshComponent* MagMesh = GetMagazineSkelMesh1P();
	if (IsValid(MagMesh))
	{
		const FAttachmentVisual* Mag = FindVisual(ShooterTags::Tag_Attachment_Slot_Magazine);
		if (Mag && IsValid(Mag->Data))
		{
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
				Seq = Mag->Data->MagReloadAnim ? Mag->Data->MagReloadAnim.Get() : Mag->Data->MagEmptyReloadAnim.Get();
			}
			if (IsValid(Seq))
			{
				MagMesh->PlayAnimation(Seq, false);
			}
		}
	}
}

void AWeapon::ShowReserveMagazineForReload()
{
	// Dual-mag only when the reserve socket exists on Mesh1P (RAR). Pack guns without it skip.
	if (ReserveMagazineSocketName.IsNone() || !IsValid(Mesh1P) || !Mesh1P->DoesSocketExist(ReserveMagazineSocketName))
	{
		return;
	}

	bool bHasReserve = false;
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh) || Mesh->GetAttachSocketName() != ReserveMagazineSocketName) continue;
		Mesh->SetHiddenInGame(false);
		Mesh->SetVisibility(true);
		bHasReserve = true;
	}

	if (!bHasReserve && IsValid(Mesh1P) && Mesh1P->DoesSocketExist(ReserveMagazineSocketName))
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
				Reserve->SetupAttachment(Mesh1P, ReserveMagazineSocketName);
				Reserve->SetRelativeTransform(MagVisual->Data->MeshOffset);
				Reserve->SetHiddenInGame(false);
				Reserve->SetVisibility(true);
				Reserve->RegisterComponent();
				bHasReserve = true;
			}
		}
	}

	const bool bSock = IsValid(Mesh1P) && Mesh1P->DoesSocketExist(ReserveMagazineSocketName);
	UE_LOG_WEAPONVIS(Warning, TEXT("[RarMag] ShowReserve sock=%d hasReserve=%d weapon=%s"),
		bSock ? 1 : 0, bHasReserve ? 1 : 0, *GetName());
	if (!bHasReserve)
	{
		UE_LOG_WEAPONVIS(Warning, TEXT("[RarMag] no reserve mag — check DA magazine Mesh + SOCKET_Magazine_Reserve on Mesh1P"));
	}
}

void AWeapon::FinishMagazineReloadVisuals()
{
	UpdateMagazineAmmoVisuals();
	bReloadReserveMagVisible = false;
	HideReserveMagazines(this, ReserveMagazineSocketName);
}


void AWeapon::ResolveMagazineVisualDuplicates()
{
	const FString Cls = GetClass() ? GetClass()->GetName() : FString();
	// Body geo is the mag: TR15 (merged mag), MX68 (belt), R08 (speedloader), Herrington (shell).
	// AK105 / Mk14 / MKR9 / SRM12 / Viper keep Mag DA on the animated mag bone.
	const bool bKinKeepBodyMag =
		Cls.Contains(TEXT("Weapon_TR15")) || Cls.Contains(TEXT("Weapon_MX68"))
		|| Cls.Contains(TEXT("Weapon_R08")) || Cls.Contains(TEXT("Weapon_Herrington"));

	// KIN pack body-mag guns: drop Mag DA; Full mesh already has the mag.
	if (bKinKeepBodyMag)
	{
		auto UnhideMag = [](USkeletalMeshComponent* Mesh)
		{
			if (!IsValid(Mesh)) return;
			static const FName Bones[] = {
				TEXT("Magazine"), TEXT("Mag"), TEXT("Mag_Primary"), TEXT("Mag_Secondary"),
				TEXT("VB Magazine"), TEXT("Magazine_Parent"), TEXT("mag_01"), TEXT("mag_02"),
				TEXT("MagazineSocket")
			};
			for (const FName Bone : Bones)
			{
				if (Mesh->GetBoneIndex(Bone) != INDEX_NONE)
				{
					Mesh->UnHideBoneByName(Bone);
				}
			}
		};
		UnhideMag(Mesh1P);
		UnhideMag(Mesh3P);

		// Strip Mag DA from equipped list + visuals.
		RemoveAttachment(ShooterTags::Tag_Attachment_Slot_Magazine);

		TArray<USkeletalMeshComponent*> Comps;
		GetComponents<USkeletalMeshComponent>(Comps);
		for (USkeletalMeshComponent* Comp : Comps)
		{
			if (!IsValid(Comp) || Comp == Mesh1P || Comp == Mesh3P) continue;
			const FString N = Comp->GetName();
			const FName Sock = Comp->GetAttachSocketName();
			const bool bLooksMag = N.Contains(TEXT("Mag")) || Sock.ToString().Contains(TEXT("Mag"));
			if (!bLooksMag) continue;
			UE_LOG_WEAPONVIS(Warning, TEXT("[KinMag] destroying Mag DA/orphan %s sock=%s"), *N, *Sock.ToString());
			Comp->DestroyComponent();
		}
		TArray<UStaticMeshComponent*> SMs;
		GetComponents<UStaticMeshComponent>(SMs);
		for (UStaticMeshComponent* Comp : SMs)
		{
			if (!IsValid(Comp)) continue;
			const FString N = Comp->GetName();
			if (!N.Contains(TEXT("Mag"))) continue;
			UE_LOG_WEAPONVIS(Warning, TEXT("[KinMag] destroying Mag SM %s"), *N);
			Comp->DestroyComponent();
		}

		// Speedloader / shell bones often have no skin weights, so the clip never moves them.
		TArray<FName> BodyBones;
		CollectKinFollowBones(Cls, BodyBones);
		if (BodyBones.Num() > 0)
		{
			auto UnhideNamed = [&BodyBones](USkeletalMeshComponent* Mesh)
			{
				if (!IsValid(Mesh)) return;
				for (const FName Bone : BodyBones)
				{
					if (Mesh->GetBoneIndex(Bone) != INDEX_NONE)
					{
						Mesh->UnHideBoneByName(Bone);
					}
				}
			};
			UnhideNamed(Mesh1P);
			UnhideNamed(Mesh3P);
			ArmKinEvaluatedBones(Mesh1P, BodyBones);
			ArmKinEvaluatedBones(Mesh3P, BodyBones);
		}

		UE_LOG_WEAPONVIS(Log, TEXT("[KinMag] %s keep body mag; Mag DA dropped"), *GetName());
		return;
	}

	const FAttachmentVisual* Mag = FindVisual(ShooterTags::Tag_Attachment_Slot_Magazine);
	if (!Mag) return;

	// Mag-DA guns: same as RAR — Mag DA stays on the magazine socket/bone (SOCKET_Magazine style).
	// Weapon MagCheck / MagReloadAnim move it with the gun; do NOT reparent to hand_l.
	// Hide builtin body mag bones so only the Mag DA shows.
	// IMPORTANT: HideBone zeros bone scale. Mag DA attached to that bone would collapse unless
	// it uses absolute scale (keeps size while still following bone loc/rot — RAR-like).
	FName MagAttachBone = NAME_None;
	if (IsValid(Mag->SkelMesh1P.Get())) MagAttachBone = Mag->SkelMesh1P->GetAttachSocketName();
	else if (IsValid(Mag->Mesh1P.Get())) MagAttachBone = Mag->Mesh1P->GetAttachSocketName();

	auto HideBodyMagBones = [](USkeletalMeshComponent* WeaponMesh)
	{
		if (!IsValid(WeaponMesh)) return;
		static const FName Bones[] = {
			TEXT("Magazine"), TEXT("Mag"), TEXT("Mag_Primary"), TEXT("Mag_Secondary"),
			TEXT("VB Magazine"), TEXT("Magazine_Parent"), TEXT("MagazineSocket"), TEXT("mag_01"), TEXT("mag_02")
		};
		for (const FName Bone : Bones)
		{
			if (WeaponMesh->GetBoneIndex(Bone) != INDEX_NONE)
			{
				WeaponMesh->HideBoneByName(Bone, EPhysBodyOp::PBO_None);
			}
		}
	};
	HideBodyMagBones(Mesh1P);
	HideBodyMagBones(Mesh3P);

	auto PrepMagDA = [](UPrimitiveComponent* Comp)
	{
		if (!IsValid(Comp)) return;
		Comp->SetHiddenInGame(false);
		Comp->SetVisibility(true);
		Comp->SetOwnerNoSee(false);
		// Survive HideBone scale=0 on the mount bone so MagCheck/reload still moves Mag DA.
		Comp->SetUsingAbsoluteScale(true);
		Comp->SetRelativeScale3D(FVector::OneVector);
	};
	if (IsValid(Mag->SkelMesh3P.Get()))
	{
		Mag->SkelMesh3P->SetOwnerNoSee(true);
		Mag->SkelMesh3P->SetUsingAbsoluteScale(true);
		Mag->SkelMesh3P->SetRelativeScale3D(FVector::OneVector);
	}
	if (IsValid(Mag->Mesh3P.Get()))
	{
		Mag->Mesh3P->SetOwnerNoSee(true);
		Mag->Mesh3P->SetUsingAbsoluteScale(true);
		Mag->Mesh3P->SetRelativeScale3D(FVector::OneVector);
	}
	if (IsValid(Mag->SkelMesh1P.Get()))
	{
		PrepMagDA(Mag->SkelMesh1P.Get());
	}
	if (IsValid(Mag->Mesh1P.Get()))
	{
		PrepMagDA(Mag->Mesh1P.Get());
	}
	UE_LOG_WEAPONVIS(Log, TEXT("[KinMag] Mag DA absolute-scale on attach=%s (HideBone-safe)"), *MagAttachBone.ToString());

	TArray<USkeletalMeshComponent*> Comps;
	GetComponents<USkeletalMeshComponent>(Comps);
	for (USkeletalMeshComponent* Comp : Comps)
	{
		if (!IsValid(Comp) || Comp == Mesh1P || Comp == Mesh3P) continue;
		if (Comp == Mag->SkelMesh1P.Get() || Comp == Mag->SkelMesh3P.Get()) continue;
		const FString N = Comp->GetName();
		const FName Sock = Comp->GetAttachSocketName();
		const bool bLooksMag = N.Contains(TEXT("Mag")) || Sock.ToString().Contains(TEXT("Mag"));
		if (!bLooksMag) continue;
		UE_LOG_WEAPONVIS(Warning, TEXT("[KinMag] destroying orphan mag comp %s sock=%s"), *N, *Sock.ToString());
		Comp->DestroyComponent();
	}

	// HideBone drops the mag bone out of RequiredBones (no skin weights, Mesh1P is not
	// AlwaysTickPoseAndRefreshBones). Put it back so the attached Mag DA follows the clip.
	TArray<FName> FollowBones;
	CollectKinFollowBones(Cls, FollowBones);
	if (!MagAttachBone.IsNone())
	{
		FollowBones.AddUnique(MagAttachBone);
	}
	ArmKinEvaluatedBones(Mesh1P, FollowBones);
	ArmKinEvaluatedBones(Mesh3P, FollowBones);

	// MKR9 keys two mag bones. The second copy stays hidden until those bones separate
	// during reload / mag check, so idle does not draw two magazines in the well.
	if (Cls.Contains(TEXT("Weapon_MKR9")))
	{
		auto SpawnSecondary = [this](USkeletalMeshComponent* WeaponMesh, USkeletalMeshComponent* Primary, bool bFirstPerson)
		{
			if (!IsValid(WeaponMesh) || !IsValid(Primary) || !IsValid(Primary->GetSkeletalMeshAsset())) return;
			const FName BoneName(TEXT("Mag_Secondary"));
			if (WeaponMesh->GetBoneIndex(BoneName) == INDEX_NONE) return;

			USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(this,
				MakeUniqueObjectName(this, USkeletalMeshComponent::StaticClass(),
					bFirstPerson ? TEXT("KinMagSecondary1P") : TEXT("KinMagSecondary3P")));
			Comp->SetMobility(EComponentMobility::Movable);
			Comp->SetSkeletalMesh(Primary->GetSkeletalMeshAsset());
			Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Comp->bReceivesDecals = false;
			Comp->CastShadow = !bFirstPerson;
			Comp->SetupAttachment(WeaponMesh, BoneName);
			Comp->SetRelativeTransform(Primary->GetRelativeTransform());
			Comp->SetUsingAbsoluteScale(Primary->IsUsingAbsoluteScale());
			Comp->SetHiddenInGame(true);
			Comp->SetVisibility(false);
			if (!bFirstPerson)
			{
				Comp->SetOwnerNoSee(true);
			}
			Comp->RegisterComponent();
		};
		SpawnSecondary(Mesh1P, Mag->SkelMesh1P.Get(), true);
		SpawnSecondary(Mesh3P, Mag->SkelMesh3P.Get(), false);
		if (GetNetMode() != NM_DedicatedServer)
		{
			SetActorTickEnabled(true);
		}
	}

	UE_LOG_WEAPONVIS(Log, TEXT("[KinMag] %s Mag DA on mag socket (RAR-like); body mag bones hidden"), *GetName());
}

void AWeapon::RebuildAttachmentVisuals()
{
	// 专用服务器不需要画面
	if (GetNetMode() == NM_DedicatedServer) return;

	// 手电挂在配件模型上。先拆下来，避免销毁模型时被带到枪根上并留下隐藏标记。
	if (IsValid(Flashlight1P))
	{
		Flashlight1P->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	}
	if (IsValid(Flashlight3P))
	{
		Flashlight3P->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	}

	if (IsValid(LaserBeam))
	{
		LaserBeam->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	}
	// 备用弹夹不在 AttachmentVisuals 里，先拆掉，避免每次重装多一只。
	DestroyReserveMagazines(this, ReserveMagazineSocketName);

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

	// KIN: TR15 keeps body mag; Mag-DA guns keep Mag DA on mag socket (RAR-like) and hide body mag bones.
	ResolveMagazineVisualDuplicates();

	// 3. Dual flashlights: 1P for local view, 3P so other clients see the beam.
	if (HasFlashlight())
	{
		if (!IsValid(Flashlight1P))
		{
			Flashlight1P = CreateFlashlightLight(Mesh1P);
		}
		if (!IsValid(Flashlight3P))
		{
			Flashlight3P = CreateFlashlightLight(Mesh3P);
		}
		for (const UAttachmentData* Attachment : EquippedAttachments)
		{
			if (IsValid(Attachment) && Attachment->bProvidesFlashlight)
			{
				auto ApplyParams = [Attachment](USpotLightComponent* Light)
				{
					if (!IsValid(Light)) return;
					Light->SetIntensity(Attachment->LightIntensity);
					Light->SetLightColor(Attachment->LightColor);
					Light->SetAttenuationRadius(Attachment->LightAttenuationRadius);
					Light->SetInnerConeAngle(Attachment->LightInnerConeAngle);
					Light->SetOuterConeAngle(Attachment->LightOuterConeAngle);
				};
				ApplyParams(Flashlight1P);
				ApplyParams(Flashlight3P);
				break;
			}
		}
		UpdateFlashlightAttachment();
	}
	else
	{
		if (IsValid(Flashlight1P))
		{
			Flashlight1P->DestroyComponent();
			Flashlight1P = nullptr;
		}
		if (IsValid(Flashlight3P))
		{
			Flashlight3P->DestroyComponent();
			Flashlight3P = nullptr;
		}
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

USpotLightComponent* AWeapon::CreateFlashlightLight(USceneComponent* AttachParent)
{
	USpotLightComponent* Light = NewObject<USpotLightComponent>(this);
	Light->SetMobility(EComponentMobility::Movable);
	Light->bUseInverseSquaredFalloff = true;
	Light->SetCastShadows(true);
	Light->SetVisibility(false);
	Light->SetHiddenInGame(true);
	if (IsValid(AttachParent))
	{
		Light->SetupAttachment(AttachParent);
	}
	Light->RegisterComponent();
	return Light;
}

void AWeapon::UpdateFlashlightAttachment()
{
	const FAttachmentVisual* Visual = AttachmentVisuals.FindByPredicate([](const FAttachmentVisual& V)
	{
		return IsValid(V.Data) && V.Data->bProvidesFlashlight;
	});

	auto AttachOne = [&](USpotLightComponent* Light, bool bFirstPerson)
	{
		if (!IsValid(Light)) return;
		USceneComponent* Parent = nullptr;
		FName Socket = NAME_None;
		if (Visual)
		{
			Parent = AttachmentViewComponent(*Visual, bFirstPerson);
			if (IsValid(Parent) && IsValid(Visual->Data))
			{
				Socket = Parent->DoesSocketExist(Visual->Data->LightSocketName) ? Visual->Data->LightSocketName : NAME_None;
			}
		}
		if (!IsValid(Parent))
		{
			Parent = bFirstPerson ? Mesh1P.Get() : Mesh3P.Get();
		}
		if (!IsValid(Parent)) return;
		Light->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	};

	// Always keep both: local client uses 1P light; every client enables 3P light so others see the beam.
	AttachOne(Flashlight1P, true);
	AttachOne(Flashlight3P, false);
	ApplyFlashlightState();
	ApplyLaserState();
}

void AWeapon::ApplyFlashlightState()
{
	const APawn* OwningPawn = GetInstigator();
	const AShooterCharacter* ShooterOwner = Cast<AShooterCharacter>(OwningPawn);
	const bool bLocal = IsValid(OwningPawn) && (OwningPawn->IsLocallyControlled() || (ShooterOwner && ShooterOwner->IsFirstPersonViewedLocally()));
	const bool bOn = bFlashlightOn && HasFlashlight();

	auto SetLight = [](USpotLightComponent* Light, bool bEnable)
	{
		if (!IsValid(Light)) return;
		Light->SetActive(bEnable);
		Light->SetHiddenInGame(!bEnable);
		Light->SetVisibility(bEnable);
	};

	// 1P light only for the local first-person viewer (Mesh1P hidden for everyone else).
	SetLight(Flashlight1P, bOn && bLocal);
	// 3P light for remote viewers (and local third-person debug). Owning local FP relies on 1P light.
	const bool bUse3P = bOn && (!bLocal || (ShooterOwner && !ShooterOwner->IsFirstPersonViewedLocally()));
	SetLight(Flashlight3P, bUse3P);
}

void AWeapon::SetMeshVisibility(APawn* OwningPawn)
{
	if (!IsValid(OwningPawn)) return;

	const AShooterCharacter* ShooterOwner = Cast<AShooterCharacter>(OwningPawn);
	const bool bLocal = (OwningPawn->IsLocallyControlled() || (ShooterOwner && ShooterOwner->IsFirstPersonViewedLocally()))
  && !(ShooterOwner && ShooterOwner->IsDebugThirdPerson());
	Mesh3P->SetOwnerNoSee(bLocal);
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
	HideReserveMagazines(this, ReserveMagazineSocketName);
	if (bReloadReserveMagVisible)
	{
		ShowReserveMagazineForReload();
	}
	ApplyChamberShellVisibility();
	// Parent SetHiddenInGame(propagate) also stomps child lights — restore after.
	ApplyFlashlightState();
	ApplyLaserState();
}


namespace
{
	// B_WeaponImpacts / B_WeaponDecals 用数组参数（Impact Positions / Normals / Surface Types）。按名字反射填充，不依赖蓝图变量的确切拼写。
	void FillImpactArrays(AActor* Actor, const TArray<FVector>& Points, const TArray<FVector>& Normals, const TArray<uint8>& Surfaces)
	{
		for (TFieldIterator<FProperty> It(Actor->GetClass()); It; ++It)
		{
			const FString Name = It->GetName();
			if (Name.Contains(TEXT("Sorted")) || Name.Contains(TEXT("Previous")) || Name.Contains(TEXT("Current"))) continue;

			if (FArrayProperty* Arr = CastField<FArrayProperty>(*It))
			{
				FScriptArrayHelper Helper(Arr, Arr->ContainerPtrToValuePtr<void>(Actor));
				if (FStructProperty* SP = CastField<FStructProperty>(Arr->Inner))
				{
					if (SP->Struct != TBaseStructure<FVector>::Get()) continue;
					const TArray<FVector>* Src = nullptr;
					if (Name.Contains(TEXT("Normal"))) Src = &Normals;
					else if (Name.Contains(TEXT("Position")) || Name.Contains(TEXT("Point")) || Name.Contains(TEXT("Location"))) Src = &Points;
					if (!Src) continue;
					Helper.Resize(Src->Num());
					for (int32 i = 0; i < Src->Num(); ++i) *reinterpret_cast<FVector*>(Helper.GetRawPtr(i)) = (*Src)[i];
				}
				else if (Name.Contains(TEXT("Surface")))
				{
					if (CastField<FByteProperty>(Arr->Inner))
					{
						Helper.Resize(Surfaces.Num());
						for (int32 i = 0; i < Surfaces.Num(); ++i) *Helper.GetRawPtr(i) = Surfaces[i];
					}
					else if (FEnumProperty* EP = CastField<FEnumProperty>(Arr->Inner))
					{
						Helper.Resize(Surfaces.Num());
						for (int32 i = 0; i < Surfaces.Num(); ++i) EP->GetUnderlyingProperty()->SetIntPropertyValue(Helper.GetRawPtr(i), (int64)Surfaces[i]);
					}
				}
			}
			else if (FIntProperty* IP = CastField<FIntProperty>(*It))
			{
				if (Name.Contains(TEXT("Number")) && (Name.Contains(TEXT("Hit")) || Name.Contains(TEXT("Impact"))))
				{
					IP->SetPropertyValue_InContainer(Actor, Points.Num());
				}
			}
		}
	}
}

void AWeapon::SpawnPelletImpacts(const TArray<FHitResult>& Hits)
{
	UWorld* World = GetWorld();
	if (!World || GetNetMode() == NM_DedicatedServer) return;

	TArray<FVector> Points, Normals;
	TArray<uint8> Surfaces;
	for (const FHitResult& Hit : Hits)
	{
		if (!Hit.bBlockingHit) continue;
		Points.Add(Hit.ImpactPoint);
		Normals.Add(Hit.ImpactNormal);
		Surfaces.Add(Hit.PhysMaterial.IsValid(false) ? (uint8)Hit.PhysMaterial->SurfaceType.GetValue() : (uint8)SurfaceType2);
	}
	if (Points.Num() == 0) return;

	const FTransform SpawnXform(Normals[0].Rotation(), Points[0]);
	for (const TSoftClassPtr<AActor>& Soft : { PelletImpactClass, PelletDecalClass })
	{
		UClass* Cls = Soft.LoadSynchronous();
		if (!Cls) continue;
		AActor* Actor = World->SpawnActorDeferred<AActor>(Cls, SpawnXform, this, GetInstigator(), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Actor) continue;
		FillImpactArrays(Actor, Points, Normals, Surfaces);
		Actor->FinishSpawning(SpawnXform);
		if (Actor->InitialLifeSpan <= 0.f) Actor->SetLifeSpan(5.f);
	}
}



UAnimMontage* AWeapon::PlaySequenceAsDynamicMontage(UAnimInstance* Anim, UAnimSequenceBase* Sequence, FName SlotName, float BlendIn, float BlendOut)
{
	if (!IsValid(Anim) || !IsValid(Sequence)) return nullptr;
	return Anim->PlaySlotAnimationAsDynamicMontage(Sequence, SlotName, BlendIn, BlendOut, 1.f, 1);
}

void AWeapon::PlayBodySequence(USkeletalMeshComponent* WeaponMesh, UAnimSequence* Sequence)
{
	if (!IsValid(WeaponMesh) || !IsValid(Sequence)) return;
	const UWorld* World = WeaponMesh->GetWorld();
	if (World && World->GetNetMode() == NM_DedicatedServer) return;
	// PlayAnimation can InitAnim and rebuild RequiredBones, which drops unweighted mag bones.
	// Merge them again after that rebuild so the clip actually evaluates the mag.
	WeaponMesh->PlayAnimation(Sequence, false);
	RefreshKinFollowBones(Cast<AWeapon>(WeaponMesh->GetOwner()), WeaponMesh);
}

void AWeapon::PlayWeaponSequence(USkeletalMeshComponent* WeaponMesh, UAnimSequence* Sequence)
{
	if (!IsValid(WeaponMesh) || !IsValid(Sequence)) return;
	UAnimInstance* Anim = WeaponMesh->GetAnimInstance();
	if (IsValid(Anim) && !Anim->IsA<UAnimSingleNodeInstance>())
	{
		PlaySequenceAsDynamicMontage(Anim, Sequence);
		return;
	}
	WeaponMesh->PlayAnimation(Sequence, false);
}

void AWeapon::PlayWeaponMontageOrSequence(USkeletalMeshComponent* WeaponMesh, UAnimMontage* Montage)
{
	if (!IsValid(WeaponMesh) || !IsValid(Montage)) return;
	if (AWeapon* Weapon = Cast<AWeapon>(WeaponMesh->GetOwner()))
	{
		if (Weapon->GetClass() && Weapon->GetClass()->GetName().Contains(TEXT("Weapon_SRM12"))
			&& Montage->GetName().Contains(TEXT("Mag_Check")))
		{
			Weapon->SetActorTickEnabled(true);
		}
	}
	UAnimInstance* Anim = WeaponMesh->GetAnimInstance();
	if (IsValid(Anim) && !Anim->IsA<UAnimSingleNodeInstance>())
	{
		Anim->Montage_Play(Montage);
		RefreshKinFollowBones(Cast<AWeapon>(WeaponMesh->GetOwner()), WeaponMesh);
		return;
	}
	// 没有 AnimBP：取蒙太奇第一段序列直接播（资源包蒙太奇常把 semi/auto 两段串在一起，只要第一段）。
	for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
	{
		if (Track.AnimTrack.AnimSegments.Num() == 0) continue;
		const FAnimSegment& Seg = Track.AnimTrack.AnimSegments[0];
		if (UAnimSequenceBase* Seq = Seg.GetAnimReference())
		{
			WeaponMesh->PlayAnimation(Seq, false);
			WeaponMesh->SetPlayRate(Seg.AnimPlayRate * Montage->RateScale);
			RefreshKinFollowBones(Cast<AWeapon>(WeaponMesh->GetOwner()), WeaponMesh);
			return;
		}
	}
}


