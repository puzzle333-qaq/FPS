#include "Ui/PauseMenuScreen.h"

#include "ShooterPlayerController.h"
#include "ShooterGameInstance.h"
#include "Components/Button.h"

void UPauseMenuScreen::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (Button_Resume) Button_Resume->OnClicked.AddUniqueDynamic(this, &ThisClass::Resume);
	if (Button_Settings) Button_Settings->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSettings);
	if (Button_Lobby) Button_Lobby->OnClicked.AddUniqueDynamic(this, &ThisClass::ReturnToLobby);
}

FReply UPauseMenuScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// 菜单打开时是纯 UI 输入，游戏里的 Esc/P 绑定收不到，这里自己关
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::P)
	{
		Resume();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPauseMenuScreen::Resume()
{
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		PC->ClosePauseMenu();
	}
	else
	{
		RemoveFromParent();
	}
}

void UPauseMenuScreen::ReturnToLobby()
{
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		PC->ClosePauseMenu();
	}
	if (UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance()))
	{
		if (!GI->GoToMainMenu())
		{
			UE_LOG(LogTemp, Warning, TEXT("[PauseMenu] GoToMainMenu failed: MainMenuMap is not set in BP_ShooterGameInstance."));
		}
	}
}

void UPauseMenuScreen::HandleSettings()
{
	OnSettingsRequested();
}