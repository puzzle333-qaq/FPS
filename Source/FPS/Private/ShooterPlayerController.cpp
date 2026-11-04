#include "ShooterPlayerController.h"

#include "Game/PoliceCriminalGameMode.h"

#include "Game/PoliceCriminalGameState.h"

#include "Game/HostageActor.h"

#include "Game/ShooterPlayerState.h"
#include "ShooterCharacter.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Stats/StatsComponent.h"
#include "CombatComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "InputCoreTypes.h"
#include "ShooterGameInstance.h"
#include "Ui/PauseMenuScreen.h"
#include "ShooterGameModeBase.h"
#include "Ui/ModeHudWidget.h"

#include "Ui/MatchHUD.h"

#include "Ui/CombatHudKeepAlive.h"

#include "Ui/ScreenEffectOverlay.h"

#include "Ui/CombatStatusHud.h"

#include "Ui/BodyInjuryHud.h"

#include "Blueprint/WidgetBlueprintLibrary.h"

#include "Weapon/Weapon.h"
#include "Weapon/AttachmentData.h"
#include "Game/ShooterGameState.h"
#include "GameFramework/GameStateBase.h"
#include "Interface/Interactable.h"
#include "CollisionQueryParams.h"
#include "World/BreakableDoor.h"
#include "World/TripwireTrap.h"
#include "World/GasCan.h"
#include "Ui/ScoreboardScreen.h"
#include "Engine/Engine.h"
#include "GameFramework/HUD.h"
#include "EngineUtils.h"
#include "Ui/SettingsScreen.h"
#include "Settings/ShooterGameUserSettings.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "Game/ShooterPlayerState.h"

AShooterPlayerController::AShooterPlayerController()
{
	bReplicates = true;
	bPawnAlive = true;
}

void AShooterPlayerController::NoteSprintEnded()
{
	if (const UWorld* World = GetWorld())
	{
		SprintEndedAt = World->GetTimeSeconds();
	}
}

bool AShooterPlayerController::CanStartAds() const
{
	return GetAdsSettleRemaining() <= KINDA_SMALL_NUMBER;
}

float AShooterPlayerController::GetAdsSettleRemaining() const
{
	float Remain = 0.f;
	if (bIsSprinting)
	{
		Remain = FMath::Max(0.f, SprintToAdsDelay);
	}
	else
	{
		const UWorld* World = GetWorld();
		if (World && SprintToAdsDelay > KINDA_SMALL_NUMBER)
		{
			const float Elapsed = static_cast<float>(World->GetTimeSeconds() - SprintEndedAt);
			Remain = FMath::Max(0.f, SprintToAdsDelay - Elapsed);
		}
	}
	return Remain;
}

void AShooterPlayerController::StopSprinting()
{
	if (bIsSprinting) NoteSprintEnded();
	bIsSprinting = false;
	UpdateMovementSpeed();
}



void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();
	// TacticalBatch1: force CQB speeds (override BP defaults if any)
	RunSpeed = 220.f;
	SprintSpeed = 320.f;
	WalkSpeed = 120.f;

	// 从主菜单跳过来时 UI 输入和鼠标会跟着视口保留，进局恢复游戏输入
	if (IsLocalController())
	{
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
		// TacticalTrim: hide default crosshair leftovers.
		if (AHUD* Hud = GetHUD())
		{
			Hud->bShowHUD = true;
		}
		EnsureMatchHUD();
		EnsureCombatHudKeepAlive();
		if (GetNetMode() == NM_Client)
		{
			RequestServerTimeSync();
			GetWorldTimerManager().SetTimer(TimeSyncTimer, this, &ThisClass::RequestServerTimeSync, 2.f, true);
		}
		EnsureSpectateInput();
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(Subsystem) && ShooterIMC)
	{
		Subsystem->AddMappingContext(ShooterIMC, 0);
	}
	EnsureCrouchInput();
	EnsurePauseInput();
	EnsureNVGInput();
	EnsureWatchInput();
	EnsureModeHud();

	// 把局外配装发给服务器（服务器校验后重新生成武器）
	if (IsLocalController())
	{
		if (const UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance()))
		{
			const FLoadoutPreset Loadout = GI->GetCurrentLoadout();
			if (Loadout.Weapons.Num() > 0)
			{
				Server_SubmitLoadout(Loadout);
			}
		}
	}
}

void AShooterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* ShooterInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!IsValid(ShooterInputComponent)) return;

	if (MoveAction)
	{
		ShooterInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
	}
	if (LookAction)
	{
		ShooterInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
	}
	if (JumpAction)
	{
		ShooterInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ThisClass::Input_Jump);

	}

	EnsureCrouchInput();

	// C 切换蹲已去掉。只保留 Left Ctrl 按住蹲。旧 IA_Crouch / IA_Walk / I 键战术不再绑定。

	if (CrouchHoldAction)

	{

		ShooterInputComponent->BindAction(CrouchHoldAction, ETriggerEvent::Started, this, &ThisClass::Input_CrouchHold_Started);

		ShooterInputComponent->BindAction(CrouchHoldAction, ETriggerEvent::Completed, this, &ThisClass::Input_CrouchHold_Completed);

		ShooterInputComponent->BindAction(CrouchHoldAction, ETriggerEvent::Canceled, this, &ThisClass::Input_CrouchHold_Completed);

	}

	if (SprintAction)

	{

		ShooterInputComponent->BindAction(SprintAction, ETriggerEvent::Started,this, &ThisClass::Input_Sprint);

	}

	EnsureTacticalDeviceInput();

	if (TacticalDeviceAction)

	{

		ShooterInputComponent->BindAction(TacticalDeviceAction, ETriggerEvent::Started, this, &ThisClass::Input_TacticalDevice_Started);

		ShooterInputComponent->BindAction(TacticalDeviceAction, ETriggerEvent::Completed, this, &ThisClass::Input_TacticalDevice_Completed);

		ShooterInputComponent->BindAction(TacticalDeviceAction, ETriggerEvent::Canceled, this, &ThisClass::Input_TacticalDevice_Completed);

	}



	EnsureNVGInput();

	EnsureWatchInput();
	if (NVGAction && !bNVGBound)
	{
		ShooterInputComponent->BindAction(NVGAction, ETriggerEvent::Started, this, &ThisClass::Input_ToggleNVG);
		bNVGBound = true;
	}
	if (WatchAction)
	{
		ShooterInputComponent->BindAction(WatchAction, ETriggerEvent::Started, this, &ThisClass::Input_CheckWatch);
	}
	EnsureSpectateInput();
	EnsureInteractInput();
	if (InteractAction)
	{
		ShooterInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ThisClass::Input_Interact_Started);
		ShooterInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &ThisClass::Input_Interact_Completed);
		ShooterInputComponent->BindAction(InteractAction, ETriggerEvent::Canceled, this, &ThisClass::Input_Interact_Completed);
	}
	EnsurePauseInput();
	if (PauseMenuAction)
	{
		ShooterInputComponent->BindAction(PauseMenuAction, ETriggerEvent::Started, this, &ThisClass::Input_TogglePauseMenu);
	}

}



void AShooterPlayerController::EnsureTacticalDeviceInput()

{

	if (!IsLocalController()) return;

	if (!TacticalDeviceAction)

	{

		TacticalDeviceAction = NewObject<UInputAction>(this, TEXT("IA_TacticalDevice_Runtime"));

		TacticalDeviceAction->ValueType = EInputActionValueType::Boolean;

	}

	if (!TacticalDeviceIMC)

	{

		TacticalDeviceIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_TacticalDevice_Runtime"));

		// 鼠标上侧键：靠前的侧键（Mouse Button 4）。点按开关当前设备，按住切换镭射/手电。

		TacticalDeviceIMC->MapKey(TacticalDeviceAction, EKeys::ThumbMouseButton);

	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());

	if (IsValid(Subsystem) && TacticalDeviceIMC && !Subsystem->HasMappingContext(TacticalDeviceIMC))

	{

		Subsystem->AddMappingContext(TacticalDeviceIMC, 2);

	}

}



void AShooterPlayerController::Input_TacticalDevice_Started()

{

	if (!bPawnAlive) return;

	bTacticalDeviceHoldFired = false;

	if (UWorld* World = GetWorld())

	{

		World->GetTimerManager().SetTimer(TacticalDeviceHoldTimer, this, &ThisClass::Input_TacticalDevice_Hold, 0.35f, false);

	}

}



void AShooterPlayerController::Input_TacticalDevice_Completed()

{

	if (UWorld* World = GetWorld())

	{

		World->GetTimerManager().ClearTimer(TacticalDeviceHoldTimer);

	}

	if (bTacticalDeviceHoldFired || !bPawnAlive) return;



	UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetPawn());

	if (!IsValid(Combat)) return;

	if (bTacticalDeviceIsLaser)

	{

		Combat->Initiate_ToggleLaser();

	}

	else

	{

		Combat->Initiate_ToggleFlashlight();

	}

}



void AShooterPlayerController::Input_TacticalDevice_Hold()

{

	bTacticalDeviceHoldFired = true;

	bTacticalDeviceIsLaser = !bTacticalDeviceIsLaser;

	if (GEngine)

	{

		GEngine->AddOnScreenDebugMessage(

			0x74AC,

			1.2f,

			FColor::Yellow,

			bTacticalDeviceIsLaser ? TEXT("战术设备：镭射") : TEXT("战术设备：手电"));

	}

}



void AShooterPlayerController::EnsureNVGInput()

{

	if (!IsLocalController()) return;

	if (!NVGAction)
	{
		NVGAction = NewObject<UInputAction>(this, TEXT("IA_NVG_Runtime"));
		NVGAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!NVGIMC)
	{
		NVGIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_NVG_Runtime"));
		// Map K; Input_ToggleNVG requires Alt held (UE5.8 has no UInputModifierChordKey).
		NVGIMC->MapKey(NVGAction, EKeys::K);
	}
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(Subsystem) && NVGIMC && !Subsystem->HasMappingContext(NVGIMC))
	{
		Subsystem->AddMappingContext(NVGIMC, 2);
	}
}

void AShooterPlayerController::Input_ToggleNVG()
{
	if (!bPawnAlive) return;
	// Require Alt+K (mapping is K; gate on Alt here).
	if (!IsInputKeyDown(EKeys::LeftAlt) && !IsInputKeyDown(EKeys::RightAlt))
	{
		return;
	}
	if (AShooterCharacter* Char = Cast<AShooterCharacter>(GetPawn()))
	{
		Char->ToggleNVG();
	}
}



void AShooterPlayerController::EnsureCrouchInput()

{

	if (!bPawnAlive)return;

	const bool bNeedHold = !CrouchHoldAction;

	if (bNeedHold)

	{

		if (!CrouchRuntimeIMC)

		{

			CrouchRuntimeIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Crouch_Runtime"));

		}

		CrouchHoldAction = NewObject<UInputAction>(this, TEXT("IA_CrouchHold_Runtime"));

		CrouchHoldAction->ValueType = EInputActionValueType::Boolean;

		CrouchRuntimeIMC->MapKey(CrouchHoldAction, EKeys::LeftControl);

	}

	if (!CrouchRuntimeIMC) return;

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());

	if (IsValid(Subsystem) && !Subsystem->HasMappingContext(CrouchRuntimeIMC))
	{
		Subsystem->AddMappingContext(CrouchRuntimeIMC, 2);
	}
}


bool AShooterPlayerController::CanToggleCrouchNow() const
{
	const UWorld* World = GetWorld();
	if (!World) return true;
	return World->GetTimeSeconds() >= CrouchReadyTime;
}

void AShooterPlayerController::NoteCrouchToggle()
{
	const UWorld* World = GetWorld();
	if (!World) return;
	const double Now = World->GetTimeSeconds();
	if (Now - LastCrouchToggleTime <= CrouchSpamWindow)
	{
		CrouchSpamCount++;
	}
	else
	{
		CrouchSpamCount = 0;
	}
	LastCrouchToggleTime = Now;
	const float Cd = FMath::Min(CrouchSpamMaxCooldown, CrouchSpamBaseCooldown + CrouchSpamCount * CrouchSpamStep);
	CrouchReadyTime = Now + Cd;
}

void AShooterPlayerController::Input_CrouchHold_Started()
{
	if (!bPawnAlive)return;
	if (IsMovementLocked()) return;
	if (!CanToggleCrouchNow()) return;
	NoteCrouchToggle();
	bCrouchHeld = true;
	ACharacter* MyChar = GetCharacter();
	if (!IsValid(MyChar)) return;
	if (UCharacterMovementComponent* CMC = MyChar->GetCharacterMovement())
	{
		CMC->bWantsToCrouch = true;
	}
}

void AShooterPlayerController::Input_CrouchHold_Completed()
{
	bCrouchHeld = false;

	ACharacter* MyChar = GetCharacter();

	if (!IsValid(MyChar)) return;

	if (const AShooterCharacter* Shooter = Cast<AShooterCharacter>(MyChar))

	{

		if (Shooter->IsAltPeekCrouching()) return;

	}

	if (UCharacterMovementComponent* CMC = MyChar->GetCharacterMovement())

	{

		// CMC keeps trying to uncrouch every tick and stays crouched while blocked overhead.

		CMC->bWantsToCrouch = false;
	}
}

void AShooterPlayerController::Input_Crouch()
{
	if (!bPawnAlive)return;
	if (IsMovementLocked()) return;
	if (!CanToggleCrouchNow()) return;
	if (!IsValid(GetCharacter())) return;
	NoteCrouchToggle();
	if (UCharacterMovementComponent* CMC = GetCharacter()->GetCharacterMovement();IsValid(CMC))
	{
		CMC->bWantsToCrouch = !CMC->bWantsToCrouch;
	}

}

void AShooterPlayerController::Input_Jump()
{
	if (!bPawnAlive)return;
	if (IsMovementLocked()) return;
	if (!IsValid(GetCharacter())) return;
	UCharacterMovementComponent* CMC = GetCharacter()->GetCharacterMovement();
	if (!IsValid(CMC)) return;

	// CQB: no jump.
}

void AShooterPlayerController::Input_Sprint()
{
	if (const AShooterCharacter* Inj = Cast<AShooterCharacter>(GetCharacter()))
	{
		if (Inj->IsLegInjured() || Inj->IsBandaging()) return;
	}

	if (!bPawnAlive)return;
	if (IsMovementLocked()) return;

	UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetPawn());
	if (!bIsSprinting && IsValid(Combat) && Combat->IsReloading())
	{
		return;
	}

	bIsSprinting = !bIsSprinting;
	if (bIsSprinting) bIsWalking = false;   // 冲刺会打断静步
	// 第 9 步：开始冲刺时打断检视
	if (bIsSprinting)
	{
		if (UCombatComponent* SprintCombat = UCombatComponent::FindCombatComponent(GetPawn()))
		{
			SprintCombat->CancelInspect();
		}
	}
	UpdateMovementSpeed();
}

void AShooterPlayerController::Input_Walk()
{
	if (!bPawnAlive)return;
	if (IsMovementLocked()) return;
	bIsWalking = !bIsWalking;
	if (bIsWalking) bIsSprinting = false;   // 静步会打断冲刺
	UpdateMovementSpeed();
}




void AShooterPlayerController::Input_Move(const FInputActionValue& InputActionValue)
{

	if (!bPawnAlive)return;

	if (IsMovementLocked()) return;

	// Alt+A / Alt+D 是原地蹲探，不吃移动。

	const bool bAlt = IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt);

	if (bAlt && (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::D))) return;

	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();



	const FRotator Rotation = GetControlRotation();

	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->AddMovementInput(ForwardDirection, InputAxisVector.Y);
		ControlledPawn->AddMovementInput(RightDirection, InputAxisVector.X);
	}
}

void AShooterPlayerController::Input_Look(const FInputActionValue& InputActionValue)
{
	if (!bPawnAlive)return;
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();

	// 操作设置：灵敏度 / 开镜灵敏度 / 反转 Y
	float Scale = 1.f;
	bool bInvertY = false;
	if (const UShooterGameUserSettings* SS = UShooterGameUserSettings::GetShooterSettings())
	{
		Scale = SS->MouseSensitivity;
		bInvertY = SS->bInvertY;
		const UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetPawn());
		if (Combat && Combat->bAiming) Scale *= SS->ADSSensitivityMultiplier;
	}
	AddYawInput(InputAxisVector.X * Scale);
	AddPitchInput(InputAxisVector.Y * Scale * (bInvertY ? -1.f : 1.f));
}

void AShooterPlayerController::UpdateMovementSpeed()
{
	if (!bPawnAlive)return;
	// TacticalTrim: ignore stamina — sprint is never stopped by stamina.
	(void)GetStats();

	ApplyMovementSpeed();

	// 客户端改的速度会被服务器纠正回去，所以必须让服务器也改
	if (!HasAuthority())
	{
		Server_SetMovementState(bIsSprinting, bIsWalking);
	}
}

void AShooterPlayerController::ApplyMovementSpeed()
{
	if (!bPawnAlive)return;
	ACharacter* MyCharacter = GetCharacter();
	if (!IsValid(MyCharacter)) return;
	UCharacterMovementComponent* CMC = MyCharacter->GetCharacterMovement();
	if (!IsValid(CMC)) return;

	const UCombatComponent* Combat = UCombatComponent::FindCombatComponent(MyCharacter);
	const bool bAiming = IsValid(Combat) && Combat->bAiming;
	const bool bReloading = IsValid(Combat) && Combat->IsReloading();
	const bool bCrouched = MyCharacter->bIsCrouched;
	// 下蹲、开镜或换弹时不能冲刺
	if (bIsSprinting && (bAiming || bCrouched || bReloading))
	{
		NoteSprintEnded();
		bIsSprinting = false;

	}



	bIsWalking = false;

	float Speed = bIsSprinting ? SprintSpeed : RunSpeed;

	if (bAiming)    Speed *= AimSpeedMultiplier;



	// 站立时角色用 MaxWalkSpeed，蹲下时用 MaxWalkSpeedCrouched，两个都设好

	// 滑铲出口惯性：上限不低于保留速度，避免超速被刹车
	if (const AShooterCharacter* CarryChar = Cast<AShooterCharacter>(MyCharacter))
	{
		Speed = FMath::Max(Speed, CarryChar->GetCarriedSpeed());
	}
	if (const AShooterCharacter* LandChar = Cast<AShooterCharacter>(MyCharacter))
	{
		Speed *= LandChar->GetLandingSpeedMultiplier();
		Speed *= LandChar->GetInjurySpeedMultiplier();
		Speed *= LandChar->GetBandageSpeedMultiplier();
		Speed *= LandChar->GetArmorSpeedMultiplier();
		if (LandChar->IsLegInjured() && bIsSprinting)
		{
			NoteSprintEnded();

			bIsSprinting = false;

			Speed = RunSpeed;

			if (bAiming) Speed *= AimSpeedMultiplier;

			Speed *= LandChar->GetLandingSpeedMultiplier();

			Speed *= LandChar->GetInjurySpeedMultiplier();

			Speed *= LandChar->GetBandageSpeedMultiplier();
			Speed *= LandChar->GetArmorSpeedMultiplier();
		}
	}
	CMC->MaxWalkSpeed = Speed;
	CMC->MaxWalkSpeedCrouched = Speed * CrouchSpeedMultiplier;

	if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(MyCharacter))
	{
		Shooter->RefreshLocomotionGait();
	}

	// 耐力只在服务器上计算
	if (HasAuthority())
	{
		if (UStatsComponent* Stats = GetStats())
		{
			Stats->SetSprinting(bIsSprinting);
		}
	}
}

UStatsComponent* AShooterPlayerController::GetStats()
{
	UStatsComponent* Stats = UStatsComponent::FindStatsComponent(GetPawn());
	if (Stats)
	{
		Stats->OnStaminaDepleted.AddUniqueDynamic(this, &ThisClass::HandleStaminaDepleted);
	}
	return Stats;
}

void AShooterPlayerController::HandleStaminaDepleted()
{
	// TacticalTrim: stamina depleted ignored — keep sprinting.
}

void AShooterPlayerController::Server_SetMovementState_Implementation(bool bNewSprinting, bool bNewWalking)
{
	UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetPawn());
	if (bNewSprinting && IsValid(Combat) && Combat->IsReloading())
	{
		bNewSprinting = false;

	}

	bIsSprinting = bNewSprinting;

	(void)bNewWalking;

	bIsWalking = false;

	UpdateMovementSpeed();

}



void AShooterPlayerController::Client_StopSprint_Implementation()
{
	bIsSprinting = false;
	ApplyMovementSpeed();
}
void AShooterPlayerController::Server_SubmitLoadout_Implementation(const FLoadoutPreset& Loadout)
{
	// 客户端发来的不可信：只保留合法的枪，每类最多一把，配件交给 EquipAttachment 再校验白名单
	FLoadoutPreset Clean;
	Clean.Name = Loadout.Name;
	TSet<EWeaponKind> SeenKinds;
	int32 LongGunCount = 0;
	const AShooterGameState* MatchGS = GetWorld() ? GetWorld()->GetGameState<AShooterGameState>() : nullptr;
	const int32 MaxLong = 2;
	for (const FWeaponPreset& Preset : Loadout.Weapons)
	{
		if (Clean.Weapons.Num() >= 4) break;
		if (!Preset.WeaponClass || !Preset.WeaponClass->IsChildOf(AWeapon::StaticClass())) continue;
		if (AllowedLoadoutWeapons.Num() > 0 && !AllowedLoadoutWeapons.Contains(Preset.WeaponClass)) continue;
		const AWeapon* CDO = Preset.WeaponClass->GetDefaultObject<AWeapon>();
		if (CDO->WeaponKind == EWeaponKind::Bomb) continue;
		if (CDO->WeaponKind == EWeaponKind::LongGun)
		{
			if (LongGunCount >= MaxLong) continue;
			++LongGunCount;
		}
		else
		{
			if (SeenKinds.Contains(CDO->WeaponKind)) continue;
			SeenKinds.Add(CDO->WeaponKind);
		}

		FWeaponPreset CleanPreset;
		CleanPreset.WeaponClass = Preset.WeaponClass;
		for (UAttachmentData* Data : Preset.Attachments)
		{
			if (IsValid(Data) && CDO->CanEquipAttachment(Data))
			{
				CleanPreset.Attachments.Add(Data);
			}
		}
		Clean.Weapons.Add(CleanPreset);
	}

	SubmittedLoadout = Clean;
	bHasSubmittedLoadout = Clean.Weapons.Num() > 0;
	UE_LOG(LogTemp, Log, TEXT("[Loadout] %s submitted %d weapon(s)."), *GetName(), Clean.Weapons.Num());

	// 角色已经生成过默认武器：按配装重建
	if (bHasSubmittedLoadout && IsValid(GetPawn()))
	{
		if (UCombatComponent* CombatComp = GetPawn()->FindComponentByClass<UCombatComponent>())
		{
			CombatComp->RebuildInventory();
		}
	}
}
void AShooterPlayerController::EnsurePauseInput()
{
	if (!IsLocalController()) return;
	if (!PauseMenuAction)
	{
		PauseMenuAction = NewObject<UInputAction>(this, TEXT("IA_PauseMenu_Runtime"));
		PauseMenuAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!PauseMenuIMC)
	{
		PauseMenuIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_PauseMenu_Runtime"));
		PauseMenuIMC->MapKey(PauseMenuAction, EKeys::Escape);
		PauseMenuIMC->MapKey(PauseMenuAction, EKeys::P);
	}
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(Subsystem) && !Subsystem->HasMappingContext(PauseMenuIMC))
	{
		Subsystem->AddMappingContext(PauseMenuIMC, 2);
	}
}

void AShooterPlayerController::EnsureMatchHUD()
{
	// [AdsHudYaw] User: MatchHUD fully disabled (top score/timer header + match chrome).
	// Do not create or show WBP_MatchHUD. Combat ammo/health stay on WBP_ShooterOverlay.
	if (IsValid(MatchHUD))
	{
		MatchHUD->RemoveFromParent();
		MatchHUD = nullptr;
	}
	EnsureCombatHudKeepAlive();
}

void AShooterPlayerController::EnsureCombatHudKeepAlive()
{
	if (!IsLocalController())
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUD] EnsureCombatHudKeepAlive skipped (not local) PC=%s"), *GetName());
		return;
	}

	if (!IsValid(CombatHudKeepAlive))
	{
		CombatHudKeepAlive = CreateWidget<UCombatHudKeepAlive>(this, UCombatHudKeepAlive::StaticClass());
		if (IsValid(CombatHudKeepAlive))
		{
			CombatHudKeepAlive->SetVisibility(ESlateVisibility::HitTestInvisible);
			CombatHudKeepAlive->AddToViewport(199);
			UE_LOG(LogTemp, Warning, TEXT("[HUD] PATH=PC.Ensure CombatHudKeepAlive CREATED+AddToViewport(199)"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[HUD] CombatHudKeepAlive CreateWidget FAILED"));
		}
	}
	else if (!CombatHudKeepAlive->IsInViewport())
	{
		CombatHudKeepAlive->AddToViewport(199);
		UE_LOG(LogTemp, Warning, TEXT("[HUD] PATH=PC.Ensure CombatHudKeepAlive re-AddToViewport"));

	}



	if (!IsValid(ScreenEffectOverlay))

	{

		ScreenEffectOverlay = CreateWidget<UScreenEffectOverlay>(this, UScreenEffectOverlay::StaticClass());

		if (IsValid(ScreenEffectOverlay))

		{

			ScreenEffectOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);

			ScreenEffectOverlay->AddToViewport(250);

		}

	}

	else if (!ScreenEffectOverlay->IsInViewport())

	{

		ScreenEffectOverlay->AddToViewport(250);

	}



	// Auto-create BodyInjuryHud (bottom-left) if no WBP/instance of it is on screen.

	{

		TArray<UUserWidget*> Found;

		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, UBodyInjuryHud::StaticClass(), false);
		bool bOnScreen = false;
		for (UUserWidget* W : Found) { if (W && W->IsInViewport()) { bOnScreen = true; break; } }
		UE_LOG(LogTemp, Warning, TEXT("[BodyInjury] found=%d onScreen=%d"), Found.Num(), bOnScreen ? 1 : 0);
		if (!bOnScreen)
		{
			if (UBodyInjuryHud* BI = CreateWidget<UBodyInjuryHud>(this, UBodyInjuryHud::StaticClass()))
			{
				BI->SetVisibility(ESlateVisibility::HitTestInvisible);
				BI->AddToViewport(198);
				BI->SetAnchorsInViewport(FAnchors(0.f, 1.f));
				BI->SetAlignmentInViewport(FVector2D(0.f, 1.f));
				BI->SetPositionInViewport(FVector2D(30.f, -30.f), false);
				BI->SetDesiredSizeInViewport(FVector2D(180.f, 180.f));
				UE_LOG(LogTemp, Warning, TEXT("[BodyInjury] auto-created, tex=%s"), BI->BodyTexture ? *BI->BodyTexture->GetName() : TEXT("NULL"));
			}
		}
	}

	// RoN CombatStatusHud removed by request: never create; tear down if present.
	if (IsValid(CombatStatusHud))
	{
		CombatStatusHud->RemoveFromParent();
		CombatStatusHud = nullptr;
	}
}

void AShooterPlayerController::Input_TogglePauseMenu()
{
	TogglePauseMenu();
}

bool AShooterPlayerController::IsPauseMenuOpen() const
{
	return IsValid(PauseMenu) && PauseMenu->IsInViewport();
}

void AShooterPlayerController::TogglePauseMenu()
{
	if (IsPauseMenuOpen()) ClosePauseMenu();
	else OpenPauseMenu();
}

void AShooterPlayerController::OpenPauseMenu()
{
	if (!IsLocalController() || IsPauseMenuOpen()) return;
	if (!PauseMenuWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PauseMenu] PauseMenuWidgetClass is not set on %s."), *GetClass()->GetName());
		return;
	}
	if (!IsValid(PauseMenu))
	{
		PauseMenu = CreateWidget<UPauseMenuScreen>(this, PauseMenuWidgetClass);
		if (!IsValid(PauseMenu)) return;
	}

	// 停掉冲刺等持续状态，避免菜单打开时角色一直跑
	StopSprinting();
	PauseMenu->AddToViewport(50);
	// 纯 UI 输入：开火/视角/移动都收不到，不会误开枪；Esc/P 由菜单自己处理关闭
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(PauseMenu->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	bShowMouseCursor = true;
	PauseMenu->OnMenuOpened();
}

void AShooterPlayerController::ClosePauseMenu()
{
	if (IsValid(PauseMenu))
	{
		PauseMenu->RemoveFromParent();
	}
	if (!IsLocalController()) return;
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}
void AShooterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (HasAuthority()) Auth_StopSpectating();
	if (IsLocalController() && PlayerCameraManager)
	{
		// 复活：清掉死亡时的黑屏
		PlayerCameraManager->StopCameraFade();
		PlayerCameraManager->SetManualCameraFade(0.f, FLinearColor::Black, false);
	}
	// 复活后的新角色：房主本地也恢复可操作
	if (IsLocalController() && !bMatchOver)
	{
		bPawnAlive = true;
	}
	ApplyControlSettings();
	EnsureCombatHudKeepAlive();
}

void AShooterPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StopCameraFade();
		PlayerCameraManager->SetManualCameraFade(0.f, FLinearColor::Black, false);
	}
	ApplyControlSettings();
	EnsureCombatHudKeepAlive();
	// 客户端收到新角色（复活）时恢复可操作
	if (!bMatchOver)
	{
		bPawnAlive = true;
	}
}

bool AShooterPlayerController::IsMovementLocked() const
{
	const UWorld* World = GetWorld();
	const AShooterGameState* GS = World ? World->GetGameState<AShooterGameState>() : nullptr;
	if (!GS) return false;
	if (GS->IsMovementLocked()) return true;
	if (const APoliceCriminalGameState* PCGS = Cast<APoliceCriminalGameState>(GS))
	{
		const AShooterPlayerState* PS = GetPlayerState<AShooterPlayerState>();
		if (PS && PS->HasTeam() && PCGS->IsFactionLocked(PS->GetTeamId()))
		{
			return true;
		}
	}
	return false;
}

void AShooterPlayerController::EnsureInteractInput()
{
	if (!InteractAction)
	{
		InteractAction = NewObject<UInputAction>(this, TEXT("IA_Interact_Runtime"));
		InteractAction->ValueType = EInputActionValueType::Boolean;
		if (!InteractIMC)
		{
			InteractIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Interact_Runtime"));
			InteractIMC->MapKey(InteractAction, EKeys::F);
		}
	}
	if (!InteractIMC) return;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(Subsystem) && !Subsystem->HasMappingContext(InteractIMC))
	{
		Subsystem->AddMappingContext(InteractIMC, 1);
	}
}

void AShooterPlayerController::Input_Interact_Started()
{
	if (!bPawnAlive) return;
	if (AActor* Target = TraceInteractable())
	{
		Server_Interact(Target);
	}
}

void AShooterPlayerController::Input_Interact_Completed()
{
}



void AShooterPlayerController::Client_OnMatchEnded_Implementation(uint8 WinningTeam)
{
	bMatchOver = true;
	bPawnAlive = false;
	ClosePauseMenu();
	if (APawn* P = GetPawn())
	{
		P->DisableInput(this);
	}
	StopSprinting();
}

void AShooterPlayerController::Client_GoToMainMenu_Implementation()
{
	if (UShooterGameInstance* GI = Cast<UShooterGameInstance>(GetGameInstance()))
	{
		if (!GI->GoToMainMenu())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Match] Client_GoToMainMenu failed: MainMenuMap not set in BP_ShooterGameInstance"));
		}
	}
}













void AShooterPlayerController::EnsureModeHud()
{
	if (!IsLocalController() || IsValid(ModeHud)) return;
	const AShooterGameModeBase* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AShooterGameModeBase>() : nullptr;
	TSubclassOf<UModeHudWidget> ClassToSpawn = GM ? GM->ModeHudWidgetClass : nullptr;
	if (!ClassToSpawn)
	{
		// 客户端没有 AuthGameMode：从 GameState 模式猜默认（可选，留空即可靠 MatchHUD）
		return;
	}
	ModeHud = CreateWidget<UModeHudWidget>(this, ClassToSpawn);
	if (IsValid(ModeHud))
	{
		ModeHud->AddToViewport(5);
	}
}



AActor* AShooterPlayerController::TraceInteractable() const
{
	const APawn* MyPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!MyPawn || !World) return nullptr;
	FVector ViewLoc; FRotator ViewRot;
	GetPlayerViewPoint(ViewLoc, ViewRot);
	const FVector End = ViewLoc + ViewRot.Vector() * InteractDistance;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(FPSInteractTrace), false, MyPawn);
	TArray<AActor*> AttachedToPawn;
	MyPawn->GetAttachedActors(AttachedToPawn, true, true);
	Q.AddIgnoredActors(AttachedToPawn);
	FHitResult Hit;
	if (!World->SweepSingleByChannel(Hit, ViewLoc, End, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(8.f), Q))
	{
		return nullptr;
	}
	AActor* HitActor = Hit.GetActor();
	if (!IsValid(HitActor) || !HitActor->GetClass()->ImplementsInterface(UInteractable::StaticClass())) return nullptr;
	if (!IInteractable::Execute_CanInteract(HitActor, const_cast<APawn*>(MyPawn))) return nullptr;
	return HitActor;
}

FText AShooterPlayerController::GetInteractPromptText() const
{
	if (!bPawnAlive || !GetPawn()) return FText::GetEmpty();
	if (AActor* Target = TraceInteractable())
	{
		return IInteractable::Execute_GetInteractPrompt(Target, GetPawn());
	}
	return FText::GetEmpty();
}

void AShooterPlayerController::Server_Interact_Implementation(AActor* Target)
{
	APawn* MyPawn = GetPawn();
	if (!bPawnAlive || !IsValid(MyPawn) || !IsValid(Target)) return;
	if (!Target->GetClass()->ImplementsInterface(UInteractable::StaticClass())) return;
	// 服务器距离校验（相机到 Actor 中心有偏差，放宽一些）
	if (FVector::Dist(MyPawn->GetActorLocation(), Target->GetActorLocation()) > InteractDistance + 200.f) return;
	if (!IInteractable::Execute_CanInteract(Target, MyPawn)) return;
	UE_LOG(LogTemp, Log, TEXT("[Interact] %s -> %s"), *GetNameSafe(this), *GetNameSafe(Target));
	IInteractable::Execute_Interact(Target, MyPawn);
}




void AShooterPlayerController::EnsureScoreboardInput()
{
	if (!IsLocalController()) return;
	if (!ScoreboardAction)
	{
		ScoreboardAction = NewObject<UInputAction>(this, TEXT("IA_Scoreboard_Runtime"));
		ScoreboardAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!ScoreboardIMC)
	{
		ScoreboardIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Scoreboard_Runtime"));
		ScoreboardIMC->MapKey(ScoreboardAction, EKeys::Tab);
	}
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(Subsystem) && !Subsystem->HasMappingContext(ScoreboardIMC))
	{
		Subsystem->AddMappingContext(ScoreboardIMC, 1);
	}
	if (!bScoreboardBound)
	{
		if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
		{
			EIC->BindAction(ScoreboardAction, ETriggerEvent::Started, this, &ThisClass::Input_Scoreboard_Started);
			EIC->BindAction(ScoreboardAction, ETriggerEvent::Completed, this, &ThisClass::Input_Scoreboard_Completed);
			EIC->BindAction(ScoreboardAction, ETriggerEvent::Canceled, this, &ThisClass::Input_Scoreboard_Completed);
			bScoreboardBound = true;
		}
	}
}

void AShooterPlayerController::Input_Scoreboard_Started()
{
	ShowScoreboard(true);
}

void AShooterPlayerController::Input_Scoreboard_Completed()
{
	ShowScoreboard(false);
}

void AShooterPlayerController::ShowScoreboard(bool bShow)
{
	if (!IsLocalController()) return;
	if (!bShow)
	{
		if (IsValid(Scoreboard) && Scoreboard->IsInViewport()) Scoreboard->RemoveFromParent();
		return;
	}
	if (!ScoreboardWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Scoreboard] ScoreboardWidgetClass 未设置"));
		return;
	}
	if (!IsValid(Scoreboard))
	{
		Scoreboard = CreateWidget<UScoreboardScreen>(this, ScoreboardWidgetClass);
		if (!IsValid(Scoreboard)) return;
	}
	if (!Scoreboard->IsInViewport())
	{
		Scoreboard->AddToViewport(20);
	}
	Scoreboard->RefreshScoreboard();
}


// ================= 战术大地图 (M) =================






void AShooterPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShooterPlayerController, bSpectating, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AShooterPlayerController, SpectateTarget, COND_OwnerOnly);
}

FString AShooterPlayerController::GetSpectateTargetName() const
{
	return IsValid(SpectateTarget) ? SpectateTarget->GetPlayerName() : FString();
}

static bool FPS_IsAlivePawn(const APawn* Pawn)
{
	if (!IsValid(Pawn)) return false;
	const UStatsComponent* Stats = UStatsComponent::FindStatsComponent(Pawn);
	return !Stats || Stats->DeathState == EDeathState::NotDead;
}

void AShooterPlayerController::Auth_StartSpectating()
{
	if (!HasAuthority() || bMatchOver) return;
	if (FPS_IsAlivePawn(GetPawn())) return; // 已经复活了
	bSpectating = true;
	bPawnAlive = false;
	SpectateTarget = nullptr;
	Auth_SpectateNext(false);
}

void AShooterPlayerController::Auth_StopSpectating()
{
	if (!HasAuthority()) return;
	const bool bWas = bSpectating;
	bSpectating = false;
	SpectateTarget = nullptr;
	if (bWas)
	{
		if (APawn* P = GetPawn()) SetViewTarget(P);
		ApplySpectateLocal();
		ForceNetUpdate();
	}
}

void AShooterPlayerController::Auth_SpectateNext(bool bOnlyIfInvalid)
{
	Auth_SpectateCycle(+1, bOnlyIfInvalid);
}

void AShooterPlayerController::Auth_SpectatePrev(bool bOnlyIfInvalid)
{
	Auth_SpectateCycle(-1, bOnlyIfInvalid);
}

void AShooterPlayerController::Auth_SpectateCycle(int32 Direction, bool bOnlyIfInvalid)
{
	if (!HasAuthority() || !bSpectating || !GetWorld()) return;
	const int32 Step = Direction < 0 ? -1 : +1;

	auto TargetPawnOf = [](APlayerState* PS) -> APawn*
	{
		return IsValid(PS) ? PS->GetPawn() : nullptr;
	};
	if (bOnlyIfInvalid && FPS_IsAlivePawn(TargetPawnOf(SpectateTarget))) return;

	const AShooterPlayerState* MyPS = GetPlayerState<AShooterPlayerState>();
	const uint8 MyTeam = MyPS ? MyPS->GetTeamId() : AShooterPlayerState::NoTeam;

	TArray<APlayerState*> Mates, Others;
	if (const AGameStateBase* GS = GetWorld()->GetGameState())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (!IsValid(PS) || PS == PlayerState) continue;
			if (!FPS_IsAlivePawn(PS->GetPawn())) continue;
			const AShooterPlayerState* SPS = Cast<AShooterPlayerState>(PS);
			if (SPS && MyTeam != AShooterPlayerState::NoTeam && SPS->GetTeamId() == MyTeam) Mates.Add(PS);
			else Others.Add(PS);
		}
	}
	// 只看队友；队友全死才看其他人
	const TArray<APlayerState*>& Pool = Mates.Num() > 0 ? Mates : Others;

	APlayerState* NewTarget = nullptr;
	if (Pool.Num() > 0)
	{
		const int32 Cur = Pool.IndexOfByKey(SpectateTarget.Get());
		const int32 N = Pool.Num();
		const int32 Next = (Cur < 0) ? 0 : ((Cur + Step) % N + N) % N;
		NewTarget = Pool[Next];
	}
	SpectateTarget = NewTarget;

	if (APawn* TP = TargetPawnOf(NewTarget))
	{
		SetViewTargetWithBlend(TP, 0.15f);
	}
	else
	{
		// 没人可看：固定镜头（自己的尸体视角/出生点）
		AActor* Fixed = GetPawn();
		if (!IsValid(Fixed))
		{
			for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It) { Fixed = *It; break; }
		}
		if (IsValid(Fixed)) SetViewTarget(Fixed);
	}
	ApplySpectateLocal();
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[Spectate] %s -> %s (dir=%d)"), PlayerState ? *PlayerState->GetPlayerName() : TEXT("?"),
		NewTarget ? *NewTarget->GetPlayerName() : TEXT("<fixed>"), Step);
}

void AShooterPlayerController::OnRep_Spectate()
{
	ApplySpectateLocal();
}

void AShooterPlayerController::ApplySpectateLocal()
{
	if (!IsLocalController()) return;
	if (bSpectating)
	{
		bPawnAlive = false;
		// 死亡黑屏是定时器加的，观战期间一直清掉
		TWeakObjectPtr<AShooterPlayerController> WeakThis = this;
		auto ClearFade = [WeakThis]()
		{
			if (WeakThis.IsValid() && WeakThis->PlayerCameraManager)
			{
				WeakThis->PlayerCameraManager->StopCameraFade();
				WeakThis->PlayerCameraManager->SetManualCameraFade(0.f, FLinearColor::Black, false);
			}
			if (WeakThis.IsValid()) WeakThis->SyncSpectatedCharacter();
		};
		ClearFade();
		GetWorldTimerManager().SetTimer(SpectateFadeTimer, FTimerDelegate::CreateWeakLambda(this, ClearFade), 0.2f, true);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(SpectateFadeTimer);
		SyncSpectatedCharacter();
	}
	OnSpectateTargetChanged.Broadcast(bSpectating, SpectateTarget);
}

void AShooterPlayerController::Server_SpectateNext_Implementation()
{
	Auth_SpectateNext(false);
}

void AShooterPlayerController::Server_SpectatePrev_Implementation()
{
	Auth_SpectatePrev(false);
}

void AShooterPlayerController::Input_SpectateNext()
{
	if (bSpectating) Server_SpectateNext();
}

void AShooterPlayerController::Input_SpectatePrev()
{
	if (bSpectating) Server_SpectatePrev();
}

void AShooterPlayerController::EnsureSpectateInput()
{
	if (!IsLocalController()) return;
	if (!SpectateNextAction)
	{
		SpectateNextAction = NewObject<UInputAction>(this, TEXT("IA_SpectateNext_Runtime"));
		SpectateNextAction->ValueType = EInputActionValueType::Boolean;
		// 不吞键：左键开火照常工作
		SpectateNextAction->bConsumeInput = false;
		SpectatePrevAction = NewObject<UInputAction>(this, TEXT("IA_SpectatePrev_Runtime"));
		SpectatePrevAction->ValueType = EInputActionValueType::Boolean;
		SpectatePrevAction->bConsumeInput = false;
		SpectateIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Spectate_Runtime"));
		SpectateIMC->MapKey(SpectateNextAction, EKeys::LeftMouseButton);
		// 上一个目标：右键 或 Q（观战中才有效；不吞键）
		SpectateIMC->MapKey(SpectatePrevAction, EKeys::RightMouseButton);
		SpectateIMC->MapKey(SpectatePrevAction, EKeys::Q);
	}
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(Subsystem) && SpectateIMC && !Subsystem->HasMappingContext(SpectateIMC))
	{
		Subsystem->AddMappingContext(SpectateIMC, 2);
	}
	if (!bSpectateBound)
	{
		if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
		{
			EIC->BindAction(SpectateNextAction, ETriggerEvent::Started, this, &ThisClass::Input_SpectateNext);
			EIC->BindAction(SpectatePrevAction, ETriggerEvent::Started, this, &ThisClass::Input_SpectatePrev);
			bSpectateBound = true;
		}
	}
}

void AShooterPlayerController::SyncSpectatedCharacter()
{
	// 本机观战时，让被看的角色显示第一人称（手臂+1P 枪），换人/停止观战时还原
	AShooterCharacter* Want = nullptr;
	if (bSpectating && IsLocalController())
	{
		Want = Cast<AShooterCharacter>(GetViewTarget());
		if (Want == GetPawn()) Want = nullptr;
	}
	if (Viewed1PCharacter.Get() == Want) return;
	if (AShooterCharacter* Old = Viewed1PCharacter.Get()) Old->SetFirstPersonViewedLocally(false);
	Viewed1PCharacter = Want;
	if (Want) Want->SetFirstPersonViewedLocally(true);
}

// ================= 客户端校时 =================

void AShooterPlayerController::RequestServerTimeSync()
{
	if (GetWorld()) Server_RequestServerTime(GetWorld()->GetTimeSeconds());
}

void AShooterPlayerController::Server_RequestServerTime_Implementation(double ClientSendTime)
{
	if (GetWorld()) Client_ReportServerTime(ClientSendTime, GetWorld()->GetTimeSeconds());
}

void AShooterPlayerController::Client_ReportServerTime_Implementation(double ClientSendTime, double ServerTime)
{
	UWorld* World = GetWorld();
	AShooterGameState* GS = World ? World->GetGameState<AShooterGameState>() : nullptr;
	if (!GS) return;
	const double Now = World->GetTimeSeconds();
	const double RTT = FMath::Max(0.0, Now - ClientSendTime);
	const double NewOffset = ServerTime + RTT * 0.5 - Now;

	// 慢包（RTT 明显比最好的大）估计误差大，少信它；偏差过大直接跳过去
	if (BestTimeSyncRTT < 0.0 || RTT < BestTimeSyncRTT) BestTimeSyncRTT = RTT;
	else BestTimeSyncRTT = FMath::Lerp(BestTimeSyncRTT, RTT, 0.1); // 网络变化时慢慢跟上
	if (!GS->HasSyncedServerTime() || FMath::Abs(NewOffset - GS->GetSyncedServerTimeOffset()) > 1.0)
	{
		GS->SetSyncedServerTimeOffset(NewOffset);
		return;
	}
	const double Weight = RTT <= BestTimeSyncRTT * 1.5 ? 0.3 : 0.05;
	GS->SetSyncedServerTimeOffset(FMath::Lerp(GS->GetSyncedServerTimeOffset(), NewOffset, Weight));
}

// ================= 设置（局内） =================

void AShooterPlayerController::ShowSettings()
{
	if (!IsLocalController()) return;
	if (!SettingsWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Menu] SettingsWidgetClass is not set on %s"), *GetClass()->GetName());
		return;
	}
	if (!IsValid(SettingsScreen)) SettingsScreen = CreateWidget<USettingsScreen>(this, SettingsWidgetClass);
	if (!IsValid(SettingsScreen)) return;
	// 局内只能改操作设置
	SettingsScreen->SetGraphicsLocked(true);
	// 盖在暂停菜单（Z=50）上面，并把暂停菜单先藏起来
	if (!SettingsScreen->IsInViewport()) SettingsScreen->AddToViewport(60);
	if (IsValid(PauseMenu) && PauseMenu->IsInViewport() && PauseMenu->GetVisibility() != ESlateVisibility::Collapsed)
	{
		PauseMenu->SetVisibility(ESlateVisibility::Collapsed);
		bPauseHiddenForSettings = true;
	}
	SettingsScreen->RefreshFromCurrent();
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(SettingsScreen->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
}

void AShooterPlayerController::HideSettings()
{
	if (IsValid(SettingsScreen) && SettingsScreen->IsInViewport()) SettingsScreen->RemoveFromParent();
	if (!IsLocalController()) return;
	if (bPauseHiddenForSettings && IsValid(PauseMenu))
	{
		PauseMenu->SetVisibility(ESlateVisibility::Visible);
		bPauseHiddenForSettings = false;
	}
	if (IsPauseMenuOpen())
	{
		// 回到暂停菜单
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(PauseMenu->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		SetShowMouseCursor(true);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}

void AShooterPlayerController::ApplyControlSettings()
{
	if (!IsLocalController()) return;
	const UShooterGameUserSettings* SS = UShooterGameUserSettings::GetShooterSettings();
	if (!SS) return;
	if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetPawn()))
	{
		Shooter->SetDefaultFieldOfView(SS->FieldOfView);
	}
}


void AShooterPlayerController::Armor(int32 Level)
{
	AShooterCharacter* Char = Cast<AShooterCharacter>(GetPawn());
	if (!IsValid(Char))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Armor] No ShooterCharacter pawn"));
		return;
	}
	EBodyArmorType Type = EBodyArmorType::None;
	if (Level == 1) Type = EBodyArmorType::Light;
	else if (Level >= 2) Type = EBodyArmorType::Heavy;
	Char->SetBodyArmorLocal(Type);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
			FString::Printf(TEXT("BodyArmor -> %d (0=None 1=Light 2=Heavy)"), static_cast<int32>(Type)));
	}
	UE_LOG(LogTemp, Log, TEXT("[Armor] set to %d"), static_cast<int32>(Type));
}

void AShooterPlayerController::FactionLook(int32 Level)
{
	AShooterCharacter* Char = Cast<AShooterCharacter>(GetPawn());
	if (!IsValid(Char))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Look] No ShooterCharacter pawn"));
		return;
	}
	ECharacterLookFaction Type = ECharacterLookFaction::Default;
	if (Level == 1) Type = ECharacterLookFaction::Criminal;
	else if (Level >= 2) Type = ECharacterLookFaction::Police;
	Char->SetFactionLookLocal(Type);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
			FString::Printf(TEXT("FactionLook -> %d (0=Manny 1=Bandits05 2=Bandits02Police)"), static_cast<int32>(Type)));
	}
	UE_LOG(LogTemp, Log, TEXT("[Look] set to %d"), static_cast<int32>(Type));
}

void AShooterPlayerController::ToggleThirdPerson()
{
	AShooterCharacter* Char = Cast<AShooterCharacter>(GetPawn());
	if (!IsValid(Char))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Debug3P] No ShooterCharacter pawn"));
		return;
	}
	Char->ToggleDebugThirdPerson();
}

void AShooterPlayerController::ThirdPerson(int32 Enabled)
{
	AShooterCharacter* Char = Cast<AShooterCharacter>(GetPawn());
	if (!IsValid(Char))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Debug3P] No ShooterCharacter pawn"));
		return;
	}
	Char->SetDebugThirdPerson(Enabled != 0);
}

void AShooterPlayerController::Vision(int32 Mode)
{
	AShooterCharacter* Char = Cast<AShooterCharacter>(GetPawn());
	if (!IsValid(Char))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Vision] No ShooterCharacter pawn"));
		return;
	}
	ETacticalVisionMode Type = ETacticalVisionMode::Off;
	if (Mode == 1) Type = ETacticalVisionMode::NVG;
	else if (Mode >= 2) Type = ETacticalVisionMode::Thermal;
	Char->SetTacticalVisionMode(Type);
	UE_LOG(LogTemp, Log, TEXT("[Vision] set to %d"), static_cast<int32>(Type));
}

void AShooterPlayerController::CycleVision()
{
	AShooterCharacter* Char = Cast<AShooterCharacter>(GetPawn());
	if (!IsValid(Char))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Vision] No ShooterCharacter pawn"));
		return;
	}
	Char->CycleTacticalVisionMode();
}

void AShooterPlayerController::RefillVision(float Amount)
{
	AShooterCharacter* Char = Cast<AShooterCharacter>(GetPawn());
	if (!IsValid(Char))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Vision] No ShooterCharacter pawn"));
		return;
	}
	Char->RefillVisionBattery(Amount);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan,
			FString::Printf(TEXT("Vision battery -> %.0f%%"), Char->GetVisionBatteryNormalized() * 100.f));
	}
	UE_LOG(LogTemp, Log, TEXT("[Vision] refill amount=%.2f battery=%.0f%%"), Amount, Char->GetVisionBatteryNormalized() * 100.f);

}



void AShooterPlayerController::Goggles(int32 Enabled)

{

	AShooterCharacter* Char = Cast<AShooterCharacter>(GetPawn());

	if (!IsValid(Char))

	{

		UE_LOG(LogTemp, Warning, TEXT("[Goggles] No ShooterCharacter pawn"));

		return;

	}

	Char->SetHasFlashGoggles(Enabled != 0);

	if (GEngine)

	{

		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan,

			Enabled != 0 ? TEXT("护目镜：开（闪光弹变重影）") : TEXT("护目镜：关（闪光弹白屏）"));

	}

}



void AShooterPlayerController::FlashWash(float Amount)

{

	AShooterCharacter* Char = Cast<AShooterCharacter>(GetPawn());

	if (!IsValid(Char))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Vision] No ShooterCharacter pawn"));
		return;
	}
	Char->AddVisionFlashWash(Amount);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow,
			FString::Printf(TEXT("FlashWash +%.1f"), Amount));
	}
}

void AShooterPlayerController::SpawnTestDoor()
{
	if (HasAuthority())
	{
		Server_SpawnTestDoor_Implementation();
	}
	else
	{
		Server_SpawnTestDoor();
	}
}

void AShooterPlayerController::Server_SpawnTestDoor_Implementation()
{
	APawn* MyPawn = GetPawn();
	if (!IsValid(MyPawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Door] SpawnTestDoor: no pawn"));
		return;
	}
	ABreakableDoor* Door = ABreakableDoor::SpawnInFrontOf(MyPawn, 180.f);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Cyan,
			Door ? TEXT("Spawned BreakableDoor ahead (F open/close, shoot to break)")
			     : TEXT("Failed to spawn BreakableDoor"));
	}
}


void AShooterPlayerController::SpawnTripwire()
{
	Server_SpawnTripwire(true);
}

void AShooterPlayerController::SpawnGasCan()
{
	Server_SpawnGasCan(true);
}

void AShooterPlayerController::PlaceTripwire()
{
	Server_SpawnTripwire(false);
}

void AShooterPlayerController::PlaceGasCan()
{
	Server_SpawnGasCan(false);
}

void AShooterPlayerController::Server_SpawnTripwire_Implementation(bool bIgnoreSetup)
{
	APawn* MyPawn = GetPawn();
	if (!IsValid(MyPawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Trap] SpawnTripwire: no pawn"));
		return;
	}
	FString Err;
	ATripwireTrap* Trap = ATripwireTrap::Auth_TryPlace(MyPawn, bIgnoreSetup, &Err);
	if (GEngine)
	{
		if (Trap)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Orange,
				TEXT("绊雷已放置（踩线触发 / 警察按 F 拆除）"));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
				*(Err.IsEmpty() ? FString(TEXT("无法放置绊雷")) : Err));
		}
	}
}

void AShooterPlayerController::Server_SpawnGasCan_Implementation(bool bIgnoreSetup)
{
	APawn* MyPawn = GetPawn();
	if (!IsValid(MyPawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Trap] SpawnGasCan: no pawn"));
		return;
	}
	FString Err;
	AGasCan* Can = AGasCan::Auth_TryPlace(MyPawn, bIgnoreSetup, &Err);
	if (GEngine)
	{
		if (Can)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Orange,
				TEXT("汽油桶已放置（开枪引爆，爆炸/燃烧不吃护甲）"));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
				*(Err.IsEmpty() ? FString(TEXT("无法放置汽油桶")) : Err));
		}
	}
}


// ===== 警匪战术 PIE / loadout helpers =====
void AShooterPlayerController::Auth_SetSubmittedLoadout(const FLoadoutPreset& Loadout, bool bStripAttachments)
{
	if (!HasAuthority()) return;
	FLoadoutPreset Clean = Loadout;
	if (bStripAttachments)
	{
		for (FWeaponPreset& P : Clean.Weapons)
		{
			P.Attachments.Reset();
		}
	}
	SubmittedLoadout = Clean;
	bHasSubmittedLoadout = Clean.Weapons.Num() > 0;
	UE_LOG(LogTemp, Log, TEXT("[PCMode] Auth loadout weapons=%d stripAtt=%d"), Clean.Weapons.Num(), bStripAttachments ? 1 : 0);
	if (bHasSubmittedLoadout && IsValid(GetPawn()))
	{
		if (UCombatComponent* CombatComp = GetPawn()->FindComponentByClass<UCombatComponent>())
		{
			CombatComp->RebuildInventory();
		}
	}
}

void AShooterPlayerController::PickFaction(int32 Faction)
{
	Server_PickFaction(static_cast<uint8>(Faction <= 0 ? 0 : 1));
}

void AShooterPlayerController::Server_PickFaction_Implementation(uint8 FactionByte)
{
	APoliceCriminalGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APoliceCriminalGameMode>() : nullptr;
	if (!GM)
	{
		// Soft fallback: set team + look even outside mode (range PIE)
		if (AShooterPlayerState* PS = GetPlayerState<AShooterPlayerState>())
		{
			PS->SetTeamId(FactionByte == 0 ? PoliceCriminalTeams::Police : PoliceCriminalTeams::Criminal);
		}
		if (AShooterCharacter* Char = Cast<AShooterCharacter>(GetPawn()))
		{
			Char->SetFactionLookLocal(FactionByte == 0 ? ECharacterLookFaction::Police : ECharacterLookFaction::Criminal);
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Yellow, TEXT("PickFaction: 当前不是警匪 GameMode，仅设置队伍/外观"));
		}
		return;
	}
	GM->Auth_AssignFaction(this, FactionByte == 0 ? EPoliceCriminalFaction::Police : EPoliceCriminalFaction::Criminal);
}

static TSubclassOf<AWeapon> FPS_ResolveNamedWeapon(const TArray<TSubclassOf<AWeapon>>& Catalog, const FString& Name)
{
	const FString Key = Name.TrimStartAndEnd().ToLower();
	if (Key.IsEmpty()) return nullptr;
	for (TSubclassOf<AWeapon> C : Catalog)
	{
		if (!C) continue;
		const FString N = C->GetName().ToLower();
		if (N.Contains(Key)) return C;
	}
	// aliases
	auto MatchAlias = [&](const TCHAR* Alias, const TCHAR* Needle) -> TSubclassOf<AWeapon>
	{
		if (!Key.Equals(Alias) && !Key.Contains(Alias)) return nullptr;
		for (TSubclassOf<AWeapon> C : Catalog)
		{
			if (C && C->GetName().ToLower().Contains(Needle)) return C;
		}
		return nullptr;
	};
	if (TSubclassOf<AWeapon> W = MatchAlias(TEXT("ak"), TEXT("ak"))) return W;
	if (TSubclassOf<AWeapon> W = MatchAlias(TEXT("glock"), TEXT("pistol"))) return W;
	if (TSubclassOf<AWeapon> W = MatchAlias(TEXT("pistol"), TEXT("pistol"))) return W;
	if (TSubclassOf<AWeapon> W = MatchAlias(TEXT("deagle"), TEXT("deagle"))) return W;
	if (TSubclassOf<AWeapon> W = MatchAlias(TEXT("uzi"), TEXT("pcc"))) return W;
	if (TSubclassOf<AWeapon> W = MatchAlias(TEXT("pcc"), TEXT("pcc"))) return W;
	if (TSubclassOf<AWeapon> W = MatchAlias(TEXT("rar"), TEXT("rar"))) return W;
	if (TSubclassOf<AWeapon> W = MatchAlias(TEXT("x24"), TEXT("x24"))) return W;
	if (TSubclassOf<AWeapon> W = MatchAlias(TEXT("knife"), TEXT("knife"))) return W;
	return nullptr;
}

void AShooterPlayerController::VanPick(FString WeaponName)
{
	Server_VanPickByName(WeaponName);
}

void AShooterPlayerController::Server_VanPickByName_Implementation(const FString& WeaponName)
{
	APoliceCriminalGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APoliceCriminalGameMode>() : nullptr;
	if (!GM)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PCMode] VanPick needs PoliceCriminalGameMode"));
		return;
	}
	const FString Key = WeaponName.TrimStartAndEnd().ToLower();
	if (Key.IsEmpty() || Key == TEXT("mvp") || Key == TEXT("kit") || Key == TEXT("default"))
	{
		GM->Auth_ApplyMvpLoadout(this);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange, TEXT("面包车 MVP：AK+手枪+燃烧瓶/手雷"));
		return;
	}
	TSubclassOf<AWeapon> W = FPS_ResolveNamedWeapon(GM->GetResolvedCriminalWeapons(), WeaponName);
	if (!W)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PCMode] VanPick unknown '%s'"), *WeaponName);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Red, FString::Printf(TEXT("VanPick 未知: %s (AK/Pistol 或 mvp)"), *WeaponName));
		return;
	}
	GM->Auth_ApplyVanLoadout(this, W);
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange, TEXT("面包车 MVP 配装已应用（含副武器+投掷）"));
}

void AShooterPlayerController::ArmoryPick(FString WeaponName)
{
	Server_ArmoryPickByName(WeaponName);
}

void AShooterPlayerController::Server_ArmoryPickByName_Implementation(const FString& WeaponName)
{
	APoliceCriminalGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APoliceCriminalGameMode>() : nullptr;
	if (!GM)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PCMode] ArmoryPick needs PoliceCriminalGameMode"));
		return;
	}
	const FString Key = WeaponName.TrimStartAndEnd().ToLower();
	if (Key.IsEmpty() || Key == TEXT("mvp") || Key == TEXT("kit") || Key == TEXT("default"))
	{
		GM->Auth_ApplyMvpLoadout(this);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, TEXT("军械库 MVP：RAR+X24+闪光/手雷"));
		return;
	}
	TSubclassOf<AWeapon> W = FPS_ResolveNamedWeapon(GM->GetResolvedPoliceWeapons(), WeaponName);
	if (!W)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PCMode] ArmoryPick unknown '%s'"), *WeaponName);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Red, FString::Printf(TEXT("ArmoryPick 未知: %s (RAR/X24 或 mvp)"), *WeaponName));
		return;
	}
	GM->Auth_ApplyArmoryLoadout(this, W, true);
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, TEXT("军械库 MVP 配装已应用（含副武器+投掷）"));
}

void AShooterPlayerController::SkipSetup()
{
	Server_SkipSetup();
}

void AShooterPlayerController::Server_SkipSetup_Implementation()
{
	if (APoliceCriminalGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APoliceCriminalGameMode>() : nullptr)
	{
		GM->Auth_SkipSetup();
	}
}

void AShooterPlayerController::SpawnTestHostage()
{
	if (HasAuthority()) Server_SpawnTestHostage_Implementation();
	else Server_SpawnTestHostage();
}

void AShooterPlayerController::Server_SpawnTestHostage_Implementation()
{
	APawn* MyPawn = GetPawn();
	if (!IsValid(MyPawn) || !GetWorld()) return;
	const FVector Loc = MyPawn->GetActorLocation() + MyPawn->GetActorForwardVector() * 200.f;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AHostageActor* H = GetWorld()->SpawnActor<AHostageActor>(AHostageActor::StaticClass(), Loc, MyPawn->GetActorRotation(), Params);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Cyan,
			H ? TEXT("已生成人质占位（Tag=Hostage，F 跟随 stub，可受伤）") : TEXT("人质生成失败"));
	}
}


void AShooterPlayerController::EnsureWatchInput()
{
	if (!IsLocalController()) return;
	if (!WatchAction)
	{
		WatchAction = NewObject<UInputAction>(this, TEXT("IA_Watch_Runtime"));
		WatchAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!WatchIMC)
	{
		WatchIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Watch_Runtime"));
		// Map T; Input_CheckWatch requires Alt held (same pattern as Alt+K NVG).
		WatchIMC->MapKey(WatchAction, EKeys::T);
	}
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(Subsystem) && WatchIMC && !Subsystem->HasMappingContext(WatchIMC))
	{
		Subsystem->AddMappingContext(WatchIMC, 2);
	}
}

void AShooterPlayerController::Input_CheckWatch()
{
	if (!bPawnAlive) return;
	if (!IsInputKeyDown(EKeys::LeftAlt) && !IsInputKeyDown(EKeys::RightAlt))
	{
		return;
	}
	CheckWatch();
}

void AShooterPlayerController::CheckWatch()
{
	if (AShooterCharacter* Char = Cast<AShooterCharacter>(GetPawn()))
	{
		Char->CheckWatch();
	}
}

void AShooterPlayerController::GrantRadar(int32 Enabled)
{
	AShooterCharacter* Char = Cast<AShooterCharacter>(GetPawn());
	if (!IsValid(Char))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Radar] No ShooterCharacter pawn"));
		return;
	}
	const bool bOn = Enabled != 0;
	Char->SetHasRadar(bOn);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
			FString::Printf(TEXT("Radar -> %s (GrantRadar 0/1)"), bOn ? TEXT("ON") : TEXT("OFF")));
	}
}

void AShooterPlayerController::DebugBarrel(int32 Enabled)
{
	AShooterCharacter* Char = Cast<AShooterCharacter>(GetPawn());
	if (!IsValid(Char))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Barrel] No ShooterCharacter pawn"));
		return;
	}
	Char->SetDebugWeaponObstacle(Enabled != 0);
}

void AShooterPlayerController::MarkBlood(int32 Steps)
{
	AShooterCharacter* Char = Cast<AShooterCharacter>(GetPawn());
	if (!IsValid(Char))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Blood] No ShooterCharacter pawn"));
		return;
	}
	Char->MarkBloodyFeet(Steps > 0 ? Steps : 10);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("BloodyFeet marked — walk to leave prints"));
	}
}



// ---------------- MvpRound debug ----------------
void AShooterPlayerController::KillSelf() { Server_MvpDebug(0, 0.f); }
void AShooterPlayerController::ForceWin(int32 Team) { Server_MvpDebug(1, (float)Team); }
void AShooterPlayerController::SetRoundTime(float Seconds) { Server_MvpDebug(2, Seconds); }

void AShooterPlayerController::Server_MvpDebug_Implementation(int32 Op, float Value)
{
	APoliceCriminalGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APoliceCriminalGameMode>() : nullptr;
	if (!GM) return;
	if (Op == 0) GM->Auth_DebugKill(this);
	else if (Op == 1) GM->Auth_DebugForceWin((uint8)FMath::Clamp((int32)Value, 0, 255));
	else if (Op == 2) GM->Auth_DebugSetRemaining(FMath::Max(Value, 1.f));
}
void AShooterPlayerController::TravelToMatch(FString MapName)
{
	Server_TravelToMatch(MapName);
}

void AShooterPlayerController::Server_TravelToMatch_Implementation(const FString& MapName)
{
	if (APoliceCriminalGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APoliceCriminalGameMode>() : nullptr)
	{
		GM->Auth_TravelToMatchMap(MapName);
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[PCMode] TravelToMatch needs PoliceCriminalGameMode"));

}





