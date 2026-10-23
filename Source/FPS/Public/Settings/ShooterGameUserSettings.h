#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "ShooterGameUserSettings.generated.h"

// 项目自己的 GameUserSettings：多存操作设置（灵敏度/开镜灵敏度/反转Y/FOV），写在 GameUserSettings.ini
// DefaultEngine.ini [/Script/Engine.Engine] GameUserSettingsClassName=/Script/FPS.ShooterGameUserSettings
UCLASS(config = GameUserSettings, configdonotcheckdefaults)
class FPS_API UShooterGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FPS|Settings")
	static UShooterGameUserSettings* GetShooterSettings();

	virtual void SetToDefaults() override;

	UPROPERTY(Config, BlueprintReadWrite, Category = "FPS|Controls")
	float MouseSensitivity = 1.f;

	// 开镜时灵敏度 = MouseSensitivity * 这个
	UPROPERTY(Config, BlueprintReadWrite, Category = "FPS|Controls")
	float ADSSensitivityMultiplier = 1.f;

	UPROPERTY(Config, BlueprintReadWrite, Category = "FPS|Controls")
	bool bInvertY = false;

	UPROPERTY(Config, BlueprintReadWrite, Category = "FPS|Controls")
	float FieldOfView = 90.f;
};
