


#include "Stats/StatsComponent.h"
#include "Net/UnrealNetwork.h"


UStatsComponent::UStatsComponent()
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;
	DeathState = EDeathState::NotDead;
	SetIsReplicatedByDefault(true);

	Health = 100.f;
	MaxHealth = 100.f;
	

}

void UStatsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	DOREPLIFETIME(UStatsComponent, DeathState);
	DOREPLIFETIME_CONDITION(UStatsComponent, Health, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UStatsComponent, MaxHealth, COND_OwnerOnly);
}

bool UStatsComponent::ChangeHealthByAmout(float Amout, AActor* Instigator)
{
	float OldValue = Health;
	Health = FMath::Clamp(Health + Amout, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(this, OldValue, Health, Instigator);
	return false;
}

void UStatsComponent::ChangeMaxHealthByAmout(float Amout, AActor* Instigator)
{
	float OldValue = MaxHealth;
	MaxHealth += Amout;
	OnMaxHealthChanged.Broadcast(this, OldValue, MaxHealth, Instigator);
}

float UStatsComponent::GetHealthNormolize() const
{
	return (MaxHealth > 0) ? (Health / MaxHealth) : 0.0f;
}


void UStatsComponent::BeginPlay()
{
	Super::BeginPlay();

}

void UStatsComponent::OnRep_DeathState(EDeathState OldDeathState)
{

}

void UStatsComponent::OnRep_Health(float OldValue)
{
	OnHealthChanged.Broadcast(this, OldValue, Health, nullptr);
}

void UStatsComponent::OnRep_MaxHealth(float OldValue)
{
	OnMaxHealthChanged.Broadcast(this, OldValue, MaxHealth, nullptr);
}




