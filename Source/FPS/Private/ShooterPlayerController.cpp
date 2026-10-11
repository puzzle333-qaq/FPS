#include "ShooterPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Stats/StatsComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/LocalPlayer.h"

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
	if (!IsValid(GetCharacter())) return;
	UCharacterMovementComponent* CMC = GetCharacter()->GetCharacterMovement();
	if (!IsValid(CMC)) return;

	if (bIsSprinting)      CMC->MaxWalkSpeed = SprintSpeed;
	else if (bIsWalking)   CMC->MaxWalkSpeed = WalkSpeed;
	else                   CMC->MaxWalkSpeed = RunSpeed;

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