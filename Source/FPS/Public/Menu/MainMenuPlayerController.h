#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Loadout/LoadoutTypes.h"
#include "MainMenuPlayerController.generated.h"

class UUserWidget;

// Main menu shell. Shows MainMenuWidgetClass and can start solo or listen-host coop.
// Room browser and the gunsmith bench are removed; those screens will be rebuilt.
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

	UFUNCTION(BlueprintCallable, Category = "FPS|Menu")
	void StartPractice(EShooterGameMode Mode);

	UFUNCTION(Exec, BlueprintCallable, Category = "FPS|Menu")
	void MenuStartCoop();

	UFUNCTION(Exec, BlueprintCallable, Category = "FPS|LocalMP")
	void MenuHostCoop();

	UFUNCTION(BlueprintCallable, Category = "FPS|LocalMP")
	void HostLocalListenTest(FString Target);

	UFUNCTION(BlueprintCallable, Category = "FPS|LocalMP")
	void JoinLocalhostTest(FString Address);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> MainMenuWidget;
};
