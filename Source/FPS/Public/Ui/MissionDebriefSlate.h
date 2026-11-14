#pragma once
#include "CoreMinimal.h"
class SWidget;
class AShooterPlayerController;
TSharedRef<SWidget> MakeMissionDebriefScreen(AShooterPlayerController* Owner);
bool SkipMissionDebriefAnimation(const TSharedPtr<SWidget>& Screen);
