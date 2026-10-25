// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShooterReticle.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class AWeapon;
class USoundBase;

UCLASS()
class FPS_API UShooterReticle : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Reticle;

	// ---------- Damage direction ring (victim) ----------
	// Ring radius in widget pixels, measured from the widget center.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|DamageRing", meta = (ClampMin = "1"))
	float DamageRingRadius = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|DamageRing", meta = (ClampMin = "0.5"))
	float DamageRingThickness = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|DamageRing")
	FLinearColor DamageRingColor = FLinearColor(1.f, 0.05f, 0.05f, 0.9f);

	// Seconds for a lit arc to fade from full to zero.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|DamageRing", meta = (ClampMin = "0.01"))
	float DamageRingFadeTime = 1.2f;

	// Gap in degrees between two neighbouring arcs (each arc spans 90 - gap).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|DamageRing", meta = (ClampMin = "0", ClampMax = "80"))
	float DamageRingGapDegrees = 6.f;

	// Line segments per arc.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|DamageRing", meta = (ClampMin = "2", ClampMax = "64"))
	int32 DamageRingSegments = 16;

	// ---------- Hit confirm marks (shooter) ----------
	// Distance from the center to the start of each diagonal line.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|HitMarker", meta = (ClampMin = "0"))
	float HitMarkerInnerOffset = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|HitMarker", meta = (ClampMin = "1"))
	float HitMarkerLength = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|HitMarker", meta = (ClampMin = "0.5"))
	float HitMarkerThickness = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|HitMarker")
	FLinearColor HitMarkerColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|HitMarker", meta = (ClampMin = "0.01"))
	float HitMarkerDuration = 0.15f;

	// Optional 2D sound when a hit is confirmed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|HitMarker")
	TObjectPtr<USoundBase> HitMarkerSound;

	// HitMarkerSound 没填时用这个（默认 Infima 的 hitmarker 音）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|HitMarker")
	TSoftObjectPtr<USoundBase> HitMarkerSoundFallback = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/InfimaGames/ArtCore/Audio/_Common/SC_IG_WEP_Hitmarker.SC_IG_WEP_Hitmarker")));

	// 霰弹多颗弹丸同时命中时，命中音最短间隔（秒）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|HitMarker", meta = (ClampMin = "0"))
	float HitMarkerSoundMinInterval = 0.05f;

	// 爆头命中音（非致死）。空 = 用普通命中音
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|HitMarker")
	TSoftObjectPtr<USoundBase> HeadshotSound;

	// ---------- Kill marker (shooter) ----------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|KillMarker")
	FLinearColor KillMarkerColor = FLinearColor(1.f, 0.5f, 0.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|KillMarker", meta = (ClampMin = "0.01"))
	float KillMarkerDuration = 0.3f;

	// 相对命中 X 的放大倍数（长度、内偏移）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|KillMarker", meta = (ClampMin = "0.1"))
	float KillMarkerScale = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|KillMarker", meta = (ClampMin = "0.5"))
	float KillMarkerThickness = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|KillMarker")
	TSoftObjectPtr<USoundBase> KillSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/DEFINITIVE_FPS_KIT/NPC/Enemys/Sound/Kill/Kill_Enemy.Kill_Enemy")));

	// 爆头击杀音。空 = 用 KillSound
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|HitFeedback|KillMarker")
	TSoftObjectPtr<USoundBase> HeadshotKillSound;

	UFUNCTION(BlueprintCallable, Category = "FPS|HitFeedback")
	void ShowKillMarker(bool bHeadshot);

	// Call these from Blueprint to test without a second player.
	UFUNCTION(BlueprintCallable, Category = "FPS|HitFeedback")
	void ShowDamageFrom(FVector InstigatorLocation);

	UFUNCTION(BlueprintCallable, Category = "FPS|HitFeedback")
	void ShowHitMarker();

	// ---------- Spreading crosshair (hip-fire) ----------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Crosshair|Spread", meta = (ClampMin = "0"))
	float CrosshairBaseGap = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Crosshair|Spread", meta = (ClampMin = "1"))
	float CrosshairLineLength = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Crosshair|Spread", meta = (ClampMin = "0.5"))
	float CrosshairLineThickness = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Crosshair|Spread")
	FLinearColor CrosshairLineColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Crosshair|Spread", meta = (ClampMin = "0"))
	float CrosshairSpreadKick = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Crosshair|Spread", meta = (ClampMin = "0"))
	float CrosshairSpreadMax = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Crosshair|Spread", meta = (ClampMin = "0"))
	float CrosshairSpreadRecovery = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Crosshair|Spread")
	bool bDrawSpreadingCrosshair = true;

	UFUNCTION(BlueprintCallable, Category = "FPS|Crosshair|Spread")
	void NotifyWeaponFired();

private:
	TWeakObjectPtr<UMaterialInstanceDynamic> CurrentReticle_DynMatInst;
	TWeakObjectPtr<APawn> BoundPawn;

	// 0 = top (front), 1 = right, 2 = bottom (back), 3 = left
	float ArcIntensity[4] = { 0.f, 0.f, 0.f, 0.f };
	float HitMarkerRemaining = 0.f;
	float KillMarkerRemaining = 0.f;
	double LastHitSoundTime = -1.0;

	void PlayFeedbackSound(USoundBase* Sound);
	float CrosshairSpread = 0.f;

	UImage* ResolveReticleImage();

	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void OnWeaponFirstReplicated(AWeapon* Weapon);

	UFUNCTION()
	void OnReticleChanged(UMaterialInstanceDynamic* ReticleMaterialInst);

	UFUNCTION()
	void HandleDamageIndicator(FVector InstigatorLocation);

	UFUNCTION()
	void HandleHitConfirmed(bool bHeadshot, bool bKilled);

	UFUNCTION()
	void HandleWeaponFired();
};
