


#include "Stats/StatsComponent.h"
#include "Net/UnrealNetwork.h"


UStatsComponent::UStatsComponent()
{
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bCanEverTick = true;
	DeathState = EDeathState::NotDead;
	SetIsReplicatedByDefault(true);

	Health = 100.f;
	MaxHealth = 100.f;
	
	Stamina = 100.f;
	MaxStamina = 100.f;

}

void UStatsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UStatsComponent, DeathState);
	DOREPLIFETIME_CONDITION(UStatsComponent, Health, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UStatsComponent, MaxHealth, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UStatsComponent, Stamina, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UStatsComponent, MaxStamina, COND_OwnerOnly);
}

void UStatsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 耐力只由服务器计算，再同步给客户端，避免两边各算各的
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority()) return;

	// 只看水平速度，站着不动或者只是在跳都不算在跑
	const bool bIsMoving = Owner->GetVelocity().Size2D() > 10.f;
	const bool bIsDraining = bIsSprinting && bIsMoving;

	const float Amout = bIsDraining ? -StaminaDrainRate * DeltaTime : StaminaRegenRate * DeltaTime;
	const bool bHasStamina = ChangeStaminaByAmout(Amout);

	if (bIsDraining && !bHasStamina)
	{
		bIsSprinting = false;
		OnStaminaDepleted.Broadcast();
	}
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

bool UStatsComponent::ChangeStaminaByAmout(float Amout)
{
	const float OldValue = Stamina;
	Stamina = FMath::Clamp(Stamina + Amout, 0.f, MaxStamina);
	if (OldValue != Stamina)
	{
		OnStaminaChanged.Broadcast(this, OldValue, Stamina);
	}
	return  Stamina > 0.f;
}

void UStatsComponent::ChangeMaxStaminaAmout(float Amout)
{
	float OldValue = MaxStamina;
	MaxStamina += Amout;
	OnMaxStaminaChanged.Broadcast(this, OldValue, MaxStamina);
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

void UStatsComponent::OnRep_Stamina(float OldValue)
{
	OnStaminaChanged.Broadcast(this, OldValue, Stamina);
}

void UStatsComponent::OnRep_MaxStamina(float OldValue)
{
	OnMaxStaminaChanged.Broadcast(this, OldValue, MaxStamina);
}




