// Fill out your copyright notice in the Description page of Project Settings.


#include "Ui/ShooterReticle.h"

#include "CombatComponent.h"
#include "Interface/PlayerInterface.h"
#include "ShooterCharacter.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
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

	// The HUD can be built before the pawn is possessed (or on respawn); rebind when the pawn changes.
	APawn* CurrentPawn = GetOwningPlayerPawn();
	if (CurrentPawn != BoundPawn.Get())
	{
		OnPossessedPawnChanged(BoundPawn.Get(), CurrentPawn);
	}

	// Fade the hit feedback overlays. They are drawn in NativePaint and stay
	// visible in ADS; only the crosshair image below is hidden while aiming.
	const float ArcDecay = InDeltaTime / FMath::Max(DamageRingFadeTime, 0.01f);
	for (float& Intensity : ArcIntensity)
	{
		Intensity = FMath::Max(0.f, Intensity - ArcDecay);
	}
	HitMarkerRemaining = FMath::Max(0.f, HitMarkerRemaining - InDeltaTime);
	CrosshairSpread = FMath::Max(0.f, CrosshairSpread - CrosshairSpreadRecovery * InDeltaTime);

	UImage* ReticleImage = ResolveReticleImage();
	if (!IsValid(ReticleImage))
	{
		return;
	}

	// The HUD crosshair stays up while hip firing. ADS aims through the weapon
	// sight, so the widget mark has to leave the screen. The widget itself keeps
	// ticking so the mark can come back when aim is released.
	const UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetOwningPlayerPawn());
	const AWeapon* Weapon = IsValid(Combat) ? Combat->CurrentWeapon.Get() : nullptr;
	const bool bAimThroughSight = IsValid(Combat) && Combat->bAiming
		&& IsValid(Weapon) && Weapon->WeaponKind != EWeaponKind::Melee;
	const ESlateVisibility DesiredVisibility = bAimThroughSight
		? ESlateVisibility::Collapsed
		: ESlateVisibility::HitTestInvisible;
	if (ReticleImage->GetVisibility() != DesiredVisibility)
	{
		ReticleImage->SetVisibility(DesiredVisibility);
	}
}

int32 UShooterReticle::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const int32 DrawLayer = MaxLayer + 1;
	const FVector2D Center = FVector2D(AllottedGeometry.GetLocalSize()) * 0.5f;
	bool bDrew = false;

	// Damage ring: angle A is clockwise from screen-up. Arc i is centered on i * 90.
	const int32 Segments = FMath::Clamp(DamageRingSegments, 2, 64);
	const float HalfSpan = FMath::Max(1.f, 45.f - DamageRingGapDegrees * 0.5f);
	for (int32 i = 0; i < 4; ++i)
	{
		if (ArcIntensity[i] <= KINDA_SMALL_NUMBER) continue;

		TArray<FVector2D> Points;
		Points.Reserve(Segments + 1);
		const float Start = i * 90.f - HalfSpan;
		const float Step = (HalfSpan * 2.f) / Segments;
		for (int32 s = 0; s <= Segments; ++s)
		{
			const float Rad = FMath::DegreesToRadians(Start + Step * s);
			Points.Add(Center + FVector2D(FMath::Sin(Rad), -FMath::Cos(Rad)) * DamageRingRadius);
		}
		FLinearColor Color = DamageRingColor;
		Color.A *= ArcIntensity[i];
		FSlateDrawElement::MakeLines(OutDrawElements, DrawLayer, AllottedGeometry.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, Color * InWidgetStyle.GetColorAndOpacityTint(), true, DamageRingThickness);
		bDrew = true;
	}

	// Hit marker: four short lines at 45, 135, -135, -45 degrees.
	if (HitMarkerRemaining > 0.f)
	{
		FLinearColor Color = HitMarkerColor;
		Color.A *= HitMarkerRemaining / FMath::Max(HitMarkerDuration, 0.01f);
		static const FVector2D Diagonals[4] = {
			FVector2D(1.f, -1.f), FVector2D(1.f, 1.f), FVector2D(-1.f, 1.f), FVector2D(-1.f, -1.f) };
		for (const FVector2D& D : Diagonals)
		{
			const FVector2D Dir = D.GetSafeNormal();
			TArray<FVector2D> Points;
			Points.Add(Center + Dir * HitMarkerInnerOffset);
			Points.Add(Center + Dir * (HitMarkerInnerOffset + HitMarkerLength));
			FSlateDrawElement::MakeLines(OutDrawElements, DrawLayer, AllottedGeometry.ToPaintGeometry(), Points,
				ESlateDrawEffect::None, Color * InWidgetStyle.GetColorAndOpacityTint(), true, HitMarkerThickness);
		}
		bDrew = true;
	}

	// Spreading crosshair: four cardinal lines. Hidden while ADS.
	if (bDrawSpreadingCrosshair)
	{
		const UCombatComponent* CombatForSpread = UCombatComponent::FindCombatComponent(GetOwningPlayerPawn());
		const AWeapon* Wpn = IsValid(CombatForSpread) ? CombatForSpread->CurrentWeapon.Get() : nullptr;
		const bool bAdsHide = IsValid(CombatForSpread) && CombatForSpread->bAiming
			&& IsValid(Wpn) && Wpn->WeaponKind != EWeaponKind::Melee;
		if (!bAdsHide)
		{
			const float Gap = CrosshairBaseGap + CrosshairSpread;
			static const FVector2D Cardinals[4] = {
				FVector2D(0.f, -1.f), FVector2D(1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(-1.f, 0.f) };
			for (const FVector2D& Dir : Cardinals)
			{
				TArray<FVector2D> Points;
				Points.Add(Center + Dir * Gap);
				Points.Add(Center + Dir * (Gap + CrosshairLineLength));
				FSlateDrawElement::MakeLines(OutDrawElements, DrawLayer, AllottedGeometry.ToPaintGeometry(), Points,
					ESlateDrawEffect::None, CrosshairLineColor * InWidgetStyle.GetColorAndOpacityTint(), true, CrosshairLineThickness);
			}
			bDrew = true;
		}
	}
	return bDrew ? DrawLayer : MaxLayer;
}

void UShooterReticle::ShowDamageFrom(FVector InstigatorLocation)
{
	const APawn* Pawn = GetOwningPlayerPawn();
	const APlayerController* PC = GetOwningPlayer();
	if (!IsValid(Pawn)) return;

	FVector ToSource = InstigatorLocation - Pawn->GetActorLocation();
	ToSource.Z = 0.f;
	if (!ToSource.Normalize()) return;

	const float ViewYaw = IsValid(PC) ? PC->GetControlRotation().Yaw : Pawn->GetActorRotation().Yaw;
	const FVector Local = FRotator(0.f, ViewYaw, 0.f).UnrotateVector(ToSource);
	// 0 = in front, +90 = to the right (UE Y is right), +-180 = behind.
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
	const int32 Arc = FMath::RoundToInt(FRotator::ClampAxis(Angle) / 90.f) % 4;
	ArcIntensity[Arc] = 1.f;
}

void UShooterReticle::NotifyWeaponFired()
{
	CrosshairSpread = FMath::Min(CrosshairSpreadMax, CrosshairSpread + CrosshairSpreadKick);
}

void UShooterReticle::HandleWeaponFired()
{
	NotifyWeaponFired();
}

void UShooterReticle::ShowHitMarker()
{
	HitMarkerRemaining = HitMarkerDuration;
	if (HitMarkerSound)
	{
		UGameplayStatics::PlaySound2D(this, HitMarkerSound);
	}
}

void UShooterReticle::HandleDamageIndicator(FVector InstigatorLocation)
{
	ShowDamageFrom(InstigatorLocation);
}

void UShooterReticle::HandleHitConfirmed()
{
	ShowHitMarker();
}

void UShooterReticle::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	UCombatComponent* OldPawnCombat = UCombatComponent::FindCombatComponent(OldPawn);
	if (IsValid(OldPawnCombat))
	{
		OldPawnCombat->OnReticleChanged.RemoveDynamic(this, &ThisClass::OnReticleChanged);
		OldPawnCombat->OnWeaponFired.RemoveDynamic(this, &ThisClass::HandleWeaponFired);
	}

	if (AShooterCharacter* OldShooter = Cast<AShooterCharacter>(BoundPawn.Get()))
	{
		OldShooter->OnDamageIndicator.RemoveDynamic(this, &ThisClass::HandleDamageIndicator);
		OldShooter->OnHitConfirmed.RemoveDynamic(this, &ThisClass::HandleHitConfirmed);
	}
	BoundPawn = nullptr;
	if (AShooterCharacter* NewShooter = Cast<AShooterCharacter>(NewPawn))
	{
		NewShooter->OnDamageIndicator.AddUniqueDynamic(this, &ThisClass::HandleDamageIndicator);
		NewShooter->OnHitConfirmed.AddUniqueDynamic(this, &ThisClass::HandleHitConfirmed);
		BoundPawn = NewShooter;
	}

	UCombatComponent* NewPawnCombat = UCombatComponent::FindCombatComponent(NewPawn);
	if (IsValid(NewPawnCombat))
	{
		NewPawnCombat->OnReticleChanged.AddUniqueDynamic(this, &ThisClass::OnReticleChanged);
		NewPawnCombat->OnWeaponFired.AddUniqueDynamic(this, &ThisClass::HandleWeaponFired);
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
