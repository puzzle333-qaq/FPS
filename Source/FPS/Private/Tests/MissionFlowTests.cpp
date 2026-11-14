#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Game/MissionTypes.h"
#include "Game/CoopCqbGameMode.h"
#include "Game/CoopCqbGameState.h"
#include "Game/HostageActor.h"
#include "Game/HostileAICharacter.h"
#include "World/MissionObjective.h"
#include "World/MissionDeployPoint.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMissionScoreTest, "FPS.Mission.Score", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMissionScoreTest::RunTest(const FString& Parameters)
{
 FMissionScoreRules Rules;
 FMissionReport R;
 R.SuspectsTotal = 4; R.Arrested = 4; R.HostagesTotal = 2; R.Rescued = 2;
 R.OperatorsTotal = R.Survivors = 4; R.MarkedTotal = R.MarkedCompleted = 1; R.bPrimaryComplete = true;
 R.Calculate(Rules, true, true);
 TestEqual(TEXT("All arrested, hostages safe, squad alive earns S"), R.Grade, FString(TEXT("S")));
 TestEqual(TEXT("Perfect score matches denominator without hidden objectives"), R.Score, R.MaximumScore);
 const int32 Perfect = R.Score;
 R.HiddenCompleted = 1; R.Calculate(Rules, true, true);
 TestEqual(TEXT("Hidden task is bonus and never increases denominator"), R.MaximumScore, Perfect);
 TestEqual(TEXT("Hidden task grants points"), R.Score, Perfect + Rules.Hidden);
 R.HiddenCompleted = 0; R.Arrested = 0; R.Neutralized = 4; R.Calculate(Rules, true, true);
 TestTrue(TEXT("Lethal resolution earns less than arrest"), R.Score < Perfect);
 R.SurrenderedKilled = 4; R.Calculate(Rules, true, true);
 TestEqual(TEXT("Surrendered suspects carry penalties"), R.Penalties, 4 * Rules.SurrenderedKilledPenalty);
 TestTrue(TEXT("Surrendered kills never earn S"), R.Grade != TEXT("S"));
 R.Calculate(Rules, false, true);
 TestEqual(TEXT("Failure always grades F even with earned points"), R.Grade, FString(TEXT("F")));
 R.Calculate(Rules, false, false);
 TestEqual(TEXT("Live score has no final grade"), R.Grade, FString(TEXT("—")));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMissionObjectiveFlowTest, "FPS.Mission.ObjectiveFlow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMissionObjectiveFlowTest::RunTest(const FString& Parameters)
{
 UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
 auto* GS = World->SpawnActor<ACoopCqbGameState>();
 World->SetGameState(GS);
 auto* GM = World->SpawnActor<ACoopCqbGameMode>();
 GM->GameState = GS;
 auto* Suspect = World->SpawnActor<AHostileAICharacter>();
 auto* Hostage = World->SpawnActor<AHostageActor>();
 auto* Marked = World->SpawnActor<AMissionObjective>();
 auto* Hidden = World->SpawnActor<AMissionObjective>(); Hidden->bHiddenObjective = true;
 auto* Operator = World->SpawnActor<APawn>();
 if (!TestNotNull(TEXT("Suspect spawned"), Suspect)) { World->DestroyWorld(false); return false; }
 GS->SetCoopPhase(ECoopCqbPhase::InProgress);
 GM->MissionSuspects.Add(Suspect); GM->MissionHostages.Add(Hostage);
 GM->MissionObjectives = {Marked, Hidden};
 GM->Auth_RefreshObjectives();
 TestFalse(TEXT("Unfinished tasks do not end mission"), GM->bMissionEnded);
 const FVector HostageLocation = Hostage->GetActorLocation();
 Hostage->Interact_Implementation(Operator);
 TestTrue(TEXT("Interaction rescues hostage"), Hostage->IsRescued());
 TestTrue(TEXT("Rescued hostage stays exactly in place"), Hostage->GetActorLocation().Equals(HostageLocation));
 Hostage->Interact_Implementation(Operator);
 GM->UpdateReport();
 Hidden->bCompleted=true;GM->UpdateReport();
 TestEqual(TEXT("Completed secret remains unnamed during mission"),GS->ObjectiveDescriptions.Num(),1);
 TestTrue(TEXT("No hidden names replicated before final report"),GS->CompletedHiddenRecords.IsEmpty());
 Hidden->bCompleted=false;
 TestEqual(TEXT("Repeated rescue cannot award points twice"), GS->Report.Rescued, 1);
 Marked->bCompleted = true;
 Suspect->SetMorale(EHostileMorale::Surrender);
 GM->Auth_RefreshObjectives();
 TestFalse(TEXT("Surrender alone cannot finish the mission"), GM->bMissionEnded);
 TestEqual(TEXT("Surrendered suspect remains unresolved"), GS->GetHostileAliveCount(), 1);
 const FVector SuspectLocation = Suspect->GetActorLocation();
 Suspect->Interact_Implementation(Operator);
 TestTrue(TEXT("Surrendered suspect can be arrested"), Suspect->IsArrested());
 TestTrue(TEXT("Arrest stays in place"), Suspect->GetActorLocation().Equals(SuspectLocation));
 GM->Auth_RefreshObjectives();
 TestTrue(TEXT("All required tasks complete ends mission"), GM->bMissionEnded);
 TestTrue(TEXT("Hidden task does not block success"), GS->DidOperatorsWin());
 TestEqual(TEXT("Arrests counted separately"), GS->Report.Arrested, 1);
 TestEqual(TEXT("Arrest never counts as kill"), GS->Report.Neutralized, 0);
 TestEqual(TEXT("Undiscovered hidden objective stays off tablet"), GS->ObjectiveDescriptions.Num(), 1);
 TestEqual(TEXT("Debrief counts unrevealed hidden record"),GS->HiddenRecordTotal,1);
 TestTrue(TEXT("Debrief does not reveal unfinished secret name"),GS->CompletedHiddenRecords.IsEmpty());
 TestTrue(TEXT("Debrief waits for host instead of automatic travel"),GS->ReturnEndServerTime<0);
 TestFalse(TEXT("Postmission interaction is blocked"), Hostage->CanInteract_Implementation(Operator));
 World->DestroyWorld(false);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMissionLoadBarrierTest, "FPS.Mission.LoadBarrier", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMissionLoadBarrierTest::RunTest(const FString& Parameters)
{
 UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
 World->InitializeActorsForPlay(FURL());
 auto* GS = World->SpawnActor<ACoopCqbGameState>(); World->SetGameState(GS);
 auto* GM = World->SpawnActor<ACoopCqbGameMode>(); GM->GameState = GS;
 GM->ExpectedOperators = 2;
 GM->CheckOperatorReadiness();
 TestEqual(TEXT("Missing players keep mission waiting"), GS->GetCoopPhase(), ECoopCqbPhase::Waiting);
 TestTrue(TEXT("Loading blocks movement"), GS->IsMovementLocked());
 TestTrue(TEXT("Loading blocks fire"), GS->IsActionLocked());
 GM->LoadStartedAt = -1000;
 GM->CheckOperatorReadiness();
 TestTrue(TEXT("Load timeout aborts instead of starting partial squad"), GM->bMissionEnded);
 TestEqual(TEXT("Abort reason is loading timeout"), GS->GetWinReason(), ECoopCqbWinReason::LoadTimeout);
 // A separate roster must acknowledge every member, not just the first player.
 GM->bMissionEnded = false; GM->LoadStartedAt = 0;
 GS->SetCoopPhase(ECoopCqbPhase::Waiting);
 auto* P1 = World->SpawnActor<APlayerController>();
 auto* P2 = World->SpawnActor<APlayerController>();
 P1->Possess(World->SpawnActor<APawn>()); P2->Possess(World->SpawnActor<APawn>());
 GM->Auth_OperatorReady(P1);
 TestEqual(TEXT("One ready player cannot start two-player mission"), GS->GetCoopPhase(), ECoopCqbPhase::Waiting);
 GM->Auth_OperatorReady(P1);
 TestEqual(TEXT("Duplicate acknowledgement cannot count as second player"), GS->ReadyOperators, 1);
 GM->Auth_OperatorReady(P2);
 TestEqual(TEXT("All acknowledgements advance to briefing"), GS->GetCoopPhase(), ECoopCqbPhase::Briefing);
 TestTrue(TEXT("Briefing still blocks actions before AI spawn"), GS->IsActionLocked());
 GS->bRangeHub = true;
 TestFalse(TEXT("Range remains playable outside mission phase"), GS->IsMovementLocked());
 World->DestroyWorld(false);
 return true;
}
#endif
