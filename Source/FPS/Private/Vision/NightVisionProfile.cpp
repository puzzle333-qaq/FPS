#include "Vision/NightVisionProfile.h"
#include "Engine/Scene.h"
void NightVisionProfile::Apply(FPostProcessSettings& P,float Battery,float Wash)
{
 P.bOverride_AutoExposureMethod=true;P.AutoExposureMethod=EAutoExposureMethod::AEM_Histogram;
 P.bOverride_AutoExposureMinBrightness=true;P.AutoExposureMinBrightness=-2.f;
 P.bOverride_AutoExposureMaxBrightness=true;P.AutoExposureMaxBrightness=3.f;
 P.bOverride_AutoExposureBias=true;P.AutoExposureBias=.4f+Wash*3.5f;
 P.bOverride_BloomIntensity=true;P.BloomIntensity=1.8f+Wash*8.f;
 P.bOverride_BloomThreshold=true;P.BloomThreshold=1.05f;
 P.bOverride_VignetteIntensity=true;P.VignetteIntensity=0;
 P.bOverride_FilmGrainIntensity=true;P.FilmGrainIntensity=.22f+(1.f-Battery)*.25f;
}
