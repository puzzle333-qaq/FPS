#include "Ui/BuyShopScreen.h"

#include "ShooterPlayerController.h"
#include "Game/ShooterPlayerState.h"
#include "Game/SearchDestroyGameMode.h"
#include "Game/SearchDestroyGameState.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

void UBuyShopScreen::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}
	BindLocalMoney();
	OnCatalogUpdated();
	OnShopOpened();
}

void UBuyShopScreen::NativeDestruct()
{
	UnbindLocalMoney();
	OnShopClosed();
	Super::NativeDestruct();
}

FReply UBuyShopScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::B || InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseShop();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UBuyShopScreen::HandleCloseClicked()
{
	CloseShop();
}

void UBuyShopScreen::CloseShop()
{
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		PC->CloseBuyShop();
	}
	else
	{
		RemoveFromParent();
	}
}

bool UBuyShopScreen::BuyItem(int32 CatalogIndex)
{
	AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer());
	if (!IsValid(PC)) return false;
	return PC->RequestBuyShopItem(CatalogIndex);
}

TArray<FBuyShopItem> UBuyShopScreen::GetCatalog() const
{
	if (const AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		return PC->GetCachedBuyCatalog();
	}
	if (const UWorld* World = GetWorld())
	{
		if (const ASearchDestroyGameState* GS = World->GetGameState<ASearchDestroyGameState>())
		{
			return GS->GetBuyCatalog();
		}
	}
	return TArray<FBuyShopItem>();
}

int32 UBuyShopScreen::GetLocalMoney() const
{
	const APlayerController* PC = GetOwningPlayer();
	const AShooterPlayerState* PS = IsValid(PC) ? PC->GetPlayerState<AShooterPlayerState>() : nullptr;
	return PS ? PS->GetMoney() : 0;
}

bool UBuyShopScreen::CanBuyNow() const
{
	const AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer());
	return IsValid(PC) && PC->CanOpenBuyShop();
}

void UBuyShopScreen::BindLocalMoney()
{
	UnbindLocalMoney();
	APlayerController* PC = GetOwningPlayer();
	AShooterPlayerState* PS = IsValid(PC) ? PC->GetPlayerState<AShooterPlayerState>() : nullptr;
	BoundPlayerState = PS;
	if (IsValid(PS))
	{
		PS->OnMoneyChanged.AddUniqueDynamic(this, &ThisClass::HandleMoneyChanged);
		HandleMoneyChanged(PS->GetMoney());
	}
}

void UBuyShopScreen::UnbindLocalMoney()
{
	if (BoundPlayerState.IsValid())
	{
		BoundPlayerState->OnMoneyChanged.RemoveDynamic(this, &ThisClass::HandleMoneyChanged);
	}
	BoundPlayerState.Reset();
}

void UBuyShopScreen::HandleMoneyChanged(int32 NewMoney)
{
	if (Text_Money)
	{
		Text_Money->SetText(FText::AsNumber(NewMoney));
	}
	OnMoneyUpdated(NewMoney);
}