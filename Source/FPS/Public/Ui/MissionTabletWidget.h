#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MissionTabletWidget.generated.h"

class STextBlock;
class SVerticalBox;
class SScrollBox;
class UTexture2D;

/** Read-only, full-bleed display for the physical mission tablet. */
UCLASS()
class FPS_API UMissionTabletWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void RefreshMission();
 void ScrollBriefing(float Amount,bool bObjectives=false);
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual void NativeConstruct() override;
 virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
 TSharedPtr<STextBlock> Title, Location, Briefing, CurrentObjective;
 TSharedPtr<SVerticalBox> Objectives;
 TSharedPtr<SScrollBox> BriefingScroll;
 TSharedPtr<SScrollBox> ObjectiveScroll;
 FSlateBrush BackgroundBrush, LineBrush, RedBrush, SelectedBrush, CircleBrush, CheckBrush, ImageBrush;
 UPROPERTY(Transient) TObjectPtr<UTexture2D> ReconImage;
 void AddObjective(const FText& Name, const FText& Detail, bool bComplete);
};
