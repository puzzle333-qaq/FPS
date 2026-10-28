#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "RadarWidget.generated.h"

class AShooterCharacter;
class AShooterPlayerState;
class AShooterGameState;
class ABombSite;
class ABombActor;
class ASceneCapture2D;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

UENUM(BlueprintType)
enum class ERadarBlipKind : uint8
{
	Local,
	Teammate,
	Enemy,
	Bomb,
	SiteA,
	SiteB
};

UENUM(BlueprintType)
enum class ERadarDisplayMode : uint8
{
	/** Left-corner mini radar, player-centered, rotates with view. */
	Mini,
	/** Large M-key map: covers playable bounds, north-up. */
	FullMap
};

USTRUCT()
struct FRadarBlip
{
	GENERATED_BODY()

	// Radar space: X = right, Y = forward/up on screen, both in [-1,1] after clamp
	FVector2D Pos = FVector2D::ZeroVector;
	ERadarBlipKind Kind = ERadarBlipKind::Teammate;
	bool bClampedToEdge = false;
	/** Degrees; Local arrow facing when north-up full map. */
	float FacingYaw = 0.f;
};

/**
 * COD/CS-style radar / tactical map with live top-down SceneCapture2D.
 *
 * Fairness:
 * - Local player marker
 * - Teammates (alive): always
 * - S&D: bomb sites A/B + bomb
 * - Enemies: LOS within range OR damaged local within EnemySpotDuration
 */
UCLASS()
class FPS_API URadarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FPS|HUD|Radar")
	void SetDisplayMode(ERadarDisplayMode NewMode);

	UFUNCTION(BlueprintPure, Category = "FPS|HUD|Radar")
	ERadarDisplayMode GetDisplayMode() const { return DisplayMode; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar")
	ERadarDisplayMode DisplayMode = ERadarDisplayMode::Mini;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar", meta = (ClampMin = "500"))
	float RadarRange = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar", meta = (ClampMin = "80"))
	float RadarPixelSize = 232.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar", meta = (ClampMin = "200"))
	float FullMapPixelSize = 640.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar", meta = (ClampMin = "0.05"))
	float UpdateInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar", meta = (ClampMin = "0.5"))
	float EnemySpotDuration = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar")
	bool bShowMapBackground = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar", meta = (ClampMin = "64", ClampMax = "1024"))
	int32 MapRTSize = 256;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar", meta = (ClampMin = "64", ClampMax = "1024"))
	int32 FullMapRTSize = 512;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar", meta = (ClampMin = "500"))
	float CaptureHeight = 12000.f;

	/**
	 * Capture pitch. Pure -90 only sees floors (walls edge-on).
	 * ~-78 to -82 lets vertical walls show as faces while staying mostly top-down.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar", meta = (ClampMin = "-89.5", ClampMax = "-60"))
	float CapturePitch = -80.f;

	/** Extra ortho padding. <1 tightens framing so geometry fills more of the RT. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar", meta = (ClampMin = "0.85", ClampMax = "1.5"))
	float FullMapBoundsPadding = 1.02f;

	/** Mini: added to player yaw for capture. -90 matches WorldToRadar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar")
	float MapYawOffset = -90.f;

	/** Brightness boost on the capture (AutoExposureBias). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar")
	float MapExposureBias = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar")
	FLinearColor MapTint = FLinearColor(1.f, 1.f, 1.f, 1.f);

	/** Void / panel behind the capture (not pure black). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar")
	FLinearColor BackgroundColor = FLinearColor(0.08f, 0.09f, 0.10f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar")
	FLinearColor RingColor = FLinearColor(0.35f, 0.85f, 0.55f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar")
	FLinearColor CrosshairColor = FLinearColor(0.35f, 0.85f, 0.55f, 0.25f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar")
	FLinearColor LocalColor = FLinearColor(0.95f, 0.95f, 0.95f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar")
	FLinearColor TeammateColor = FLinearColor(0.25f, 0.75f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar")
	FLinearColor EnemyColor = FLinearColor(1.f, 0.28f, 0.22f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar")
	FLinearColor BombColor = FLinearColor(1.f, 0.85f, 0.15f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar")
	FLinearColor SiteColor = FLinearColor(0.9f, 0.9f, 0.55f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar")
	FLinearColor FullMapPanelColor = FLinearColor(0.07f, 0.08f, 0.09f, 0.94f);

	/** RT clear / empty sky fill (dark grey, not pure black). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|Radar")
	FLinearColor MapClearColor = FLinearColor(0.09f, 0.10f, 0.11f, 1.f);

protected:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	void RefreshBlips();
	void BindLocalDamage();
	void UnbindLocalDamage();
	void HandleRefreshTimer();
	bool IsPawnAlive(const APawn* Pawn) const;
	bool HasLineOfSightTo(const APawn* Viewer, const APawn* Target) const;
	FVector2D WorldToRadar(const FVector& WorldLoc, const FVector& Origin, float YawDeg, float Range) const;
	void AddBlip(const FVector& WorldLoc, const FVector& Origin, float YawDeg, float Range, ERadarBlipKind Kind, float FacingYaw = 0.f);
	void SpotEnemyNear(const FVector& WorldLoc);
	void ApplyRootSize();
	bool ResolveMapFrame(FVector& OutOrigin, float& OutYawDeg, float& OutRange) const;

	void EnsureMapCapture();
	void DestroyMapCapture();
	void UpdateMapCapture(const FVector& Origin, float YawDeg, float Range);
	void RefreshCaptureHiddenActors();
	void ConfigureCaptureComponent(USceneCaptureComponent2D* Cap, float Range);

	UFUNCTION()
	void HandleDamageIndicator(FVector InstigatorLocation);

	UPROPERTY(Transient)
	TArray<FRadarBlip> Blips;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> MapRT;

	UPROPERTY(Transient)
	TObjectPtr<ASceneCapture2D> CaptureActor;

	UPROPERTY(Transient)
	FSlateBrush MapBrush;

	TWeakObjectPtr<AShooterCharacter> BoundLocalChar;
	TMap<TWeakObjectPtr<APawn>, float> SpottedUntil;

	bool bHasLocal = false;
	bool bMapReady = false;
	FTimerHandle RefreshTimer;
};
