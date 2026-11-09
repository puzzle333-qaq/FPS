#include "Weapon/ThrownProjectile.h"
#include "ShooterCharacter.h"
#include "ShooterGameModeBase.h"

#include "Interface/PlayerInterface.h"
#include "FPSTraceChannels.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Sound/SoundBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AThrownProjectile::AThrownProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	bNetLoadOnClient = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(8.f);
	Collision->SetCollisionProfileName(TEXT("Projectile"));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionObjectType(FPSTraceChannels::ECC_Projectile);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Collision->SetNotifyRigidBodyCollision(true);
	Collision->SetGenerateOverlapEvents(false);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.12f));
	}

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = Collision;
	ProjectileMovement->InitialSpeed = 0.f;
	ProjectileMovement->MaxSpeed = 4000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = GravityScale;
	ProjectileMovement->bShouldBounce = bShouldBounce;
	ProjectileMovement->Bounciness = Bounciness;
	ProjectileMovement->Friction = Friction;
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = 40.f;
	ProjectileMovement->bInterpMovement = true;
	ProjectileMovement->bInterpRotation = true;

	InitialLifeSpan = 12.f;
}

void AThrownProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AThrownProjectile::BeginPlay()
{
	Super::BeginPlay();

	ProjectileMovement->ProjectileGravityScale = GravityScale;
	ProjectileMovement->bShouldBounce = bShouldBounce;
	ProjectileMovement->Bounciness = Bounciness;
	ProjectileMovement->Friction = Friction;

	Collision->OnComponentHit.AddDynamic(this, &AThrownProjectile::OnSphereHit);

	if (AActor* Thrower = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(Thrower, true);
	}

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(FuseTimer, this, &AThrownProjectile::Detonate, FMath::Max(0.05f, FuseTime), false);
	}
}

void AThrownProjectile::Launch(const FVector& Direction, float Speed)
{
	bLaunched = true;
	const FVector Dir = Direction.GetSafeNormal();
	const float LaunchSpeed = FMath::Max(0.f, Speed);
	if (IsValid(ProjectileMovement))
	{
		ProjectileMovement->InitialSpeed = LaunchSpeed;
		ProjectileMovement->MaxSpeed = FMath::Max(LaunchSpeed, ProjectileMovement->MaxSpeed);
		ProjectileMovement->Velocity = Dir * LaunchSpeed;
		ProjectileMovement->SetUpdatedComponent(Collision);
	}
	if (!Dir.IsNearlyZero())
	{
		SetActorRotation(Dir.Rotation());
	}
}

void AThrownProjectile::OnSphereHit(UPrimitiveComponent* /*HitComp*/, AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/, FVector /*NormalImpulse*/, const FHitResult& /*Hit*/)
{
	if (!HasAuthority() || bDetonated) return;
	if (!bDetonateOnImpact) return;
	if (OtherActor == GetInstigator()) return;
	Detonate();
}

void AThrownProjectile::Detonate()
{
	if (bDetonated) return;
	bDetonated = true;
	GetWorldTimerManager().ClearTimer(FuseTimer);

	if (IsValid(Collision))
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (IsValid(ProjectileMovement))
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->SetUpdatedComponent(nullptr);
	}

	if (HasAuthority())
	{
		ApplyThrownEffect();
		Multicast_OnDetonate();
	}

	if (IsValid(Mesh))
	{
		Mesh->SetHiddenInGame(true);
	}
	SetLifeSpan(2.f);
}

void AThrownProjectile::Multicast_OnDetonate_Implementation()
{
	OnDetonate();
}

void AThrownProjectile::OnDetonate_Implementation()
{
	const FVector Loc = GetActorLocation();
	if (IsValid(ExplosionFX))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionFX, Loc, GetActorRotation());
	}
	if (IsValid(ExplosionSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, Loc);
	}
}

void AThrownProjectile::ApplyThrownEffect()
{
	switch (EffectType)
	{
	case EThrownEffect::Frag:
		ApplyFragDamage();
		break;
	case EThrownEffect::Molotov:
		// MVP stand-in: one-shot burn radius (full fire pool later).
		ApplyFragDamage();
		break;
	case EThrownEffect::Flash:
		ApplyFlashWash();
		break;
	default:
		break;
	}
}

void AThrownProjectile::ApplyFlashWash()
{
	UWorld* World = GetWorld();
	if (!IsValid(World)) return;

	const FVector Origin = GetActorLocation();
	const float Radius = FMath::Max(DamageRadius, 600.f);
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ThrownFlash), false, this);
	QueryParams.AddIgnoredActor(this);
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectQuery, FCollisionShape::MakeSphere(Radius), QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AShooterCharacter* Char = Cast<AShooterCharacter>(Overlap.GetActor());
		if (!IsValid(Char)) continue;
		const float Dist = FVector::Dist(Origin, Char->GetActorLocation());
		if (Dist > Radius) continue;
		float Amount = 2.5f;
		if (Dist > InnerRadius)
		{
			const float Range = FMath::Max(1.f, Radius - InnerRadius);
			Amount *= 1.f - (Dist - InnerRadius) / Range;
		}
		float Facing = 0.35f;
		if (const AController* Viewer = Char->GetController())
		{
			FVector ViewLoc;
			FRotator ViewRot;
			Viewer->GetPlayerViewPoint(ViewLoc, ViewRot);
			const FVector ToFlash = (Origin - ViewLoc).GetSafeNormal();
			const float Dot = FVector::DotProduct(ViewRot.Vector(), ToFlash);
			if (Dot >= 0.35f) Facing = 1.f;
			else if (Dot >= -0.1f) Facing = 0.42f;
			else Facing = 0.12f;
		}

		FCollisionQueryParams SightParams(SCENE_QUERY_STAT(ThrownFlashSight), false, this);
		SightParams.AddIgnoredActor(this);
		SightParams.AddIgnoredActor(Char);
		FHitResult SightHit;
		const FVector Eye = Char->GetPawnViewLocation();
		if (World->LineTraceSingleByChannel(SightHit, Origin, Eye, ECC_Visibility, SightParams))
		{
			continue;
		}

		const float Strength = FMath::Clamp((Amount / 2.5f) * Facing, 0.f, 1.f);
		if (Strength < 0.08f) continue;
		Char->Client_ReceiveFlashbang(Strength);
	}
}

void AThrownProjectile::ApplyFragDamage()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || Damage <= 0.f || DamageRadius <= 0.f) return;

	const FVector Origin = GetActorLocation();
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ThrownFrag), false, this);
	QueryParams.AddIgnoredActor(this);

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);

	World->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(DamageRadius),
		QueryParams);

	TSet<AActor*> Damaged;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!IsValid(Actor) || Damaged.Contains(Actor)) continue;
		if (!Actor->Implements<UPlayerInterface>()) continue;
		Damaged.Add(Actor);

		const FVector TargetLoc = Actor->GetActorLocation() + FVector(0.f, 0.f, 50.f);
		const float Dist = FVector::Dist(Origin, Actor->GetActorLocation());
		if (Dist > DamageRadius) continue;

		if (bRequireLineOfSight)
		{
			FHitResult SightHit;
			FCollisionQueryParams SightParams(SCENE_QUERY_STAT(ThrownFragSight), false, this);
			SightParams.AddIgnoredActor(this);
			if (AActor* Thrower = GetInstigator())
			{
				SightParams.AddIgnoredActor(Thrower);
			}
			if (World->LineTraceSingleByChannel(SightHit, Origin, TargetLoc, ECC_Visibility, SightParams))
			{
				if (SightHit.GetActor() != Actor)
				{
					continue;
				}
			}
		}

		float Scale = 1.f;
		if (Dist > InnerRadius)
		{
			const float Range = FMath::Max(1.f, DamageRadius - InnerRadius);
			Scale = 1.f - (Dist - InnerRadius) / Range;
		}
		Scale = FMath::Clamp(Scale, 0.f, 1.f);
		const float Applied = Damage * Scale;
		if (Applied < 1.f) continue;

		AShooterGameModeBase* DamageGM = GetWorld() ? GetWorld()->GetAuthGameMode<AShooterGameModeBase>() : nullptr;
		if (DamageGM) DamageGM->SetPendingDamageIcon(KillFeedIcon);
		IPlayerInterface::Execute_DoDamage(Actor, Applied, GetInstigator());
		if (DamageGM) DamageGM->ClearPendingDamageIcon();
	}
}

AThrownFlash::AThrownFlash()
{
	// The held weapon and the projectile are separate actors. Replace the inherited
	// placeholder sphere with the released flashbang body (without its fuse assembly).
	Mesh->SetStaticMesh(nullptr);
	Mesh->SetHiddenInGame(true);
	FlashMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FlashMesh"));
	FlashMesh->SetupAttachment(Collision);
	FlashMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlashMesh->SetGenerateOverlapEvents(false);
	FlashMesh->SetCastShadow(true);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> FlashBody(
		TEXT("/Game/FPSTP/Content/Weapons/Grenades/GrenadeList/FlashGrenade/SkeletalMesh/SK_FlashGrenadeWithoutFuse.SK_FlashGrenadeWithoutFuse"));
	if (FlashBody.Succeeded())
	{
		FlashMesh->SetSkeletalMesh(FlashBody.Object);
	}

	EffectType = EThrownEffect::Flash;
	FuseTime = 1.4f;
	bDetonateOnImpact = false;
	Damage = 0.f;
	InnerRadius = 200.f;
	DamageRadius = 900.f;
	bRequireLineOfSight = true;
}

void AThrownFlash::OnDetonate_Implementation()
{
	// OnDetonate is multicast, so the released body disappears on every client.
	if (IsValid(FlashMesh)) FlashMesh->SetHiddenInGame(true);
	Super::OnDetonate_Implementation();
}

AThrownMolotov::AThrownMolotov()
{
	EffectType = EThrownEffect::Molotov;
	FuseTime = 10.f;
	bDetonateOnImpact = true;
	Damage = 40.f;
	InnerRadius = 90.f;
	DamageRadius = 260.f;
	bRequireLineOfSight = false;
	bShouldBounce = false;
}
