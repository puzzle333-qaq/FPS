#include "World/TutorialPromptVolume.h"

#include "ShooterPlayerController.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"

ATutorialPromptVolume::ATutorialPromptVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->SetBoxExtent(FVector(140.f, 140.f, 100.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->SetHiddenInGame(true);
	Trigger->SetCanEverAffectNavigation(false);
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

void ATutorialPromptVolume::BeginPlay()
{
	Super::BeginPlay();
	if (Trigger)
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &ATutorialPromptVolume::OnTriggerBeginOverlap);
	}
}

void ATutorialPromptVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!IsValid(Pawn) || !Pawn->IsPlayerControlled()) return;
	AShooterPlayerController* PC = Cast<AShooterPlayerController>(Pawn->GetController());
	if (!IsValid(PC) || !PC->IsLocalController()) return;
	PC->ShowTutorialPrompt(PromptText, PromptId, DisplayDuration);
}
