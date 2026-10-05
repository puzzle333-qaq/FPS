// Fill out your copyright notice in the Description page of Project Settings.


#include "Ui/ShooterReticle.h"

#include "CombatComponent.h"
#include "Interface/PlayerInterface.h"
#include "ShooterCharacter.h"
#include "Components/Image.h"
#include "Weapon/Weapon.h"

UImage* UShooterReticle::ResolveReticleImage()
{
	if (IsValid(Image_Reticle))
	{
		return Image_Reticle;
	}

	Image_Reticle = Cast<UImage>(GetWidgetFromName(TEXT("Image_Reticle")));
	if (!IsValid(Image_Reticle))
	{
		Image_Reticle = Cast<UImage>(GetWidgetFromName(TEXT("Image_Redicle")));
	}
	return Image_Reticle;
}

void UShooterReticle::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ResolveReticleImage();

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
		}
	}
	else
	{
		ShooterCharacter->OnWeaponFirstReplicated.AddDynamic(this, &ThisClass::OnWeaponFirstReplicated);
	}

	if (ShooterCharacter->HasAuthority())
	{
		AWeapon* Weapon = IPlayerInterface::Execute_GetCurrentWeapon(ShooterCharacter);
		if (IsValid(Weapon))
		{
			OnReticleChanged(Weapon->GetReticleDynamicMaterialInstance());
		}
	}
}

void UShooterReticle::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}

void UShooterReticle::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	UCombatComponent* OldPawnCombat = UCombatComponent::FindCombatComponent(OldPawn);
	if (IsValid(OldPawnCombat))
	{
		OldPawnCombat->OnReticleChanged.RemoveDynamic(this, &ThisClass::OnReticleChanged);
	}
	UCombatComponent* NewPawnCombat = UCombatComponent::FindCombatComponent(NewPawn);
	if (IsValid(NewPawnCombat))
	{
		NewPawnCombat->OnReticleChanged.AddDynamic(this, &ThisClass::OnReticleChanged);
	}
}

void UShooterReticle::OnWeaponFirstReplicated(AWeapon* Weapon)
{
	OnReticleChanged(Weapon->GetReticleDynamicMaterialInstance());
}

void UShooterReticle::OnReticleChanged(UMaterialInstanceDynamic* ReticleMaterialInst)
{
	CurrentReticle_DynMatInst = ReticleMaterialInst;
	if (UImage* ReticleImage = ResolveReticleImage())
	{
		ReticleImage->SetBrushResourceObject(ReticleMaterialInst);
	}
}
