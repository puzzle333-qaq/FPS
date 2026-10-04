// Fill out your copyright notice in the Description page of Project Settings.


#include "Ui/ShooterRedicle.h"

#include "ShooterCharacter.h"

void UShooterRedicle::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	GetOwningPlayer()->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);

	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(GetOwningPlayer()->GetPawn());
	if (!IsValid(ShooterCharacter)) return;

	OnPossessedPawnChanged(nullptr, ShooterCharacter);

	if (ShooterCharacter->HasWeaponFirstReplicated())
	{
		//Get Dynamic Material Instance from Weapon
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
	//Unbind from delegate on the old pawn
	//Bind to delegate on  the combat component
}

void UShooterRedicle::OnWeaponFirstReplicated(AWeapon* Weapon)
{
	//Get Dynamic Material Instance from Weapon
}
