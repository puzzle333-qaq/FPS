#include "Ui/CombatHudKeepAlive.h"

#include "Blueprint/WidgetTree.h"
#include "UObject/UObjectIterator.h"
#include "Components/Widget.h"
#include "Ui/MatchHUD.h"
#include "Ui/CombatStatusHud.h"
#include "ShooterPlayerController.h"

namespace
{
	void ForceVisible(UWidget* W)
	{
		if (!IsValid(W)) return;
		const ESlateVisibility V = W->GetVisibility();
		if (V == ESlateVisibility::Collapsed || V == ESlateVisibility::Hidden)
		{
			W->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	void ForceCollapsed(UWidget* W)
	{
		if (!IsValid(W)) return;
		if (W->GetVisibility() != ESlateVisibility::Collapsed)
		{
			W->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	void ForceNamedChild(UUserWidget* Root, FName LeafName, bool bShow)
	{
		if (!IsValid(Root)) return;
		if (UWidget* Child = Root->GetWidgetFromName(LeafName))
		{
			if (bShow) ForceVisible(Child);
			else ForceCollapsed(Child);
		}
	}
}

UCombatHudKeepAlive::UCombatHudKeepAlive(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasScriptImplementedTick = true;
}

void UCombatHudKeepAlive::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UE_LOG(LogTemp, Warning, TEXT("[HUD] CombatHudKeepAlive NativeConstruct"));
}

void UCombatHudKeepAlive::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (GetVisibility() != ESlateVisibility::HitTestInvisible)
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	APlayerController* PC = GetOwningPlayer();
	if (!IsValid(PC)) return;

	ReensureAccum += InDeltaTime;
	if (ReensureAccum >= 0.25f)
	{
		ReensureAccum = 0.f;
		if (AShooterPlayerController* SPC = Cast<AShooterPlayerController>(PC))
		{
			SPC->EnsureCombatHudKeepAlive();
		}
	}

	if (!CachedOverlay.IsValid())
	{
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			UUserWidget* W = *It;
			if (!IsValid(W) || W->GetOwningPlayer() != PC) continue;
			const FString N = W->GetClass()->GetName();
			if (N.Contains(TEXT("ShooterOverlay")))
			{
				CachedOverlay = W;
				UE_LOG(LogTemp, Warning, TEXT("[HUD] KeepAlive found overlay %s"), *N);
				break;
			}
		}
	}

	// Strip MatchHUD if BP still creates it after C++ EnsureMatchHUD no-op.
	for (TObjectIterator<UMatchHUD> It; It; ++It)
	{
		UMatchHUD* Hud = *It;
		if (!IsValid(Hud) || Hud->GetOwningPlayer() != PC) continue;
		Hud->RemoveFromParent();
	}

	bool bStatusOk = false;
	for (TObjectIterator<UCombatStatusHud> It; It; ++It)
	{
		UCombatStatusHud* Status = *It;
		if (!IsValid(Status) || Status->GetOwningPlayer() != PC) continue;
		Status->RemoveFromParent(); // RoN HUD disabled
	}

	if (UUserWidget* Overlay = CachedOverlay.Get())
	{
		ForceVisible(Overlay);
		// PRIORITY: never leave player HUD-less.
		// Only hide old health/ammo chrome when CombatStatusHud is actually on screen.
		const bool bShowLegacy = !bStatusOk;
		ForceNamedChild(Overlay, TEXT("WB_Health"), bShowLegacy);
		ForceNamedChild(Overlay, TEXT("WBP_ReserveAmmo"), bShowLegacy);
		ForceNamedChild(Overlay, TEXT("Health"), bShowLegacy);
		ForceNamedChild(Overlay, TEXT("ReserveAmmo"), bShowLegacy);
		ForceNamedChild(Overlay, TEXT("Ammo"), bShowLegacy);
		if (UWidgetTree* Tree = Overlay->WidgetTree)
		{
			Tree->ForEachWidget([bShowLegacy](UWidget* Child)
			{
				if (!IsValid(Child)) return;
				const FString N = Child->GetName();
				if (N.Contains(TEXT("Health")) || N.Contains(TEXT("Ammo")) || N.Contains(TEXT("Reserve")))
				{
					if (bShowLegacy) ForceVisible(Child);
					else ForceCollapsed(Child);
				}
			});
		}
		if (bShowLegacy)
		{
			static bool bLoggedLegacy = false;
			if (!bLoggedLegacy)
			{
				bLoggedLegacy = true;
				UE_LOG(LogTemp, Warning, TEXT("[HUD] CombatStatusHud missing — restoring WBP_ShooterOverlay health/ammo"));
			}
		}
	}
}
