#include "Lobby/LobbyGameMode.h"

#include "Lobby/LobbyGameState.h"
#include "Lobby/LobbyPlayerController.h"
#include "Game/ShooterPlayerState.h"
#include "ShooterGameInstance.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ALobbyGameMode::ALobbyGameMode()
{
	GameStateClass = ALobbyGameState::StaticClass();
	PlayerStateClass = AShooterPlayerState::StaticClass();
	PlayerControllerClass = ALobbyPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	bUseSeamlessTravel = true;
}

ALobbyGameState* ALobbyGameMode::GetLobbyState() const
{
	return GetGameState<ALobbyGameState>();
}

void ALobbyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>();
	if (GI && !GI->Room.bActive)
	{
		EShooterGameMode Mode = GI->SelectedMode;
		if (UGameplayStatics::HasOption(Options, TEXT("Mode")))
		{
			Mode = static_cast<EShooterGameMode>(UGameplayStatics::GetIntOption(Options, TEXT("Mode"), (int32)Mode));
		}
		if (!AllowedModes.Contains(Mode) && AllowedModes.Num() > 0) Mode = AllowedModes[0];
		GI->ResetRoom(Mode);
	}
	if (GI)
	{
		GI->Room.ExpectedPlayerKeys.Reset(); // 回到大厅：等待列表清空
	}
}

FString ALobbyGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	// 父类会处理 ?Name=（ChangeName）
	const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (UGameplayStatics::GetIntOption(Options, TEXT("Host"), 0) == 1)
	{
		HostClaimers.Add(NewPlayerController);
	}
	if (UGameplayStatics::HasOption(Options, TEXT("Mode")))
	{
		RequestedModes.Add(NewPlayerController, UGameplayStatics::GetIntOption(Options, TEXT("Mode"), 0));
	}
	return Result;
}

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	OnPlayerEnteredRoom(NewPlayer);
}

void ALobbyGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);
	OnPlayerEnteredRoom(Cast<APlayerController>(C));
}

void ALobbyGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// 大厅不生成角色
}

void ALobbyGameMode::EnsureUniqueName(APlayerController* PC)
{
	APlayerState* PS = PC ? PC->PlayerState.Get() : nullptr;
	if (!PS || !GameState) return;
	FString Base = PS->GetPlayerName().TrimStartAndEnd();
	if (Base.IsEmpty()) Base = TEXT("Player");
	FString Candidate = Base;
	int32 Suffix = 2;
	auto Taken = [&](const FString& Name)
	{
		for (const APlayerState* Other : GameState->PlayerArray)
		{
			if (Other && Other != PS && Other->GetPlayerName() == Name) return true;
		}
		return false;
	};
	while (Taken(Candidate))
	{
		Candidate = FString::Printf(TEXT("%s(%d)"), *Base, Suffix++);
	}
	if (Candidate != PS->GetPlayerName())
	{
		ChangeName(PC, Candidate, false);
	}
}

void ALobbyGameMode::OnPlayerEnteredRoom(APlayerController* PC)
{
	if (!IsValid(PC)) return;
	EnsureUniqueName(PC);
	ALobbyGameState* GS = GetLobbyState();
	UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>();
	if (AShooterPlayerState* PS = PC->GetPlayerState<AShooterPlayerState>())
	{
		if (PS->GetJoinOrder() <= 0)
		{
			PS->SetJoinOrder(NextJoinOrder++);
		}
		else
		{
			NextJoinOrder = FMath::Max(NextJoinOrder, PS->GetJoinOrder() + 1);
		}
		PS->SetReady(false);
		// 房主：没有房主时，带 ?Host=1 的或第一个进来的
		const bool bNoHost = !GS || !IsValid(GS->GetHostPlayerState());
		const bool bWasHost = GI && !GI->Room.HostKey.IsEmpty() && GI->Room.HostKey == UShooterGameInstance::GetRoomKey(PS);
		if (GS && (bWasHost || (bNoHost && (HostClaimers.Contains(PC) || true))))
		{
			if (bWasHost || bNoHost)
			{
				GS->SetHostPlayerState(PS);
				if (GI) GI->Room.HostKey = UShooterGameInstance::GetRoomKey(PS);
				// 房主建房时带的模式
				if (const int32* M = RequestedModes.Find(PC))
				{
					const EShooterGameMode Mode = static_cast<EShooterGameMode>(*M);
					if (AllowedModes.Contains(Mode) && GI)
					{
						GI->Room.Mode = Mode;
					}
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[Room] %s entered (order %d, team %d)"), *PS->GetPlayerName(), PS->GetJoinOrder(), PS->GetTeamId());
	}
	HostClaimers.Remove(PC);
	RequestedModes.Remove(PC);
	if (GS && GI)
	{
		GS->SetRoomCode(GI->Room.RoomCode);
		GS->SetRoomMode(GI->Room.Mode);
		GS->SetRules(MaxPlayersPerTeam, MaxTeamDifference);
		GS->SetStarting(false);
	}
}

void ALobbyGameMode::Logout(AController* Exiting)
{
	APlayerState* Leaving = Exiting ? Exiting->PlayerState.Get() : nullptr;
	Super::Logout(Exiting);
	RefreshHost(Leaving);
	// 专用服务器上最后一个人走了：房间作废，下一个人进来是新房间
	int32 Remaining = 0;
	if (GameState)
	{
		for (const APlayerState* PS : GameState->PlayerArray)
		{
			if (PS && PS != Leaving && !PS->IsInactive()) ++Remaining;
		}
	}
	if (Remaining == 0)
	{
		if (UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>())
		{
			GI->ResetRoom(AllowedModes.Num() > 0 ? AllowedModes[0] : EShooterGameMode::TDM);
			if (ALobbyGameState* GS = GetLobbyState())
			{
				GS->SetRoomCode(GI->Room.RoomCode);
				GS->SetRoomMode(GI->Room.Mode);
				GS->SetHostPlayerState(nullptr);
			}
		}
	}
}

void ALobbyGameMode::RefreshHost(APlayerState* Leaving)
{
	ALobbyGameState* GS = GetLobbyState();
	if (!GS || !GameState) return;
	APlayerState* Current = GS->GetHostPlayerState();
	if (IsValid(Current) && Current != Leaving) return;
	// 迁移给最早进房间的人
	AShooterPlayerState* Best = nullptr;
	for (APlayerState* PS : GameState->PlayerArray)
	{
		AShooterPlayerState* SPS = Cast<AShooterPlayerState>(PS);
		if (!SPS || SPS == Leaving || SPS->IsInactive()) continue;
		if (!Best || SPS->GetJoinOrder() < Best->GetJoinOrder()) Best = SPS;
	}
	GS->SetHostPlayerState(Best);
	if (UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>())
	{
		GI->Room.HostKey = Best ? UShooterGameInstance::GetRoomKey(Best) : FString();
	}
	UE_LOG(LogTemp, Log, TEXT("[Room] Host -> %s"), Best ? *Best->GetPlayerName() : TEXT("<none>"));
}

void ALobbyGameMode::CountTeams(int32& OutA, int32& OutB, const APlayerState* Exclude) const
{
	OutA = OutB = 0;
	if (!GameState) return;
	for (const APlayerState* PS : GameState->PlayerArray)
	{
		const AShooterPlayerState* SPS = Cast<AShooterPlayerState>(PS);
		if (!SPS || SPS == Exclude || SPS->IsInactive()) continue;
		if (SPS->GetTeamId() == 0) ++OutA;
		else if (SPS->GetTeamId() == 1) ++OutB;
	}
}

bool ALobbyGameMode::IsHost(const APlayerController* PC) const
{
	const ALobbyGameState* GS = GetLobbyState();
	return PC && GS && GS->GetHostPlayerState() == PC->PlayerState;
}

void ALobbyGameMode::Reject(ALobbyPlayerController* PC, const FText& Reason)
{
	if (PC) PC->Client_LobbyRejected(Reason);
}

void ALobbyGameMode::ResetAllReady()
{
	if (!GameState) return;
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (AShooterPlayerState* SPS = Cast<AShooterPlayerState>(PS)) SPS->SetReady(false);
	}
}

void ALobbyGameMode::RequestTeam(ALobbyPlayerController* PC, int32 TeamId)
{
	AShooterPlayerState* PS = PC ? PC->GetPlayerState<AShooterPlayerState>() : nullptr;
	const ALobbyGameState* GS = GetLobbyState();
	if (!PS || !GS) return;
	if (GS->IsStarting()) { Reject(PC, NSLOCTEXT("Lobby", "Starting", "游戏正在开始")); return; }
	if (TeamId != 0 && TeamId != 1) { Reject(PC, NSLOCTEXT("Lobby", "BadTeam", "无效的队伍")); return; }
	if (PS->GetTeamId() == TeamId) return;

	int32 A = 0, B = 0;
	CountTeams(A, B, PS); // 不算自己
	const int32 Target = TeamId == 0 ? A : B;
	if (Target >= MaxPlayersPerTeam)
	{
		Reject(PC, FText::Format(NSLOCTEXT("Lobby", "TeamFull", "该队已满（{0} 人）"), FText::AsNumber(MaxPlayersPerTeam)));
		return;
	}
	if (TeamId == 0) ++A; else ++B;
	if (FMath::Abs(A - B) > MaxTeamDifference)
	{
		Reject(PC, FText::Format(NSLOCTEXT("Lobby", "TeamDiff", "两队人数差不能超过 {0}"), FText::AsNumber(MaxTeamDifference)));
		return;
	}
	PS->SetTeamId((uint8)TeamId);
	PS->SetReady(false);
	PS->ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[Room] %s -> team %d (A=%d B=%d)"), *PS->GetPlayerName(), TeamId, A, B);
}

void ALobbyGameMode::ToggleReady(ALobbyPlayerController* PC)
{
	AShooterPlayerState* PS = PC ? PC->GetPlayerState<AShooterPlayerState>() : nullptr;
	const ALobbyGameState* GS = GetLobbyState();
	if (!PS || !GS || GS->IsStarting()) return;
	if (!PS->HasTeam())
	{
		Reject(PC, NSLOCTEXT("Lobby", "PickTeamFirst", "请先选择左边或右边"));
		return;
	}
	PS->SetReady(!PS->IsReady());
}

void ALobbyGameMode::RequestSetMode(ALobbyPlayerController* PC, EShooterGameMode Mode)
{
	ALobbyGameState* GS = GetLobbyState();
	UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>();
	if (!GS || !GI) return;
	if (!IsHost(PC)) { Reject(PC, NSLOCTEXT("Lobby", "NotHost", "只有房主可以操作")); return; }
	if (GS->IsStarting()) return;
	if (!AllowedModes.Contains(Mode) || !GI->IsModeAvailable(Mode))
	{
		Reject(PC, NSLOCTEXT("Lobby", "ModeNA", "该模式不可用（检查 GameInstance 的模式表）"));
		return;
	}
	if (GI->Room.Mode == Mode) return;
	GI->Room.Mode = Mode;
	GS->SetRoomMode(Mode);
	ResetAllReady();
}

bool ALobbyGameMode::CheckCanStart(const UWorld* World, FText& OutReason)
{
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS) { OutReason = FText::GetEmpty(); return false; }
	int32 A = 0, B = 0;
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const AShooterPlayerState* SPS = Cast<AShooterPlayerState>(PS);
		if (!SPS || SPS->IsInactive()) continue;
		if (!SPS->HasTeam())
		{
			OutReason = FText::Format(NSLOCTEXT("Lobby", "NoTeam", "{0} 还没选边"), FText::FromString(SPS->GetPlayerName()));
			return false;
		}
		if (!SPS->IsReady())
		{
			OutReason = FText::Format(NSLOCTEXT("Lobby", "NotReady", "{0} 还没准备"), FText::FromString(SPS->GetPlayerName()));
			return false;
		}
		if (SPS->GetTeamId() == 0) ++A; else ++B;
	}
	if (A < 1 || B < 1)
	{
		OutReason = NSLOCTEXT("Lobby", "NeedBoth", "每队至少需要 1 人");
		return false;
	}
	OutReason = FText::GetEmpty();
	return true;
}

void ALobbyGameMode::RequestStart(ALobbyPlayerController* PC)
{
	ALobbyGameState* GS = GetLobbyState();
	UShooterGameInstance* GI = GetGameInstance<UShooterGameInstance>();
	if (!GS || !GI) return;
	if (!IsHost(PC)) { Reject(PC, NSLOCTEXT("Lobby", "NotHost", "只有房主可以操作")); return; }
	if (GS->IsStarting()) return;
	FText Reason;
	if (!CheckCanStart(GetWorld(), Reason)) { Reject(PC, Reason); return; }

	FShooterModeInfo Info;
	if (!GI->GetModeInfo(GI->Room.Mode, Info) || Info.Map.IsNull())
	{
		Reject(PC, NSLOCTEXT("Lobby", "NoMap", "该模式没有设置地图"));
		return;
	}
	// 记录期望玩家
	GI->Room.ExpectedPlayerKeys.Reset();
	for (const APlayerState* PS : GameState->PlayerArray)
	{
		if (PS && !PS->IsInactive()) GI->Room.ExpectedPlayerKeys.Add(UShooterGameInstance::GetRoomKey(PS));
	}
	FString URL = Info.Map.ToSoftObjectPath().GetLongPackageName();
	if (!Info.GameModeClass.IsNull())
	{
		URL += FString::Printf(TEXT("?game=%s"), *Info.GameModeClass.ToSoftObjectPath().ToString());
	}
	GS->SetStarting(true);
	UE_LOG(LogTemp, Log, TEXT("[Room] Start %s with %d players -> %s"), *GI->Room.RoomCode, GI->Room.ExpectedPlayerKeys.Num(), *URL);
	GetWorld()->ServerTravel(URL);
}
