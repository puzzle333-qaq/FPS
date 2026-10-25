#include "Range/TargetDummy.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "FPSTraceChannels.h"
#include "ShooterCharacter.h"
#include "ShooterGameModeBase.h"
#include "Game/ShooterGameState.h"
#include "Game/ShooterPlayerState.h"
#include "GameFramework/Pawn.h"

ATargetDummy::ATargetDummy()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
	SetReplicatingMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetRelativeRotation(FRotator(0.f, MeshFacingYaw, 0.f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(FPSTraceChannels::ECC_Weapon, ECR_Block);
	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Mesh->SetAnimInstanceClass(nullptr);
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh->SetComponentTickEnabled(true);
	Mesh->bNoSkeletonUpdate = false;
	Mesh->bPauseAnims = false;
}

void ATargetDummy::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(Mesh))
	{
		Mesh->SetAnimInstanceClass(nullptr);
		Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		// 朝向：最初正确时是 0；不要再强制 +90
		Mesh->SetRelativeRotation(FRotator(0.f, MeshFacingYaw, 0.f));
	}

	if (!IsValid(FallAnim))
	{
		FallAnim = LoadObject<UAnimSequenceBase>(nullptr,
			TEXT("/Game/FPS_Controller/Animations/TargetDummy/A_TargetDummy_Down.A_TargetDummy_Down"));
	}
	if (!IsValid(RiseAnim))
	{
		RiseAnim = LoadObject<UAnimSequenceBase>(nullptr,
			TEXT("/Game/FPS_Controller/Animations/TargetDummy/A_TargetDummy_Up.A_TargetDummy_Up"));
	}
	if (HasAuthority())
	{
		Health = MaxHealth;
		bDown = false;
	}
	HingeAlpha = 0.f;
	HingeAlphaTarget = 0.f;
	bUsingAnimVisual = false;
	// 出生必须站立（见 TryPlayStateAnim：Rise 第 0 帧是倒下姿势）
	ApplyDownVisual(false);
}

void ATargetDummy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bUsingAnimVisual) return;
	if (FMath::IsNearlyEqual(HingeAlpha, HingeAlphaTarget, 0.001f))
	{
		HingeAlpha = HingeAlphaTarget;
		return;
	}
	HingeAlpha = FMath::FInterpTo(HingeAlpha, HingeAlphaTarget, DeltaTime, HingeInterpSpeed);
	ApplyHingeRotation(FMath::Lerp(0.f, FallPitch, HingeAlpha));
}

void ATargetDummy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATargetDummy, bDown);
	DOREPLIFETIME(ATargetDummy, Health);
}

FName ATargetDummy::ResolveHitBone(const FHitResult& Hit, FName DefaultBone) const
{
	if (!bEnableHeadshotZone || bDown) return DefaultBone;
	const FVector Local = GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);
	return Local.Z >= HeadZoneMinHeight ? FName(TEXT("head")) : DefaultBone;
}

bool ATargetDummy::DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator)
{
	if (!HasAuthority() || bDown || DamageAmout <= 0.f) return false;

	Health = FMath::Max(0.f, Health - DamageAmout);
	const bool bKilled = Health <= 0.f;

	AShooterCharacter* Shooter = Cast<AShooterCharacter>(DamageInstigator);
	const bool bHeadshot = IsValid(Shooter) && Shooter->bPendingHeadshotHit;
	if (IsValid(Shooter) && Shooter->IsPlayerControlled())
	{
		if (bKilled) Shooter->Client_ConfirmKill(bHeadshot);
		else Shooter->Client_ConfirmHit(bHeadshot);
	}

	if (bKilled)
	{
		bDown = true;
		ApplyDownVisual(true);
		GetWorldTimerManager().SetTimer(ResetTimer, this, &ATargetDummy::Auth_Reset, FMath::Max(0.1f, ResetDelay), false);

		if (bPostKillFeed)
		{
			if (AShooterGameState* GS = GetWorld() ? GetWorld()->GetGameState<AShooterGameState>() : nullptr)
			{
				const APawn* Pawn = Cast<APawn>(DamageInstigator);
				const AShooterPlayerState* PS = Pawn ? Pawn->GetPlayerState<AShooterPlayerState>() : nullptr;
				GS->Multicast_KillFeed(PS ? PS->GetPlayerName() : FString(), PS ? PS->GetTeamId() : AShooterPlayerState::NoTeam,
					KillFeedName, AShooterPlayerState::NoTeam, nullptr);
			}
		}
	}
	return bKilled;
}

void ATargetDummy::Auth_Reset()
{
	Health = MaxHealth;
	bDown = false;
	ApplyDownVisual(true);
}

void ATargetDummy::OnRep_Down()
{
	ApplyDownVisual(true);
}

void ATargetDummy::ApplyHingeRotation(float Pitch)
{
	if (!IsValid(Mesh)) return;
	Mesh->SetRelativeRotation(FRotator(Pitch, MeshFacingYaw, 0.f));
}

bool ATargetDummy::TryPlayStateAnim(bool bAnimate)
{
	if (!bPreferAnimation || !IsValid(Mesh)) return false;

	// Fall：0=站立 → 末帧=倒下
	// Rise：0=倒下 → 末帧=站立
	// 以前站立时误用 Rise@0，出生就会显示倒下。
	UAnimSequenceBase* Anim = nullptr;
	float StartPos = 0.f;
	float HoldPos = 0.f;
	if (bDown)
	{
		Anim = FallAnim.Get();
		if (!IsValid(Anim)) Anim = RiseAnim.Get();
		if (!IsValid(Anim)) return false;
		StartPos = (Anim == FallAnim.Get()) ? 0.f : Anim->GetPlayLength(); // Fall从站播到倒；若只有Rise则从末帧倒放不方便，钉末帧
		HoldPos = (Anim == FallAnim.Get()) ? Anim->GetPlayLength() : 0.f;
		if (Anim == RiseAnim.Get())
		{
			// 只有 Rise：倒下钉在 0，起立才播
			StartPos = 0.f;
			HoldPos = 0.f;
		}
	}
	else
	{
		Anim = RiseAnim.Get();
		if (!IsValid(Anim)) Anim = FallAnim.Get();
		if (!IsValid(Anim)) return false;
		if (Anim == RiseAnim.Get())
		{
			StartPos = 0.f;                 // 起立动画从头播
			HoldPos = Anim->GetPlayLength(); // 站立静止 = Rise 末帧
		}
		else
		{
			StartPos = Anim->GetPlayLength(); // 不该走到
			HoldPos = 0.f;                   // Fall 第 0 帧 = 站立
		}
	}

	Mesh->SetAnimInstanceClass(nullptr);
	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Mesh->SetComponentTickEnabled(true);
	Mesh->bPauseAnims = false;
	Mesh->bNoSkeletonUpdate = false;
	Mesh->InitAnim(true);
	Mesh->SetRelativeRotation(FRotator(0.f, MeshFacingYaw, 0.f));

	const bool bPlay = bAnimate && ((bDown && Anim == FallAnim.Get()) || (!bDown && Anim == RiseAnim.Get()));
	Mesh->OverrideAnimationData(Anim, false, bPlay, bPlay ? StartPos : HoldPos, 1.f);

	if (UAnimSingleNodeInstance* Node = Cast<UAnimSingleNodeInstance>(Mesh->GetAnimInstance()))
	{
		Node->SetAnimationAsset(Anim, false);
		Node->SetLooping(false);
		Node->SetPlayRate(1.f);
		if (bPlay)
		{
			Node->SetPosition(StartPos);
			Node->SetPlaying(true);
		}
		else
		{
			Node->SetPosition(HoldPos);
			Node->SetPlaying(false);
		}
	}
	else
	{
		Mesh->PlayAnimation(Anim, false);
		Mesh->SetPosition(bPlay ? StartPos : HoldPos, false);
		if (!bPlay) Mesh->Stop();
	}

	Mesh->TickAnimation(0.f, false);
	Mesh->RefreshBoneTransforms();

	UE_LOG(LogTemp, Log, TEXT("TargetDummy %s anim=%s bDown=%d play=%d hold=%.2f"),
		*GetName(), *GetNameSafe(Anim), bDown ? 1 : 0, bPlay ? 1 : 0, HoldPos);
	return true;
}

void ATargetDummy::ApplyDownVisual(bool bAnimate)
{
	if (!IsValid(Mesh)) return;

	UE_LOG(LogTemp, Log, TEXT("TargetDummy %s ApplyDownVisual bDown=%d fall=%s rise=%s"),
		*GetName(), bDown ? 1 : 0, *GetNameSafe(FallAnim.Get()), *GetNameSafe(RiseAnim.Get()));

	HingeAlphaTarget = bDown ? 1.f : 0.f;

	if (TryPlayStateAnim(bAnimate))
	{
		bUsingAnimVisual = true;
		HingeAlpha = 0.f;
		HingeAlphaTarget = 0.f;
		ApplyHingeRotation(0.f);
	}
	else
	{
		bUsingAnimVisual = false;
		if (!bAnimate)
		{
			HingeAlpha = HingeAlphaTarget;
			ApplyHingeRotation(FMath::Lerp(0.f, FallPitch, HingeAlpha));
		}
	}

	Mesh->SetCollisionResponseToChannel(FPSTraceChannels::ECC_Weapon, bDown ? ECR_Ignore : ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, bDown ? ECR_Ignore : ECR_Block);
	if (bAnimate)
	{
		if (USoundBase* S = bDown ? FallSound.Get() : RiseSound.Get())
		{
			UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation());
		}
	}
}
