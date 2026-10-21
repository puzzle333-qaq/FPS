#include "Ui/ModeHudWidget.h"
#include "Game/ShooterGameState.h"

void UModeHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UWorld* World = GetWorld())
	{
		BindMatch(World->GetGameState<AShooterGameState>());
	}
}

void UModeHudWidget::NativeDestruct()
{
	UnbindMatch();
	Super::NativeDestruct();
}

void UModeHudWidget::BindMatch(AShooterGameState* InGameState)
{
	BoundGameState = InGameState;
}

void UModeHudWidget::UnbindMatch()
{
	BoundGameState.Reset();
}