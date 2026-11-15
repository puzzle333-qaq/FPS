#include "Vision/NightVisionProfile.h"
#include "Engine/Scene.h"
void NightVisionProfile::Apply(FPostProcessSettings& P,float Battery,float Wash)
{
 P.bOverride_AutoExposureMethod=true;P.AutoExposureMethod=EAutoExposureMethod::AEM_Histogram;
 P.bOverride_AutoExposureMinBrightness=true;P.AutoExposureMinBrightness=-3.f;
 P.bOverride_AutoExposureMaxBrightness=true;P.AutoExposureMaxBrightness=2.5f;
 P.bOverride_AutoExposureBias=true;P.AutoExposureBias=1.5f+Wash*3.5f;
 P.bOverride_LocalExposureMethod=true;P.LocalExposureMethod=ELocalExposureMethod::Bilateral;
 P.bOverride_LocalExposureHighlightContrastScale=true;P.LocalExposureHighlightContrastScale=.6f;
 P.bOverride_LocalExposureShadowContrastScale=true;P.LocalExposureShadowContrastScale=.8f;
 P.bOverride_BloomIntensity=true;P.BloomIntensity=1.8f+Wash*8.f;
 P.bOverride_BloomThreshold=true;P.BloomThreshold=1.f;
 P.bOverride_VignetteIntensity=true;P.VignetteIntensity=0;
 // Grain is generated in the tube shader, driven by signal and battery level.
 P.bOverride_FilmGrainIntensity=true;P.FilmGrainIntensity=0.f;
 (void)Battery;
}
