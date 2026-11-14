#pragma once
#include "CoreMinimal.h"
#include "Ui/RangeLoadoutWidget.h"
#include "FullScreenLoadoutWidget.generated.h"

class SVerticalBox;
class SCanvas;
class UTextureRenderTarget2D;
class USceneCaptureComponent2D;
class USkeletalMeshComponent;

/** Native presentation over the existing bench/catalog/pouch and save systems. */
UCLASS()
class FPS_API UFullScreenLoadoutWidget : public URangeLoadoutWidget
{
 GENERATED_BODY()
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual bool UsesNativePresentation() const override { return true; }
 virtual void RefreshNativePresentation() override;
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual void ReleaseSlateResources(bool bReleaseChildren) override;
 virtual FReply NativeOnPreviewKeyDown(const FGeometry&,const FKeyEvent&) override;
private:
 void RefreshScreen();
public:
 UFUNCTION(BlueprintCallable,Category="Loadout") void SetDetail(TSubclassOf<AWeapon> Weapon);
 UFUNCTION(BlueprintCallable,Category="Loadout") void Step(TSubclassOf<AWeapon> Weapon,int32 Delta,bool Throwable);
 UFUNCTION(BlueprintCallable,Category="Loadout") void ChangeOperator(int32 Delta);
 UFUNCTION(BlueprintCallable,Category="Loadout") void SavePreset();
 UFUNCTION(BlueprintCallable,Category="Loadout") void LoadPreset(int32 Index);
private:
 void RefreshStage();
 void CaptureStage();
 FString SelectionSignature() const;
 FString SavedSignature;
 int32 UsedSlots() const;
 TSharedRef<SWidget> Text(const FString&,int32 Size=16,FLinearColor Color=FLinearColor(.93,.94,.95));
 TSharedRef<SWidget> Button(const FString&,TFunction<void()> Action,bool Enabled=true,int32 Size=16);
 TSharedRef<SWidget> Icon(UTexture2D*,FVector2D Size);
 void AddTitle(const FString&);
 void AddWeapon(const FRangeWeaponChoice&,const FString& Type);
 void AddStepper(const FRangeWeaponChoice&,bool Throwable);
 void AddProtection();
 TSharedPtr<SCanvas> Canvas;
 TSharedPtr<SVerticalBox> Content;
 FSlateBrush BlackBrush,PreviewBrush,LineBrush,EmptyIconBrush,TabUnderlineBrush;
 FButtonStyle ButtonStyle;
 TArray<TSharedPtr<FSlateBrush>> ImageBrushes;
 TArray<TSharedPtr<FString>> PresetOptions;
 TSharedPtr<class SEditableTextBox> PresetName;
 UPROPERTY(Transient) TObjectPtr<AActor> Stage;
 UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> StageBody;
 UPROPERTY(Transient) TObjectPtr<UPoliceAppearanceComponent> StageAppearance;
 UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> Capture;
 UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> RenderTarget;
 UPROPERTY(Transient) TObjectPtr<AWeapon> StageWeapon;
 UPROPERTY(Transient) TObjectPtr<class AShooterCharacter> ViewedOperator;
 TSubclassOf<AWeapon> Detail;
 FGameplayTag ExpandedSlot;
 bool Customization=false,Deployables=false,Unsaved=false;
 int32 PresetIndex=0;
 FString CurrentPresetName=TEXT("行动配装 1"),Status;
 double OverflowAt=-10;
 FTimerHandle CaptureTimer;
};
