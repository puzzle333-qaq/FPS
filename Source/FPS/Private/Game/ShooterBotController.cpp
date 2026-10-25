#include "Game/ShooterBotController.h"

#include "CombatComponent.h"
#include "FPSTraceChannels.h"
#include "Game/BombActor.h"
#include "Game/BombSite.h"
#include "Game/SearchDestroyGameMode.h"
#include "Game/SearchDestroyGameState.h"
#include "Game/ShooterGameState.h"
#include "Game/ShooterPlayerState.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Interface/Interactable.h"
#include "Game/WeaponPickup.h"
#include "Components/PrimitiveComponent.h"
#include "ShooterCharacter.h"
#include "Stats/StatsComponent.h"
#include "Weapon/Weapon.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"

AShooterBotController::AShooterBotController()
{
	// 玩家控制器默认不 Tick。傀儡要自己寻路、开枪、下包。
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AShooterBotController::BeginPlay()
{
	// 不走 AShooterPlayerController::BeginPlay：那会给“本地玩家”创建 HUD，并提交主机改枪台配装。
	APlayerController::BeginPlay();
	SetActorTickEnabled(true);
}

void AShooterBotController::SetupInputComponent()
{
	APlayerController::SetupInputComponent();
}

bool AShooterBotController::IsLocalController() const
{
	// 必须是 true：PossessedBy 看到本地控制器时，不会把 Pawn 标成 AutonomousProxy。
	// 没有所属连接的 AutonomousProxy 不会把移动复制给其他人。
	// 角色上的 IsLocallyControlled 对傀儡返回 false，枪械和扣弹走远程那条。
	return true;
}

void AShooterBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	bPawnAlive = true;
	bHoldingBombInteract = false;
	ReloadCommitTime = 0.f;
	NextFireTime = 0.f;
	CachedPath.Reset();
	SetActorTickEnabled(true);
	if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(InPawn))
	{
		Shooter->bUseControllerRotationYaw = true;
	}
}

void AShooterBotController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !bPawnAlive) return;
	Think(DeltaSeconds);
}

bool AShooterBotController::IsAlive() const
{
	const APawn* MyPawn = GetPawn();
	if (!IsValid(MyPawn)) return false;
	const UStatsComponent* Stats = UStatsComponent::FindStatsComponent(MyPawn);
	return !Stats || Stats->DeathState == EDeathState::NotDead;
}

void AShooterBotController::BuyOnce()
{
	ASearchDestroyGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASearchDestroyGameMode>() : nullptr;
	const ASearchDestroyGameState* GS = GetWorld() ? GetWorld()->GetGameState<ASearchDestroyGameState>() : nullptr;
	if (!GM || !GS || !GS->IsBuyPhase()) return;

	const int32 Round = GS->GetRoundIndex();
	if (BoughtRound == Round) return;
	BoughtRound = Round;

	const AShooterPlayerState* PS = GetPlayerState<AShooterPlayerState>();
	const int32 Money = PS ? PS->GetMoney() : 0;
	int32 BestIndex = INDEX_NONE;
	int32 BestPrice = -1;
	const TArray<FBuyShopItem>& Catalog = GM->GetBuyCatalog();
	for (int32 Index = 0; Index < Catalog.Num(); ++Index)
	{
		const FBuyShopItem& Item = Catalog[Index];
		if (!Item.WeaponClass || Item.Price > Money || Item.Price <= BestPrice) continue;
		const AWeapon* CDO = Item.WeaponClass->GetDefaultObject<AWeapon>();
		if (!CDO || CDO->WeaponKind == EWeaponKind::Bomb) continue;
		BestIndex = Index;
		BestPrice = Item.Price;
	}
	if (BestIndex != INDEX_NONE)
	{
		GM->Auth_TryBuyItem(this, BestIndex);
	}
}

void AShooterBotController::EnsureGun()
{
	UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetPawn());
	if (!Combat || !IsValid(Combat->CurrentWeapon)) return;
	const EWeaponKind Kind = Combat->CurrentWeapon->WeaponKind;
	if (Kind == EWeaponKind::LongGun || Kind == EWeaponKind::Pistol) return;
	if (Combat->IsReloading()) return;

	AWeapon* Fallback = nullptr;
	for (AWeapon* Weapon : Combat->GetInventory())
	{
		if (!IsValid(Weapon)) continue;
		if (Weapon->WeaponKind == EWeaponKind::LongGun)
		{
			Combat->Equip(Weapon);
			return;
		}
		if (!Fallback && Weapon->WeaponKind == EWeaponKind::Pistol)
		{
			Fallback = Weapon;
		}
	}
	if (Fallback)
	{
		Combat->Equip(Fallback);
	}
}

void AShooterBotController::TryReload(float Now)
{
	UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetPawn());
	if (!Combat || !IsValid(Combat->CurrentWeapon)) return;
	AWeapon* Weapon = Combat->CurrentWeapon;
	if (!Weapon->UsesAmmo()) return;

	if (Weapon->WeaponStatus == EWeaponStatus::Reloading)
	{
		if (ReloadCommitTime > 0.f && Now >= ReloadCommitTime)
		{
			ReloadCommitTime = 0.f;
			Combat->Notify_ReloadWeapon();
		}
		return;
	}

	if (Weapon->Ammo > 0 || Combat->CurrentReserveAmmo <= 0) return;

	const EReloadType Type = Weapon->Ammo <= 0 ? EReloadType::Empty : EReloadType::Tactical;
	Combat->Auth_ReloadWeapon(Type);
	ReloadCommitTime = Now + 2.4f;
}

void AShooterBotController::TryShoot(float Now, APawn* Enemy)
{
	if (!IsValid(Enemy) || Now < NextFireTime) return;
	UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetPawn());
	AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetPawn());
	if (!Combat || !Shooter || !IsValid(Combat->CurrentWeapon)) return;
	AWeapon* Weapon = Combat->CurrentWeapon;
	if (Weapon->WeaponKind != EWeaponKind::LongGun && Weapon->WeaponKind != EWeaponKind::Pistol) return;
	if (Weapon->WeaponStatus == EWeaponStatus::Reloading || Weapon->WeaponStatus == EWeaponStatus::Cycling) return;
	if (Weapon->UsesAmmo() && Weapon->Ammo <= 0) return;

	FVector Eye = FVector::ZeroVector;
	FRotator EyeRot = FRotator::ZeroRotator;
	Shooter->GetActorEyesViewPoint(Eye, EyeRot);
	const FVector AimPoint = Enemy->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	const FRotator Look = (AimPoint - Eye).Rotation();
	SetControlRotation(Look);
	if (UCameraComponent* Camera = Shooter->GetFirstPersonCamera())
	{
		Camera->SetWorldRotation(Look);
	}

	FHitResult Hit;
	Weapon->WeaponTrace(Hit, 20000.f);
	const float Interval = Weapon->FireTime > 0.f ? Weapon->FireTime : 0.12f;
	NextFireTime = Now + Interval;
	TArray<FHitResult> Hits;
	Hits.Add(Hit);
	Combat->Auth_FireWeapon(Hits);
}

APawn* AShooterBotController::FindEnemy() const
{
	const APawn* MyPawn = GetPawn();
	const AShooterPlayerState* MyPS = GetPlayerState<AShooterPlayerState>();
	UWorld* World = GetWorld();
	if (!MyPawn || !MyPS || !World) return nullptr;

	FVector Eye = FVector::ZeroVector;
	FRotator EyeRot = FRotator::ZeroRotator;
	MyPawn->GetActorEyesViewPoint(Eye, EyeRot);

	APawn* Best = nullptr;
	float BestDistSq = FMath::Square(4500.f);
	const AGameStateBase* GS = World->GetGameState();
	if (!GS) return nullptr;

	for (APlayerState* PS : GS->PlayerArray)
	{
		const AShooterPlayerState* OtherPS = Cast<AShooterPlayerState>(PS);
		if (!OtherPS || OtherPS == MyPS || !OtherPS->HasTeam() || OtherPS->GetTeamId() == MyPS->GetTeamId()) continue;
		APawn* OtherPawn = OtherPS->GetPawn();
		if (!IsValid(OtherPawn)) continue;
		const UStatsComponent* Stats = UStatsComponent::FindStatsComponent(OtherPawn);
		if (Stats && Stats->DeathState != EDeathState::NotDead) continue;

		const FVector Chest = OtherPawn->GetActorLocation() + FVector(0.f, 0.f, 60.f);
		const float DistSq = FVector::DistSquared(Eye, Chest);
		if (DistSq >= BestDistSq) continue;

		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(BotSight), false, MyPawn);
		Query.AddIgnoredActor(OtherPawn);
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, Eye, Chest, ECC_Visibility, Query);
		if (bBlocked && Hit.GetActor() != OtherPawn) continue;

		Best = OtherPawn;
		BestDistSq = DistSq;
	}
	return Best;
}

APawn* AShooterBotController::FindChasePawn() const
{
	const APawn* MyPawn = GetPawn();
	const AShooterPlayerState* MyPS = GetPlayerState<AShooterPlayerState>();
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!MyPawn || !MyPS || !GS) return nullptr;

	APawn* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (APlayerState* PS : GS->PlayerArray)
	{
		const AShooterPlayerState* OtherPS = Cast<AShooterPlayerState>(PS);
		if (!OtherPS || OtherPS == MyPS || !OtherPS->HasTeam() || OtherPS->GetTeamId() == MyPS->GetTeamId()) continue;
		APawn* OtherPawn = OtherPS->GetPawn();
		if (!IsValid(OtherPawn)) continue;
		const UStatsComponent* Stats = UStatsComponent::FindStatsComponent(OtherPawn);
		if (Stats && Stats->DeathState != EDeathState::NotDead) continue;
		const float DistSq = FVector::DistSquared(MyPawn->GetActorLocation(), OtherPawn->GetActorLocation());
		if (DistSq >= BestDistSq) continue;
		Best = OtherPawn;
		BestDistSq = DistSq;
	}
	return Best;
}

ABombSite* AShooterBotController::FindPreferredSite() const
{
	ABombSite* Fallback = nullptr;
	for (TActorIterator<ABombSite> It(GetWorld()); It; ++It)
	{
		if (!Fallback) Fallback = *It;
		if (It->GetSiteId() == PreferredSite) return *It;
	}
	return Fallback;
}

bool AShooterBotController::WantsToPlantOrDefuse(FVector& OutGoal) const
{
	const ASearchDestroyGameState* GS = GetWorld() ? GetWorld()->GetGameState<ASearchDestroyGameState>() : nullptr;
	const AShooterPlayerState* PS = GetPlayerState<AShooterPlayerState>();
	const APawn* MyPawn = GetPawn();
	if (!GS || !PS || !MyPawn || GS->GetRoundPhase() != EBombRoundPhase::Live) return false;

	const bool bAttacker = GS->IsAttackingTeam(PS->GetTeamId());
	if (bAttacker && PS->IsCarryingBomb())
	{
		if (ABombSite* Site = FindPreferredSite())
		{
			OutGoal = Site->GetActorLocation();
			return true;
		}
	}
	if (bAttacker && !PS->IsCarryingBomb())
	{
		if (const ABombActor* Bomb = GS->GetBomb(); IsValid(Bomb) && Bomb->IsDropped())
		{
			OutGoal = Bomb->GetActorLocation();
			return true;
		}
	}
	if (!bAttacker && GS->IsBombDown())
	{
		if (const ABombActor* Bomb = GS->GetBomb(); IsValid(Bomb))
		{
			OutGoal = Bomb->GetActorLocation();
			return true;
		}
	}
	return false;
}

void AShooterBotController::ReleasePlant()
{
	if (!bHoldingBombInteract) return;
	bHoldingBombInteract = false;
	if (ASearchDestroyGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASearchDestroyGameMode>() : nullptr)
	{
		GM->EndBombInteract(this);
	}
}

namespace
{
	// 返回 true = 探测点已能投影到导航网格（可寻路）。
	// 旧逻辑只要世界里有 RecastNavMesh Actor 就直接 return，
	// 胶囊体扫掠：只关心墙/门等 Visibility 阻挡。Pawn 之间的挤压由 separation 处理。
	bool SweepBotCapsule(const UWorld* World, const APawn* Pawn, const FVector& From, const FVector& To, FHitResult& OutHit)
	{
		if (!World || !Pawn) return false;

		float Radius = 34.f;
		float HalfHeight = 88.f;
		if (const UCapsuleComponent* Capsule = Pawn->FindComponentByClass<UCapsuleComponent>())
		{
			Radius = FMath::Max(1.f, Capsule->GetScaledCapsuleRadius() - 2.f);
			HalfHeight = FMath::Max(1.f, Capsule->GetScaledCapsuleHalfHeight() - 2.f);
		}

		FCollisionQueryParams Query(SCENE_QUERY_STAT(BotPathClearance), false, Pawn);
		return World->SweepSingleByChannel(
			OutHit, From, To, FQuat::Identity, ECC_Visibility,
			FCollisionShape::MakeCapsule(Radius, HalfHeight), Query);
	}

	bool ProjectBotPointToNav(UNavigationSystemV1* NavSys, const FVector& Point, FVector& OutPoint)
	{
		if (!NavSys) return false;

		FNavLocation Projected;
		if (!NavSys->ProjectPointToNavigation(Point, Projected, FVector(220.f, 220.f, 350.f)))
		{
			return false;
		}
		OutPoint = Projected.Location;
		return true;
	}

	void DrawBotPathDebug(UWorld* World, const TArray<FVector>& Path, int32 PathIndex, const FVector& PawnLocation)
	{
		static bool bDraw = false;
		static FAutoConsoleVariableRef CVarDraw(
			TEXT("fps.Bot.NavDebug"),
			bDraw,
			TEXT("Draw one-frame nav path and steering target for ShooterBotController."),
			ECVF_Cheat);

		if (!bDraw) return;

		for (int32 Index = 0; Index + 1 < Path.Num(); ++Index)
		{
			DrawDebugLine(World, Path[Index], Path[Index + 1],
				Index < PathIndex ? FColor::Silver : FColor::Green, false, 0.f, 0, 2.f);
		}
		if (Path.IsValidIndex(PathIndex))
		{
			DrawDebugSphere(World, Path[PathIndex], 22.f, 8, FColor::Yellow, false, 0.f);
			DrawDebugLine(World, PawnLocation, Path[PathIndex], FColor::Cyan, false, 0.f, 0, 3.f);
		}
	}
	// 前方/卡住时尝试开 IInteractable 门（与玩家 F -> Server_Interact 同一套 Interact）。
	// 不碰 WeaponPickup。冷却避免 BP 门 Toggle 连开连关。
	void TryBotDoorInteract(AController* Controller, APawn* Pawn, const FVector& MoveDir, float Now)
	{
		if (!Controller || !Pawn) return;
		UWorld* World = Pawn->GetWorld();
		if (!World || !Controller->HasAuthority()) return;

		static TMap<TWeakObjectPtr<AController>, float> DoorCooldown;
		for (auto It = DoorCooldown.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid()) It.RemoveCurrent();
		}
		TWeakObjectPtr<AController> Key(Controller);
		if (const float* Next = DoorCooldown.Find(Key))
		{
			if (Now < *Next) return;
		}

		const FVector Loc = Pawn->GetActorLocation();
		FVector Dir = MoveDir.GetSafeNormal2D();
		if (Dir.IsNearlyZero())
		{
			Dir = Pawn->GetActorForwardVector().GetSafeNormal2D();
		}

		auto TryActor = [&](AActor* Candidate) -> bool
		{
			if (!IsValid(Candidate) || Candidate == Pawn) return false;
			if (Candidate->IsA(AWeaponPickup::StaticClass())) return false;
			if (!Candidate->GetClass()->ImplementsInterface(UInteractable::StaticClass())) return false;
			if (!IInteractable::Execute_CanInteract(Candidate, Pawn)) return false;
			UE_LOG(LogTemp, Log, TEXT("[Bot] 开门/交互 %s -> %s"), *GetNameSafe(Controller), *GetNameSafe(Candidate));
			IInteractable::Execute_Interact(Candidate, Pawn);
			DoorCooldown.Add(Key, Now + 0.85f);
			return true;
		};

		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(BotDoorSweep), false, Pawn);
		const bool bHit = World->SweepSingleByChannel(
			Hit, Loc, Loc + Dir * 150.f, FQuat::Identity, ECC_WorldDynamic,
			FCollisionShape::MakeCapsule(34.f, 70.f), Query);
		if (bHit)
		{
			if (TryActor(Hit.GetActor())) return;
			if (Hit.GetComponent())
			{
				if (TryActor(Hit.GetComponent()->GetOwner())) return;
			}
		}

		// 再扫静态 / Visibility（玩家 F 交互用 Visibility）
		for (ECollisionChannel Channel : { ECC_WorldStatic, ECC_Visibility })
		{
			FHitResult HitExtra;
			if (World->SweepSingleByChannel(
				HitExtra, Loc, Loc + Dir * 150.f, FQuat::Identity, Channel,
				FCollisionShape::MakeCapsule(34.f, 70.f), Query))
			{
				if (TryActor(HitExtra.GetActor())) return;
				if (HitExtra.GetComponent() && TryActor(HitExtra.GetComponent()->GetOwner())) return;
			}
		}

		// 近距离重叠兜底
		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams ObjParams;
		ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjParams.AddObjectTypesToQuery(ECC_Visibility);
		if (World->OverlapMultiByObjectType(
			Overlaps, Loc + Dir * 60.f, FQuat::Identity, ObjParams,
			FCollisionShape::MakeSphere(90.f), Query))
		{
			for (const FOverlapResult& O : Overlaps)
			{
				if (TryActor(O.GetActor())) return;
			}
		}
	}

	void AbortBotPathFollowing(AController* Controller)
	{
		if (!Controller) return;
		if (UPathFollowingComponent* PF = Controller->FindComponentByClass<UPathFollowingComponent>())
		{
			if (PF->GetStatus() != EPathFollowingStatus::Idle)
			{
				PF->AbortMove(*Controller, FPathFollowingResultFlags::OwnerFinished);
			}
		}
	}

	void LogBotPath(float Now, const TCHAR* Msg)
	{
		static float LastLogTime = -100.f;
		static FString LastMsg;
		if (Now - LastLogTime < 2.f && LastMsg == Msg) return;
		LastLogTime = Now;
		LastMsg = Msg;
		UE_LOG(LogTemp, Warning, TEXT("[Bot] %s"), Msg);
	}
}

void AShooterBotController::ClearCachedPath()
{
	CachedPath.Reset();
	PathIndex = 0;
	CachedGoal = FVector::ZeroVector;
	NextPathTime = 0.f;
}

bool AShooterBotController::HasClearCapsulePath(const FVector& From, const FVector& To) const
{
	if (FVector::DistSquared2D(From, To) <= FMath::Square(4.f))
	{
		return true;
	}

	FHitResult Hit;
	const FVector SweepEnd(To.X, To.Y, From.Z);
	return !SweepBotCapsule(GetWorld(), GetPawn(), From, SweepEnd, Hit);
}

bool AShooterBotController::RebuildCachedPath(const FVector& Start, const FVector& Goal)
{
	CachedPath.Reset();
	PathIndex = 0;

	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	ANavigationData* NavData = NavSys ? NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;

	FVector StartNav = FVector::ZeroVector;
	FVector GoalNav = FVector::ZeroVector;
	if (NavData
		&& ProjectBotPointToNav(NavSys, Start, StartNav)
		&& ProjectBotPointToNav(NavSys, Goal, GoalNav))
	{
		FPathFindingQuery Query(this, *NavData, StartNav, GoalNav, NavData->GetDefaultQueryFilter());
		Query.SetAllowPartialPaths(true);
		const FPathFindingResult Result = NavSys->FindPathSync(Query);
		if (Result.IsSuccessful() && Result.Path.IsValid())
		{
			for (const FNavPathPoint& Point : Result.Path->GetPathPoints())
			{
				const FVector PathPoint = Point.Location;
				if (CachedPath.Num() == 0 || FVector::DistSquared2D(CachedPath.Last(), PathPoint) > FMath::Square(3.f))
				{
					CachedPath.Add(PathPoint);
				}
			}

			// Recast 路径终点有时是为胶囊安全投影后的点。站点/炸弹本身仍可再贴近一点。
			if (CachedPath.Num() > 0 && FVector::DistSquared2D(CachedPath.Last(), GoalNav) > FMath::Square(4.f)
				&& HasClearCapsulePath(CachedPath.Last(), GoalNav))
			{
				CachedPath.Add(GoalNav);
			}
		}
	}

	// 没有可烘焙导航时只允许走完全无遮挡的直线，不再让 Bot 盲目顶墙。
	if (CachedPath.Num() == 0 && HasClearCapsulePath(Start, Goal))
	{
		CachedPath.Add(Goal);
	}

	while (CachedPath.IsValidIndex(PathIndex)
		&& FVector::DistSquared2D(Start, CachedPath[PathIndex]) <= FMath::Square(45.f))
	{
		++PathIndex;
	}

	return CachedPath.IsValidIndex(PathIndex);
}

FVector AShooterBotController::GetCurrentSteeringTarget() const
{
	return CachedPath.IsValidIndex(PathIndex) ? CachedPath[PathIndex] : CachedGoal;
}

void AShooterBotController::MoveToward(const FVector& Goal, float DeltaSeconds)
{
	APawn* MyPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!MyPawn || !World || DeltaSeconds <= KINDA_SMALL_NUMBER) return;

	ACharacter* Body = Cast<ACharacter>(MyPawn);
	UCharacterMovementComponent* Move = Body ? Body->GetCharacterMovement() : nullptr;
	if (!Body || !Move) return;

	const FVector Loc = MyPawn->GetActorLocation();
	const float Now = World->GetTimeSeconds();

	if (Move->MaxWalkSpeed < 450.f)
	{
		Move->MaxWalkSpeed = 500.f;
	}
	if (Move->MovementMode != MOVE_Walking && Move->MovementMode != MOVE_NavWalking && Move->MovementMode != MOVE_Falling)
	{
		Move->SetMovementMode(MOVE_Walking);
	}

	if (FVector::DistSquared2D(Loc, Goal) <= FMath::Square(55.f))
	{
		Move->StopActiveMovement();
		Move->Velocity = FVector::ZeroVector;
		Move->UpdateComponentVelocity();
		return;
	}

	const float GoalShiftSq = FVector::DistSquared2D(Goal, CachedGoal);
	const bool bGoalMovedFar = GoalShiftSq > FMath::Square(250.f);
	const bool bPathFinished = !CachedPath.IsValidIndex(PathIndex);
	const bool bNeedRepath = bPathFinished || Now >= NextPathTime || bGoalMovedFar;

	if (bNeedRepath)
	{
		CachedGoal = Goal;
		NextPathTime = Now + FMath::FRandRange(0.65f, 0.9f);
		if (!RebuildCachedPath(Loc, Goal))
		{
			LogBotPath(Now, TEXT("NavMesh 无可用路径，且直线被墙挡住。"));
			Move->StopActiveMovement();
			Move->Velocity = FVector::ZeroVector;
			Move->UpdateComponentVelocity();
			return;
		}
	}

	while (CachedPath.IsValidIndex(PathIndex)
		&& FVector::DistSquared2D(Loc, CachedPath[PathIndex]) <= FMath::Square(45.f))
	{
		++PathIndex;
	}

	// 用胶囊扫掠跳过不需要踩的拐点，避免角色贴着 NavMesh 拐角/墙边蹭过去。
	while (CachedPath.IsValidIndex(PathIndex + 1)
		&& HasClearCapsulePath(Loc, CachedPath[PathIndex + 1]))
	{
		++PathIndex;
	}

	if (!CachedPath.IsValidIndex(PathIndex))
	{
		Move->StopActiveMovement();
		Move->Velocity = FVector::ZeroVector;
		Move->UpdateComponentVelocity();
		return;
	}

	FVector SteeringTarget = GetCurrentSteeringTarget();
	FVector Dir = (SteeringTarget - Loc).GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = GetPawn() ? GetPawn()->GetActorForwardVector().GetSafeNormal2D() : GetControlRotation().Vector().GetSafeNormal2D();
	}

	// 视线不跟脚下拐点：优先看更远的路径段，没敌人时就不会一直“盯着墙”。
	const FVector LookTarget = CachedPath.IsValidIndex(PathIndex + 1) ? CachedPath[PathIndex + 1] : SteeringTarget;
	const FVector LookDir = (LookTarget - Loc).GetSafeNormal2D();
	LastLookDirection = LookDir.IsNearlyZero() ? Dir : LookDir;

	// 同队轻推，避免多个 Bot 挤在门口形成整块碰撞体。
	{
		FVector Separation = FVector::ZeroVector;
		int32 NearCount = 0;
		const AShooterPlayerState* MyPS = GetPlayerState<AShooterPlayerState>();
		const AGameStateBase* GS = World->GetGameState();
		if (MyPS && GS)
		{
			for (APlayerState* PS : GS->PlayerArray)
			{
				const AShooterPlayerState* OtherPS = Cast<AShooterPlayerState>(PS);
				if (!OtherPS || OtherPS == MyPS || !OtherPS->HasTeam()
					|| OtherPS->GetTeamId() != MyPS->GetTeamId())
				{
					continue;
				}
				APawn* OtherPawn = OtherPS->GetPawn();
				if (!IsValid(OtherPawn) || OtherPawn == MyPawn) continue;
				const UStatsComponent* Stats = UStatsComponent::FindStatsComponent(OtherPawn);
				if (Stats && Stats->DeathState != EDeathState::NotDead) continue;

				const FVector ToOther = OtherPawn->GetActorLocation() - Loc;
				const float Dist2D = ToOther.Size2D();
				if (Dist2D < 1.f || Dist2D > 120.f) continue;
				const FVector Away = (-ToOther).GetSafeNormal2D();
				const float Ahead = FVector::DotProduct(Dir, ToOther.GetSafeNormal2D());
				const float Weight = (120.f - Dist2D) / 120.f;
				Separation += Away * Weight * (Ahead > 0.2f ? 1.2f : 0.7f);
				++NearCount;
			}
		}
		if (NearCount > 0 && !Separation.IsNearlyZero())
		{
			Dir = (Dir * 0.72f + Separation.GetSafeNormal2D() * 0.75f).GetSafeNormal2D();
		}
	}

	TryBotDoorInteract(this, MyPawn, Dir, Now);

	// 前方贴墙时用侧向探测选一边绕，优先保持上一帧的绕行方向，避免左右抖动。
	FHitResult AheadHit;
	if (SweepBotCapsule(World, MyPawn, Loc, Loc + Dir * 120.f, AheadHit) && !AheadHit.bStartPenetrating)
	{
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal2D();
		const FVector Left = -Right;

		FHitResult ProbeHit;
		const bool bLeftBlocked = SweepBotCapsule(World, MyPawn, Loc, Loc + Left * 95.f, ProbeHit);
		const bool bRightBlocked = SweepBotCapsule(World, MyPawn, Loc, Loc + Right * 95.f, ProbeHit);

		if (Now >= AvoidSideUntil)
		{
			if (bLeftBlocked != bRightBlocked)
			{
				AvoidSide = bLeftBlocked ? Right : Left;
			}
			else if (AvoidSide.IsNearlyZero())
			{
				AvoidSide = FMath::RandBool() ? Right : Left;
			}
			AvoidSideUntil = Now + 0.55f;
		}

		if (!AvoidSide.IsNearlyZero())
		{
			Dir = (Dir * 0.45f + AvoidSide.GetSafeNormal2D() * 0.9f).GetSafeNormal2D();
		}
	}

	LastSteerDirection = Dir;

	if (LastMoveAnchor.IsNearlyZero())
	{
		LastMoveAnchor = Loc;
	}
	if (FVector::DistSquared2D(Loc, LastMoveAnchor) < FMath::Square(22.f))
	{
		StuckTime += DeltaSeconds;
		if (StuckTime > 0.8f)
		{
			TryBotDoorInteract(this, MyPawn, Dir, Now);
			ClearCachedPath();
			AvoidSide = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal2D();
			if (FMath::RandBool()) AvoidSide *= -1.f;
			AvoidSideUntil = Now + 0.65f;
			StuckTime = 0.f;
			LastMoveAnchor = Loc;
		}
	}
	else
	{
		StuckTime = 0.f;
		LastMoveAnchor = Loc;
	}

	if (Dir.IsNearlyZero())
	{
		return;
	}

	UCapsuleComponent* Capsule = Body->GetCapsuleComponent();
	float Speed = Move->MaxWalkSpeed > 0.f ? Move->MaxWalkSpeed : 500.f;
	Speed = FMath::Clamp(Speed, 450.f, 700.f);

	FVector Delta(Dir.X * Speed * DeltaSeconds, Dir.Y * Speed * DeltaSeconds, 0.f);
	FFindFloorResult Floor;
	Move->FindFloor(Body->GetActorLocation(), Floor, false);
	if (Floor.IsWalkableFloor())
	{
		const FVector N = Floor.HitResult.ImpactNormal;
		Delta = FVector::VectorPlaneProject(Delta, N);
		const float WantSize = Speed * DeltaSeconds;
		if (!Delta.IsNearlyZero() && WantSize > KINDA_SMALL_NUMBER)
		{
			Delta = Delta.GetSafeNormal() * WantSize;
		}
	}
	else
	{
		Delta.Z -= 980.f * DeltaSeconds * DeltaSeconds * 0.5f + 600.f * DeltaSeconds * 0.016f;
	}

	FHitResult MoveHit;
	Move->SafeMoveUpdatedComponent(Delta, Body->GetActorQuat(), true, MoveHit);
	if (MoveHit.bBlockingHit && !Cast<APawn>(MoveHit.GetActor())
		&& MoveHit.ImpactNormal.Z < Move->GetWalkableFloorZ())
	{
		const float StepUp = Move->MaxStepHeight;
		const FVector BeforeStep = Body->GetActorLocation();
		FHitResult UpHit;
		Move->SafeMoveUpdatedComponent(FVector(0.f, 0.f, StepUp), Body->GetActorQuat(), true, UpHit);
		const bool bRaised = !UpHit.bStartPenetrating && (UpHit.Time > 0.2f || !UpHit.bBlockingHit);
		if (bRaised)
		{
			const FVector Remain(Dir.X * Speed * DeltaSeconds * (1.f - MoveHit.Time),
				Dir.Y * Speed * DeltaSeconds * (1.f - MoveHit.Time), 0.f);
			FHitResult FwdHit;
			Move->SafeMoveUpdatedComponent(Remain, Body->GetActorQuat(), true, FwdHit);
		}
		else
		{
			Body->SetActorLocation(BeforeStep, false);
			const FVector Slide = FVector::VectorPlaneProject(Delta, MoveHit.Normal);
			FHitResult SlideHit;
			Move->SafeMoveUpdatedComponent(Slide, Body->GetActorQuat(), true, SlideHit);
		}
	}

	Move->FindFloor(Body->GetActorLocation(), Floor, false);
	if (Floor.IsWalkableFloor() && Capsule)
	{
		const float HalfH = Capsule->GetScaledCapsuleHalfHeight();
		const float TargetZ = Floor.HitResult.ImpactPoint.Z + HalfH;
		const float ZErr = TargetZ - Body->GetActorLocation().Z;
		if (FMath::Abs(ZErr) > 1.5f && ZErr < (Move->MaxStepHeight + 30.f) && ZErr > -(Move->MaxStepHeight + 80.f))
		{
			FHitResult ZHit;
			Move->SafeMoveUpdatedComponent(FVector(0.f, 0.f, ZErr), Body->GetActorQuat(), true, ZHit);
		}
		if (Move->MovementMode == MOVE_Falling)
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}

	Move->Velocity = Dir * Speed;
	Move->UpdateComponentVelocity();
	DrawBotPathDebug(World, CachedPath, PathIndex, Loc);
}

void AShooterBotController::Think(float DeltaSeconds)
{
	// BOTS DISABLED — set bBotsDisabled=false (or delete this if) to re-enable
	static volatile bool bBotsDisabled = true;
	if (bBotsDisabled) { return; }
	if (!IsAlive())
	{
		ReleasePlant();
		return;
	}

	const AShooterGameState* MatchGS = GetWorld() ? GetWorld()->GetGameState<AShooterGameState>() : nullptr;
	if (MatchGS && MatchGS->GetMatchState() == EShooterMatchState::PostMatch)
	{
		ReleasePlant();
		return;
	}

	const ASearchDestroyGameState* GS = Cast<ASearchDestroyGameState>(MatchGS);
	if (GS)
	{
		if (GS->IsBuyPhase())
		{
			ReleasePlant();
			BuyOnce();
			return;
		}
		if (GS->GetRoundPhase() != EBombRoundPhase::Live)
		{
			ReleasePlant();
			return;
		}
	}

	const float Now = GetWorld()->GetTimeSeconds();
	EnsureGun();
	TryReload(Now);

	APawn* MyPawn = GetPawn();
	UCombatComponent* Combat = UCombatComponent::FindCombatComponent(MyPawn);
	const bool bReloading = Combat && Combat->IsReloading();
	APawn* Enemy = bReloading ? nullptr : FindEnemy();

	FVector Objective = FVector::ZeroVector;
	const bool bObjective = GS && WantsToPlantOrDefuse(Objective);
	const AShooterPlayerState* PS = GetPlayerState<AShooterPlayerState>();
	const bool bAttacker = GS && GS->IsAttackingTeam(PS ? PS->GetTeamId() : AShooterPlayerState::NoTeam);

	bool bMustStand = false;
	if (GS && bObjective && MyPawn)
	{
		if (bAttacker && PS && PS->IsCarryingBomb())
		{
			if (ABombSite* Site = FindPreferredSite(); Site && Site->ContainsPawn(MyPawn))
			{
				bMustStand = true;
				if (ASearchDestroyGameMode* GM = GetWorld()->GetAuthGameMode<ASearchDestroyGameMode>())
				{
					GM->BeginBombInteract(this);
					bHoldingBombInteract = true;
				}
			}
		}
		else if (bAttacker && PS && !PS->IsCarryingBomb())
		{
			if (const ABombActor* Bomb = GS->GetBomb(); IsValid(Bomb) && Bomb->IsDropped()
				&& FVector::DistSquared(MyPawn->GetActorLocation(), Bomb->GetActorLocation()) < FMath::Square(160.f))
			{
				bMustStand = true;
				if (ASearchDestroyGameMode* GM = GetWorld()->GetAuthGameMode<ASearchDestroyGameMode>())
				{
					GM->BeginBombInteract(this);
				}
			}
		}
		else if (!bAttacker && GS->IsBombDown())
		{
			if (const ABombActor* Bomb = GS->GetBomb(); IsValid(Bomb)
				&& FVector::DistSquared(MyPawn->GetActorLocation(), Bomb->GetActorLocation()) <= FMath::Square(120.f))
			{
				bMustStand = true;
				if (ASearchDestroyGameMode* GM = GetWorld()->GetAuthGameMode<ASearchDestroyGameMode>())
				{
					GM->BeginBombInteract(this);
					bHoldingBombInteract = true;
				}
			}
		}
	}

	if (bMustStand)
	{
		AbortBotPathFollowing(this);
		if (ACharacter* Body = Cast<ACharacter>(MyPawn))
		{
			if (UCharacterMovementComponent* Move = Body->GetCharacterMovement())
			{
				Move->StopActiveMovement();
				Move->StopMovementImmediately();
			}
		}
		return;
	}

	if (bHoldingBombInteract && (!bObjective || (PS && bAttacker && !PS->IsCarryingBomb() && !GS->IsBombDown())))
	{
		ReleasePlant();
	}

	// 看见人就边走边打，只有下包/拆包才站住。
	FVector Goal = bObjective ? Objective : FVector::ZeroVector;
	if (!bObjective)
	{
		if (GS)
		{
			if (ABombSite* Site = FindPreferredSite())
			{
				Goal = Site->GetActorLocation();
			}
		}
		else if (APawn* Chase = FindChasePawn())
		{
			Goal = Chase->GetActorLocation();
		}
	}
	if (!Goal.IsNearlyZero())
	{
		MoveToward(Goal, DeltaSeconds);
	}

	// 先挪人再瞄准：避免 MoveToward 把 TryShoot 的朝向盖掉，来回抢方向转圈。
	if (Enemy && !bReloading && MyPawn)
	{
		FVector Eye = FVector::ZeroVector;
		FRotator EyeRot = FRotator::ZeroRotator;
		MyPawn->GetActorEyesViewPoint(Eye, EyeRot);
		const FVector AimPoint = Enemy->GetActorLocation() + FVector(0.f, 0.f, 60.f);
		const float AimYaw = (AimPoint - Eye).Rotation().Yaw;
		const float CurYaw = GetControlRotation().Yaw;
		const float SmoothYaw = FMath::FixedTurn(CurYaw, AimYaw, 540.f * DeltaSeconds);
		const FRotator Look(0.f, SmoothYaw, 0.f);
		SetControlRotation(Look);
		if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(MyPawn))
		{
			Shooter->SetActorRotation(FRotator(0.f, SmoothYaw, 0.f));
			if (UCameraComponent* Camera = Shooter->GetFirstPersonCamera())
			{
				Camera->SetWorldRotation((AimPoint - Eye).Rotation());
			}
		}
		TryShoot(Now, Enemy);
	}
}
