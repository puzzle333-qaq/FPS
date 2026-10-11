
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShooterPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
/**
 * 
 */
UCLASS()
class FPS_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AShooterPlayerController();

	// 只按当前状态重新算速度（不发 RPC），角色在开镜/下蹲变化时调用
	void RefreshMovementSpeed() { ApplyMovementSpeed(); }
	

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputMappingContext> ShooterIMC;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction>SprintAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction>WalkAction;

	void Input_Crouch();
	void Input_Jump();
	void Input_Sprint();
	void Input_Walk();
	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Look(const FInputActionValue& InputActionValue);

	UPROPERTY(EditAnywhere, Category = "FPS|Movement")
	float WalkSpeed = 250.f;

	UPROPERTY(EditAnywhere, Category = "FPS|Movement")
	float RunSpeed = 500.f;

	UPROPERTY(EditAnywhere, Category = "FPS|Movement")
	float SprintSpeed = 800.f;

	// 下面三个倍率会相乘叠加，例如 下蹲+静步+开镜 = 500 * 0.6 * 0.5 * 0.7
	UPROPERTY(EditAnywhere, Category = "FPS|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrouchSpeedMultiplier = 0.6f;

	UPROPERTY(EditAnywhere, Category = "FPS|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WalkSpeedMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, Category = "FPS|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimSpeedMultiplier = 0.7f;

	bool bIsSprinting = false;
	bool bIsWalking = false;

	void UpdateMovementSpeed();
	class UStatsComponent* GetStats();

	UFUNCTION()  
	void HandleStaminaDepleted();

	// 只改本地速度，不发 RPC
	void ApplyMovementSpeed();

	// 客户端 -> 服务器：告诉服务器我现在的移动状态
	UFUNCTION(Server, Reliable)
	void Server_SetMovementState(bool bNewSprinting, bool bNewWalking);

	// 服务器 -> 客户端：耐力耗尽，退出冲刺
	UFUNCTION(Client, Reliable)
	void Client_StopSprint();
};
