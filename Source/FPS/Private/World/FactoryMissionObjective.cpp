#include "World/FactoryMissionObjective.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Game/CoopCqbGameState.h"
#include "Stats/StatsComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

AFactoryMissionObjective::AFactoryMissionObjective()
{
 InteractionBox=CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
 InteractionBox->SetupAttachment(RootComponent);
 InteractionBox->SetBoxExtent(FVector(25,25,20));
 InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
 InteractionBox->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 InteractionBox->SetCanEverAffectNavigation(false);
 if(auto* Mesh=FindComponentByClass<UStaticMeshComponent>())Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 SetActorScale3D(FVector::OneVector);
}
void AFactoryMissionObjective::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 bHiddenObjective=Task!=EFactoryTask::Ledger&&Task!=EFactoryTask::Extraction;
 CommanderResponse=EMissionVoiceCue::None;
 switch(Task)
 {
 case EFactoryTask::Ledger:ObjectiveName=NSLOCTEXT("Factory","Ledger","找到并带走账本");CompletionVoice=EMissionVoiceCue::LedgerFound;break;
 case EFactoryTask::Shipping:ObjectiveName=NSLOCTEXT("Factory","Shipping","出货单");CompletionVoice=EMissionVoiceCue::None;CommanderResponse=EMissionVoiceCue::HiddenShipping;break;
 case EFactoryTask::Crates:ObjectiveName=NSLOCTEXT("Factory","Crates","锅炉房的秘密");CompletionVoice=EMissionVoiceCue::HiddenCrates;break;
 case EFactoryTask::Breaker:ObjectiveName=NSLOCTEXT("Factory","Breaker","断电者");CompletionVoice=EMissionVoiceCue::None;CommanderResponse=EMissionVoiceCue::HiddenBreaker;break;
 case EFactoryTask::Extraction:ObjectiveName=NSLOCTEXT("Factory","Extraction","携账本到撤离点集合");CompletionVoice=EMissionVoiceCue::RoomClear;InteractionBox->SetBoxExtent(FVector(95,95,70));break;
 }
}
void AFactoryMissionObjective::BeginPlay()
{
 Super::BeginPlay();OnConstruction(GetActorTransform());OnRep_Completed();
}
bool AFactoryMissionObjective::IsLedgerCollected() const
{
 for(TActorIterator<AFactoryMissionObjective> It(GetWorld());It;++It)
  if(It->Task==EFactoryTask::Ledger&&It->bCompleted)return true;
 return false;
}
int32 AFactoryMissionObjective::GetOperatorsAtExtraction(int32& LivingOperators) const
{
 LivingOperators=0;int32 Arrived=0;
 const auto* GS=GetWorld()->GetGameState<ACoopCqbGameState>();
 if(!GS)return 0;
 for(const auto& PS:GS->PlayerArray)
 {
  const APawn* Pawn=PS?PS->GetPawn():nullptr;
  if(!Pawn)continue;
  if(const auto* Stats=UStatsComponent::FindStatsComponent(Pawn);Stats&&Stats->DeathState!=EDeathState::NotDead)continue;
  ++LivingOperators;
  if(FVector::DistSquared2D(Pawn->GetActorLocation(),GetActorLocation())<=FMath::Square(ExtractionRadius)&&FMath::Abs(Pawn->GetActorLocation().Z-GetActorLocation().Z)<220)++Arrived;
 }
 return Arrived;
}
bool AFactoryMissionObjective::CanInteract_Implementation(APawn* Pawn) const
{
 if(!Super::CanInteract_Implementation(Pawn))return false;
 if(Task!=EFactoryTask::Extraction)return true;
 if(!IsLedgerCollected())return false;
 int32 Living;const int32 Arrived=GetOperatorsAtExtraction(Living);
 return Living>0&&Arrived==Living;
}
void AFactoryMissionObjective::CompleteObjective()
{
 if(Task==EFactoryTask::Extraction)
 {
  int32 Living;const int32 Arrived=GetOperatorsAtExtraction(Living);
  if(!IsLedgerCollected()||Living<=0||Arrived!=Living)return;
 }
 Super::CompleteObjective();
}
FText AFactoryMissionObjective::GetInteractPrompt_Implementation(APawn* Pawn) const
{
 if(Task==EFactoryTask::Extraction)
 {
  if(!IsLedgerCollected())return NSLOCTEXT("Factory","NeedLedger","先取得办公区的账本");
  int32 Living;const int32 Arrived=GetOperatorsAtExtraction(Living);
  if(Arrived<Living)return FText::FromString(FString::Printf(TEXT("等待队友集合 %d / %d"),Arrived,Living));
  return NSLOCTEXT("Factory","ExtractPrompt","按 F 全队撤离");
 }
 if(Task==EFactoryTask::Breaker)return NSLOCTEXT("Factory","PowerPrompt","按 F 拉下总闸");
 return Super::GetInteractPrompt_Implementation(Pawn);
}
void AFactoryMissionObjective::OnRep_Completed()
{
 Super::OnRep_Completed();
 if(!bCompleted)return;
 InteractionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 if(Task==EFactoryTask::Ledger||Task==EFactoryTask::Shipping)
  if(auto* Mesh=FindComponentByClass<UStaticMeshComponent>())Mesh->SetVisibility(false);
 if(Task==EFactoryTask::Breaker)
 {
  TArray<AActor*> Powered;UGameplayStatics::GetAllActorsWithTag(this,PowerGroupTag,Powered);
  for(auto* Actor:Powered)Actor->SetActorHiddenInGame(true);
 }
}
