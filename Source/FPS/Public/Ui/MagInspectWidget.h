#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "MagInspectWidget.generated.h"

class UCombatComponent;
class SVerticalBox;
class SHorizontalBox;
class STextBlock;

/** Bottom-center magazine row. Hidden until a hold-R mag check is playing. */
UCLASS()
class FPS_API UMagInspectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMagInspectWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

 void BindPawn(APawn*);
 void InspectionChanged(bool);
 void RefreshMagazine();
 TWeakObjectPtr<UCombatComponent> Combat;
 TSharedPtr<SVerticalBox> Panel;
	TSharedPtr<SHorizontalBox> Row;
	TSharedPtr<STextBlock> FireMode;
	TArray<float> Fills;
 int32 CurrentRounds=0;
 float Opacity=0.f,Linger=0.f;
 bool bInspecting=false;
};
