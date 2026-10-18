#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuPlayerController.generated.h"

class UUserWidget;
class UGunsmithComponent;

// 主菜单控制器：开局创建 MainMenuWidgetClass，切成纯 UI 输入并显示鼠标。改枪台逻辑在 Gunsmith 组件上
UCLASS()
class FPS_API AMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMainMenuPlayerController();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Menu")
	TSubclassOf<UUserWidget> MainMenuWidgetClass;

	UFUNCTION(BlueprintPure, Category = "FPS|Menu")
	UUserWidget* GetMainMenuWidget() const { return MainMenuWidget; }

	UFUNCTION(BlueprintPure, Category = "FPS|Gunsmith")
	UGunsmithComponent* GetGunsmith() const { return Gunsmith; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith")
	TObjectPtr<UGunsmithComponent> Gunsmith;

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> MainMenuWidget;
};