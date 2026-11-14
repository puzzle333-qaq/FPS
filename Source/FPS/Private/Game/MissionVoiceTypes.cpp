#include "Game/MissionVoiceTypes.h"
#include "Sound/SoundBase.h"

namespace
{
 struct FLine { const TCHAR* Name; float Seconds; };
 const FLine Lines[] = {
  {TEXT(""),0}, {TEXT("VO_CMD_Briefing"),27.008f}, {TEXT("VO_CMD_Opening"),10.592f},
  {TEXT("VO_CMD_Hidden_Silent"),3.56f}, {TEXT("VO_CMD_Hidden_Shipping"),4.4f}, {TEXT("VO_CMD_Hidden_Breaker"),6.32f},
  {TEXT("VO_TM_Hidden_Crates"),6.632f}, {TEXT("VO_TM_Rpt_BenchSecured"),3.512f},
  {TEXT("VO_TM_Rpt_CivilianSecured"),2.192f}, {TEXT("VO_TM_Rpt_LedgerFound"),2.36f},
  {TEXT("VO_TM_Rpt_RoomClear"),1.52f}, {TEXT("VO_TM_Rpt_SuspectDown"),1.88f},
  {TEXT("VO_TM_Rpt_SuspectIncap"),3.44f}, {TEXT("VO_TM_Rpt_TargetSecured"),1.928f},
  {TEXT("VO_TM_Shout_DontMove"),1.608f}, {TEXT("VO_TM_Shout_DropWeapon"),2.424f},
  {TEXT("VO_TM_Shout_Ground"),2.376f}, {TEXT("VO_TM_Shout_HandsUp"),2.688f},
  {TEXT("VO_TM_Shout_SeeHands"),2.352f}, {TEXT("VO_TM_Shout_ShowHands"),2.424f}
 };
}
const TCHAR* MissionVoice::AssetName(EMissionVoiceCue Cue)
{
 const int32 I=int32(Cue);return I>=0&&I<UE_ARRAY_COUNT(Lines)?Lines[I].Name:TEXT("");
}
float MissionVoice::Duration(EMissionVoiceCue Cue)
{
 const int32 I=int32(Cue);return I>=0&&I<UE_ARRAY_COUNT(Lines)?Lines[I].Seconds:0;
}
USoundBase* MissionVoice::LoadSound(EMissionVoiceCue Cue)
{
 if(Cue==EMissionVoiceCue::None)return nullptr;
 const FString Name=AssetName(Cue);
 return LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/FPS/Audio/Voice/Factory/%s.%s"),*Name,*Name));
}
EMissionVoiceCue MissionVoice::NextWarning(int32& PreviousIndex)
{
 int32 Pick=FMath::RandRange(0,PreviousIndex>=0?4:5);
 if(PreviousIndex>=0&&Pick>=PreviousIndex)++Pick;
 PreviousIndex=Pick;
 return EMissionVoiceCue(int32(EMissionVoiceCue::ShoutDontMove)+Pick);
}
