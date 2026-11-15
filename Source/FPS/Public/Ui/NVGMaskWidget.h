#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "NVGMaskWidget.generated.h"
/** Two/four tube union, opaque outside, with a 2.5 pixel soft edge and lower blind wedge. */
UCLASS()
class FPS_API UNVGMaskWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UNVGMaskWidget(const FObjectInitializer& ObjectInitializer);
protected:
 virtual TSharedRef<SWidget> RebuildWidget()override;
 virtual void NativeConstruct()override;
 virtual void NativeDestruct()override;
 void BindPawn(APawn* Pawn);
 void RefreshVisibility();
 TWeakObjectPtr<class AShooterCharacter> Character;
 virtual void NativeTick(const FGeometry& Geometry,float DeltaTime)override;
 UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> MaskMaterial;
 FSlateBrush MaskBrush;
};
