#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MissionFlowWidget.generated.h"
class AMissionDeployPoint;
class SVerticalBox;
class SBorder;
class STextBlock;
class SButton;

UENUM()
enum class EMissionPanel : uint8 { Selection, Loading, Tablet, Debrief };

/** Asset-independent mission UI, replaceable later with art-directed Blueprint widgets. */
UCLASS()
class FPS_API UMissionFlowWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void Configure(EMissionPanel InPanel, AMissionDeployPoint* InDeploy = nullptr);
 UFUNCTION() void Refresh();
 EMissionPanel Panel = EMissionPanel::Tablet;
 UPROPERTY() TObjectPtr<AMissionDeployPoint> Deploy;
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual FReply NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) override;
 virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
 virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
 TSharedPtr<SVerticalBox> Rows;
 TSharedPtr<SBorder> LegacyPanel;
 TSharedPtr<SBorder> ReconPanel;
 TSharedPtr<STextBlock> ReconMap,ReconTask,ReconCount;
 FSlateBrush ReconBrush;
 TSharedPtr<SButton> FirstButton;
 TSharedPtr<STextBlock> Clock;
 FSlateBrush PanelBrush;
 FSlateBrush DimBrush;
 FButtonStyle ButtonStyle;
 FTimerHandle ClockTimer;
 void UpdateClock();
 void AddLine(const FString& Text, int32 Size = 18);
 void AddButton(const FText& Label, TFunction<void()> Action);
};
