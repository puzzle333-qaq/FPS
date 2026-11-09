#include "Ui/PauseMenuScreen.h"

#include "ShooterPlayerController.h"
#include "ShooterGameInstance.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Online/PartySubsystem.h"
#include "Ui/TacticalUIStyle.h"

void UPauseMenuScreen::NativeConstruct()
{
	Super::NativeConstruct();
	FTacticalUIStyle::ApplyToUserWidget(this);
	SetIsFocusable(true);
	EnsureInviteButton();
	if (Button_Resume) Button_Resume->OnClicked.AddUniqueDynamic(this, &ThisClass::Resume);
	if (Button_Settings) Button_Settings->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSettings);
	if (Button_Lobby) Button_Lobby->OnClicked.AddUniqueDynamic(this, &ThisClass::ReturnToLobby);
}

void UPauseMenuScreen::EnsureInviteButton()
{
	if (GetWidgetFromName(TEXT("Button_Invite"))) return;
	UPanelWidget* Panel = Button_Settings ? Cast<UPanelWidget>(Button_Settings->GetParent()) : nullptr;
	if (!Panel) return;

	UButton* Invite = NewObject<UButton>(this, TEXT("Button_Invite"));
	UTextBlock* Label = NewObject<UTextBlock>(Invite, TEXT("Text_Invite"));
	Label->SetText(NSLOCTEXT("Party", "Invite", "邀请好友"));
	Invite->SetContent(Label);
	Panel->InsertChildAt(Panel->GetChildIndex(Button_Settings) + 1, Invite);
	FTacticalUIStyle::StyleButton(Invite);
	FTacticalUIStyle::StyleText(Label);
	Invite->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleInvite);
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
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		PC->ShowSettings();
	}
	OnSettingsRequested();
}

void UPauseMenuScreen::HandleInvite()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UPartySubsystem* Party = GI->GetSubsystem<UPartySubsystem>())
		{
			Party->OpenInvite();
		}
	}
}

