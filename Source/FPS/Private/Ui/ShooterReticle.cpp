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

	APlayerController* OwningPlayer = GetOwningPlayer();
	if (!IsValid(OwningPlayer)) return;

	OwningPlayer->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);

	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(OwningPlayer->GetPawn());
	if (!IsValid(ShooterCharacter)) return;

	OnPossessedPawnChanged(nullptr, ShooterCharacter);

	AWeapon* Weapon = IPlayerInterface::Execute_GetCurrentWeapon(ShooterCharacter);
	if (IsValid(Weapon))
	{
		OnReticleChanged(Weapon->GetReticleDynamicMaterialInstance());
	}
	else
	{
		ShooterCharacter->OnWeaponFirstReplicated.AddDynamic(this, &ThisClass::OnWeaponFirstReplicated);
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
		if (IsValid(NewPawnCombat->CurrentWeapon))
		{
			OnReticleChanged(NewPawnCombat->CurrentWeapon->GetReticleDynamicMaterialInstance());
		}
	}
}

void UShooterReticle::OnWeaponFirstReplicated(AWeapon* Weapon)
{
	if (!IsValid(Weapon)) return;
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
