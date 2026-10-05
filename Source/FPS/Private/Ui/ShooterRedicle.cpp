// Fill out your copyright notice in the Description page of Project Settings.


#include "Ui/ShooterRedicle.h"

#include "CombatComponent.h"
#include "Interface/PlayerInterface.h"
#include "ShooterCharacter.h"
#include "Components/Image.h"
#include "Weapon/Weapon.h"

void UShooterRedicle::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	GetOwningPlayer()->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);

	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(GetOwningPlayer()->GetPawn());
	if (!IsValid(ShooterCharacter)) return;

	OnPossessedPawnChanged(nullptr, ShooterCharacter);

	if (ShooterCharacter->HasWeaponFirstReplicated())
	{
		AWeapon* Weapon = IPlayerInterface::Execute_GetCurrentWeapon(ShooterCharacter);
		if (IsValid(Weapon))
		{
			OnReticleChanged(Weapon->GetReticleDynamicMaterialInstance());
			OnAmmoCounterChanged(Weapon->GetAmmoCounterDynamicMaterialInstance(), Weapon->Ammo, Weapon->MagCapacity);
		}
	}
	else
	{
		ShooterCharacter->OnWeaponFirstReplicated.AddDynamic(this, &ThisClass::OnWeaponFirstReplicated);
	}
}

void UShooterRedicle::NativeTick(const FGeometry & MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}

void UShooterRedicle::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	UCombatComponent* OldPawnCombat = UCombatComponent::FindCombatComponent(OldPawn);
	if(IsValid(OldPawnCombat))
	{
		OldPawnCombat->OnReticleChanged.RemoveDynamic(this, &ThisClass::OnReticleChanged);
		OldPawnCombat->OnAmmoCounterChanged.RemoveDynamic(this, &ThisClass::OnAmmoCounterChanged);
	}
	UCombatComponent* NewPawnCombat = UCombatComponent::FindCombatComponent(NewPawn);
	if (IsValid(NewPawnCombat))
	{
		NewPawnCombat->OnReticleChanged.AddDynamic(this, &ThisClass::OnReticleChanged);
		NewPawnCombat->OnAmmoCounterChanged.AddDynamic(this, &ThisClass::OnAmmoCounterChanged);
	}
}

void UShooterRedicle::OnWeaponFirstReplicated(AWeapon* Weapon)
{
	OnReticleChanged(Weapon->GetReticleDynamicMaterialInstance());
	OnAmmoCounterChanged(Weapon->GetAmmoCounterDynamicMaterialInstance(), Weapon->Ammo, Weapon->MagCapacity);
}

void UShooterRedicle::OnReticleChanged(UMaterialInstanceDynamic* ReticleMaterialInst)
{
	CurrentRedicle_DynMatInst = ReticleMaterialInst;

	FSlateBrush Brush;
	Brush.SetResourceObject(ReticleMaterialInst);
	if(IsValid(Image_Redicle))
	{
		Image_Redicle->SetBrush(Brush);
	}
}

void UShooterRedicle::OnAmmoCounterChanged(UMaterialInstanceDynamic * AmmoCounterDynamicMaterialInst, int32 RoundsCurrent, int32 RoundsMax)
{
	CurrentAmmoCounter_DynMatInst = AmmoCounterDynamicMaterialInst;

	FSlateBrush Brush;
	Brush.SetResourceObject(AmmoCounterDynamicMaterialInst);
	if (IsValid(Image_AmmoCounter))
	{
		Image_AmmoCounter->SetBrush(Brush);
	}
}
