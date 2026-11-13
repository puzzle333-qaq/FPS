#pragma once
#include "CoreMinimal.h"
struct FPostProcessSettings;
/** Shared by the first-person camera and real-time deployment camera. */
namespace NightVisionProfile { FPS_API void Apply(FPostProcessSettings& Settings,float Battery=1.f,float FlashWash=0.f); }
