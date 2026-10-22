#include "Menu/MainMenuPlayerController.h"
#include "Loadout/GunsmithComponent.h"
#include "Blueprint/UserWidget.h"
#include "Ui/RoomBrowserScreen.h"
#include "ShooterGameInstance.h"
#include "Components/WidgetSwitcher.h"

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

void AMainMenuPlayerController::ShowRoomBrowser(EShooterGameMode Mode)
{
	if (!IsLocalController()) return;
	if (UShooterGameInstance* GI = GetGameInstance() ? Cast<UShooterGameInstance>(GetGameInstance()) : nullptr)
	{
		GI->SelectMode(Mode);
	}

	// 优先：WBP_MainMenu 的 WidgetSwitcher 里嵌了一页 RoomBrowserPage
	if (IsValid(MainMenuWidget))
	{
		if (URoomBrowserScreen* Page = Cast<URoomBrowserScreen>(MainMenuWidget->GetWidgetFromName(TEXT("RoomBrowserPage"))))
		{
			UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Page->GetParent());
			if (!Switcher) Switcher = Cast<UWidgetSwitcher>(MainMenuWidget->GetWidgetFromName(TEXT("MenuSwitcher")));
			if (Switcher)
			{
				RoomBrowser = Page;
				EmbeddedSwitcher = Switcher;
				if (Switcher->GetActiveWidget() != Page)
				{
					PreviousSwitcherIndex = Switcher->GetActiveWidgetIndex();
				}
				Page->SetMode(Mode);
				Switcher->SetActiveWidget(Page);
				Page->RefreshRooms();
				return;
			}
			UE_LOG(LogTemp, Warning, TEXT("[Menu] RoomBrowserPage found but not inside a WidgetSwitcher; using viewport fallback"));
		}
	}

	// 兼容：单独一层加到屏幕
	if (!RoomBrowserWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Menu] RoomBrowserWidgetClass is not set on %s"), *GetClass()->GetName());
		return;
	}
	if (!IsValid(RoomBrowser) || EmbeddedSwitcher.IsValid())
	{
		EmbeddedSwitcher.Reset();
		RoomBrowser = CreateWidget<URoomBrowserScreen>(this, RoomBrowserWidgetClass);
		if (!IsValid(RoomBrowser)) return;
	}
	if (!RoomBrowser->IsInViewport())
	{
		RoomBrowser->AddToViewport(5);
	}
	RoomBrowser->SetMode(Mode);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(RoomBrowser->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void AMainMenuPlayerController::HideRoomBrowser()
{
	if (UWidgetSwitcher* Switcher = EmbeddedSwitcher.Get())
	{
		const int32 Index = FMath::Clamp(PreviousSwitcherIndex, 0, FMath::Max(0, Switcher->GetNumWidgets() - 1));
		Switcher->SetActiveWidgetIndex(Index);
		return;
	}
	if (IsValid(RoomBrowser) && RoomBrowser->IsInViewport()) RoomBrowser->RemoveFromParent();
}

bool AMainMenuPlayerController::IsRoomBrowserEmbedded() const
{
	return EmbeddedSwitcher.IsValid();
}
