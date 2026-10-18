#include "Menu/MainMenuPlayerController.h"
#include "Loadout/GunsmithComponent.h"
#include "Blueprint/UserWidget.h"

AMainMenuPlayerController::AMainMenuPlayerController()
{
	Gunsmith = CreateDefaultSubobject<UGunsmithComponent>(TEXT("Gunsmith"));
	bShowMouseCursor = true;
}

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;

	bShowMouseCursor = true;
	if (MainMenuWidgetClass)
	{
		MainMenuWidget = CreateWidget<UUserWidget>(this, MainMenuWidgetClass);
		if (IsValid(MainMenuWidget))
		{
			MainMenuWidget->AddToViewport();
			FInputModeUIOnly Mode;
			Mode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(Mode);
			return;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[Menu] MainMenuWidgetClass is not set on %s."), *GetClass()->GetName());
	SetInputMode(FInputModeUIOnly());
}