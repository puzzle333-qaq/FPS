
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> EquipMainWeapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> EquipSeconedaryWeapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> EquipMeleeWeapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> EquipPistolWeapon;


	void Input_Crouch();
	void Input_Jump();
	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Look(const FInputActionValue& InputActionValue);
	void Input_EquipMainWeapon();
	void Input_EquipSeconedaryWeapon();
	void Input_EquipMeleeWeapon();
	void Input_EquipPistolWeapon();
};
