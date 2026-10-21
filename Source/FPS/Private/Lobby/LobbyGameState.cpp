#include "Lobby/LobbyGameState.h"

#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

void ALobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALobbyGameState, RoomCode);
	DOREPLIFETIME(ALobbyGameState, RoomMode);
	DOREPLIFETIME(ALobbyGameState, HostPlayerState);
	DOREPLIFETIME(ALobbyGameState, bStarting);
	DOREPLIFETIME(ALobbyGameState, MaxPlayersPerTeam);
	DOREPLIFETIME(ALobbyGameState, MaxTeamDifference);
}

void ALobbyGameState::OnRep_Lobby()
{
	OnLobbyStateChanged.Broadcast();
}

void ALobbyGameState::NotifyChanged()
{
	ForceNetUpdate();
	OnLobbyStateChanged.Broadcast();
}
