#include "Game/HostageHidePoint.h"
#include "Components/SceneComponent.h"
AHostageHidePoint::AHostageHidePoint()
{
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("CoverFloor")));
 Approach=CreateDefaultSubobject<USceneComponent>(TEXT("Approach"));Approach->SetupAttachment(RootComponent);Approach->SetRelativeLocation(FVector(-140,0,0));
}
