#include "World/MissionObjective.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Game/CoopCqbGameMode.h"
#include "Game/CoopCqbGameState.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"
#include "Game/MissionRadioComponent.h"

AMissionObjective::AMissionObjective()
{
 bReplicates = true;
 auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ObjectiveMesh"));
 SetRootComponent(Mesh);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 if (Cube.Succeeded()) Mesh->SetStaticMesh(Cube.Object);
 Mesh->SetRelativeScale3D(FVector(.25f));
 Mesh->SetCollisionProfileName(TEXT("BlockAll"));
 ObjectiveName = NSLOCTEXT("Mission", "Evidence", "收集现场证据");
}
void AMissionObjective::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(AMissionObjective, bCompleted);
}
bool AMissionObjective::CanInteract_Implementation(APawn* Pawn) const
{
 const auto* GS = GetWorld()->GetGameState<ACoopCqbGameState>();
 return IsValid(Pawn) && !bCompleted && GS && GS->IsMissionLive() && !GS->bRangeHub;
}
FText AMissionObjective::GetInteractPrompt_Implementation(APawn* Pawn) const
{
 return FText::Format(NSLOCTEXT("Mission", "ObjectivePrompt", "按 F：{0}"), ObjectiveName);
}
void AMissionObjective::Interact_Implementation(APawn* Pawn)
{
 if (CanInteract_Implementation(Pawn)) CompleteObjective();
}
void AMissionObjective::CompleteObjective()
{
 if (!HasAuthority() || bCompleted) return;
 auto* GM = GetWorld()->GetAuthGameMode<ACoopCqbGameMode>();
 const auto* GS = GetWorld()->GetGameState<ACoopCqbGameState>();
 if (!GM || !GS || !GS->IsMissionLive() || GS->bRangeHub) return;
 bCompleted = true;
 OnRep_Completed();
 ForceNetUpdate();
 if(GS->MissionRadio)
 {
  GS->MissionRadio->QueueCue(CompletionVoice);
  GS->MissionRadio->QueueCue(CommanderResponse,true);
 }
 GM->Auth_RefreshObjectives();
}
void AMissionObjective::OnRep_Completed(){}
