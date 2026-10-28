#include "Ui/RadarWidget.h"

#include "ShooterCharacter.h"
#include "Stats/StatsComponent.h"
#include "Game/ShooterPlayerState.h"
#include "Game/ShooterGameState.h"
#include "Game/SearchDestroyGameState.h"
#include "Game/BombSite.h"
#include "Game/BombActor.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"
#include "TimerManager.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "GameFramework/Pawn.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Components/BrushComponent.h"
#include "Engine/StaticMeshActor.h"

void URadarWidget::SetDisplayMode(ERadarDisplayMode NewMode)
{
	if (DisplayMode == NewMode) return;
	DisplayMode = NewMode;
	DestroyMapCapture();
	ApplyRootSize();
	if (bShowMapBackground && GetWorld())
	{
		EnsureMapCapture();
	}
	RefreshBlips();
}

void URadarWidget::ApplyRootSize()
{
	const float Size = (DisplayMode == ERadarDisplayMode::FullMap) ? FullMapPixelSize : RadarPixelSize;
	if (USizeBox* Box = Cast<USizeBox>(GetRootWidget()))
	{
		Box->SetWidthOverride(Size);
		Box->SetHeightOverride(Size);
	}
	else if (WidgetTree)
	{
		if (USizeBox* Existing = Cast<USizeBox>(WidgetTree->RootWidget))
		{
			Existing->SetWidthOverride(Size);
			Existing->SetHeightOverride(Size);
		}
	}
}

bool URadarWidget::Initialize()
{
	const bool bOk = Super::Initialize();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RadarSize"));
		const float Size = (DisplayMode == ERadarDisplayMode::FullMap) ? FullMapPixelSize : RadarPixelSize;
		Box->SetWidthOverride(Size);
		Box->SetHeightOverride(Size);
		WidgetTree->RootWidget = Box;
	}
	return bOk;
}

void URadarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	bHasLocal = false;
	bMapReady = false;
	Blips.Reset();
	ApplyRootSize();
	BindLocalDamage();
	if (bShowMapBackground)
	{
		EnsureMapCapture();
	}
	RefreshBlips();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RefreshTimer,
			this,
			&ThisClass::HandleRefreshTimer,
			FMath::Max(UpdateInterval, 0.05f),
			true);
	}
}

void URadarWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	UnbindLocalDamage();
	DestroyMapCapture();
	SpottedUntil.Empty();
	Blips.Reset();
	Super::NativeDestruct();
}

void URadarWidget::HandleRefreshTimer()
{
	if (!BoundLocalChar.IsValid())
	{
		BindLocalDamage();
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	for (auto It = SpottedUntil.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || It.Value() < Now)
		{
			It.RemoveCurrent();
		}
	}
	RefreshBlips();
}

void URadarWidget::BindLocalDamage()
{
	UnbindLocalDamage();
	APlayerController* PC = GetOwningPlayer();
	AShooterCharacter* Char = IsValid(PC) ? Cast<AShooterCharacter>(PC->GetPawn()) : nullptr;
	if (!IsValid(Char) && IsValid(PC))
	{
		Char = Cast<AShooterCharacter>(PC->GetViewTarget());
	}
	BoundLocalChar = Char;
	if (IsValid(Char))
	{
		Char->OnDamageIndicator.AddUniqueDynamic(this, &ThisClass::HandleDamageIndicator);
	}
}

void URadarWidget::UnbindLocalDamage()
{
	if (BoundLocalChar.IsValid())
	{
		BoundLocalChar->OnDamageIndicator.RemoveDynamic(this, &ThisClass::HandleDamageIndicator);
	}
	BoundLocalChar.Reset();
}

void URadarWidget::HandleDamageIndicator(FVector InstigatorLocation)
{
	SpotEnemyNear(InstigatorLocation);
}

void URadarWidget::SpotEnemyNear(const FVector& WorldLoc)
{
	UWorld* World = GetWorld();
	APlayerController* PC = GetOwningPlayer();
	AShooterPlayerState* LocalPS = IsValid(PC) ? PC->GetPlayerState<AShooterPlayerState>() : nullptr;
	if (!World || !IsValid(LocalPS) || !LocalPS->HasTeam()) return;

	const uint8 LocalTeam = LocalPS->GetTeamId();
	APawn* Best = nullptr;
	float BestDistSq = 600.f * 600.f;

	for (TActorIterator<AShooterCharacter> It(World); It; ++It)
	{
		AShooterCharacter* Other = *It;
		if (!IsValid(Other) || !IsPawnAlive(Other)) continue;
		AShooterPlayerState* OPS = Other->GetPlayerState<AShooterPlayerState>();
		if (!IsValid(OPS) || !OPS->HasTeam() || OPS->GetTeamId() == LocalTeam) continue;

		const float DistSq = FVector::DistSquared(Other->GetActorLocation(), WorldLoc);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Other;
		}
	}

	if (Best)
	{
		SpottedUntil.FindOrAdd(Best) = World->GetTimeSeconds() + EnemySpotDuration;
	}
}

bool URadarWidget::IsPawnAlive(const APawn* Pawn) const
{
	if (!IsValid(Pawn)) return false;
	const UStatsComponent* Stats = UStatsComponent::FindStatsComponent(Pawn);
	if (!Stats) return true;
	return Stats->DeathState == EDeathState::NotDead;
}

bool URadarWidget::HasLineOfSightTo(const APawn* Viewer, const APawn* Target) const
{
	if (!IsValid(Viewer) || !IsValid(Target) || !GetWorld()) return false;

	FVector Start = Viewer->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	if (const AController* Ctrl = Viewer->GetController())
	{
		if (const APlayerController* PC = Cast<APlayerController>(Ctrl))
		{
			FVector Eyes;
			FRotator Rot;
			PC->GetPlayerViewPoint(Eyes, Rot);
			Start = Eyes;
		}
	}
	const FVector End = Target->GetActorLocation() + FVector(0.f, 0.f, 40.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RadarLOS), false, Viewer);
	Params.AddIgnoredActor(Target);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	return !bHit || Hit.GetActor() == Target;
}

FVector2D URadarWidget::WorldToRadar(const FVector& WorldLoc, const FVector& Origin, float YawDeg, float Range) const
{
	const FVector Delta = WorldLoc - Origin;
	const FRotator YawRot(0.f, YawDeg, 0.f);
	const FVector Local = YawRot.UnrotateVector(Delta);
	const float Inv = 1.f / FMath::Max(Range, 1.f);
	return FVector2D(Local.Y * Inv, -Local.X * Inv);
}

void URadarWidget::AddBlip(const FVector& WorldLoc, const FVector& Origin, float YawDeg, float Range, ERadarBlipKind Kind, float FacingYaw)
{
	FRadarBlip Blip;
	Blip.Pos = WorldToRadar(WorldLoc, Origin, YawDeg, Range);
	Blip.Kind = Kind;
	Blip.FacingYaw = FacingYaw;
	const float Len = Blip.Pos.Size();
	if (Len > 1.f)
	{
		Blip.Pos /= Len;
		Blip.bClampedToEdge = true;
	}
	Blips.Add(Blip);
}

bool URadarWidget::ResolveMapFrame(FVector& OutOrigin, float& OutYawDeg, float& OutRange) const
{
	UWorld* World = GetWorld();
	APlayerController* PC = GetOwningPlayer();
	if (!World || !IsValid(PC)) return false;

	APawn* ViewPawn = PC->GetPawn();
	if (!IsValid(ViewPawn))
	{
		ViewPawn = Cast<APawn>(PC->GetViewTarget());
	}
	if (!IsValid(ViewPawn)) return false;

	float PlayerYaw = PC->GetControlRotation().Yaw;
	if (AController* Ctrl = ViewPawn->GetController())
	{
		PlayerYaw = Ctrl->GetControlRotation().Yaw;
	}

	if (DisplayMode == ERadarDisplayMode::Mini)
	{
		OutOrigin = ViewPawn->GetActorLocation();
		OutYawDeg = PlayerYaw;
		OutRange = RadarRange;
		return true;
	}

	// Full map: north-up. Prefer mesh footprint over NavMesh (nav often includes empty void).
	FBox MeshBounds(EForceInit::ForceInit);
	bool bHasMesh = false;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		if (!IsValid(*It)) continue;
		const FBox B = (*It)->GetComponentsBoundingBox(true);
		const FVector Ext = B.GetExtent();
		if (Ext.GetMax() < 80.f) continue;
		if ((Ext.X + Ext.Y) < 200.f && Ext.Z < 150.f) continue;
		MeshBounds += B;
		bHasMesh = true;
	}

	FBox SiteBounds(EForceInit::ForceInit);
	bool bHasSites = false;
	for (TActorIterator<ABombSite> It(World); It; ++It)
	{
		if (!IsValid(*It)) continue;
		SiteBounds += (*It)->GetActorLocation();
		bHasSites = true;
	}

	FBox NavBounds(EForceInit::ForceInit);
	bool bHasNav = false;
	for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
	{
		if (!IsValid(*It)) continue;
		if (UBrushComponent* Brush = (*It)->GetBrushComponent())
		{
			NavBounds += Brush->Bounds.GetBox();
			bHasNav = true;
		}
		else
		{
			NavBounds += (*It)->GetComponentsBoundingBox(true);
			bHasNav = true;
		}
	}

	FBox Bounds(EForceInit::ForceInit);
	bool bHasBounds = false;
	if (bHasMesh)
	{
		Bounds = MeshBounds;
		bHasBounds = true;
		if (bHasSites)
		{
			Bounds += SiteBounds;
		}
		if (bHasNav)
		{
			const FBox Overlap = Bounds.Overlap(NavBounds);
			if (Overlap.IsValid && Overlap.GetExtent().GetMax() > 500.f)
			{
				Bounds.Min.X = Overlap.Min.X;
				Bounds.Min.Y = Overlap.Min.Y;
				Bounds.Max.X = Overlap.Max.X;
				Bounds.Max.Y = Overlap.Max.Y;
			}
		}
	}
	else if (bHasNav)
	{
		Bounds = NavBounds;
		bHasBounds = true;
		if (bHasSites) Bounds += SiteBounds;
	}
	else if (bHasSites)
	{
		Bounds = SiteBounds;
		bHasBounds = true;
	}

	if (bHasBounds)
	{
		Bounds += ViewPawn->GetActorLocation();
		OutOrigin = Bounds.GetCenter();
		OutOrigin.Z = ViewPawn->GetActorLocation().Z;
		const FVector Ext = Bounds.GetExtent();
		OutRange = FMath::Max(Ext.X, Ext.Y) * FullMapBoundsPadding;
		OutRange = FMath::Max(OutRange, 2500.f);
	}
	else
	{
		OutOrigin = ViewPawn->GetActorLocation();
		OutRange = FMath::Max(RadarRange * 2.5f, 8000.f);
	}
	OutYawDeg = 0.f; // north-up
	return true;
}

void URadarWidget::ConfigureCaptureComponent(USceneCaptureComponent2D* Cap, float Range)
{
	if (!Cap || !IsValid(MapRT)) return;

	Cap->TextureTarget = MapRT;
	Cap->ProjectionType = ECameraProjectionMode::Orthographic;
	Cap->OrthoWidth = FMath::Max(Range, 1.f) * 2.f;
	Cap->bCaptureEveryFrame = false;
	Cap->bCaptureOnMovement = false;
	Cap->bAlwaysPersistRenderingState = true;
	// Lit final color — keep materials/textures (do NOT strip to BaseColor).
	Cap->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Cap->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	Cap->bUseCustomProjectionMatrix = false;

	Cap->ShowFlags.SetAtmosphere(false);
	Cap->ShowFlags.SetFog(false);
	Cap->ShowFlags.SetParticles(false);
	Cap->ShowFlags.SetDeferredLighting(true);
	Cap->ShowFlags.SetLighting(true);
	Cap->ShowFlags.SetDynamicShadows(false);
	Cap->ShowFlags.SetInstancedFoliage(false);
	Cap->ShowFlags.SetInstancedGrass(false);
	Cap->ShowFlags.SetInstancedStaticMeshes(true);
	Cap->ShowFlags.SetLandscape(true);
	Cap->ShowFlags.SetStaticMeshes(true);
	Cap->ShowFlags.SetSkeletalMeshes(false);
	Cap->ShowFlags.SetTranslucency(true);
	Cap->ShowFlags.SetBSP(true);
	Cap->ShowFlags.SetNaniteMeshes(true);
	// Tonemap + fixed exposure so outdoor isn't crushed black / blown out.
	Cap->ShowFlags.SetPostProcessing(true);
	Cap->ShowFlags.SetBloom(false);
	Cap->ShowFlags.SetEyeAdaptation(true);
	Cap->ShowFlags.SetTemporalAA(false);
	Cap->ShowFlags.SetAntiAliasing(false);
	Cap->ShowFlags.SetAmbientOcclusion(false);
	Cap->ShowFlags.SetGlobalIllumination(false);

	Cap->PostProcessBlendWeight = 1.f;
	Cap->PostProcessSettings = FPostProcessSettings();
	Cap->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Cap->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	Cap->PostProcessSettings.bOverride_AutoExposureBias = true;
	Cap->PostProcessSettings.AutoExposureBias = MapExposureBias;
	Cap->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Cap->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
}

void URadarWidget::EnsureMapCapture()
{
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown) return;

	const int32 WantSize = (DisplayMode == ERadarDisplayMode::FullMap)
		? FMath::Clamp(FullMapRTSize, 64, 1024)
		: FMath::Clamp(MapRTSize, 64, 1024);

	if (!IsValid(MapRT) || MapRT->SizeX != WantSize || MapRT->SizeY != WantSize)
	{
		MapRT = NewObject<UTextureRenderTarget2D>(this, TEXT("RadarMapRT"));
		MapRT->RenderTargetFormat = RTF_RGBA8;
		MapRT->ClearColor = MapClearColor;
		MapRT->bAutoGenerateMips = false;
		MapRT->InitCustomFormat(WantSize, WantSize, PF_B8G8R8A8, false);
		MapRT->UpdateResourceImmediate(true);
	}
	else
	{
		MapRT->ClearColor = MapClearColor;
	}

	if (!IsValid(CaptureActor))
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		CaptureActor = World->SpawnActor<ASceneCapture2D>(
			ASceneCapture2D::StaticClass(),
			FVector::ZeroVector,
			FRotator(CapturePitch, 0.f, 0.f),
			Params);
		if (!IsValid(CaptureActor))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Radar] Failed to spawn SceneCapture2D"));
			return;
		}
		CaptureActor->SetActorEnableCollision(false);
		CaptureActor->SetActorHiddenInGame(true);
	}

	USceneCaptureComponent2D* Cap = CaptureActor->GetCaptureComponent2D();
	if (!Cap) return;

	ConfigureCaptureComponent(Cap, RadarRange);

	MapBrush = FSlateBrush();
	MapBrush.SetResourceObject(MapRT);
	MapBrush.ImageSize = FVector2D(static_cast<float>(MapRT->SizeX), static_cast<float>(MapRT->SizeY));
	MapBrush.DrawAs = ESlateBrushDrawType::Image;
	MapBrush.Tiling = ESlateBrushTileType::NoTile;
	MapBrush.TintColor = FSlateColor(FLinearColor::White);
	bMapReady = true;
	UE_LOG(LogTemp, Log, TEXT("[Radar] Map capture ready mode=%d RT=%dx%d pitch=%.1f"),
		static_cast<int32>(DisplayMode), MapRT->SizeX, MapRT->SizeY, CapturePitch);
}

void URadarWidget::DestroyMapCapture()
{
	bMapReady = false;
	MapBrush = FSlateBrush();
	if (IsValid(CaptureActor))
	{
		CaptureActor->Destroy();
	}
	CaptureActor = nullptr;
	MapRT = nullptr;
}

void URadarWidget::RefreshCaptureHiddenActors()
{
	if (!IsValid(CaptureActor)) return;
	USceneCaptureComponent2D* Cap = CaptureActor->GetCaptureComponent2D();
	if (!Cap) return;

	Cap->HiddenActors.Reset();
	UWorld* World = GetWorld();
	if (!World) return;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			Cap->HiddenActors.Add(*It);
		}
	}
	for (TActorIterator<ABombActor> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			Cap->HiddenActors.Add(*It);
		}
	}
}

void URadarWidget::UpdateMapCapture(const FVector& Origin, float YawDeg, float Range)
{
	if (!bShowMapBackground) return;
	EnsureMapCapture();
	if (!IsValid(CaptureActor) || !IsValid(MapRT)) return;

	USceneCaptureComponent2D* Cap = CaptureActor->GetCaptureComponent2D();
	if (!Cap) return;

	ConfigureCaptureComponent(Cap, Range);

	// Mini: near-vertical so the live projection stays player-centered.
	// Full map: slight tilt so walls read as faces. MapYawOffset aligns with WorldToRadar.
	// With pitch != -90 the look-at point shifts forward — pull the camera back on XY
	// so Origin (player for mini / map center for full) stays at the image center.
	const float Pitch = (DisplayMode == ERadarDisplayMode::Mini)
		? -89.2f
		: FMath::Clamp(CapturePitch, -89.5f, -60.f);
	const FRotator CapRot(Pitch, YawDeg + MapYawOffset, 0.f);
	const float AngleFromNadir = FMath::DegreesToRadians(FMath::Abs(Pitch + 90.f));
	const FVector YawForward = FRotator(0.f, CapRot.Yaw, 0.f).Vector();
	const float GroundShift = CaptureHeight * FMath::Tan(AngleFromNadir);
	const FVector CapLoc(
		Origin.X - YawForward.X * GroundShift,
		Origin.Y - YawForward.Y * GroundShift,
		Origin.Z + CaptureHeight);
	CaptureActor->SetActorLocationAndRotation(CapLoc, CapRot);
	RefreshCaptureHiddenActors();
	Cap->CaptureScene();
	bMapReady = true;
}

void URadarWidget::RefreshBlips()
{
	Blips.Reset();
	bHasLocal = false;

	UWorld* World = GetWorld();
	APlayerController* PC = GetOwningPlayer();
	if (!World || !IsValid(PC)) return;

	APawn* ViewPawn = PC->GetPawn();
	if (!IsValid(ViewPawn))
	{
		ViewPawn = Cast<APawn>(PC->GetViewTarget());
	}
	if (!IsValid(ViewPawn)) return;

	FVector Origin;
	float YawDeg = 0.f;
	float Range = RadarRange;
	if (!ResolveMapFrame(Origin, YawDeg, Range)) return;

	AShooterPlayerState* LocalPS = PC->GetPlayerState<AShooterPlayerState>();
	const uint8 LocalTeam = (IsValid(LocalPS) && LocalPS->HasTeam()) ? LocalPS->GetTeamId() : AShooterPlayerState::NoTeam;

	float PlayerYaw = PC->GetControlRotation().Yaw;
	if (AController* Ctrl = ViewPawn->GetController())
	{
		PlayerYaw = Ctrl->GetControlRotation().Yaw;
	}

	UpdateMapCapture(Origin, YawDeg, Range);

	if (DisplayMode == ERadarDisplayMode::Mini)
	{
		FRadarBlip Local;
		Local.Pos = FVector2D::ZeroVector;
		Local.Kind = ERadarBlipKind::Local;
		Local.FacingYaw = 0.f;
		Blips.Add(Local);
		bHasLocal = true;
	}
	else
	{
		AddBlip(ViewPawn->GetActorLocation(), Origin, YawDeg, Range, ERadarBlipKind::Local, PlayerYaw);
		bHasLocal = true;
	}

	AShooterGameState* GS = World->GetGameState<AShooterGameState>();
	if (IsValid(GS))
	{
		for (APlayerState* BasePS : GS->PlayerArray)
		{
			AShooterPlayerState* SPS = Cast<AShooterPlayerState>(BasePS);
			if (!IsValid(SPS) || !SPS->HasTeam()) continue;
			APawn* Pawn = SPS->GetPawn();
			if (!IsValid(Pawn) || Pawn == ViewPawn) continue;
			if (!IsPawnAlive(Pawn)) continue;

			const bool bTeammate = (LocalTeam != AShooterPlayerState::NoTeam && SPS->GetTeamId() == LocalTeam);
			if (bTeammate)
			{
				AddBlip(Pawn->GetActorLocation(), Origin, YawDeg, Range, ERadarBlipKind::Teammate);
				continue;
			}

			const float DistSq = FVector::DistSquared(Pawn->GetActorLocation(), ViewPawn->GetActorLocation());
			const float SpotRange = (DisplayMode == ERadarDisplayMode::FullMap) ? Range : RadarRange;
			if (DistSq > SpotRange * SpotRange) continue;

			bool bShow = false;
			if (const float* Until = SpottedUntil.Find(TWeakObjectPtr<APawn>(Pawn)))
			{
				bShow = (*Until >= World->GetTimeSeconds());
			}
			if (!bShow && HasLineOfSightTo(ViewPawn, Pawn))
			{
				bShow = true;
			}
			if (bShow)
			{
				AddBlip(Pawn->GetActorLocation(), Origin, YawDeg, Range, ERadarBlipKind::Enemy);
			}
		}
	}

	if (ASearchDestroyGameState* SDGS = Cast<ASearchDestroyGameState>(GS))
	{
		for (TActorIterator<ABombSite> It(World); It; ++It)
		{
			ABombSite* Site = *It;
			if (!IsValid(Site)) continue;
			const ERadarBlipKind Kind = (Site->GetSiteId() == EBombSiteId::B) ? ERadarBlipKind::SiteB : ERadarBlipKind::SiteA;
			AddBlip(Site->GetActorLocation(), Origin, YawDeg, Range, Kind);
		}

		if (ABombActor* Bomb = SDGS->GetBomb())
		{
			FVector BombLoc = Bomb->GetActorLocation();
			if (APawn* Carrier = Bomb->GetCarrier())
			{
				BombLoc = Carrier->GetActorLocation();
			}
			if (Bomb->IsPlanted() || Bomb->IsDropped() || Bomb->GetCarrier())
			{
				AddBlip(BombLoc, Origin, YawDeg, Range, ERadarBlipKind::Bomb);
			}
		}
	}
}

static const FSlateBrush* RadarWhiteBrush()
{
	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("GenericWhiteBox");
	if (!Brush)
	{
		Brush = FAppStyle::GetBrush("WhiteBrush");
	}
	return Brush;
}

static void DrawFilledBox(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geo,
	const FVector2D& Center, const FVector2D& HalfSize, const FLinearColor& Color)
{
	const FSlateBrush* Brush = RadarWhiteBrush();
	if (!Brush) return;
	const FVector2D TopLeft = Center - HalfSize;
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		Geo.ToPaintGeometry(HalfSize * 2.f, FSlateLayoutTransform(TopLeft)),
		Brush,
		ESlateDrawEffect::None,
		Color);
}

int32 URadarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Base = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X < 8.f || Size.Y < 8.f) return Base;

	const bool bFull = (DisplayMode == ERadarDisplayMode::FullMap);
	const FVector2D Center = Size * 0.5f;
	const float Radius = FMath::Min(Size.X, Size.Y) * (bFull ? 0.46f : 0.48f);
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
	const FVector2D Half(Radius, Radius);
	const FVector2D TopLeft = Center - Half;

	if (bFull)
	{
		DrawFilledBox(OutDrawElements, Base + 1, AllottedGeometry, Center, Size * 0.5f, FullMapPanelColor * Tint);
	}
	else
	{
		DrawFilledBox(OutDrawElements, Base + 1, AllottedGeometry, Center, Half, BackgroundColor * Tint);
	}

	if (bShowMapBackground && bMapReady && IsValid(MapRT) && MapBrush.GetResourceObject())
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			Base + 2,
			AllottedGeometry.ToPaintGeometry(Half * 2.f, FSlateLayoutTransform(TopLeft)),
			&MapBrush,
			ESlateDrawEffect::None,
			MapTint * Tint);
	}

	{
		TArray<FVector2D> HLine = { FVector2D(Center.X - Radius, Center.Y), FVector2D(Center.X + Radius, Center.Y) };
		TArray<FVector2D> VLine = { FVector2D(Center.X, Center.Y - Radius), FVector2D(Center.X, Center.Y + Radius) };
		FSlateDrawElement::MakeLines(OutDrawElements, Base + 4, AllottedGeometry.ToPaintGeometry(), HLine, ESlateDrawEffect::None, CrosshairColor * Tint, true, 1.f);
		FSlateDrawElement::MakeLines(OutDrawElements, Base + 4, AllottedGeometry.ToPaintGeometry(), VLine, ESlateDrawEffect::None, CrosshairColor * Tint, true, 1.f);
	}

	{
		TArray<FVector2D> Ring;
		const int32 Segs = 48;
		Ring.Reserve(Segs + 1);
		for (int32 i = 0; i <= Segs; ++i)
		{
			const float A = (2.f * PI * static_cast<float>(i)) / static_cast<float>(Segs);
			Ring.Add(FVector2D(Center.X + FMath::Cos(A) * Radius, Center.Y + FMath::Sin(A) * Radius));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, Base + 5, AllottedGeometry.ToPaintGeometry(), Ring, ESlateDrawEffect::None, RingColor * Tint, true, bFull ? 2.f : 1.5f);
	}

	auto ColorFor = [this](ERadarBlipKind Kind) -> FLinearColor
	{
		switch (Kind)
		{
		case ERadarBlipKind::Local: return LocalColor;
		case ERadarBlipKind::Teammate: return TeammateColor;
		case ERadarBlipKind::Enemy: return EnemyColor;
		case ERadarBlipKind::Bomb: return BombColor;
		case ERadarBlipKind::SiteA:
		case ERadarBlipKind::SiteB: return SiteColor;
		default: return LocalColor;
		}
	};

	for (const FRadarBlip& Blip : Blips)
	{
		const FVector2D P(Center.X + Blip.Pos.X * Radius, Center.Y + Blip.Pos.Y * Radius);
		const FLinearColor C = ColorFor(Blip.Kind) * Tint;

		if (Blip.Kind == ERadarBlipKind::Local)
		{
			const float Scale = bFull ? 1.35f : 1.f;
			// Arrow points "up" in widget space; rotate by FacingYaw for north-up full map
			const float Rad = FMath::DegreesToRadians(Blip.FacingYaw);
			const float CosA = FMath::Cos(Rad);
			const float SinA = FMath::Sin(Rad);
			auto Rot = [P, CosA, SinA, Scale](float X, float Y) -> FVector2D
			{
				const float SX = X * Scale;
				const float SY = Y * Scale;
				// FacingYaw 0 = north = up (-Y). Positive yaw turns clockwise in UE → rotate in screen space.
				return FVector2D(P.X + SX * CosA - SY * SinA, P.Y + SX * SinA + SY * CosA);
			};
			TArray<FVector2D> Arrow = {
				Rot(0.f, -7.f),
				Rot(-5.f, 5.f),
				Rot(0.f, 2.f),
				Rot(5.f, 5.f),
				Rot(0.f, -7.f)
			};
			FSlateDrawElement::MakeLines(OutDrawElements, Base + 6, AllottedGeometry.ToPaintGeometry(), Arrow, ESlateDrawEffect::None, C, true, 2.f);
			continue;
		}

		if (Blip.Kind == ERadarBlipKind::SiteA || Blip.Kind == ERadarBlipKind::SiteB)
		{
			const float S = bFull ? 7.f : 5.f;
			TArray<FVector2D> Dia = {
				FVector2D(P.X, P.Y - S),
				FVector2D(P.X + S, P.Y),
				FVector2D(P.X, P.Y + S),
				FVector2D(P.X - S, P.Y),
				FVector2D(P.X, P.Y - S)
			};
			FSlateDrawElement::MakeLines(OutDrawElements, Base + 6, AllottedGeometry.ToPaintGeometry(), Dia, ESlateDrawEffect::None, C, true, 1.5f);
			continue;
		}

		if (Blip.Kind == ERadarBlipKind::Bomb)
		{
			const float Hs = bFull ? 5.5f : 4.5f;
			DrawFilledBox(OutDrawElements, Base + 7, AllottedGeometry, P, FVector2D(Hs, Hs), C);
			continue;
		}

		const float HalfBlip = (Blip.Kind == ERadarBlipKind::Enemy) ? (bFull ? 4.5f : 3.5f) : (bFull ? 4.f : 3.f);
		DrawFilledBox(OutDrawElements, Base + 7, AllottedGeometry, P, FVector2D(HalfBlip, HalfBlip), C);
	}

	return Base + 8;
}
