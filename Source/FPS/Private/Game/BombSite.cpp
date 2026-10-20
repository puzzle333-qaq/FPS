#include "Game/BombSite.h"

#include "Components/BoxComponent.h"
#include "Components/BillboardComponent.h"
#include "GameFramework/Pawn.h"

ABombSite::ABombSite()
{
	PrimaryActorTick.bCanEverTick = false;

	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	SetRootComponent(Zone);
	Zone->SetBoxExtent(FVector(400.f, 300.f, 160.f));
	Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Zone->SetCollisionResponseToAllChannels(ECR_Ignore);
	Zone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Zone->SetGenerateOverlapEvents(true);
	Zone->SetLineThickness(2.f);

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(Zone);
		Sprite->bIsScreenSizeScaled = true;
	}
#endif
}

bool ABombSite::ContainsPawn(const APawn* Pawn) const
{
	return IsValid(Pawn) && IsValid(Zone) && Zone->IsOverlappingActor(Pawn);
}

FText ABombSite::GetSiteLetter(EBombSiteId Id)
{
	switch (Id)
	{
	case EBombSiteId::A: return NSLOCTEXT("SearchDestroy", "SiteA", "A");
	case EBombSiteId::B: return NSLOCTEXT("SearchDestroy", "SiteB", "B");
	default: return FText::GetEmpty();
	}
}
