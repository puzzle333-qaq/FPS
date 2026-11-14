#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Game/MissionVoiceTypes.h"
#include "Game/MissionRadioComponent.h"
#include "Game/CoopCqbGameState.h"
#include "World/MissionDeployPoint.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Sound/SoundBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFactoryVoiceCatalogTest,"FPS.FactoryVoice.Catalog",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFactoryVoiceCatalogTest::RunTest(const FString&)
{
 for(int32 I=1;I<=int32(EMissionVoiceCue::ShoutShowHands);++I)
 {
  const auto Cue=EMissionVoiceCue(I);auto* Sound=MissionVoice::LoadSound(Cue);
  TestNotNull(MissionVoice::AssetName(Cue),Sound);
  if(Sound)TestTrue(TEXT("Catalog duration matches supplied WAV"),FMath::Abs(Sound->GetDuration()-MissionVoice::Duration(Cue))<.04f);
 }
 int32 A=-1,B=-1;
 for(int32 I=0;I<100;++I)
 {
  const int32 Previous=A;const int32 Other=B;
  MissionVoice::NextWarning(A);
  TestNotEqual(TEXT("One operator never repeats the previous shout"),A,Previous);
  TestEqual(TEXT("Choosing A's voice never changes B's independent state"),B,Other);
  MissionVoice::NextWarning(B);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFactoryReadyBarrierTest,"FPS.FactoryVoice.ReadyBarrier",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFactoryReadyBarrierTest::RunTest(const FString&)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);World->InitializeActorsForPlay(FURL());
 auto* GS=World->SpawnActor<ACoopCqbGameState>();World->SetGameState(GS);GS->bRangeHub=true;
 auto* Host=World->SpawnActor<APlayerController>();auto* Guest=World->SpawnActor<APlayerController>();
 Host->SetPlayer(NewObject<ULocalPlayer>(GEngine));
 Host->PlayerState=World->SpawnActor<APlayerState>();Guest->PlayerState=World->SpawnActor<APlayerState>();
 Host->Possess(World->SpawnActor<APawn>());Guest->Possess(World->SpawnActor<APawn>());
 auto* Deploy=World->SpawnActor<AMissionDeployPoint>();
 FMissionDestination D;D.Map=TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Lvl_Factory_Whitebox.Lvl_Factory_Whitebox")));Deploy->Missions.Add(D);
 TestTrue(TEXT("Host selects map"),Deploy->Auth_SelectMission(Host,0));
 TestEqual(TEXT("Selecting immediately plays the briefing before anyone presses Y"),Deploy->BriefingRadio->Transmission.Cue,EMissionVoiceCue::Briefing);
 const int32 SelectedSerial=Deploy->BriefingRadio->Transmission.Serial;
 TestEqual(TEXT("No readiness is assumed for the host"),Deploy->ReadyPlayers.Num(),0);
 TestTrue(TEXT("Selection alone cannot start countdown"),Deploy->CountdownEndServerTime<0);
 Deploy->Auth_ToggleReady(Host);
 TestTrue(TEXT("One ready operator cannot start two-player countdown"),Deploy->CountdownEndServerTime<0);
 Deploy->Auth_ToggleReady(Guest);
 TestTrue(TEXT("Both operators start the countdown"),Deploy->CountdownEndServerTime>0);
 TestEqual(TEXT("Y never starts/restarts the narration"),Deploy->BriefingRadio->Transmission.Serial,SelectedSerial);
 Deploy->Auth_ToggleReady(Host);
 TestTrue(TEXT("Unready cancels countdown"),Deploy->CountdownEndServerTime<0);
 TestEqual(TEXT("Unready keeps selection narration playing"),Deploy->BriefingRadio->Transmission.Cue,EMissionVoiceCue::Briefing);
 Deploy->Auth_ToggleReady(Host);
 TestTrue(TEXT("Ready again restarts countdown"),Deploy->CountdownEndServerTime>0);
 Deploy->Auth_Cancel(Host);
 TestEqual(TEXT("Cancel clears selection"),Deploy->SelectedMission,INDEX_NONE);
 TestEqual(TEXT("Cancel stops the selected map narration"),Deploy->BriefingRadio->Transmission.Cue,EMissionVoiceCue::None);
 World->DestroyWorld(false);return true;
}
#endif
