
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShooterPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UAttachmentMenu;
class ULoadoutScreen;
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
	bool IsSprinting() const { return bIsSprinting; }
	

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

	// 战术配件开关（例如 F 键）：手电和激光一起开关（第 12 步）
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> FlashlightAction;

	void Input_ToggleFlashlight();

	// 配件菜单开关（例如 B 键）
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> ToggleAttachmentMenuAction;

	// 填 WBP_AttachmentMenu
	UPROPERTY(EditAnywhere, Category = "FPS|UI")
	TSubclassOf<UAttachmentMenu> AttachmentMenuClass;

	// 只创建一次，之后反复加到屏幕/移除
	UPROPERTY(Transient)
	TObjectPtr<UAttachmentMenu> AttachmentMenu;

	void Input_ToggleAttachmentMenu();

	// Tab 打开的整备界面。按键在运行时注册，不写进 IMC_Shooter。
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadoutAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> LoadoutIMC;

	// Tab 整备界面用的控件类（填 WBP_LoadoutScreen）。为空用 C++ 默认布局
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Loadout")
	TSubclassOf<ULoadoutScreen> LoadoutScreenClass;

	UPROPERTY(Transient)
	TObjectPtr<ULoadoutScreen> LoadoutScreen;

	void EnsureLoadoutInput();
	void Input_ToggleLoadout();

	void Input_Crouch();          // C: toggle crouch / stand (prone -> crouch); slides when fast
	void Input_CrouchHold_Started();   // Left Ctrl pressed: crouch (or slide when fast)
	void Input_CrouchHold_Completed(); // Left Ctrl released: stand (CMC waits for headroom)
	void EnsureCrouchInput();

	// Empty = runtime IAs: C -> toggle, Left Ctrl -> hold. Runtime IMC has priority 2 and consumes those keys.
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> CrouchToggleAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> CrouchHoldAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> CrouchRuntimeIMC;

	bool bCrouchHeld = false;
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

	// 趴行速度 = RunSpeed * 这个倍率（再乘静步/开镜倍率）。不能冲刺
	UPROPERTY(EditAnywhere, Category = "FPS|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ProneSpeedMultiplier = 0.25f;

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
