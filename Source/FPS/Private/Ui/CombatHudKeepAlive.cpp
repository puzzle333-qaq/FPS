#include "Ui/CombatHudKeepAlive.h"

#include "Blueprint/WidgetTree.h"
#include "UObject/UObjectIterator.h"
#include "Components/Widget.h"
#include "Ui/CombatStatusHud.h"
#include "ShooterPlayerController.h"
#include "Game/CoopCqbGameState.h"

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

	bool bStatusOk = false;
	for (TObjectIterator<UCombatStatusHud> It; It; ++It)
	{
		UCombatStatusHud* Status = *It;
		if (!IsValid(Status) || Status->GetOwningPlayer() != PC) continue;
		Status->RemoveFromParent(); // RoN HUD disabled
	}

	if (UUserWidget* Overlay = CachedOverlay.Get())
	{
        const auto* Flow=GetWorld()->GetGameState<ACoopCqbGameState>();
        const bool bRecon=Flow&&!Flow->bRangeHub&&(Flow->GetCoopPhase()==ECoopCqbPhase::Waiting||Flow->GetCoopPhase()==ECoopCqbPhase::Briefing);
        if(bRecon) ForceCollapsed(Overlay);else ForceVisible(Overlay);
		// Health bar removed by request. Never restore WB_Health / Health.
		// Keep reserve ammo when CombatStatusHud is off (it is always off).
		const bool bShowAmmo = false;
		ForceNamedChild(Overlay, TEXT("WB_Health"), false);
		ForceNamedChild(Overlay, TEXT("Health"), false);
		ForceNamedChild(Overlay, TEXT("WB_Stamina"), false);
		ForceNamedChild(Overlay, TEXT("Stamina"), false);
		ForceNamedChild(Overlay, TEXT("WBP_ReserveAmmo"), bShowAmmo);
		ForceNamedChild(Overlay, TEXT("ReserveAmmo"), bShowAmmo);
		ForceNamedChild(Overlay, TEXT("Ammo"), bShowAmmo);
		if (UWidgetTree* Tree = Overlay->WidgetTree)
		{
			Tree->ForEachWidget([bShowAmmo](UWidget* Child)
			{
				if (!IsValid(Child)) return;
				const FString N = Child->GetName();
				if (N.Contains(TEXT("BodyInjury")) || N.Contains(TEXT("Watch")) || N.Contains(TEXT("Stamina")) || (N.Contains(TEXT("Health")) && !N.Contains(TEXT("BodyInjury"))))
				{
					ForceCollapsed(Child);
					return;
				}
				if (N.Contains(TEXT("Ammo")) || N.Contains(TEXT("Reserve")))
				{
					if (bShowAmmo) ForceVisible(Child);
					else ForceCollapsed(Child);
				}
			});
		}
	}
}
