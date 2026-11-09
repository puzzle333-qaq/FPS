#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerNameplateWidget.generated.h"

class AShooterCharacter;

/** Screen-space name above another player's head. */
UCLASS()
class FPS_API UPlayerNameplateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSubject(AShooterCharacter* Subject);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	void BuildPlate();

	UPROPERTY()
	TObjectPtr<class UTextBlock> NameText;

	TWeakObjectPtr<AShooterCharacter> SubjectActor;
	FString ShownName;
};
