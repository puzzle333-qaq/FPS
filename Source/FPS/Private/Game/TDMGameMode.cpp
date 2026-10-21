#include "Game/TDMGameMode.h"

#include "Game/ShooterPlayerState.h"
#include "Game/ShooterGameState.h"
#include "ShooterPlayerController.h"
#include "ShooterGameInstance.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"

ATDMGameMode::ATDMGameMode()
{
}

void ATDMGameMode::InitGameState()
{
	Super::InitGameState();
	ApplyMatchRules();
}

void ATDMGameMode::StartPlay()
{
	Super::StartPlay();
	ApplyMatchRules();
	BeginWaitingForPlayers();
}

void ATDMGameMode::ApplyMatchRules()
{
	AShooterGameState* GS = GetGameState<AShooterGameState>();
	if (!GS) return;
	GS->SetMatchRules(EShooterGameMode::TDM);
	if (GS->GetNumTeams() < 2)
	{
		GS->InitTeams(2, ScoreToWin);
	}
	if (GS->GetMatchState() == EShooterMatchState::WaitingToStart)
	{
		const float Now = GS->GetServerWorldTimeSeconds();
		GS->SetMatchStartServerTime(WarmupTime > 0.f ? Now + WarmupTime : Now);
	}
}

void ATDMGameMode::StartWarmupOrMatch()
{
	if (bMatchClockStarted) return;
	bMatchClockStarted = true;
	if (WarmupTime > 0.f)
	{
		GetWorldTimerManager().SetTimer(WarmupTimer, this, &ThisClass::StartMatch, WarmupTime, false);
	}
	else
	{
		StartMatch();
	}
}

uint8 ATDMGameMode::PickTeamForNewPlayer() const
{
	int32 Counts[2] = { 0, 0 };
	if (const AGameStateBase* GS = GameState)
	{
		for (const APlayerState* PS : GS->PlayerArray)
		{
			const AShooterPlayerState* SPS = Cast<AShooterPlayerState>(PS);
			if (SPS && SPS->GetTeamId() < 2) ++Counts[SPS->GetTeamId()];
		}
	}
	return Counts[1] < Counts[0] ? 1 : 0;
}

void ATDMGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	AssignShortPlayerName(NewPlayer);
	// 先分队再生成角色，出生点才能按队伍选
	if (AShooterPlayerState* PS = IsValid(NewPlayer) ? NewPlayer->GetPlayerState<AShooterPlayerState>() : nullptr)
	{
		if (!PS->HasTeam())
		{
			PS->SetTeamId(PickTeamForNewPlayer());
			UE_LOG(LogTemp, Log, TEXT("[TDM] %s -> team %d"), *PS->GetPlayerName(), PS->GetTeamId());
		}
	}
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

AActor* ATDMGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const AShooterPlayerState* PS = IsValid(Player) ? Player->GetPlayerState<AShooterPlayerState>() : nullptr;
	if (PS && PS->GetTeamId() < 2)
	{
		const FName Tag = PS->GetTeamId() == 0 ? TeamAStartTag : TeamBStartTag;
		TArray<APlayerStart*> Free, Any;
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			if (It->PlayerStartTag != Tag) continue;
			Any.Add(*It);
			// 出生点附近没人才算空
			bool bOccupied = false;
			for (TActorIterator<APawn> PawnIt(GetWorld()); PawnIt; ++PawnIt)
			{
				if (FVector::DistSquared(PawnIt->GetActorLocation(), It->GetActorLocation()) < FMath::Square(150.f))
				{
					bOccupied = true;
					break;
				}
			}
			if (!bOccupied) Free.Add(*It);
		}
		if (Free.Num() > 0) return Free[FMath::RandRange(0, Free.Num() - 1)];
		if (Any.Num() > 0) return Any[FMath::RandRange(0, Any.Num() - 1)];
		UE_LOG(LogTemp, Warning, TEXT("[TDM] No PlayerStart tagged '%s'. Using any start."), *Tag.ToString());
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

bool ATDMGameMode::CanDamage(AController* InstigatorController, AController* VictimController) const
{
	if (!Super::CanDamage(InstigatorController, VictimController)) return false;
	if (bFriendlyFire || !IsValid(InstigatorController) || !IsValid(VictimController) || InstigatorController == VictimController) return true;
	const AShooterPlayerState* A = InstigatorController->GetPlayerState<AShooterPlayerState>();
	const AShooterPlayerState* B = VictimController->GetPlayerState<AShooterPlayerState>();
	return !(A && B && A->HasTeam() && A->GetTeamId() == B->GetTeamId());
}

void ATDMGameMode::StartMatch()
{
	AShooterGameState* GS = GetGameState<AShooterGameState>();
	if (!GS || GS->GetMatchState() == EShooterMatchState::InProgress || GS->GetMatchState() == EShooterMatchState::PostMatch) return;
	GS->SetMatchEndServerTime(MatchDuration > 0.f ? GS->GetServerWorldTimeSeconds() + MatchDuration : 0.f);
	GS->SetMatchState(EShooterMatchState::InProgress);
	if (MatchDuration > 0.f)
	{
		GetWorldTimerManager().SetTimer(MatchTimer, this, &ThisClass::TickMatchTimer, 1.f, true);
	}
	UE_LOG(LogTemp, Log, TEXT("[TDM] Match started. ScoreToWin=%d Duration=%.0fs"), ScoreToWin, MatchDuration);
}

void ATDMGameMode::TickMatchTimer()
{
	const AShooterGameState* GS = GetGameState<AShooterGameState>();
	if (!GS || GS->GetMatchState() != EShooterMatchState::InProgress) return;
	if (GS->GetRemainingTime() <= 0.f)
	{
		const int32 A = GS->GetTeamScore(0);
		const int32 B = GS->GetTeamScore(1);
		EndMatch(A == B ? AShooterPlayerState::NoTeam : (A > B ? 0 : 1));
	}
}

void ATDMGameMode::ScoreKill(AShooterPlayerState* KillerPS, AShooterPlayerState* VictimPS)
{
	AShooterGameState* GS = GetGameState<AShooterGameState>();
	if (!GS || !KillerPS || GS->GetMatchState() != EShooterMatchState::InProgress) return;
	// 打死队友（开了友伤时）不加分
	if (!KillerPS->HasTeam() || (VictimPS && VictimPS->GetTeamId() == KillerPS->GetTeamId())) return;

	GS->AddTeamScore(KillerPS->GetTeamId(), 1);
	if (GS->GetTeamScore(KillerPS->GetTeamId()) >= ScoreToWin)
	{
		EndMatch(KillerPS->GetTeamId());
	}
}

bool ATDMGameMode::CanRespawn(AController* Controller) const
{
	const AShooterGameState* GS = GetGameState<AShooterGameState>();
	return !GS || GS->GetMatchState() != EShooterMatchState::PostMatch;
}

void ATDMGameMode::EndMatch(uint8 WinningTeam)
{
	AShooterGameState* GS = GetGameState<AShooterGameState>();
	if (!GS || GS->GetMatchState() == EShooterMatchState::PostMatch) return;

	GetWorldTimerManager().ClearTimer(MatchTimer);
	ClearRespawnTimers();
	GS->SetWinningTeam(WinningTeam);
	GS->SetMatchState(EShooterMatchState::PostMatch);
	UE_LOG(LogTemp, Log, TEXT("[TDM] Match ended. Winner=%d (%d : %d)"), WinningTeam, GS->GetTeamScore(0), GS->GetTeamScore(1));

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(It->Get()))
		{
			PC->Client_OnMatchEnded(WinningTeam);
		}
	}
	GetWorldTimerManager().SetTimer(PostMatchTimer, this, &ThisClass::ReturnAllToMainMenu, FMath::Max(PostMatchDelay, 0.1f), false);
}

void ATDMGameMode::ReturnAllToMainMenu()
{
	// 从房间来的：回房间大厅（房间保留、准备重置）
	if (ReturnToRoomLobby()) return;
	// 先让远程客户端各自回主菜单，房主最后走（房主一走服务器就关了）
	APlayerController* HostPC = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!IsValid(PC)) continue;
		if (PC->IsLocalController())
		{
			HostPC = PC;
			continue;
		}
		if (AShooterPlayerController* SPC = Cast<AShooterPlayerController>(PC))
		{
			SPC->Client_GoToMainMenu();
		}
	}

	FTimerHandle HostTimer;
	GetWorldTimerManager().SetTimer(HostTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance()))
		{
			GI->GoToMainMenu();
		}
	}), HostPC ? 1.0f : 0.1f, false);
}

void ATDMGameMode::BeginWaitingForPlayers()
{
	WaitForPlayersStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(WaitForPlayersTimer, this, &ThisClass::TickWaitingForPlayers, 0.5f, true);
	TickWaitingForPlayers();
}

void ATDMGameMode::TickWaitingForPlayers()
{
	if (bMatchClockStarted)
	{
		GetWorldTimerManager().ClearTimer(WaitForPlayersTimer);
		return;
	}
	int32 Arrived = 0, Expected = 0;
	const bool bAllHere = AreExpectedPlayersPresent(Arrived, Expected);
	// 没有房间（直接开图测试）：照旧立刻开始
	const bool bTeamsOk = !HasActiveRoom() || (CountActivePlayersOnTeam(0) >= 1 && CountActivePlayersOnTeam(1) >= 1);
	if (bAllHere && bTeamsOk)
	{
		GetWorldTimerManager().ClearTimer(WaitForPlayersTimer);
		if (AShooterGameState* GS = GetGameState<AShooterGameState>())
		{
			const float Now = GS->GetServerWorldTimeSeconds();
			GS->SetMatchStartServerTime(WarmupTime > 0.f ? Now + WarmupTime : Now);
		}
		UE_LOG(LogTemp, Log, TEXT("[TDM] Players ready %d/%d -> warmup"), Arrived, Expected);
		StartWarmupOrMatch();
		return;
	}
	if (WaitForPlayersTimeout > 0.f && GetWorld()->GetTimeSeconds() - WaitForPlayersStartTime >= WaitForPlayersTimeout)
	{
		GetWorldTimerManager().ClearTimer(WaitForPlayersTimer);
		UE_LOG(LogTemp, Warning, TEXT("[TDM] Wait for players timed out %d/%d -> back to room"), Arrived, Expected);
		if (!ReturnToRoomLobby())
		{
			StartWarmupOrMatch();
		}
	}
}

void ATDMGameMode::HandleTeamForfeit(uint8 WinningTeam)
{
	EndMatch(WinningTeam);
}
