#include "Settings/ShooterGameUserSettings.h"
#include "Engine/Engine.h"

UShooterGameUserSettings* UShooterGameUserSettings::GetShooterSettings()
{
	return GEngine ? Cast<UShooterGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UShooterGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	MouseSensitivity = 1.f;
	ADSSensitivityMultiplier = 1.f;
	bInvertY = false;
	FieldOfView = 90.f;
}
