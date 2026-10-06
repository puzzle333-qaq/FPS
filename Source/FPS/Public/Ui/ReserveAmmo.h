// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Texture2D.h"
#include "ReserveAmmo.generated.h"

class UTextBlock;
class UImage;
class AWeapon;

UCLASS()
class FPS_API UReserveAmmo : public UUserWidget
{
	GENERATED_BODY()
public:

	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_WeaponIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Ammo;

private:
	void RefreshAmmoFromPawn(APawn* Pawn);

	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void OnCurrentReserveAmmoChanged(int32 RoundsInServe, int32 RoundsInWeapon, UTexture2D* WeaponIcon);

	UFUNCTION()
	void OnWeaponFirstReplicated(AWeapon* Weapon);
};
