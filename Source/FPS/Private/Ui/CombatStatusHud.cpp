#include "Ui/CombatStatusHud.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"
#include "GameFramework/PlayerController.h"
#include "Stats/StatsComponent.h"
#include "CombatComponent.h"
#include "ShooterCharacter.h"
#include "Weapon/Weapon.h"

namespace CombatStatusHudPrivate
{
	static const FLinearColor OutlineTint(1.f, 1.f, 1.f, 0.92f);
	static const FLinearColor HealthyZone(1.f, 1.f, 1.f, 0.f);      // hidden when healthy
	static const FLinearColor InjuredZone(0.86f, 0.08f, 0.08f, 0.95f); // RoN solid red
	static const FLinearColor VestIdle(1.f, 1.f, 1.f, 0.85f);
	static const FVector2D DollSize(110.f, 170.f);
	static const FVector2D PanelSize(320.f, 210.f);
}

void UCombatStatusHud::NativeConstruct()
{
	Super::NativeConstruct();
	BuildUi();
	EnsureViewportSlot();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	ForceShow();
}

void UCombatStatusHud::ForceShow()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!IsInViewport())
	{
		AddToViewport(40);
	}
	EnsureViewportSlot();
}

void UCombatStatusHud::EnsureViewportSlot()
{
	// Fullscreen root canvas; children anchor themselves (doll bottom-left, mags bottom-right).
}

UTexture2D* UCombatStatusHud::LoadTex(const TCHAR* Path) const
{
	if (UTexture2D* Existing = LoadObject<UTexture2D>(nullptr, Path))
	{
		return Existing;
	}
	// Runtime PNG fallback (Content/FPS/UI/<AssetName>.png) so editor need not pre-import.
	FString AssetPath(Path);
	FString AssetName = FPackageName::GetShortName(AssetPath);
	const FString PngPath = FPaths::ProjectContentDir() / TEXT("FPS/UI") / (AssetName + TEXT(".png"));
	if (FPaths::FileExists(PngPath))
	{
		return FImageUtils::ImportFileAsTexture2D(PngPath);
	}
	return nullptr;
}

UImage* UCombatStatusHud::MakeImage(UCanvasPanel* Canvas, const FString& Name, UTexture2D* Tex, FVector2D Pos, FVector2D Size, FLinearColor Tint)
{
	UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*Name));
	if (Tex)
	{
		Img->SetBrushFromTexture(Tex, true);
	}
	Img->SetColorAndOpacity(Tint);
	Img->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Img))
	{
		S->SetAutoSize(false);
		S->SetPosition(Pos);
		S->SetSize(Size);
		S->SetZOrder(1);
	}
	return Img;
}

void UCombatStatusHud::BuildUi()
{
	if (bBuilt || !WidgetTree) return;
	bBuilt = true;

	using namespace CombatStatusHudPrivate;

	TexOutline = LoadTex(TEXT("/Game/FPS/UI/T_BodyOutline.T_BodyOutline"));
	TexHead    = LoadTex(TEXT("/Game/FPS/UI/T_Zone_Head.T_Zone_Head"));
	TexTorso   = LoadTex(TEXT("/Game/FPS/UI/T_Zone_Torso.T_Zone_Torso"));
	TexArmL    = LoadTex(TEXT("/Game/FPS/UI/T_Zone_ArmL.T_Zone_ArmL"));
	TexArmR    = LoadTex(TEXT("/Game/FPS/UI/T_Zone_ArmR.T_Zone_ArmR"));
	TexLegL    = LoadTex(TEXT("/Game/FPS/UI/T_Zone_LegL.T_Zone_LegL"));
	TexLegR    = LoadTex(TEXT("/Game/FPS/UI/T_Zone_LegR.T_Zone_LegR"));
	// Fallback to older apose if new assets missing
	if (!TexOutline)
	{
		TexOutline = LoadTex(TEXT("/Game/FPS/UI/T_BodyApose.T_BodyApose"));
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
	PanelBorder->SetPadding(FMargin(10.f));
	if (UCanvasPanelSlot* BS = RootCanvas->AddChildToCanvas(PanelBorder))
	{
		BS->SetAnchors(FAnchors(0.f, 1.f, 0.f, 1.f));
		BS->SetAlignment(FVector2D(0.f, 1.f));
		BS->SetPosition(FVector2D(24.f, -24.f));
		BS->SetSize(FVector2D(150.f, 220.f));
	}

	UCanvasPanel* Inner = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InnerCanvas"));
	PanelBorder->SetContent(Inner);

	const FVector2D DollPos(8.f, 8.f);

	// Injured fills UNDER outline
	ZoneHead  = MakeImage(Inner, TEXT("ZoneHead"),  TexHead,  DollPos, DollSize, HealthyZone);
	ZoneTorso = MakeImage(Inner, TEXT("ZoneTorso"), TexTorso, DollPos, DollSize, HealthyZone);
	ZoneArmR  = MakeImage(Inner, TEXT("ZoneArmR"),  TexArmR,  DollPos, DollSize, HealthyZone);
	ZoneArmL  = MakeImage(Inner, TEXT("ZoneArmL"),  TexArmL,  DollPos, DollSize, HealthyZone);
	ZoneLegR  = MakeImage(Inner, TEXT("ZoneLegR"),  TexLegR,  DollPos, DollSize, HealthyZone);
	ZoneLegL  = MakeImage(Inner, TEXT("ZoneLegL"),  TexLegL,  DollPos, DollSize, HealthyZone);

	OutlineImage = MakeImage(Inner, TEXT("Outline"), TexOutline, DollPos, DollSize, OutlineTint);
	if (UCanvasPanelSlot* OS = Cast<UCanvasPanelSlot>(OutlineImage->Slot))
	{
		OS->SetZOrder(5);
	}

	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	HealthBar->SetPercent(1.f);
	FProgressBarStyle Style = HealthBar->GetWidgetStyle();
	Style.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f));
	Style.FillImage.TintColor = FSlateColor(FLinearColor(0.85f, 0.15f, 0.12f, 0.95f));
	HealthBar->SetWidgetStyle(Style);
	if (UCanvasPanelSlot* HS = Inner->AddChildToCanvas(HealthBar))
	{
		HS->SetPosition(FVector2D(8.f, 186.f));
		HS->SetSize(FVector2D(110.f, 6.f));
		HS->SetZOrder(6);
	}

	MagRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MagRow"));
	if (UCanvasPanelSlot* MS = RootCanvas->AddChildToCanvas(MagRow))
	{
		MS->SetAnchors(FAnchors(1.f, 1.f, 1.f, 1.f));
		MS->SetAlignment(FVector2D(1.f, 1.f));
		MS->SetPosition(FVector2D(-40.f, -40.f));
		MS->SetAutoSize(true);
		MS->SetZOrder(6);
	}

	AmmoFallbackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AmmoFallback"));
	AmmoFallbackText->SetText(FText::FromString(TEXT("--")));
	AmmoFallbackText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo Font = AmmoFallbackText->GetFont();
	Font.Size = 18;
	AmmoFallbackText->SetFont(Font);
	if (UCanvasPanelSlot* AS = RootCanvas->AddChildToCanvas(AmmoFallbackText))
	{
		AS->SetAnchors(FAnchors(1.f, 1.f, 1.f, 1.f));
		AS->SetAlignment(FVector2D(1.f, 1.f));
		AS->SetPosition(FVector2D(-40.f, -120.f));
		AS->SetAutoSize(true);
		AS->SetZOrder(6);
	}
}

void UCombatStatusHud::SetZoneInjured(UImage* ZoneImg, bool bInjured)
{
	if (!ZoneImg) return;
	ZoneImg->SetColorAndOpacity(bInjured ? CombatStatusHudPrivate::InjuredZone : CombatStatusHudPrivate::HealthyZone);
}

void UCombatStatusHud::RebuildMagRow(int32 MagCount, int32 RoundsInMag, int32 MagCapacity, int32 ReserveRounds)
{
	if (!MagRow || !WidgetTree) return;
	MagRow->ClearChildren();

	TArray<float> Fills;
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AShooterCharacter* Char = Cast<AShooterCharacter>(PC->GetPawn()))
		{
			if (UCombatComponent* Combat = Char->FindComponentByClass<UCombatComponent>())
			{
				Combat->GetHudMagazineFills(Fills);
			}
		}
	}
	if (Fills.Num() == 0)
	{
		const int32 Cap = FMath::Max(1, MagCapacity);
		Fills.Add(float(FMath::Clamp(RoundsInMag, 0, Cap)) / float(Cap));
		int32 ReserveLeft = FMath::Max(0, ReserveRounds);
		// ReserveRounds here may be spare-mag count; treat as full mags if Cap unknown path
		for (int32 i = 0; i < FMath::Min(ReserveLeft, 4); ++i)
		{
			Fills.Add(1.f);
		}
	}

	const int32 TotalMags = FMath::Clamp(Fills.Num(), 1, 8);
	for (int32 i = 0; i < TotalMags; ++i)
	{
		const float Fill = FMath::Clamp(Fills[i], 0.f, 1.f);

		UOverlay* Cell = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Bg->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.18f));
		UBorder* Fg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Fg->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.95f));

		USizeBox* FillBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		FillBox->SetWidthOverride(10.f);
		FillBox->SetHeightOverride(56.f * Fill);
		FillBox->SetContent(Fg);

		Cell->AddChildToOverlay(Bg);
		if (UOverlaySlot* BgS = Cast<UOverlaySlot>(Bg->Slot))
		{
			BgS->SetHorizontalAlignment(HAlign_Fill);
			BgS->SetVerticalAlignment(VAlign_Fill);
		}
		if (UOverlaySlot* Fs = Cell->AddChildToOverlay(FillBox))
		{
			Fs->SetHorizontalAlignment(HAlign_Fill);
			Fs->SetVerticalAlignment(VAlign_Bottom);
		}

		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(10.f);
		Box->SetHeightOverride(56.f);
		Box->SetContent(Cell);

		if (UHorizontalBoxSlot* HS = MagRow->AddChildToHorizontalBox(Box))
		{
			HS->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
			HS->SetVerticalAlignment(VAlign_Bottom);
		}
	}

	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Label->SetText(FText::FromString(TEXT("AP")));
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	if (UHorizontalBoxSlot* LS = MagRow->AddChildToHorizontalBox(Label))
	{
		LS->SetVerticalAlignment(VAlign_Bottom);
		LS->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f));
	}
}


void UCombatStatusHud::RefreshFromPawn()
{
	APlayerController* PC = GetOwningPlayer();
	AShooterCharacter* Char = PC ? Cast<AShooterCharacter>(PC->GetPawn()) : nullptr;
	if (!Char)
	{
		return;
	}

	// Injuries: current API is combined arm/leg. Light both sides when injured (RoN lights the hit side;
	// without L/R replication we approximate both until side-specific flags exist).
	const bool bArm = Char->IsArmInjured();
	const bool bLeg = Char->IsLegInjured();
	SetZoneInjured(ZoneArmL, bArm);
	SetZoneInjured(ZoneArmR, bArm);
	SetZoneInjured(ZoneLegL, bLeg);
	SetZoneInjured(ZoneLegR, bLeg);

	float HP = 1.f;
	if (UStatsComponent* StatsComp = UStatsComponent::FindStatsComponent(Char))
	{
		HP = StatsComp->GetHealthNormolize();
	}
	if (HealthBar) HealthBar->SetPercent(HP);
	// Soft flash torso/head from overall HP until per-zone HP exists
	SetZoneInjured(ZoneTorso, HP < 0.55f);
	SetZoneInjured(ZoneHead, HP < 0.2f);

	int32 Rounds = 0, Reserve = 0, Cap = 30;
	if (UCombatComponent* Combat = Char->FindComponentByClass<UCombatComponent>())
	{
		if (AWeapon* W = Combat->CurrentWeapon)
		{
			Rounds = W->Ammo;
			Cap = FMath::Max(1, W->MagCapacity);
		}
		Reserve = Combat->CurrentReserveAmmo;
	}

	if (AmmoFallbackText)
	{
		AmmoFallbackText->SetText(FText::FromString(FString::Printf(TEXT("%d  |  %d"), Rounds, Reserve)));
		AmmoFallbackText->SetVisibility(MagRow && MagRow->GetChildrenCount() > 0 ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	const int32 MagKey = (Rounds & 0xFF) | ((Reserve & 0xFFF) << 8) | ((Cap & 0xFF) << 20);
	if (MagKey != LastMagKey)
	{
		LastMagKey = MagKey;
		RebuildMagRow(0, Rounds, Cap, Reserve);
	}
}

void UCombatStatusHud::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsInViewport())
	{
		ForceShow();
	}
	RefreshFromPawn();
}
