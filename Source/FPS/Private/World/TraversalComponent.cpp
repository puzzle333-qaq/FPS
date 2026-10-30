#include "World/TraversalComponent.h"

#include "World/Ladder.h"
#include "ShooterCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "UObject/SoftObjectPath.h"

static TAutoConsoleVariable<int32> CVarDebugVault(TEXT("DebugVault"), 0, TEXT("VaultMantle: 1 = draw vault/mantle/ladder traces + log"), ECVF_Cheat);

namespace
{
	bool DebugOn() { return CVarDebugVault.GetValueOnGameThread() != 0; }
	float Smooth(float A) { A = FMath::Clamp(A, 0.f, 1.f); return A * A * (3.f - 2.f * A); }

	void FaceFlat(ACharacter* C, FVector Dir)
	{
		if (!C) return;
		Dir.Z = 0.f;
		if (!Dir.Normalize()) return;
		const float Yaw = Dir.Rotation().Yaw;
		C->SetActorRotation(FRotator(0.f, Yaw, 0.f));
		if (AController* PC = C->GetController())
		{
			FRotator CR = PC->GetControlRotation();
			CR.Yaw = Yaw;
			PC->SetControlRotation(CR);
		}
	}

	static const TCHAR* PathVault = TEXT("/Game/FreeAnimationLibrary/Animations/Vault/anim_Vault.anim_Vault");
	static const TCHAR* PathMantle1 = TEXT("/Game/FreeAnimationLibrary/Animations/Mantle/anim_Mantle_1M_R.anim_Mantle_1M_R");
	static const TCHAR* PathMantle2 = TEXT("/Game/FreeAnimationLibrary/Animations/Mantle/anim_Mantle_2M_R.anim_Mantle_2M_R");
	static const TCHAR* PathLadderUp = TEXT("/Game/FreeAnimationLibrary/Animations/Ladder/anim_Ladder_Up.anim_Ladder_Up");
	static const TCHAR* PathLadderDown = TEXT("/Game/FreeAnimationLibrary/Animations/Ladder/anim_Ladder_Down.anim_Ladder_Down");
	static const TCHAR* PathLadderIdle = TEXT("/Game/FreeAnimationLibrary/Animations/Ladder/anim_Ladder_Idle.anim_Ladder_Idle");
	static const TCHAR* PathLadderTop = TEXT("/Game/FreeAnimationLibrary/Animations/LedgeClimb/anim_LedgeClimb_ClimbUp.anim_LedgeClimb_ClimbUp");
	static const TCHAR* PathFreeSkel = TEXT("/Game/FreeAnimationLibrary/Demo/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin");
}

UTraversalComponent::UTraversalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	VaultAnim = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(PathVault));
	MantleAnimLow = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(PathMantle1));
	MantleAnimHigh = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(PathMantle2));
	LadderUpAnim = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(PathLadderUp));
	LadderDownAnim = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(PathLadderDown));
	LadderIdleAnim = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(PathLadderIdle));
	LadderTopExitAnim = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(PathLadderTop));
}

void UTraversalComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureSkeletonCompat();
}

void UTraversalComponent::EnsureSkeletonCompat()
{
	// FreeAnimationLibrary clips use its Demo SK_Mannequin. Make 3P + Mesh1P skeletons compatible.
	ACharacter* C = GetChar();
	if (!C) return;

	USkeleton* FreeSkel = nullptr;
	if (USkeletalMesh* FreeMesh = LoadObject<USkeletalMesh>(nullptr, PathFreeSkel))
	{
		FreeSkel = FreeMesh->GetSkeleton();
	}
	if (!FreeSkel) return;

	auto Compat = [FreeSkel](USkeletalMeshComponent* MeshComp)
	{
		if (!MeshComp) return;
		USkeletalMesh* SkMesh = MeshComp->GetSkeletalMeshAsset();
		USkeleton* Skel = SkMesh ? SkMesh->GetSkeleton() : nullptr;
		if (!Skel || Skel == FreeSkel) return;
		Skel->AddCompatibleSkeleton(FreeSkel);
		if (DebugOn())
		{
			UE_LOG(LogTemp, Log, TEXT("[Vault] EnsureSkeletonCompat %s <-> %s"), *GetNameSafe(Skel), *GetNameSafe(FreeSkel));
		}
	};

	Compat(C->GetMesh());
	if (const AShooterCharacter* Shooter = Cast<AShooterCharacter>(C))
	{
		Compat(Shooter->GetMesh1P_Implementation());
	}
}

void UTraversalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTraversalComponent, Mode);
	DOREPLIFETIME(UTraversalComponent, CurrentLadder);
}

ACharacter* UTraversalComponent::GetChar() const
{
	return Cast<ACharacter>(GetOwner());
}

bool UTraversalComponent::CapsuleFits(const FVector& At) const
{
	ACharacter* C = GetChar();
	if (!C) return false;
	UCapsuleComponent* Cap = C->GetCapsuleComponent();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(TraversalFit), false, C);
	FCollisionResponseParams R;
	Cap->InitSweepCollisionParams(Q, R);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius() - 1.f, Cap->GetScaledCapsuleHalfHeight() - 1.f);
	const bool bBlocked = GetWorld()->OverlapBlockingTestByChannel(At, FQuat::Identity, Cap->GetCollisionObjectType(), Shape, Q, R);
	if (DebugOn()) DrawDebugCapsule(GetWorld(), At, Shape.GetCapsuleHalfHeight(), Shape.GetCapsuleRadius(), FQuat::Identity, bBlocked ? FColor::Red : FColor::Green, false, 3.f);
	return !bBlocked;
}

bool UTraversalComponent::CapsuleSweepClear(const FVector& From, const FVector& To) const
{
	ACharacter* C = GetChar();
	if (!C) return false;
	UCapsuleComponent* Cap = C->GetCapsuleComponent();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(TraversalSweep), false, C);
	FCollisionResponseParams R;
	Cap->InitSweepCollisionParams(Q, R);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius() - 2.f, Cap->GetScaledCapsuleHalfHeight() - 2.f);
	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, From, To, FQuat::Identity, Cap->GetCollisionObjectType(), Shape, Q, R);
	if (DebugOn()) DrawDebugLine(GetWorld(), From, To, bHit ? FColor::Red : FColor::Cyan, false, 3.f, 0, 2.f);
	return !bHit;
}

bool UTraversalComponent::ComputeVaultMantle(ETraversalMode& OutMode, FVector& OutP1, FVector& OutP2, float& OutDuration, float& OutHeight) const
{
	ACharacter* C = GetChar();
	if (!C) return false;
	UWorld* W = GetWorld();
	UCapsuleComponent* Cap = C->GetCapsuleComponent();
	const float Radius = Cap->GetScaledCapsuleRadius();
	const float Half = Cap->GetScaledCapsuleHalfHeight();
	const FVector Loc = C->GetActorLocation();
	const float FeetZ = Loc.Z - Half;
	FVector Fwd = C->GetActorForwardVector(); Fwd.Z = 0.f; Fwd.Normalize();

	FCollisionQueryParams Q(SCENE_QUERY_STAT(TraversalTrace), false, C);
	FCollisionResponseParams R;
	Cap->InitSweepCollisionParams(Q, R);
	const ECollisionChannel Ch = Cap->GetCollisionObjectType();
	const bool bDbg = DebugOn();

	FHitResult Wall; bool bWall = false;
	for (float H = MinObstacleHeight - 10.f; H <= EyeHeight; H += 15.f)
	{
		const FVector S(Loc.X, Loc.Y, FeetZ + H);
		const FVector E = S + Fwd * (Radius + MaxReach);
		FHitResult Hit;
		const bool b = W->LineTraceSingleByChannel(Hit, S, E, Ch, Q, R);
		if (bDbg) DrawDebugLine(W, S, b ? Hit.ImpactPoint : E, b ? FColor::Orange : FColor::White, false, 3.f);
		if (b && FMath::Abs(Hit.ImpactNormal.Z) < 0.35f && (!bWall || Hit.Distance < Wall.Distance))
		{
			Wall = Hit; bWall = true;
		}
	}
	if (!bWall) return false;

	FVector N = Wall.ImpactNormal; N.Z = 0.f; N.Normalize();
	const FVector Into = -N;

	const FVector TopS = FVector(Wall.ImpactPoint.X, Wall.ImpactPoint.Y, FeetZ + EyeHeight + Half * 0.2f + 10.f) + Into * 12.f;
	const FVector TopE = FVector(TopS.X, TopS.Y, FeetZ + MinObstacleHeight - 5.f);
	FHitResult Top;
	const bool bTop = W->LineTraceSingleByChannel(Top, TopS, TopE, Ch, Q, R);
	if (bDbg) DrawDebugLine(W, TopS, bTop ? Top.ImpactPoint : TopE, FColor::Yellow, false, 3.f, 0, 2.f);
	if (!bTop || Top.bStartPenetrating || Top.ImpactNormal.Z < 0.7f) return false;
	const float TopZ = Top.ImpactPoint.Z;
	const float Height = TopZ - FeetZ;
	OutHeight = Height;
	if (Height < MinObstacleHeight || Height > EyeHeight) return false;

	const float Contact = Radius + 8.f;
	const FVector Hug = FVector(Wall.ImpactPoint.X, Wall.ImpactPoint.Y, Loc.Z) + N * Contact;
	const FVector Lift(Hug.X, Hug.Y, TopZ + Half + 4.f);
	if (!CapsuleSweepClear(Hug, Lift) && !CapsuleSweepClear(Loc, Lift)) return false;

	if (Height <= WaistHeight)
	{
		float Edge = -1.f;
		for (float D = 15.f; D <= MaxVaultDepth + 15.f; D += 10.f)
		{
			const FVector S = FVector(Wall.ImpactPoint.X, Wall.ImpactPoint.Y, TopZ + 10.f) + Into * D;
			FHitResult H;
			const bool b = W->LineTraceSingleByChannel(H, S, S - FVector(0, 0, 30.f), Ch, Q, R);
			if (bDbg) DrawDebugLine(W, S, S - FVector(0, 0, 30.f), b ? FColor::Purple : FColor::Blue, false, 3.f);
			if (!b) { Edge = D; break; }
		}
		if (Edge > 0.f)
		{
			const FVector Over = FVector(Wall.ImpactPoint.X, Wall.ImpactPoint.Y, TopZ + Half + 4.f) + Into * (Edge * 0.5f);
			FVector Land = FVector(Wall.ImpactPoint.X, Wall.ImpactPoint.Y, TopZ) + Into * (Edge + Radius + 10.f);
			FHitResult Floor;
			const FVector FS(Land.X, Land.Y, TopZ + 10.f);
			const FVector FE(Land.X, Land.Y, FeetZ - 80.f);
			if (W->LineTraceSingleByChannel(Floor, FS, FE, Ch, Q, R) && Floor.ImpactNormal.Z > 0.7f)
			{
				Land.Z = Floor.ImpactPoint.Z + Half + 2.f;
				if (bDbg) DrawDebugLine(W, FS, Floor.ImpactPoint, FColor::Green, false, 3.f, 0, 2.f);
				const FVector OverFar(Land.X, Land.Y, Over.Z);
				if (CapsuleSweepClear(Lift, Over) && CapsuleSweepClear(Over, OverFar) && CapsuleFits(Land) && CapsuleSweepClear(OverFar, Land))
				{
					OutMode = ETraversalMode::Vault;
					OutP1 = Over;
					OutP2 = Land;
					OutDuration = VaultDuration;
					return true;
				}
			}
		}
	}

	const FVector Hang(Hug.X, Hug.Y, TopZ - FMath::Clamp(Height * 0.3f, 20.f, 50.f) + Half);
	const FVector Target = FVector(Wall.ImpactPoint.X, Wall.ImpactPoint.Y, TopZ + Half + 3.f) + Into * (Radius + 10.f);
	if (!CapsuleFits(Target) || !CapsuleSweepClear(Hang, Target)) return false;
	OutMode = ETraversalMode::Mantle;
	OutP1 = Hang;
	OutP2 = Target;
	const float A = FMath::Clamp((Height - MinObstacleHeight) / FMath::Max(1.f, EyeHeight - MinObstacleHeight), 0.f, 1.f);
	OutDuration = FMath::Lerp(MantleDurationLow, MantleDurationHigh, A);
	return true;
}

ALadder* UTraversalComponent::FindLadder() const
{
	ACharacter* C = GetChar();
	if (!C) return nullptr;
	TArray<AActor*> Found;
	C->GetOverlappingActors(Found, ALadder::StaticClass());
	FVector Fwd = C->GetActorForwardVector(); Fwd.Z = 0.f; Fwd.Normalize();
	for (AActor* A : Found)
	{
		ALadder* L = Cast<ALadder>(A);
		if (L && FVector::DotProduct(Fwd, L->GetClimbForward()) > 0.3f) return L;
	}
	return nullptr;
}

void UTraversalComponent::BeginControl()
{
	ACharacter* C = GetChar();
	UCharacterMovementComponent* CMC = C ? C->GetCharacterMovement() : nullptr;
	if (!CMC) return;
	if (!bControlling)
	{
		SavedMaxFlySpeed = CMC->MaxFlySpeed;
		bControlling = true;
	}
	if (C->bIsCrouched) C->UnCrouch(false);
	CMC->SetMovementMode(MOVE_Flying);
	CMC->MaxFlySpeed = 0.f;
	CMC->Velocity = FVector::ZeroVector;
	CMC->bIgnoreClientMovementErrorChecksAndCorrection = true;
	CMC->bServerAcceptClientAuthoritativePosition = true;
	RestoreFlagsTime = -1.f;

	// Position is driven by lerp; ignore clip root motion so FreeAnim vault/mantle don't double-move.
	if (USkeletalMeshComponent* Mesh = C->GetMesh())
	{
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			if (!bSavedRootMotion)
			{
				SavedRootMotionMode = static_cast<uint8>(Anim->RootMotionMode);
				bSavedRootMotion = true;
			}
			Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		}
	}
}

void UTraversalComponent::EndControl()
{
	ACharacter* C = GetChar();
	UCharacterMovementComponent* CMC = C ? C->GetCharacterMovement() : nullptr;
	if (CMC && bControlling)
	{
		CMC->MaxFlySpeed = SavedMaxFlySpeed;
		CMC->Velocity = FVector::ZeroVector;
		CMC->SetMovementMode(MOVE_Falling);
	}
	bControlling = false;
	bInterp = false;

	if (bSavedRootMotion && C)
	{
		if (USkeletalMeshComponent* Mesh = C->GetMesh())
		{
			if (UAnimInstance* Anim = Mesh->GetAnimInstance())
			{
				Anim->SetRootMotionMode(static_cast<ERootMotionMode::Type>(SavedRootMotionMode));
			}
		}
		bSavedRootMotion = false;
	}

	RestoreFlagsTime = GetWorld() ? GetWorld()->GetTimeSeconds() + 0.5f : -1.f;
}

void UTraversalComponent::PlayTraversalAnim(ETraversalMode InMode, float ObstacleHeight, float InDuration, int8 LadderDir)
{
	ACharacter* C = GetChar();
	if (!C || GetWorld()->GetNetMode() == NM_DedicatedServer) return;

	EnsureSkeletonCompat();

	TSoftObjectPtr<UAnimSequenceBase> Soft;
	int32 LoopCount = 1;
	float BlendIn = 0.08f;
	float BlendOut = 0.15f;

	switch (InMode)
	{
	case ETraversalMode::Vault:
		Soft = VaultAnim;
		break;
	case ETraversalMode::Mantle:
		if (ObstacleHeight < 0.f)
		{
			Soft = LadderTopExitAnim.IsNull() ? MantleAnimLow : LadderTopExitAnim;
		}
		else
		{
			Soft = (ObstacleHeight >= MantleHighAnimHeight) ? MantleAnimHigh : MantleAnimLow;
		}
		break;
	case ETraversalMode::LadderAttach:
	case ETraversalMode::Ladder:
		LoopCount = 0;
		BlendIn = 0.12f;
		BlendOut = 0.12f;
		if (LadderDir > 0) Soft = LadderUpAnim;
		else if (LadderDir < 0) Soft = LadderDownAnim;
		else Soft = LadderIdleAnim;
		break;
	default:
		return;
	}

	UAnimSequenceBase* Seq = Soft.LoadSynchronous();
	if (!Seq)
	{
		if (DebugOn()) UE_LOG(LogTemp, Warning, TEXT("[Vault] anim load failed mode=%d path=%s"), (int32)InMode, *Soft.ToSoftObjectPath().ToString());
		return;
	}

	StopTraversalAnim(0.05f);

	float Rate = 1.f;
	const float Len = Seq->GetPlayLength();
	if (LoopCount == 1 && InDuration > 0.05f && Len > 0.05f)
	{
		Rate = Len / InDuration;
	}

	auto PlayOn = [&](USkeletalMeshComponent* MeshComp, TObjectPtr<UAnimMontage>& OutMontage, bool bAlsoIgnoreRoot)
	{
		if (!MeshComp) return;
		UAnimInstance* Anim = MeshComp->GetAnimInstance();
		if (!Anim) return;
		if (bAlsoIgnoreRoot)
		{
			Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		}
		OutMontage = Anim->PlaySlotAnimationAsDynamicMontage(Seq, TraversalSlotName, BlendIn, BlendOut, Rate, LoopCount);
	};

	PlayOn(C->GetMesh(), ActiveTraversalMontage, false);

	// Same FreeAnim clip on Mesh1P for the owning player (arms share compatible skeleton).
	if (C->IsLocallyControlled())
	{
		if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(C))
		{
			PlayOn(Shooter->GetMesh1P_Implementation(), ActiveTraversalMontage1P, true);
		}
	}

	if (DebugOn())
	{
		UE_LOG(LogTemp, Log, TEXT("[Vault] play %s slot=%s rate=%.2f loop=%d 3P=%s 1P=%s"),
			*GetNameSafe(Seq), *TraversalSlotName.ToString(), Rate, LoopCount,
			*GetNameSafe(ActiveTraversalMontage), *GetNameSafe(ActiveTraversalMontage1P));
	}
}

void UTraversalComponent::StopTraversalAnim(float BlendOut)
{
	ACharacter* C = GetChar();
	if (!C) return;

	auto StopOn = [&](USkeletalMeshComponent* MeshComp, TObjectPtr<UAnimMontage>& MontageRef)
	{
		if (!MeshComp) return;
		UAnimInstance* Anim = MeshComp->GetAnimInstance();
		if (Anim && IsValid(MontageRef))
		{
			Anim->Montage_Stop(BlendOut, MontageRef);
		}
		MontageRef = nullptr;
	};

	StopOn(C->GetMesh(), ActiveTraversalMontage);
	if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(C))
	{
		StopOn(Shooter->GetMesh1P_Implementation(), ActiveTraversalMontage1P);
	}
	LadderAnimDir = 0;
}

void UTraversalComponent::UpdateLadderAnim(int8 Dir)
{
	if (Dir == LadderAnimDir && IsValid(ActiveTraversalMontage)) return;
	LadderAnimDir = Dir;
	PlayTraversalAnim(ETraversalMode::Ladder, 0.f, 0.f, Dir);
}

void UTraversalComponent::OnRep_Mode()
{
	ACharacter* C = GetChar();
	if (!C || C->IsLocallyControlled() || C->HasAuthority()) return;
	// Simulated proxy: play matching 3P clip; movement still comes from replication.
	if (Mode == ETraversalMode::None)
	{
		StopTraversalAnim(0.2f);
		return;
	}
	if (Mode == ETraversalMode::Vault)
	{
		PlayTraversalAnim(ETraversalMode::Vault, LastObstacleHeight, VaultDuration, 0);
	}
	else if (Mode == ETraversalMode::Mantle)
	{
		PlayTraversalAnim(ETraversalMode::Mantle, LastObstacleHeight > 0.f ? LastObstacleHeight : MantleHighAnimHeight, MantleDurationHigh, 0);
	}
	else if (Mode == ETraversalMode::Ladder || Mode == ETraversalMode::LadderAttach)
	{
		UpdateLadderAnim(0);
	}
}

void UTraversalComponent::StartInterp(const FVector& InP1, const FVector& InP2, float InDuration, ETraversalMode InMode, ETraversalMode InAfter)
{
	ACharacter* C = GetChar();
	if (!C) return;
	BeginControl();
	if (InMode == ETraversalMode::Mantle)
	{
		FVector HugStart = InP1;
		HugStart.Z = C->GetActorLocation().Z;
		if (CapsuleFits(HugStart))
		{
			C->SetActorLocation(HugStart);
		}
		FaceFlat(C, InP2 - HugStart);
	}
	else if (InMode == ETraversalMode::Vault)
	{
		FaceFlat(C, InP2 - C->GetActorLocation());
	}
	else if (InMode == ETraversalMode::LadderAttach && CurrentLadder)
	{
		FaceFlat(C, CurrentLadder->GetClimbForward());
	}
	P0 = C->GetActorLocation();
	P1 = InP1;
	P2 = InP2;
	Elapsed = 0.f;
	Duration = FMath::Max(0.05f, InDuration);
	Mode = InMode;
	AfterMode = InAfter;
	bInterp = true;
	if (C->HasAuthority() && (InMode == ETraversalMode::Vault || InMode == ETraversalMode::Mantle))
	{
		C->MakeNoise(InMode == ETraversalMode::Vault ? VaultNoiseLoudness : VaultNoiseLoudness * 0.6f, C, C->GetActorLocation());
	}

	PlayTraversalAnim(InMode, LastObstacleHeight, Duration, 0);

	if (DebugOn()) UE_LOG(LogTemp, Log, TEXT("[Vault] %s start mode=%d dur=%.2f h=%.1f"), C->HasAuthority() ? TEXT("SV") : TEXT("CL"), (int32)InMode, Duration, LastObstacleHeight);
}

void UTraversalComponent::FinishInterp()
{
	bInterp = false;
	if (AfterMode == ETraversalMode::Ladder && CurrentLadder)
	{
		Mode = ETraversalMode::Ladder;
		UpdateLadderAnim(0);
		return;
	}
	Mode = ETraversalMode::None;
	CurrentLadder = nullptr;
	StopTraversalAnim(0.2f);
	EndControl();
}

bool UTraversalComponent::StartLadder(ALadder* Ladder)
{
	ACharacter* C = GetChar();
	if (!C || !Ladder) return false;
	const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float FeetZ = FMath::Clamp(C->GetActorLocation().Z - Half, Ladder->GetBottomZ(), Ladder->GetTopZ() - 30.f);
	const FVector Target = Ladder->GetClimbLocation(FeetZ, Half);
	CurrentLadder = Ladder;
	LadderFeetZ = FeetZ;
	LadderAnimDir = 127; // force refresh after attach
	StartInterp((C->GetActorLocation() + Target) * 0.5f, Target, 0.2f, ETraversalMode::LadderAttach, ETraversalMode::Ladder);
	return true;
}

bool UTraversalComponent::ExitLadderTop()
{
	ACharacter* C = GetChar();
	if (!C || !CurrentLadder) return false;
	const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Target = CurrentLadder->GetTopExitLocation(Half);
	const FVector Lift(C->GetActorLocation().X, C->GetActorLocation().Y, Target.Z);
	if (!CapsuleFits(Target)) return false;
	CurrentLadder = nullptr;
	LastObstacleHeight = -1.f; // signal PlayTraversalAnim to use LadderTopExitAnim
	StartInterp(Lift, Target, LadderTopExitDuration, ETraversalMode::Mantle, ETraversalMode::None);
	return true;
}

void UTraversalComponent::ExitLadderDrop()
{
	Mode = ETraversalMode::None;
	CurrentLadder = nullptr;
	StopTraversalAnim(0.15f);
	EndControl();
}

void UTraversalComponent::TickLadderLocal(float DeltaTime)
{
	ACharacter* C = GetChar();
	if (!C || !CurrentLadder) return;
	const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector In = C->GetLastMovementInputVector();
	const float Axis = FMath::Clamp(FVector::DotProduct(In, CurrentLadder->GetClimbForward()), -1.f, 1.f);

	int8 Dir = 0;
	if (Axis > 0.2f) Dir = 1;
	else if (Axis < -0.2f) Dir = -1;
	UpdateLadderAnim(Dir);
	FaceFlat(C, CurrentLadder->GetClimbForward());

	if (FMath::Abs(Axis) < 0.2f) return;

	LadderFeetZ += Axis * LadderSpeed * DeltaTime;
	if (LadderFeetZ >= CurrentLadder->GetTopZ() - Half * 0.6f)
	{
		LadderFeetZ = CurrentLadder->GetTopZ() - Half * 0.6f;
		C->SetActorLocation(CurrentLadder->GetClimbLocation(LadderFeetZ, Half));
		if (ExitLadderTop())
		{
			if (!C->HasAuthority()) ServerLadderExit(true);
		}
		return;
	}
	if (LadderFeetZ <= CurrentLadder->GetBottomZ() && Axis < 0.f)
	{
		ExitLadderDrop();
		if (!C->HasAuthority()) ServerLadderExit(false);
		return;
	}
	C->SetActorLocation(CurrentLadder->GetClimbLocation(LadderFeetZ, Half));
	FaceFlat(C, CurrentLadder->GetClimbForward());
}

void UTraversalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACharacter* C = GetChar();
	if (!C) return;

	if (RestoreFlagsTime > 0.f && GetWorld()->GetTimeSeconds() >= RestoreFlagsTime && !bControlling)
	{
		if (UCharacterMovementComponent* CMC = C->GetCharacterMovement())
		{
			CMC->bIgnoreClientMovementErrorChecksAndCorrection = false;
			CMC->bServerAcceptClientAuthoritativePosition = false;
		}
		RestoreFlagsTime = -1.f;
	}

	if (C->GetLocalRole() == ROLE_SimulatedProxy) return;

	if (bInterp)
	{
		Elapsed += DeltaTime;
		const float A = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
		const float Split = (Mode == ETraversalMode::Vault) ? 0.4f : 0.5f;
		FVector P = (A < Split) ? FMath::Lerp(P0, P1, Smooth(A / Split)) : FMath::Lerp(P1, P2, Smooth((A - Split) / (1.f - Split)));
		C->SetActorLocation(P, false, nullptr, ETeleportType::None);
		{
			FVector Face = (A < Split) ? (P1 - P0) : (P2 - P1);
			if (Mode == ETraversalMode::LadderAttach && CurrentLadder)
			{
				Face = CurrentLadder->GetClimbForward();
			}
			FaceFlat(C, Face);
		}
		if (A >= 1.f) FinishInterp();
		return;
	}

	if (Mode == ETraversalMode::Ladder && C->IsLocallyControlled())
	{
		TickLadderLocal(DeltaTime);
	}
}

bool UTraversalComponent::TryTraversal()
{
	ACharacter* C = GetChar();
	if (!C || bInterp) return false;

	if (IsOnLadder())
	{
		ExitLadderDrop();
		if (!C->HasAuthority()) ServerLadderExit(false);
		return true;
	}
	if (Mode != ETraversalMode::None) return false;

	if (ALadder* L = FindLadder())
	{
		StartLadder(L);
		if (!C->HasAuthority()) ServerAttachLadder(L);
		return true;
	}

	UCharacterMovementComponent* CMC = C->GetCharacterMovement();
	if (!CMC || !CMC->IsMovingOnGround()) return false;

	ETraversalMode M; FVector A, B; float D; float H = 0.f;
	if (!ComputeVaultMantle(M, A, B, D, H)) return false;
	LastObstacleHeight = H;
	StartLocation = C->GetActorLocation();
	StartInterp(A, B, D, M, ETraversalMode::None);
	if (!C->HasAuthority()) ServerTryVaultMantle(StartLocation);
	return true;
}

void UTraversalComponent::ServerTryVaultMantle_Implementation(FVector ClientStart)
{
	ACharacter* C = GetChar();
	if (!C || bInterp || Mode != ETraversalMode::None) return;
	if (FVector::Dist(ClientStart, C->GetActorLocation()) < 60.f)
	{
		C->SetActorLocation(ClientStart);
	}
	ETraversalMode M; FVector A, B; float D; float H = 0.f;
	if (ComputeVaultMantle(M, A, B, D, H))
	{
		LastObstacleHeight = H;
		StartInterp(A, B, D, M, ETraversalMode::None);
	}
	else
	{
		if (DebugOn()) UE_LOG(LogTemp, Warning, TEXT("[Vault] server rejected"));
		ClientRejected(C->GetActorLocation());
	}
}

void UTraversalComponent::ServerAttachLadder_Implementation(ALadder* Ladder)
{
	ACharacter* C = GetChar();
	if (!C || !Ladder || Mode != ETraversalMode::None) return;
	TArray<AActor*> Found;
	C->GetOverlappingActors(Found, ALadder::StaticClass());
	if (!Found.Contains(Ladder))
	{
		ClientRejected(C->GetActorLocation());
		return;
	}
	StartLadder(Ladder);
}

void UTraversalComponent::ServerLadderExit_Implementation(bool bTop)
{
	if (!IsOnLadder()) return;
	if (bTop && CurrentLadder)
	{
		ACharacter* C = GetChar();
		const float Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		if (C->GetActorLocation().Z - Half > CurrentLadder->GetTopZ() - Half - 60.f && ExitLadderTop()) return;
	}
	ExitLadderDrop();
}

void UTraversalComponent::ClientRejected_Implementation(FVector ServerLocation)
{
	ACharacter* C = GetChar();
	bInterp = false;
	Mode = ETraversalMode::None;
	CurrentLadder = nullptr;
	StopTraversalAnim(0.1f);
	if (C) C->SetActorLocation(ServerLocation, false, nullptr, ETeleportType::TeleportPhysics);
	EndControl();
}
