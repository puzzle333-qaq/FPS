#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "ControlHintWidget.generated.h"

class STextBlock;
class SHorizontalBox;
class UCombatComponent;
class AShooterCharacter;
class AWeapon;

/** Equipment states, without permanent input hints. */
UCLASS()
class FPS_API UControlHintWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UControlHintWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

 void BindPawn(APawn*);
 void RefreshEquipment();
 void BindWeapon();
 void Unbind();
 TWeakObjectPtr<AShooterCharacter> Character;
 TWeakObjectPtr<UCombatComponent> Combat;
 TWeakObjectPtr<AWeapon> Weapon;
 TSharedPtr<SHorizontalBox> Root;
 TSharedPtr<STextBlock> Labels[5], Dots[5];
 TSharedPtr<SWidget> Groups[5];
 bool States[5]={false,false,false,false,false};
 float FlashRemaining[5]={0,0,0,0,0};
 bool bInitialized=false;
 FSlateBrush Icons[5];
 UPROPERTY() TArray<TObjectPtr<UTexture2D>> IconTextures;
};
