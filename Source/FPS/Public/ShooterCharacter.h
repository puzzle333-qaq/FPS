// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include <GameFramework/SpringArmComponent.h>
#include <Camera/CameraComponent.h>
#include "GameFramework/Character.h"
#include "CombatComponent.h"
#include "Stats/StatsComponent.h"
#include "ShooterType/ShooterType.h"
#include "Interface/PlayerInterface.h"
#include "ShooterCharacter.generated.h"

class UCombatComponent;
class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UTimelineComponent;
class UAnimMontage;
class AWeapon;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWeaponFirstReplicated, AWeapon*, Weapon);

UCLASS()
class FPS_API AShooterCharacter : public ACharacter,public IPlayerInterface
{
	GENERATED_BODY()

public:
	AShooterCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	/** PlayerInterface */
	virtual FName GetWeaponAttachPoint_Implementation(const FGameplayTag& WeaponType)const override;
	virtual USkeletalMeshComponent* GetMesh1P_Implementation() const override;
	virtual USkeletalMeshComponent* GetMesh3P_Implementation() const override;
	virtual void WeaponReplicated_Implementation() override;
	virtual AWeapon* GetCurrentWeapon_Implementation() override;
	virtual int32 GetReserveAmmo_Implementation() const override;
	virtual void Notify_ReloadWeapon_Implementation() override;
	virtual bool DoDamage_Implementation(float DamageAmout,AActor* DamageInstigator) override;
	/** PlayerInterface */

	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

	UFUNCTION(BlueprintCallable)
	FRotator GetFixedAimRotation()const;

	UPROPERTY(BlueprintReadOnly,Category = "FPS|FABRIK")
	FTransform FABRIK_SocketTransform;

	UFUNCTION(BlueprintCallable)
	bool HasCurrentWeapon() const;

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void EquipMainWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void EquipSeconedaryWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void EquipMeleeWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void EquipPistolWeapon();

	UFUNCTION(BlueprintCallable, Category = "FPS|Heal")
	void HealPlayer(float InHealAmout);

	// Anim notify entry. The montage-end callback also commits once; the server applies the heal only once.
	UFUNCTION(BlueprintCallable, Category = "FPS|Heal")
	void Notify_HealPlayer();

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Heal")
	TObjectPtr<UAnimMontage> HealMontage;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Heal")
	float HealAmout;

	UPROPERTY(BlueprintAssignable)
	FWeaponFirstReplicated OnWeaponFirstReplicated;

	bool HasWeaponFirstReplicated() const { return bWeaponFirstReplicated; }

	UPROPERTY(EditDefaultsOnly, Category = "FPS|HitReact")
	TArray<TObjectPtr<UAnimMontage>> HitReacts;

	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	UCombatComponent* GetCombatComponent() const { return Combat; }
	float GetDefaultFieldOfView() const { return DefaultFOV; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Health")
	TObjectPtr<UStatsComponent>Stats;



	UPROPERTY(VisibleAnywhere,BlueprintReadOnly , Category = "FPS|Combat")
	TObjectPtr<UCombatComponent> Combat;

	UFUNCTION(BlueprintImplementableEvent)
	void OnAim(bool bIsAiming);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Aiming")
	float DefaultFOV;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly)
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|TurnInPlace")
	float AO_Yaw;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Strafing")
	float MovementOffsetYaw;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|TurnInPlace")
	ETurningInPlace TurningStatus;
	
	UFUNCTION(NetMulticast,Reliable)
	void Multicast_HitReact(int32 MontageIndex);


private:

	void Input_CycleWeapon();
	void Input_ReloadWeapon();
	void Input_FireWeapon_Pressed();
	void Input_FireWeapon_Released();
	void Input_Aim_Pressed();
	void Input_EquipMainWeapon();
	void Input_EquipSeconedaryWeapon();
	void Input_EquipMeleeWeapon();
	void Input_EquipPistolWeapon();
	void Input_Heal();
	void Input_VersionWeapon();
	bool PlayHealMontage(USkeletalMeshComponent* HealMesh, bool bBindEnd);
	void OnHealMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void HideEquippedWeaponForHeal();
	void RestoreWeaponAfterHeal();

	UFUNCTION(Server, Reliable)
	void Server_BeginHeal(float InHealAmout);

	UFUNCTION(Server, Reliable)
	void Server_FinishHeal(bool bCompleted);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayHealMontage();
	void ConfigureWeaponHitCollision();

	void CalculateFABRIKSocket();
	void CalculateTurnInPlaceParameters(float DeltaTime);
	void TurnInPlace(float DeltaTime);
	void ConfigureAimTickOrder();
	void UpdateAimSocketAlignment(float DeltaTime);
	float GetAimTimelineAlpha(float DeltaTime);

	bool bWeaponFirstReplicated;
	FRotator StartingAimRotation;

	float InterpAO_Yaw;
	float AimAlignAlpha = 0.f;

	bool bIsHealing = false;
	TWeakObjectPtr<AWeapon> HealHiddenWeapon;
	bool bServerHealing = false;
	float PendingHealAmout = 0.f;
	float ServerHealAmout = 0.f;
	FTimerHandle HealLocalTimer;
	FTimerHandle HealServerTimer;

	void AbortLocalHeal();
	void AbortServerHeal();

	// Mesh1P relative pose captured at BeginPlay. Used when the blueprint hip variables are absent.
	FVector AuthoredHipMeshLocation = FVector::ZeroVector;
	FRotator AuthoredHipMeshRotation = FRotator::ZeroRotator;

	TWeakObjectPtr<AWeapon> MissingAimSocketWeapon;
	TWeakObjectPtr<UTimelineComponent> AimTimeline;


	//1st preson view(arms)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Mesh", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> Mesh1P;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpringArmComponent> SpringArm;


	UPROPERTY(EditAnywhere,Category = "FPS|Input")
	TObjectPtr<UInputAction> CycleWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> FireWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> ReloadWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> AimWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> EquipMainWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> EquipSeconedaryWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> EquipMeleeWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> EquipPistolWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction>HealAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction>VersionWeaponAction;


};
