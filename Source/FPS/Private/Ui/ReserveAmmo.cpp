// Fill out your copyright notice in the Description page of Project Settings.


#include "Ui/ReserveAmmo.h"

#include "CombatComponent.h"
#include "Interface/PlayerInterface.h"
#include "ShooterCharacter.h"
#include "Weapon/Weapon.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UReserveAmmo::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(Image_WeaponIcon))
	{
		Image_WeaponIcon->SetRenderOpacity(0.f);
	}
	if (IsValid(Text_Ammo))
	{
		Text_Ammo->SetRenderOpacity(0.f);
	}

	APlayerController* OwningPlayer = GetOwningPlayer();
	if (!IsValid(OwningPlayer)) return;

	OwningPlayer->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);

	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(OwningPlayer->GetPawn());
	if (!IsValid(ShooterCharacter)) return;

	OnPossessedPawnChanged(nullptr, ShooterCharacter);
	RefreshAmmoFromPawn(ShooterCharacter);

	if (!ShooterCharacter->HasWeaponFirstReplicated())
	{
		ShooterCharacter->OnWeaponFirstReplicated.AddDynamic(this, &ThisClass::OnWeaponFirstReplicated);
	}
}

void UReserveAmmo::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	UCombatComponent* OldPawnCombat = UCombatComponent::FindCombatComponent(OldPawn);
	if (IsValid(OldPawnCombat))
	{
		OldPawnCombat->OnCurrentReserveAmmoChanged.RemoveDynamic(this, &ThisClass::OnCurrentReserveAmmoChanged);
	}

	UCombatComponent* NewPawnCombat = UCombatComponent::FindCombatComponent(NewPawn);
	if (IsValid(NewPawnCombat))
	{
		if (IsValid(Image_WeaponIcon))
		{
			Image_WeaponIcon->SetRenderOpacity(1.f);
		}
		if (IsValid(Text_Ammo))
		{
			Text_Ammo->SetRenderOpacity(1.f);
		}
		NewPawnCombat->OnCurrentReserveAmmoChanged.AddDynamic(this, &ThisClass::OnCurrentReserveAmmoChanged);
		RefreshAmmoFromPawn(NewPawn);
	}
}

void UReserveAmmo::RefreshAmmoFromPawn(APawn* Pawn)
{
	UCombatComponent* Combat = UCombatComponent::FindCombatComponent(Pawn);
	if (!IsValid(Combat) || !IsValid(Combat->CurrentWeapon)) return;

	OnCurrentReserveAmmoChanged(Combat->CurrentReserveAmmo, Combat->CurrentWeapon->Ammo, Combat->CurrentWeapon->WeaponIcon);
}

void UReserveAmmo::OnCurrentReserveAmmoChanged(int32 RoundsInServe, int32 RoundsInWeapon, UMaterialInterface* WeaponIconMaterial)
{
	if (IsValid(WeaponIconMaterial))
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(WeaponIconMaterial);
		if (IsValid(Image_WeaponIcon))
		{
			Image_WeaponIcon->SetBrush(Brush);
		}
	}
	if (IsValid(Text_Ammo))
	{
		FText AmmoText = FText::Format(NSLOCTEXT("AmmoText", "AmmoKey", "{0}/{1}"), RoundsInWeapon, RoundsInServe);
		Text_Ammo->SetText(AmmoText);
	}
}

void UReserveAmmo::OnWeaponFirstReplicated(AWeapon* Weapon)
{
	if (!IsValid(Weapon)) return;
	RefreshAmmoFromPawn(GetOwningPlayerPawn());
}
