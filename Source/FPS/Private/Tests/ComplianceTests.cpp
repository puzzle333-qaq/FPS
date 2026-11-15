#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ShooterPlayerController.h"
#include "Game/HostageActor.h"
#include "Game/HostileAICharacter.h"
#include "Game/CompliancePoseComponent.h"
#include "Game/CoopCqbGameState.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/DefaultPawn.h"
#include "Components/BoxComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectedComplianceTest,"FPS.Compliance.DirectedWarningAndBinding",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDirectedComplianceTest::RunTest(const FString&)
{
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 W->InitializeActorsForPlay(FURL());W->GetWorldSettings()->NotifyBeginPlay();
 auto* GS=W->SpawnActor<ACoopCqbGameState>();W->SetGameState(GS);GS->SetCoopPhase(ECoopCqbPhase::InProgress);
 auto* PC=W->SpawnActor<AShooterPlayerController>();
 auto* Operator=W->SpawnActor<ADefaultPawn>(FVector(0,0,100),FRotator::ZeroRotator);PC->Possess(Operator);PC->SetControlRotation(FRotator::ZeroRotator);
 auto* A=W->SpawnActor<AHostageActor>(FVector(500,0,100),FRotator::ZeroRotator);
 auto* B=W->SpawnActor<AHostageActor>(FVector(500,240,100),FRotator::ZeroRotator);
 TestTrue(TEXT("Warnings have no UI prompt"),A->GetInteractPrompt_Implementation(Operator).IsEmpty());
 PC->SetViewTarget(Operator);
 TestEqual(TEXT("Aim selects only the civilian on the ray"),PC->TraceWarningTarget(),static_cast<AActor*>(A));
 PC->ProcessEvent(PC->FindFunctionChecked(TEXT("Server_SurrenderWarning")),nullptr);
 TestEqual(TEXT("Selected civilian starts compliance"),A->GetBehavior(),EHostageBehavior::Complying);
 TestEqual(TEXT("Nearby civilian does not receive a broadcast warning"),B->GetBehavior(),EHostageBehavior::Calm);
 auto* Wall=W->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(Box);Wall->AddInstanceComponent(Box);Box->RegisterComponent();Box->SetBoxExtent(FVector(15,80,150));Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Block);Wall->SetActorLocation(FVector(250,0,100));
 TestNull(TEXT("A wall blocks the warning ray"),PC->TraceWarningTarget());Wall->Destroy();
 TestFalse(TEXT("Standing at the front cannot bind"),A->CanInteract_Implementation(Operator));
 TestTrue(TEXT("Waiting for compliance has no UI prompt"),A->GetInteractPrompt_Implementation(Operator).IsEmpty());
 // TimerManager only ticks once per engine frame; drive the behavior while advancing this isolated world.
 for(int32 I=0;I<70;++I){W->Tick(LEVELTICK_All,.1f);A->UpdateBehavior();}
 TestEqual(TEXT("Full hands-up and kneel transition completes before binding"),A->GetBehavior(),EHostageBehavior::Compliant);
 Operator->SetActorLocation(FVector(600,0,100));
 TestFalse(TEXT("Only an available restraint shows an interaction prompt"),A->GetInteractPrompt_Implementation(Operator).IsEmpty());
 TestTrue(TEXT("Officer must be nearby behind the compliant civilian"),A->CanInteract_Implementation(Operator));
 A->Interact_Implementation(Operator);TestFalse(TEXT("Binding does not award rescue immediately"),A->IsRescued());
 Operator->SetActorLocation(FVector(300,0,100));W->Tick(LEVELTICK_All,.2f);A->UpdateBehavior();
 TestEqual(TEXT("Leaving during binding cancels it"),A->GetBehavior(),EHostageBehavior::Compliant);
 Operator->SetActorLocation(FVector(600,0,100));A->Interact_Implementation(Operator);
 for(int32 I=0;I<40;++I){W->Tick(LEVELTICK_All,.1f);A->UpdateBehavior();}
 TestTrue(TEXT("Completed binding secures the civilian once"),A->IsRescued());
 A->Auth_ReceiveWarning(Operator);TestEqual(TEXT("Secured civilian cannot restart compliance"),A->GetBehavior(),EHostageBehavior::Secured);
 B->Auth_Frighten(FVector::ZeroVector);TestEqual(TEXT("Unwarned civilian is frightened by threats"),B->GetBehavior(),EHostageBehavior::Scared);
 B->Auth_ReceiveWarning(Operator);TestEqual(TEXT("A directed warning interrupts the escape grace period"),B->GetBehavior(),EHostageBehavior::Complying);
 TArray<AActor*> Actors;for(TActorIterator<AActor> It(W);It;++It)Actors.Add(*It);for(auto* Actor:Actors)Actor->Destroy();
 W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
#endif
