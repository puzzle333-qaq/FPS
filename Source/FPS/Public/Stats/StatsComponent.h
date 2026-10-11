

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "StatsComponent.generated.h"

UENUM(BlueprintType)
enum class EDeathState : uint8
{
	NotDead,
	DeadStart,
	DeadFinished
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FHealthChanged, UStatsComponent*, StatsComponent, float, OldValue, float, NewValue, AActor*, Instigator);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FStaminaChanged, UStatsComponent*, StatsComponent, float, OldValue, float, NewValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FStaminaDepleted);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class FPS_API UStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UStatsComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	void SetSprinting(bool bNewSprinting) { bIsSprinting = bNewSprinting; }

	UPROPERTY(BlueprintAssignable)
	FStaminaDepleted OnStaminaDepleted;


	UPROPERTY(EditDefaultsOnly, Category = "FPS|Stamina")
	float StaminaDrainRate = 20.f;   

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Stamina")
	float StaminaRegenRate = 10.f;   

	UFUNCTION(BlueprintPure, Category = "FPS|Health")
	static UStatsComponent* FindStatsComponent(const AActor* Actor) { return IsValid(Actor) ? Actor->FindComponentByClass<UStatsComponent>() : nullptr; }

	UFUNCTION(BlueprintCallable)
	float GetHealthNormolize() const;

	bool ChangeHealthByAmout(float Amout, AActor* Instigator);
	void ChangeMaxHealthByAmout(float Amout, AActor* Instigator);

	bool ChangeStaminaByAmout(float Amout);
	void ChangeMaxStaminaAmout(float Amout);

	UPROPERTY(ReplicatedUsing = OnRep_DeathState)
	EDeathState DeathState;
	
	UPROPERTY(ReplicatedUsing = OnRep_Health, EditDefaultsOnly, Category = "FPS|Health")
	float Health;

	UPROPERTY(ReplicatedUsing = OnRep_MaxHealth, EditDefaultsOnly, Category = "FPS|Health")
	float MaxHealth;

	UPROPERTY(ReplicatedUsing = OnRep_Stamina, EditDefaultsOnly, Category = "FPS|Stamina")
	float Stamina;

	UPROPERTY(ReplicatedUsing = OnRep_MaxStamina, EditDefaultsOnly, Category = "FPS|Stamina")
	float MaxStamina;

	UPROPERTY(BlueprintAssignable)
	FHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FHealthChanged OnMaxHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FStaminaChanged OnStaminaChanged;

	UPROPERTY(BlueprintAssignable)
	FStaminaChanged OnMaxStaminaChanged;


protected:
	virtual void BeginPlay() override;
	bool bIsSprinting = false;

	UFUNCTION()
	void OnRep_DeathState(EDeathState OldDeathState);

	UFUNCTION()
	void OnRep_Health(float OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(float OldValue);

	UFUNCTION()
	void OnRep_Stamina(float OldValue);

	UFUNCTION()
	void OnRep_MaxStamina(float OldValue);
public:	

	
};
