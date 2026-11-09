#include "Menu/MainMenuPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Components/WidgetSwitcher.h"
#include "ShooterGameInstance.h"
#include "Ui/SettingsScreen.h"

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

void AMainMenuPlayerController::ShowSettings()
{
	if (!IsLocalController()) return;
	if (IsValid(MainMenuWidget))
	{
		if (USettingsScreen* Page = Cast<USettingsScreen>(MainMenuWidget->GetWidgetFromName(TEXT("SettingsPage"))))
		{
			UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Page->GetParent());
			if (!Switcher) Switcher = Cast<UWidgetSwitcher>(MainMenuWidget->GetWidgetFromName(TEXT("MenuSwitcher")));
			if (Switcher)
			{
				SettingsScreen = Page;
				SettingsSwitcher = Switcher;
				if (Switcher->GetActiveWidget() != Page) SettingsPrevIndex = Switcher->GetActiveWidgetIndex();
				Page->SetGraphicsLocked(false);
				Page->RefreshFromCurrent();
				Switcher->SetActiveWidget(Page);
				return;
			}
		}
	}
	if (!SettingsWidgetClass)
	{
		SettingsWidgetClass = LoadClass<USettingsScreen>(nullptr, TEXT("/Game/FPS/UI/WBP_Settings.WBP_Settings_C"));
	}
	if (!SettingsWidgetClass) return;
	SettingsSwitcher.Reset();
	if (!IsValid(SettingsScreen)) SettingsScreen = CreateWidget<USettingsScreen>(this, SettingsWidgetClass);
	if (!IsValid(SettingsScreen)) return;
	SettingsScreen->SetGraphicsLocked(false);
	if (!SettingsScreen->IsInViewport()) SettingsScreen->AddToViewport(15);
	SettingsScreen->RefreshFromCurrent();
}

void AMainMenuPlayerController::HideSettings()
{
	if (UWidgetSwitcher* Switcher = SettingsSwitcher.Get())
	{
		Switcher->SetActiveWidgetIndex(FMath::Clamp(SettingsPrevIndex, 0, FMath::Max(0, Switcher->GetNumWidgets() - 1)));
		return;
	}
	if (IsValid(SettingsScreen) && SettingsScreen->IsInViewport()) SettingsScreen->RemoveFromParent();
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
