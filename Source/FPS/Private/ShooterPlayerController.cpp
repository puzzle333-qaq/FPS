#include "ShooterPlayerController.h"
#include "ShooterCharacter.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Stats/StatsComponent.h"
#include "CombatComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/LocalPlayer.h"
#include "Ui/AttachmentMenu.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "ShooterGameInstance.h"
#include "Ui/PauseMenuScreen.h"
#include "Game/SearchDestroyGameState.h"
#include "ShooterGameModeBase.h"
#include "Ui/ModeHudWidget.h"
#include "Ui/BuyShopScreen.h"
#include "Ui/MatchHUD.h"
#include "Weapon/Weapon.h"
#include "Weapon/AttachmentData.h"
#include "Game/ShooterGameState.h"
#include "GameFramework/GameStateBase.h"
#include "Game/SearchDestroyGameMode.h"
#include "Game/WeaponPickup.h"
#include "Interface/Interactable.h"
#include "Ui/ScoreboardScreen.h"
#include "EngineUtils.h"

AShooterPlayerController::AShooterPlayerController()
{
	bReplicates = true;
	bPawnAlive = true;
}

void AShooterPlayerController::StopSprinting()
{
	bIsSprinting = false;
	UpdateMovementSpeed();
}

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 从主菜单跳过来时 UI 输入和鼠标会跟着视口保留，进局恢复游戏输入
	if (IsLocalController())
	{
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
		EnsureMatchHUD();
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(Subsystem) && ShooterIMC)
	{
		Subsystem->AddMappingContext(ShooterIMC, 0);
	}
	EnsureBuyShopInput();

	// 购买阶段结束自动关商店
	if (IsLocalController())
	{
		if (UWorld* World = GetWorld())
		{
			if (ASearchDestroyGameState* SDGS = World->GetGameState<ASearchDestroyGameState>())
			{
				SDGS->OnRoundPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandleBuyPhaseChanged);
			}
			World->GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSetForBuy);
		}
	}
	EnsureCrouchInput();
	EnsurePauseInput();
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

	EnsureBuyShopInput();

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
	// Legacy IA_Crouch (IMC_Shooter: Left Ctrl toggle) is NOT bound when the new C-toggle / Ctrl-hold actions exist,
	// so Ctrl can never double-trigger a toggle. (Runtime IMC priority 2 also consumes Ctrl/C.)
	if (CrouchAction && !CrouchToggleAction && !CrouchHoldAction)
	{
		// Legacy IA_Crouch (IMC_Shooter). Its key is consumed if it is C or Left Ctrl (runtime IMC priority 2).
		ShooterInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &ThisClass::Input_Crouch);
	}
	if (CrouchToggleAction)
	{
		ShooterInputComponent->BindAction(CrouchToggleAction, ETriggerEvent::Started, this, &ThisClass::Input_Crouch);
	}
	if (CrouchHoldAction)
	{
		ShooterInputComponent->BindAction(CrouchHoldAction, ETriggerEvent::Started, this, &ThisClass::Input_CrouchHold_Started);
		ShooterInputComponent->BindAction(CrouchHoldAction, ETriggerEvent::Completed, this, &ThisClass::Input_CrouchHold_Completed);
	}
	if (SprintAction)
	{
		ShooterInputComponent->BindAction(SprintAction, ETriggerEvent::Started,this, &ThisClass::Input_Sprint);
	}
	if (WalkAction)
	{
		ShooterInputComponent->BindAction(WalkAction, ETriggerEvent::Started, this, &ThisClass::Input_Walk);
	}
	if (FlashlightAction)
	{
		ShooterInputComponent->BindAction(FlashlightAction, ETriggerEvent::Started, this, &ThisClass::Input_ToggleFlashlight);
	}
	EnsureInteractInput();
	if (InteractAction)
	{
		ShooterInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ThisClass::Input_Interact_Started);
		ShooterInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &ThisClass::Input_Interact_Completed);
		ShooterInputComponent->BindAction(InteractAction, ETriggerEvent::Canceled, this, &ThisClass::Input_Interact_Completed);
	}
	if (ToggleAttachmentMenuAction)
	{
		ShooterInputComponent->BindAction(ToggleAttachmentMenuAction, ETriggerEvent::Started, this, &ThisClass::Input_ToggleAttachmentMenu);
	}
	EnsurePauseInput();
	if (PauseMenuAction)
	{
		ShooterInputComponent->BindAction(PauseMenuAction, ETriggerEvent::Started, this, &ThisClass::Input_TogglePauseMenu);
	}
	EnsureBuyShopInput();
}

void AShooterPlayerController::Input_ToggleFlashlight()
{
	if (!bPawnAlive)return;
	// 控制器只管“按了键”，真正开关灯交给 CombatComponent -> 服务器 -> 当前武器
	if (UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetPawn()))
	{
		Combat->Initiate_ToggleFlashlight();
	}
}

void AShooterPlayerController::Input_ToggleAttachmentMenu()
{
	if (!bPawnAlive)return;
	// 爆破模式 B 是商店，配件菜单不开
	if (GetWorld() && GetWorld()->GetGameState<ASearchDestroyGameState>()) return;
	// UI 只存在于本地玩家。整备界面开着时 B 不再叠一层菜单。
	if (!IsLocalController() || !AttachmentMenuClass) return;
	if (IsValid(BuyShop) && BuyShop->IsInViewport()) return;

	if (!IsValid(AttachmentMenu))
	{
		AttachmentMenu = CreateWidget<UAttachmentMenu>(this, AttachmentMenuClass);
		if (!IsValid(AttachmentMenu)) return;
	}

	if (AttachmentMenu->IsInViewport())
	{
		// 关闭：回到纯游戏输入，隐藏鼠标，恢复转视角
		AttachmentMenu->RemoveFromParent();
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
		SetIgnoreLookInput(false);
	}
	else
	{
		// 松开扳机，避免菜单开着时连发还在继续
		if (UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetPawn()))
		{
			Combat->Initiate_FireWeapon_Released();
		}

		// 打开：游戏+UI（B 仍能关掉），显示鼠标，鼠标移动不再转视角
		AttachmentMenu->AddToViewport(10);
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(AttachmentMenu->TakeWidget());
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		SetShowMouseCursor(true);
		SetIgnoreLookInput(true);
	}
}

void AShooterPlayerController::EnsureCrouchInput()
{
	if (!bPawnAlive)return;
	const bool bNeedToggle = !CrouchToggleAction;
	const bool bNeedHold = !CrouchHoldAction;
	if (bNeedToggle || bNeedHold)
	{
		if (!CrouchRuntimeIMC)
		{
			CrouchRuntimeIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Crouch_Runtime"));
		}
		if (bNeedToggle)
		{
			CrouchToggleAction = NewObject<UInputAction>(this, TEXT("IA_CrouchToggle_Runtime"));
			CrouchToggleAction->ValueType = EInputActionValueType::Boolean;
			CrouchRuntimeIMC->MapKey(CrouchToggleAction, EKeys::C);
		}
		if (bNeedHold)
		{
			CrouchHoldAction = NewObject<UInputAction>(this, TEXT("IA_CrouchHold_Runtime"));
			CrouchHoldAction->ValueType = EInputActionValueType::Boolean;
			CrouchRuntimeIMC->MapKey(CrouchHoldAction, EKeys::LeftControl);
		}
	}
	if (!CrouchRuntimeIMC) return;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(Subsystem) && !Subsystem->HasMappingContext(CrouchRuntimeIMC))
	{
		Subsystem->AddMappingContext(CrouchRuntimeIMC, 2);
	}
}

void AShooterPlayerController::Input_CrouchHold_Started()
{
	if (!bPawnAlive)return;
	if (IsMovementLocked()) return;
	bCrouchHeld = true;
	ACharacter* MyChar = GetCharacter();
	if (!IsValid(MyChar)) return;
	if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(MyChar))
	{
		if (Shooter->IsProne())
		{
			Shooter->RequestSetProne(false, true);   // prone -> crouch
			return;
		}
		if (Shooter->IsSliding())
		{
			Shooter->SetSlideEndCrouch(true);
			return;
		}
		if (Shooter->TryStartSlide())
		{
			Shooter->SetSlideEndCrouch(true);
			return;
		}
	}
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
	if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(MyChar))
	{
		if (Shooter->IsProne()) return;
		if (Shooter->IsSliding())
		{
			Shooter->SetSlideEndCrouch(false);   // stand when the slide ends
			return;
		}
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
	if (!IsValid(GetCharacter())) return;
	if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetCharacter()); IsValid(Shooter))
	{
		// 趴着按下蹲 = 起身到蹲姿
		if (Shooter->IsProne())
		{
			Shooter->RequestSetProne(false, true);
			return;
		}
		if (Shooter->IsSliding())
		{
			return;
		}
		// 高速按 C = 滑铲，结束后保持蹲姿
		if (Shooter->TryStartSlide())
		{
			Shooter->SetSlideEndCrouch(true);
			return;
		}
	}
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

	// 滑铲中按跳 = 取消滑铲并站起
	if (AShooterCharacter* SlideShooter = Cast<AShooterCharacter>(GetCharacter()); IsValid(SlideShooter) && SlideShooter->IsSliding())
	{
		SlideShooter->CancelSlideToStand();
		return;
	}

	// 趴着按跳 = 站起来（不跳）
	if (AShooterCharacter* ProneShooter = Cast<AShooterCharacter>(GetCharacter()); IsValid(ProneShooter) && ProneShooter->IsProne())
	{
		ProneShooter->RequestSetProne(false, false);
		return;
	}

	if (CMC->bWantsToCrouch)
	{
		CMC->bWantsToCrouch = false;
	}
	else
	{
		GetCharacter()->Jump();
	}
}

void AShooterPlayerController::Input_Sprint()
{
	if (!bPawnAlive)return;
	if (IsMovementLocked()) return;

	const UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetPawn());
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
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();

	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// 滑铲中不接受移动输入（保持惯性，靠 CMC 线性减速）
	if (const AShooterCharacter* SlideChar = Cast<AShooterCharacter>(GetPawn()); IsValid(SlideChar) && SlideChar->IsSliding())
	{
		return;
	}

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

	AddYawInput(InputAxisVector.X);
	AddPitchInput(InputAxisVector.Y);
}

void AShooterPlayerController::UpdateMovementSpeed()
{
	if (!bPawnAlive)return;
	UStatsComponent* Stats = GetStats();
	if (bIsSprinting && Stats && Stats->Stamina <= 0.f)
	{
		bIsSprinting = false;
	}

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
	const AShooterCharacter* ProneChar = Cast<AShooterCharacter>(MyCharacter);
	const bool bProne = IsValid(ProneChar) && ProneChar->IsProne();
	const bool bSliding = IsValid(ProneChar) && ProneChar->IsSliding();
	if (bSliding) bIsSprinting = false;

	// 下蹲、趴下、开镜或换弹时不能冲刺
	if (bIsSprinting && (bAiming || bCrouched || bProne || bReloading))
	{
		bIsSprinting = false;
	}

	// 基础速度：冲刺或正常跑；静步/开镜作为倍率乘上去
	float Speed = bIsSprinting ? SprintSpeed : RunSpeed;
	if (bIsWalking) Speed *= WalkSpeedMultiplier;
	if (bAiming)    Speed *= AimSpeedMultiplier;

	// 站立时角色用 MaxWalkSpeed，蹲下时用 MaxWalkSpeedCrouched，两个都设好
	CMC->MaxWalkSpeed = Speed;
	// 趴下复用 CMC 的蹲伏（更矮的胶囊），所以趴行速度也写在 MaxWalkSpeedCrouched
	CMC->MaxWalkSpeedCrouched = bProne ? Speed * ProneSpeedMultiplier : Speed * CrouchSpeedMultiplier;
	if (bSliding)
	{
		// 滑铲时速度高于蹲走上限：放宽上限，减速交给 BrakingDecelerationWalking
		CMC->MaxWalkSpeedCrouched = SprintSpeed * 1.5f;
	}

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
	// 这个函数在服务器上被调用
	bIsSprinting = false;
	ApplyMovementSpeed();

	// 如果玩家是远程客户端，也要通知他退出冲刺
	if (!IsLocalController())
	{
		Client_StopSprint();
	}
}

void AShooterPlayerController::Server_SetMovementState_Implementation(bool bNewSprinting, bool bNewWalking)
{
	const UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetPawn());
	if (bNewSprinting && IsValid(Combat) && Combat->IsReloading())
	{
		bNewSprinting = false;
	}
	bIsSprinting = bNewSprinting;
	bIsWalking = bNewWalking;
	UpdateMovementSpeed();   // 在服务器上 HasAuthority 为 true，不会再发 RPC
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
	const bool bBombMode = MatchGS && MatchGS->GetMatchMode() == EShooterGameMode::SearchDestroy;
	if (bBombMode)
	{
		// 爆破不用改枪台预设：默认只有 Glock18，长枪靠商店
		UE_LOG(LogTemp, Log, TEXT("[Shop] Bomb mode: ignoring gunsmith loadout for %s"), *GetName());
		return;
	}
	// 爆破：进局只带手枪/刀/投掷，长枪靠商店买。团战/摸金仍用改枪台预设。
	const int32 MaxLong = bBombMode ? 0 : ((!MatchGS || !MatchGS->HasMatchHeader() || MatchGS->GetMatchMode() == EShooterGameMode::Extraction) ? 2 : 1);
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
	if (!IsLocalController() || IsValid(MatchHUD)) return;
	if (!MatchHUDClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MatchHUD] MatchHUDClass is not set on %s. Assign WBP_MatchHUD."), *GetClass()->GetName());
		return;
	}
	MatchHUD = CreateWidget<UMatchHUD>(this, MatchHUDClass);
	if (IsValid(MatchHUD))
	{
		MatchHUD->AddToViewport(5);
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
	EnsureBuyShopInput();
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
}

void AShooterPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	EnsureBuyShopInput();
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StopCameraFade();
		PlayerCameraManager->SetManualCameraFade(0.f, FLinearColor::Black, false);
	}
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
	return GS && GS->IsMovementLocked();
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
	// 优先：准星对着的枪/门（IInteractable）；否则走爆破拾取/安放/拆除（按住）
	if (bPawnAlive)
	{
		if (AActor* Target = TraceInteractable())
		{
			Server_Interact(Target);
			return;
		}
	}
	if (!bPawnAlive) return;
	Server_BeginBombInteract();
}

void AShooterPlayerController::Input_Interact_Completed()
{
	Server_EndBombInteract();
}

void AShooterPlayerController::Server_BeginBombInteract_Implementation()
{
	if (ASearchDestroyGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASearchDestroyGameMode>() : nullptr)
	{
		GM->BeginBombInteract(this);
	}
}

void AShooterPlayerController::Server_EndBombInteract_Implementation()
{
	if (ASearchDestroyGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASearchDestroyGameMode>() : nullptr)
	{
		GM->EndBombInteract(this);
	}
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
		GI->GoToMainMenu();
	}
}


void AShooterPlayerController::EnsureBuyShopInput()
{
	if (!IsLocalController()) return;
	EnsureScoreboardInput(); // Tab 计分板所有模式都要
	if (!BuyShopWidgetClass) return; // TDM 等不填商店类则不绑 B

	if (!BuyShopAction)
	{
		BuyShopAction = NewObject<UInputAction>(this, TEXT("IA_BuyShop_Runtime"));
		BuyShopAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!BuyShopIMC)
	{
		BuyShopIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_BuyShop_Runtime"));
		BuyShopIMC->MapKey(BuyShopAction, EKeys::B);
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(Subsystem) && !Subsystem->HasMappingContext(BuyShopIMC))
	{
		Subsystem->AddMappingContext(BuyShopIMC, 1);
		UE_LOG(LogTemp, Log, TEXT("[Shop] Tab mapped (local=%d)"), IsLocalController() ? 1 : 0);
	}
	if (!bBuyShopBound)
	{
		if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
		{
			EIC->BindAction(BuyShopAction, ETriggerEvent::Started, this, &ThisClass::Input_ToggleBuyShop);
			bBuyShopBound = true;
			UE_LOG(LogTemp, Log, TEXT("[Shop] Tab action bound"));
		}
	}
}

void AShooterPlayerController::Input_ToggleBuyShop()
{
	const ASearchDestroyGameState* GS = GetWorld() ? GetWorld()->GetGameState<ASearchDestroyGameState>() : nullptr;
	UE_LOG(LogTemp, Log, TEXT("[Shop] Tab: class=%d alive=%d matchOver=%d pause=%d buyPhase=%d phase=%d open=%d"),
		BuyShopWidgetClass ? 1 : 0, bPawnAlive ? 1 : 0, bMatchOver ? 1 : 0, IsPauseMenuOpen() ? 1 : 0,
		(GS && GS->IsBuyPhase()) ? 1 : 0, GS ? (int32)GS->GetRoundPhase() : -1, IsBuyShopOpen() ? 1 : 0);
	ToggleBuyShop();
}

bool AShooterPlayerController::CanOpenBuyShop() const
{
	if (!BuyShopWidgetClass || !bPawnAlive || bMatchOver || IsPauseMenuOpen()) return false;
	const UWorld* World = GetWorld();
	const ASearchDestroyGameState* GS = World ? World->GetGameState<ASearchDestroyGameState>() : nullptr;
	return GS && GS->IsBuyPhase();
}

bool AShooterPlayerController::IsBuyShopOpen() const
{
	return IsValid(BuyShop) && BuyShop->IsInViewport();
}

void AShooterPlayerController::ToggleBuyShop()
{
	if (IsBuyShopOpen())
	{
		CloseBuyShop();
	}
	else
	{
		OpenBuyShop();
	}
}

void AShooterPlayerController::OpenBuyShop()
{
	if (!IsLocalController() || !CanOpenBuyShop()) return;

	if (IsValid(PauseMenu) && PauseMenu->IsInViewport())
	{
		ClosePauseMenu();
	}
	if (IsValid(AttachmentMenu) && AttachmentMenu->IsInViewport())
	{
		Input_ToggleAttachmentMenu();
	}
	if (UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetPawn()))
	{
		Combat->Initiate_FireWeapon_Released();
	}

	if (!IsValid(BuyShop))
	{
		BuyShop = CreateWidget<UBuyShopScreen>(this, BuyShopWidgetClass);
		if (!IsValid(BuyShop)) return;
	}

	if (!BuyShop->IsInViewport())
	{
		BuyShop->AddToViewport(25);
	}
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(BuyShop->TakeWidget());
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
	SetIgnoreLookInput(true);
	SetIgnoreMoveInput(true);
}

void AShooterPlayerController::CloseBuyShop()
{
	if (!IsLocalController()) return;
	if (IsValid(BuyShop) && BuyShop->IsInViewport())
	{
		BuyShop->RemoveFromParent();
	}
	if (!IsPauseMenuOpen())
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
		SetIgnoreLookInput(false);
		SetIgnoreMoveInput(false);
	}
}

bool AShooterPlayerController::RequestBuyShopItem(int32 CatalogIndex)
{
	if (!CanOpenBuyShop()) return false;
	Server_BuyShopItem(CatalogIndex);
	return true;
}

TArray<FBuyShopItem> AShooterPlayerController::GetCachedBuyCatalog() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const ASearchDestroyGameState* GS = World->GetGameState<ASearchDestroyGameState>())
		{
			return GS->GetBuyCatalog();
		}
	}
	return TArray<FBuyShopItem>();
}

void AShooterPlayerController::Server_BuyShopItem_Implementation(int32 CatalogIndex)
{
	if (ASearchDestroyGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASearchDestroyGameMode>() : nullptr)
	{
		GM->Auth_TryBuyItem(this, CatalogIndex);
	}
}

bool AShooterPlayerController::Auth_ApplyBoughtWeapon(const FWeaponPreset& Preset)
{
	if (!HasAuthority() || !Preset.WeaponClass) return false;

	const AWeapon* CDO = Preset.WeaponClass->GetDefaultObject<AWeapon>();
	if (!CDO) return false;

	if (!bHasSubmittedLoadout)
	{
		SubmittedLoadout = FLoadoutPreset();
		bHasSubmittedLoadout = true;
	}

	if (CDO->WeaponKind == EWeaponKind::LongGun)
	{
		// 替换第一把长枪；没有则插入
		bool bReplaced = false;
		for (FWeaponPreset& Existing : SubmittedLoadout.Weapons)
		{
			const AWeapon* ExistingCDO = Existing.WeaponClass ? Existing.WeaponClass->GetDefaultObject<AWeapon>() : nullptr;
			if (ExistingCDO && ExistingCDO->WeaponKind == EWeaponKind::LongGun)
			{
				Existing = Preset;
				bReplaced = true;
				break;
			}
		}
		if (!bReplaced)
		{
			SubmittedLoadout.Weapons.Insert(Preset, 0);
		}
	}
	else
	{
		// 手枪/刀等同种替换
		bool bReplaced = false;
		for (FWeaponPreset& Existing : SubmittedLoadout.Weapons)
		{
			const AWeapon* ExistingCDO = Existing.WeaponClass ? Existing.WeaponClass->GetDefaultObject<AWeapon>() : nullptr;
			if (ExistingCDO && ExistingCDO->WeaponKind == CDO->WeaponKind)
			{
				Existing = Preset;
				bReplaced = true;
				break;
			}
		}
		if (!bReplaced)
		{
			SubmittedLoadout.Weapons.Add(Preset);
		}
	}

	if (UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetPawn()))
	{
		Combat->RebuildInventory();
	}
	return true;
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

void AShooterPlayerController::HandleGameStateSetForBuy(AGameStateBase* NewGameState)
{
	if (ASearchDestroyGameState* SDGS = Cast<ASearchDestroyGameState>(NewGameState))
	{
		SDGS->OnRoundPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandleBuyPhaseChanged);
	}
}

void AShooterPlayerController::HandleBuyPhaseChanged(EBombRoundPhase NewPhase)
{
	if (NewPhase != EBombRoundPhase::Freeze && IsBuyShopOpen())
	{
		CloseBuyShop();
	}
}

// ================= F 交互 / 武器掉落 =================

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
	if (!World->SweepSingleByChannel(Hit, ViewLoc, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(8.f), Q))
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
	if (const ASearchDestroyGameState* SDGS = GetWorld() ? GetWorld()->GetGameState<ASearchDestroyGameState>() : nullptr)
	{
		switch (SDGS->GetPromptForPawn(GetPawn()))
		{
		case EBombPrompt::Pickup: return NSLOCTEXT("Interact", "BombPickup", "按 F 拾取炸弹");
		case EBombPrompt::Plant:  return NSLOCTEXT("Interact", "BombPlant", "按住 F 安放炸弹");
		case EBombPrompt::Defuse: return NSLOCTEXT("Interact", "BombDefuse", "按住 F 拆除炸弹");
		default: break;
		}
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

AWeaponPickup* AShooterPlayerController::Auth_SpawnPickupFromWeapon(AWeapon* Weapon, APawn* FromPawn, const FVector& Offset)
{
	if (!IsValid(Weapon) || !IsValid(FromPawn)) return nullptr;
	FWeaponPreset Preset;
	Preset.WeaponClass = Weapon->GetClass();
	for (UAttachmentData* Att : Weapon->GetEquippedAttachments())
	{
		if (Att) Preset.Attachments.Add(Att);
	}
	const FVector Loc = FromPawn->GetActorLocation() + FromPawn->GetActorRotation().RotateVector(Offset);
	return AWeaponPickup::SpawnDropped(GetWorld(), WeaponPickupClass, Preset, Loc, FromPawn->GetActorRotation().Yaw + FMath::FRandRange(-40.f, 40.f), FromPawn);
}

void AShooterPlayerController::Auth_DropWeaponsOnDeath(APawn* DeadPawn)
{
	if (!HasAuthority() || !IsValid(DeadPawn)) return;
	int32 Index = 0;
	for (TActorIterator<AWeapon> It(GetWorld()); It; ++It)
	{
		AWeapon* W = *It;
		if (!IsValid(W) || W->GetOwner() != DeadPawn) continue;
		if (W->WeaponKind != EWeaponKind::LongGun && W->WeaponKind != EWeaponKind::Pistol) continue;
		const FVector Offset(40.f, (Index == 0 ? -25.f : 25.f), 0.f);
		if (Auth_SpawnPickupFromWeapon(W, DeadPawn, Offset))
		{
			W->SetActorHiddenInGame(true);
			++Index;
		}
	}
	// 死了买的枪就没了：下回合只拿默认手枪 + 刀（CS 规则）
	if (bHasSubmittedLoadout)
	{
		SubmittedLoadout.Weapons.RemoveAll([](const FWeaponPreset& P)
		{
			const AWeapon* CDO = P.WeaponClass ? P.WeaponClass->GetDefaultObject<AWeapon>() : nullptr;
			return CDO && (CDO->WeaponKind == EWeaponKind::LongGun || CDO->WeaponKind == EWeaponKind::Pistol);
		});
	}
}

bool AShooterPlayerController::Auth_PickupWeapon(AWeaponPickup* Pickup)
{
	APawn* MyPawn = GetPawn();
	if (!HasAuthority() || !bPawnAlive || !IsValid(MyPawn) || !IsValid(Pickup)) return false;
	const TSubclassOf<AWeapon> NewClass = Pickup->GetWeaponClass();
	const AWeapon* NewCDO = NewClass ? NewClass->GetDefaultObject<AWeapon>() : nullptr;
	if (!NewCDO || (NewCDO->WeaponKind != EWeaponKind::LongGun && NewCDO->WeaponKind != EWeaponKind::Pistol)) return false;

	// 同槽位旧枪丢在地上
	for (TActorIterator<AWeapon> It(GetWorld()); It; ++It)
	{
		AWeapon* W = *It;
		if (!IsValid(W) || W->GetOwner() != MyPawn || W->IsHidden()) continue;
		if (W->WeaponKind != NewCDO->WeaponKind) continue;
		Auth_SpawnPickupFromWeapon(W, MyPawn, FVector(60.f, 0.f, 0.f));
		break;
	}
	// 写进 SubmittedLoadout 并重建背包（注意：弹药会重置为满）
	const bool bOk = Auth_ApplyBoughtWeapon(Pickup->GetPreset());
	UE_LOG(LogTemp, Log, TEXT("[Interact] Pickup %s ok=%d"), *GetNameSafe(NewClass), bOk ? 1 : 0);
	return bOk;
}

// ================= 计分板（按住 Tab） =================

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
