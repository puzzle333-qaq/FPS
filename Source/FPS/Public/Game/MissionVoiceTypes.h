#pragma once
#include "CoreMinimal.h"
#include "MissionVoiceTypes.generated.h"

class USoundBase;

UENUM(BlueprintType)
enum class EMissionVoiceCue : uint8
{
 None, Briefing, Opening, HiddenSilent, HiddenShipping, HiddenBreaker, HiddenCrates,
 BenchSecured, CivilianSecured, LedgerFound, RoomClear, SuspectDown, SuspectIncap, TargetSecured,
 ShoutDontMove, ShoutDropWeapon, ShoutGround, ShoutHandsUp, ShoutSeeHands, ShoutShowHands
};

namespace MissionVoice
{
 FPS_API const TCHAR* AssetName(EMissionVoiceCue Cue);
 FPS_API float Duration(EMissionVoiceCue Cue);
 FPS_API USoundBase* LoadSound(EMissionVoiceCue Cue);
 FPS_API EMissionVoiceCue NextWarning(int32& PreviousIndex);
}
