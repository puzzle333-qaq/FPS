#include "Game/FactoryNavigationBounds.h"
#include "Components/BoxComponent.h"
#include "NavigationSystem.h"
AFactoryNavigationBounds::AFactoryNavigationBounds()
{
 NavigationBounds=CreateDefaultSubobject<UBoxComponent>(TEXT("NavigationBounds"));NavigationBounds->SetupAttachment(RootComponent);
 NavigationBounds->SetBoxExtent(FVector(3600,3300,800));NavigationBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);NavigationBounds->SetCanEverAffectNavigation(false);
}
void AFactoryNavigationBounds::RebuildNavigation(){if(auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld())){Nav->OnNavigationBoundsUpdated(this);Nav->Build();}}
