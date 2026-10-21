#include "Lobby/LobbyPlayerController.h"

#include "Lobby/LobbyGameMode.h"
#include "Ui/LobbyScreen.h"
#include "Engine/World.h"
#include "TimerManager.h"

void ALobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	TryCreateLobbyWidget();
}

void ALobbyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(LobbyScreen)) LobbyScreen->RemoveFromParent();
	Super::EndPlay(EndPlayReason);
}

void ALobbyPlayerController::TryCreateLobbyWidget()
{
	if (!IsLocalController()) return;
	if (!LobbyWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Room] LobbyWidgetClass 未设置（BP_LobbyPlayerController）"));
		return;
	}
	if (!IsValid(LobbyScreen))
	{
		LobbyScreen = CreateWidget<ULobbyScreen>(this, LobbyWidgetClass);
	}
	if (IsValid(LobbyScreen) && !LobbyScreen->IsInViewport())
	{
		LobbyScreen->AddToViewport(10);
	}
	SetShowMouseCursor(true);
	FInputModeUIOnly Mode;
	if (IsValid(LobbyScreen)) Mode.SetWidgetToFocus(LobbyScreen->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

void ALobbyPlayerController::Server_RequestTeam_Implementation(int32 TeamId)
{
	if (ALobbyGameMode* GM = GetWorld()->GetAuthGameMode<ALobbyGameMode>()) GM->RequestTeam(this, TeamId);
}

void ALobbyPlayerController::Server_ToggleReady_Implementation()
{
	if (ALobbyGameMode* GM = GetWorld()->GetAuthGameMode<ALobbyGameMode>()) GM->ToggleReady(this);
}

void ALobbyPlayerController::Server_SetRoomMode_Implementation(EShooterGameMode Mode)
{
	if (ALobbyGameMode* GM = GetWorld()->GetAuthGameMode<ALobbyGameMode>()) GM->RequestSetMode(this, Mode);
}

void ALobbyPlayerController::Server_StartGame_Implementation()
{
	if (ALobbyGameMode* GM = GetWorld()->GetAuthGameMode<ALobbyGameMode>()) GM->RequestStart(this);
}

void ALobbyPlayerController::Client_LobbyRejected_Implementation(const FText& Reason)
{
	UE_LOG(LogTemp, Log, TEXT("[Room] Rejected: %s"), *Reason.ToString());
	OnLobbyRejected.Broadcast(Reason);
	if (IsValid(LobbyScreen)) LobbyScreen->HandleRejected(Reason);
}
