#include "Menu/MainMenuPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "ShooterGameInstance.h"

AMainMenuPlayerController::AMainMenuPlayerController()
{
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

void AMainMenuPlayerController::StartPractice(EShooterGameMode Mode)
{
	if (UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance()))
	{
		GI->StartPractice(Mode);
	}
}

void AMainMenuPlayerController::MenuStartCoop()
{
	StartPractice(EShooterGameMode::CoopCQB);
}

void AMainMenuPlayerController::MenuHostCoop()
{
	HostLocalListenTest(TEXT("coop"));
}

void AMainMenuPlayerController::HostLocalListenTest(FString Target)
{
	if (UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance()))
	{
		GI->HostLocalListen(Target);
	}
}

void AMainMenuPlayerController::JoinLocalhostTest(FString Address)
{
	if (UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance()))
	{
		GI->JoinLocalhost(Address);
	}
}
