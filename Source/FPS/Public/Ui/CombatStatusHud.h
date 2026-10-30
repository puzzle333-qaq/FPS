#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatStatusHud.generated.h"

class UCanvasPanel;
class UImage;
class UProgressBar;
class UHorizontalBox;
class UTextBlock;
class UBorder;
class AShooterCharacter;
class AWeapon;
class UTexture2D;

/**
 * Ready-or-Not style bottom-left combat HUD:
 * white outline paper-doll; injured zones fill solid red;
 * health bar; magazine fill row (fallback ammo text).
 * Built entirely in NativeConstruct (no BindWidget required).
 */
UCLASS()
class FPS_API UCombatStatusHud : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "FPS|HUD")
	void ForceShow();
	/** Compat with existing PC/KeepAlive. */
	void PinBottomLeft() { ForceShow(); }

protected:
	void BuildUi();
	void EnsureViewportSlot();
	void RefreshFromPawn();
	void SetZoneInjured(UImage* ZoneImg, bool bInjured);
	UTexture2D* LoadTex(const TCHAR* Path) const;
	UImage* MakeImage(UCanvasPanel* Canvas, const FString& Name, UTexture2D* Tex, FVector2D Pos, FVector2D Size, FLinearColor Tint);
	void RebuildMagRow(int32 MagCount, int32 RoundsInMag, int32 MagCapacity, int32 ReserveRounds);

	UPROPERTY() TObjectPtr<UCanvasPanel> RootCanvas = nullptr;
	UPROPERTY() TObjectPtr<UBorder> PanelBorder = nullptr;
	UPROPERTY() TObjectPtr<UImage> OutlineImage = nullptr;
	UPROPERTY() TObjectPtr<UImage> ZoneHead = nullptr;
	UPROPERTY() TObjectPtr<UImage> ZoneTorso = nullptr;
	UPROPERTY() TObjectPtr<UImage> ZoneArmL = nullptr;
	UPROPERTY() TObjectPtr<UImage> ZoneArmR = nullptr;
	UPROPERTY() TObjectPtr<UImage> ZoneLegL = nullptr;
	UPROPERTY() TObjectPtr<UImage> ZoneLegR = nullptr;
	UPROPERTY() TObjectPtr<UProgressBar> HealthBar = nullptr;
	UPROPERTY() TObjectPtr<UHorizontalBox> MagRow = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> AmmoFallbackText = nullptr;

	UPROPERTY() TObjectPtr<UTexture2D> TexOutline = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexHead = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexTorso = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexArmL = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexArmR = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexLegL = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexLegR = nullptr;

	bool bBuilt = false;
	int32 LastMagKey = -1;
};
