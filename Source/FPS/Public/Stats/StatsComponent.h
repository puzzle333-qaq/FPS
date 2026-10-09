

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

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class FPS_API UStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UStatsComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "FPS|Health")
	static UStatsComponent* FindStatsComponent(const AActor* Actor) { return IsValid(Actor) ? Actor->FindComponentByClass<UStatsComponent>() : nullptr; }

	UFUNCTION(BlueprintCallable)
	float GetHealthNormolize() const;

	bool ChangeHealthByAmout(float Amout, AActor* Instigator);
	void ChangeMaxHealthByAmout(float Amout, AActor* Instigator);

	UPROPERTY(ReplicatedUsing = OnRep_DeathState)
	EDeathState DeathState;
	
	UPROPERTY(ReplicatedUsing = OnRep_Health, EditDefaultsOnly, Category = "FPS|Health")
	float Health;

	UPROPERTY(ReplicatedUsing = OnRep_MaxHealth, EditDefaultsOnly, Category = "FPS|Health")
	float MaxHealth;

	UPROPERTY(BlueprintAssignable)
	FHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FHealthChanged OnMaxHealthChanged;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_DeathState(EDeathState OldDeathState);

	UFUNCTION()
	void OnRep_Health(float OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(float OldValue);
public:	

	
};
