#include "Combat/HitZoneVitalsComponent.h"

#include "Game/CoopCqbGameMode.h"
#include "Game/HostileAICharacter.h"
#include "ShooterCharacter.h"
#include "ShooterGameModeBase.h"
#include "Stats/StatsComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

UHitZoneVitalsComponent::UHitZoneVitalsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void UHitZoneVitalsComponent::BeginPlay()
{
	Super::BeginPlay();
	if ((GetOwner() && GetOwner()->HasAuthority()))
	{
		HelmetSavesRemaining = bHasHelmet ? FMath::Max(0, HelmetMaxSaves) : 0;
		LastCombatTime = GetWorld() ? GetWorld()->GetTimeSeconds() - OutOfCombatDelay : 0.f;
		SyncDerivedHealth();
	}
}

void UHitZoneVitalsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHitZoneVitalsComponent, TorsoHitCount);
	DOREPLIFETIME(UHitZoneVitalsComponent, HelmetSavesRemaining);
	DOREPLIFETIME(UHitZoneVitalsComponent, bHasHelmet);
}

UHitZoneVitalsComponent* UHitZoneVitalsComponent::Find(AActor* Actor)
{
	return IsValid(Actor) ? Actor->FindComponentByClass<UHitZoneVitalsComponent>() : nullptr;
}

bool UHitZoneVitalsComponent::IsAlive() const
{
	if (const AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner()))
	{
		const UStatsComponent* Stats = Shooter->GetStats();
		return !Stats || Stats->DeathState == EDeathState::NotDead;
	}
	if (const AHostileAICharacter* Hostile = Cast<AHostileAICharacter>(GetOwner()))
	{
		return Hostile->IsHostileAlive();
	}
	const UStatsComponent* Stats = GetStats();
	return !Stats || Stats->DeathState == EDeathState::NotDead;
}

EBodyArmorType UHitZoneVitalsComponent::ResolveArmor() const
{
	if (const AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner()))
	{
		return Shooter->GetBodyArmor();
	}
	if (const AHostileAICharacter* Hostile = Cast<AHostileAICharacter>(GetOwner()))
	{
		return Hostile->GetBodyArmor();
	}
	return EBodyArmorType::None;
}

float UHitZoneVitalsComponent::GetTorsoHitsToKill() const
{
	switch (ResolveArmor())
	{
	case EBodyArmorType::Light: return FMath::Max(1.f, TorsoHitsToKillLight);
	case EBodyArmorType::Heavy: return FMath::Max(1.f, TorsoHitsToKillHeavy);
	default: return FMath::Max(1.f, TorsoHitsToKillNone);
	}
}

UStatsComponent* UHitZoneVitalsComponent::GetStats() const
{
	if (const AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner()))
	{
		return Shooter->GetStats();
	}
	if (const AHostileAICharacter* Hostile = Cast<AHostileAICharacter>(GetOwner()))
	{
		return Hostile->GetStats();
	}
	return GetOwner() ? GetOwner()->FindComponentByClass<UStatsComponent>() : nullptr;
}

void UHitZoneVitalsComponent::SyncDerivedHealth()
{
	UStatsComponent* Stats = GetStats();
	if (!Stats) return;
	const float Need = GetTorsoHitsToKill();
	const float NewHealth = IsAlive()
		? Stats->MaxHealth * FMath::Clamp(1.f - (TorsoHitCount / Need), 0.f, 1.f)
		: 0.f;
	const float Delta = NewHealth - Stats->Health;
	if (!FMath::IsNearlyZero(Delta))
	{
		Stats->ChangeHealthByAmout(Delta, GetOwner());
	}
}

void UHitZoneVitalsComponent::OnRep_TorsoHitCount()
{
	SyncDerivedHealth();
}

void UHitZoneVitalsComponent::NoteCombatEvent()
{
	if (!(GetOwner() && GetOwner()->HasAuthority()) || !GetWorld()) return;
	LastCombatTime = GetWorld()->GetTimeSeconds();
	RecoverAccum = 0.f;
}

void UHitZoneVitalsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!(GetOwner() && GetOwner()->HasAuthority()) || !bOutOfCombatTorsoRecovery || !IsAlive()) return;
	if (TorsoHitCount <= 0.f || !GetWorld()) return;

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastCombatTime < OutOfCombatDelay)
	{
		RecoverAccum = 0.f;
		return;
	}

	RecoverAccum += DeltaTime;
	while (RecoverAccum >= TorsoRecoverInterval && TorsoHitCount > 0.f)
	{
		RecoverAccum -= TorsoRecoverInterval;
		TorsoHitCount = FMath::Max(0.f, TorsoHitCount - 1.f);
		SyncDerivedHealth();
		GetOwner()->ForceNetUpdate();
	}
}

bool UHitZoneVitalsComponent::ConsumeHelmetSave()
{
	if (!bHasHelmet || HelmetSavesRemaining <= 0) return false;
	--HelmetSavesRemaining;
	return true;
}

bool UHitZoneVitalsComponent::KillOwner(AActor* Instigator)
{
	if (!(GetOwner() && GetOwner()->HasAuthority()) || !IsAlive()) return false;
	if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner()))
	{
		Shooter->Auth_OnHitZoneDamaged(Instigator, true);
		return true;
	}
	if (AHostileAICharacter* Hostile = Cast<AHostileAICharacter>(GetOwner()))
	{
		Hostile->Auth_Die(Instigator);
		SyncDerivedHealth();
		return true;
	}
	if (UStatsComponent* Stats = GetStats())
	{
		Stats->StartDeath();
		SyncDerivedHealth();
		return true;
	}
	return false;
}

bool UHitZoneVitalsComponent::AddTorsoHits(float Weight, AActor* Instigator)
{
	if (!(GetOwner() && GetOwner()->HasAuthority()) || !IsAlive()) return false;
	NoteCombatEvent();
	const float Add = FMath::Max(0.f, Weight);
	if (Add <= KINDA_SMALL_NUMBER) return false;
	TorsoHitCount += Add;
	SyncDerivedHealth();
	GetOwner()->ForceNetUpdate();
	if (TorsoHitCount + KINDA_SMALL_NUMBER >= GetTorsoHitsToKill())
	{
		return KillOwner(Instigator);
	}
	if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner()))
	{
		Shooter->Auth_OnHitZoneDamaged(Instigator, false);
	}
	return false;
}

bool UHitZoneVitalsComponent::ApplyBallisticHit(EHitZone Zone, AActor* Instigator, bool bPistolOrShotgun, float HitWeight)
{
	if (!(GetOwner() && GetOwner()->HasAuthority()) || !IsAlive()) return false;
	NoteCombatEvent();

	if (Zone == EHitZone::Arm || Zone == EHitZone::Leg)
	{
		if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner()))
		{
			Shooter->Auth_ApplyHitZoneInjury(Zone);
			Shooter->Auth_OnHitZoneDamaged(Instigator, false);
		}
		else if (AHostileAICharacter* Hostile = Cast<AHostileAICharacter>(GetOwner()))
		{
			Hostile->Auth_ApplyHitZoneInjury(Zone);
		}
		return false;
	}

	if (Zone == EHitZone::Head)
	{
		if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner()))
		{
			Shooter->Auth_ApplyHitZoneInjury(Zone);
		}
		if (bHeadIsLethal)
		{
			if (ConsumeHelmetSave())
			{
				if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner()))
				{
					Shooter->Auth_OnHitZoneDamaged(Instigator, false);
				}
				GetOwner()->ForceNetUpdate();
				return false;
			}
			return KillOwner(Instigator);
		}
		return AddTorsoHits(FMath::Max(0.05f, HitWeight), Instigator);
	}

	// Torso (and unknown -> torso).
	if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner()))
	{
		Shooter->Auth_ApplyHitZoneInjury(EHitZone::Torso);
	}
	(void)bPistolOrShotgun;
	return AddTorsoHits(FMath::Max(0.05f, HitWeight), Instigator);
}

bool UHitZoneVitalsComponent::ApplyExplosiveHit(AActor* Instigator, float Distance, float InnerRadius, float OuterRadius)
{
	if (!(GetOwner() && GetOwner()->HasAuthority()) || !IsAlive()) return false;
	NoteCombatEvent();
	if (Distance <= InnerRadius)
	{
		return KillOwner(Instigator);
	}
	if (Distance > OuterRadius) return false;
	const float Range = FMath::Max(1.f, OuterRadius - InnerRadius);
	const float Alpha = 1.f - (Distance - InnerRadius) / Range;
	return AddTorsoHits(ExplosionMaxTorsoWeight * FMath::Clamp(Alpha, 0.f, 1.f), Instigator);
}

bool UHitZoneVitalsComponent::ApplyFireTick(AActor* Instigator)
{
	return ApplyUnspecifiedHit(Instigator, FireTickTorsoWeight);
}

bool UHitZoneVitalsComponent::ApplyUnspecifiedHit(AActor* Instigator, float TorsoWeight)
{
	if (!(GetOwner() && GetOwner()->HasAuthority()) || !IsAlive()) return false;
	return AddTorsoHits(TorsoWeight, Instigator);
}

float HitZoneCombat::ResolveHitWeight(bool bMelee, bool bPistolOrShotgun, int32 PelletsPerShot, float FalloffScale)
{
	float Weight = 1.f;
	if (bMelee)
	{
		Weight = 1.f;
	}
	else if (PelletsPerShot > 1)
	{
		Weight = 0.35f;
	}
	else if (bPistolOrShotgun)
	{
		Weight = 0.75f;
	}
	return FMath::Max(0.f, Weight * FalloffScale);
}

bool HitZoneCombat::IsHitZoneVictim(AActor* Actor)
{
	return IsValid(Actor) && IsValid(UHitZoneVitalsComponent::Find(Actor));
}

static bool PassesCanDamage(AActor* Victim, AActor* Instigator)
{
	if (!IsValid(Victim) || !Victim->GetWorld()) return false;
	AShooterGameModeBase* GM = Victim->GetWorld()->GetAuthGameMode<AShooterGameModeBase>();
	if (!GM) return true;
	AController* InstCtrl = nullptr;
	AController* VictCtrl = nullptr;
	if (const APawn* InstPawn = Cast<APawn>(Instigator)) InstCtrl = InstPawn->GetController();
	if (const APawn* VictPawn = Cast<APawn>(Victim)) VictCtrl = VictPawn->GetController();
	return GM->CanDamage(InstCtrl, VictCtrl);
}

bool HitZoneCombat::ApplyBallisticToActor(AActor* Victim, AActor* Instigator, EHitZone Zone, bool bPistolOrShotgun, float HitWeight, bool bApplyInjury)
{
	UHitZoneVitalsComponent* Vitals = UHitZoneVitalsComponent::Find(Victim);
	if (!Vitals) return false;
	if (!PassesCanDamage(Victim, Instigator)) return false;
	(void)bApplyInjury;
	return Vitals->ApplyBallisticHit(Zone, Instigator, bPistolOrShotgun, HitWeight);
}

bool HitZoneCombat::ApplyExplosiveToActor(AActor* Victim, AActor* Instigator, float Distance, float InnerRadius, float OuterRadius)
{
	UHitZoneVitalsComponent* Vitals = UHitZoneVitalsComponent::Find(Victim);
	if (!Vitals) return false;
	if (!PassesCanDamage(Victim, Instigator)) return false;
	return Vitals->ApplyExplosiveHit(Instigator, Distance, InnerRadius, OuterRadius);
}

bool HitZoneCombat::ApplyFireTickToActor(AActor* Victim, AActor* Instigator)
{
	UHitZoneVitalsComponent* Vitals = UHitZoneVitalsComponent::Find(Victim);
	if (!Vitals) return false;
	if (!PassesCanDamage(Victim, Instigator)) return false;
	return Vitals->ApplyFireTick(Instigator);
}
