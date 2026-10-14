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
#include "Ui/LoadoutScreen.h"
#include "InputAction.h"
#include "InputMappingContext.h"

AShooterPlayerController::AShooterPlayerController()
{
	bReplicates = true;
}

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(Subsystem) && ShooterIMC)
	{
		Subsystem->AddMappingContext(ShooterIMC, 0);
	}
	EnsureLoadoutInput();
}

void AShooterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	EnsureLoadoutInput();

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
	if (CrouchAction)
	{
		ShooterInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &ThisClass::Input_Crouch);
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
	if (ToggleAttachmentMenuAction)
	{
		ShooterInputComponent->BindAction(ToggleAttachmentMenuAction, ETriggerEvent::Started, this, &ThisClass::Input_ToggleAttachmentMenu);
	}
	if (LoadoutAction)
	{
		ShooterInputComponent->BindAction(LoadoutAction, ETriggerEvent::Started, this, &ThisClass::Input_ToggleLoadout);
	}
}

void AShooterPlayerController::EnsureLoadoutInput()
{
	if (!LoadoutAction)
	{
		LoadoutAction = NewObject<UInputAction>(this, TEXT("IA_Loadout_Runtime"));
		LoadoutAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!LoadoutIMC)
	{
		LoadoutIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Loadout_Runtime"));
		LoadoutIMC->MapKey(LoadoutAction, EKeys::Tab);
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(Subsystem) && !Subsystem->HasMappingContext(LoadoutIMC))
	{
		Subsystem->AddMappingContext(LoadoutIMC, 1);
	}
}

void AShooterPlayerController::Input_ToggleLoadout()
{
	if (!IsLocalController()) return;

	if (!IsValid(LoadoutScreen))
	{
		UClass* ScreenClass = LoadoutScreenClass ? LoadoutScreenClass.Get() : ULoadoutScreen::StaticClass();
		LoadoutScreen = CreateWidget<ULoadoutScreen>(this, ScreenClass);
		if (!IsValid(LoadoutScreen)) return;
	}

	if (LoadoutScreen->IsInViewport())
	{
		LoadoutScreen->RemoveFromParent();
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
		SetIgnoreLookInput(false);
		SetIgnoreMoveInput(false);
		return;
	}

	// 整备界面自己管鼠标。B 菜单如果还开着，先按它自己的方式关掉，避免视角锁定计数对不上。
	if (IsValid(AttachmentMenu) && AttachmentMenu->IsInViewport())
	{
		Input_ToggleAttachmentMenu();
	}

	if (UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetPawn()))
	{
		Combat->Initiate_FireWeapon_Released();
	}

	LoadoutScreen->AddToViewport(20);
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(LoadoutScreen->TakeWidget());
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
	SetIgnoreLookInput(true);
	SetIgnoreMoveInput(true);
}

void AShooterPlayerController::Input_ToggleFlashlight()
{
	// 控制器只管“按了键”，真正开关灯交给 CombatComponent -> 服务器 -> 当前武器
	if (UCombatComponent* Combat = UCombatComponent::FindCombatComponent(GetPawn()))
	{
		Combat->Initiate_ToggleFlashlight();
	}
}

void AShooterPlayerController::Input_ToggleAttachmentMenu()
{
	// UI 只存在于本地玩家。整备界面开着时 B 不再叠一层菜单。
	if (!IsLocalController() || !AttachmentMenuClass) return;
	if (IsValid(LoadoutScreen) && LoadoutScreen->IsInViewport()) return;

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

void AShooterPlayerController::Input_Crouch()
{
	if (!IsValid(GetCharacter())) return;
	if (UCharacterMovementComponent* CMC = GetCharacter()->GetCharacterMovement();IsValid(CMC))
	{
		CMC->bWantsToCrouch = !CMC->bWantsToCrouch;
	}

}

void AShooterPlayerController::Input_Jump()
{
	if (!IsValid(GetCharacter())) return;
	UCharacterMovementComponent* CMC = GetCharacter()->GetCharacterMovement();
	if (!IsValid(CMC)) return;

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
	bIsWalking = !bIsWalking;
	if (bIsWalking) bIsSprinting = false;   // 静步会打断冲刺
	UpdateMovementSpeed();
}




void AShooterPlayerController::Input_Move(const FInputActionValue& InputActionValue)
{
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
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();

	AddYawInput(InputAxisVector.X);
	AddPitchInput(InputAxisVector.Y);
}

void AShooterPlayerController::UpdateMovementSpeed()
{
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
	ACharacter* MyCharacter = GetCharacter();
	if (!IsValid(MyCharacter)) return;
	UCharacterMovementComponent* CMC = MyCharacter->GetCharacterMovement();
	if (!IsValid(CMC)) return;

	const UCombatComponent* Combat = UCombatComponent::FindCombatComponent(MyCharacter);
	const bool bAiming = IsValid(Combat) && Combat->bAiming;
	const bool bCrouched = MyCharacter->bIsCrouched;

	// 下蹲或开镜时不能冲刺
	if (bIsSprinting && (bAiming || bCrouched))
	{
		bIsSprinting = false;
	}

	// 基础速度：冲刺或正常跑；静步/开镜作为倍率乘上去
	float Speed = bIsSprinting ? SprintSpeed : RunSpeed;
	if (bIsWalking) Speed *= WalkSpeedMultiplier;
	if (bAiming)    Speed *= AimSpeedMultiplier;

	// 站立时角色用 MaxWalkSpeed，蹲下时用 MaxWalkSpeedCrouched，两个都设好
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
	bIsSprinting = bNewSprinting;
	bIsWalking = bNewWalking;
	UpdateMovementSpeed();   // 在服务器上 HasAuthority 为 true，不会再发 RPC
}

void AShooterPlayerController::Client_StopSprint_Implementation()
{
	bIsSprinting = false;
	ApplyMovementSpeed();
}