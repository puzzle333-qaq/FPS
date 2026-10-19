#include "Ui/KillFeedWidget.h"

#include "Game/ShooterGameState.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Styling/SlateColor.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UKillFeedEntry::NativePreConstruct()
{
	Super::NativePreConstruct();
	ResolveWidgets();
	if (Text_Separator)
	{
		Text_Separator->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UKillFeedEntry::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveWidgets();
	if (Text_Separator)
	{
		Text_Separator->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UKillFeedEntry::ResolveWidgets()
{
	if (!Text_Killer) Text_Killer = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Killer")));
	if (!Text_Victim) Text_Victim = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Victim")));
	if (!Text_Separator) Text_Separator = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Separator")));
	if (!Image_Weapon)
	{
		Image_Weapon = Cast<UImage>(GetWidgetFromName(TEXT("Image_Weapon")));
		if (!Image_Weapon) Image_Weapon = Cast<UImage>(GetWidgetFromName(TEXT("Image_Icon")));
	}
}

void UKillFeedEntry::Setup(const FString& KillerName, uint8 KillerTeam, const FString& VictimName, uint8 VictimTeam, UTexture2D* WeaponIcon, const FString& LocalPlayerName)
{
	ResolveWidgets();

	const bool bSuicide = KillerName.IsEmpty() || KillerName.Equals(VictimName, ESearchCase::CaseSensitive);
	const bool bKillerLocal = !LocalPlayerName.IsEmpty() && KillerName.Equals(LocalPlayerName, ESearchCase::CaseSensitive);
	const bool bVictimLocal = !LocalPlayerName.IsEmpty() && VictimName.Equals(LocalPlayerName, ESearchCase::CaseSensitive);

	if (Text_Killer)
	{
		if (bSuicide)
		{
			Text_Killer->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			ApplyNameStyle(Text_Killer, KillerName, ColorFor(KillerTeam, bKillerLocal));
		}
	}

	if (Text_Victim)
	{
		ApplyNameStyle(Text_Victim, VictimName, ColorFor(VictimTeam, bVictimLocal));
	}

	if (Text_Separator)
	{
		Text_Separator->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (Image_Weapon)
	{
		if (IsValid(WeaponIcon))
		{
			Image_Weapon->SetBrushFromTexture(WeaponIcon);
			Image_Weapon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_Weapon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	OnEntryReady(KillerName, KillerTeam, VictimName, VictimTeam, bSuicide);
}

void UKillFeedEntry::ApplyNameStyle(UTextBlock* Text, const FString& Name, const FLinearColor& Color)
{
	if (!Text) return;
	Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	Text->SetAutoWrapText(true);
	Text->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
	Text->SetWrapTextAt(NameWrapWidth);
	Text->SetText(FText::FromString(Name));
	Text->SetColorAndOpacity(FSlateColor(Color));
}

FLinearColor UKillFeedEntry::ColorFor(uint8 TeamId, bool bIsLocalName) const
{
	if (bIsLocalName) return LocalPlayerColor;
	if (TeamId == 0) return TeamAColor;
	if (TeamId == 1) return TeamBColor;
	return NeutralColor;
}

void UKillFeedWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!FeedList) FeedList = Cast<UPanelWidget>(GetWidgetFromName(TEXT("FeedList")));
	if (!EntryClass)
	{
		EntryClass = LoadClass<UKillFeedEntry>(nullptr, TEXT("/Game/FPS/UI/WBP_KillFeedEntry.WBP_KillFeedEntry_C"));
	}

	if (UWorld* World = GetWorld())
	{
		if (AShooterGameState* GS = World->GetGameState<AShooterGameState>())
		{
			BindGameState(GS);
		}
		World->GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSet);
	}
}

void UKillFeedWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.RemoveAll(this);
		for (FFeedItem& Item : Items)
		{
			World->GetTimerManager().ClearTimer(Item.Handle);
		}
	}
	Items.Reset();
	UnbindGameState();
	Super::NativeDestruct();
}

void UKillFeedWidget::HandleGameStateSet(AGameStateBase* NewGameState)
{
	BindGameState(Cast<AShooterGameState>(NewGameState));
}

void UKillFeedWidget::BindGameState(AShooterGameState* GS)
{
	if (BoundGameState.Get() == GS) return;
	UnbindGameState();
	BoundGameState = GS;
	if (IsValid(GS))
	{
		GS->OnKillFeed.AddUniqueDynamic(this, &ThisClass::HandleKillFeed);
	}
}

void UKillFeedWidget::UnbindGameState()
{
	if (BoundGameState.IsValid())
	{
		BoundGameState->OnKillFeed.RemoveDynamic(this, &ThisClass::HandleKillFeed);
	}
	BoundGameState.Reset();
}

FString UKillFeedWidget::GetLocalPlayerName() const
{
	const APlayerController* PC = GetOwningPlayer();
	const APlayerState* PS = IsValid(PC) ? PC->GetPlayerState<APlayerState>() : nullptr;
	return PS ? PS->GetPlayerName() : FString();
}

void UKillFeedWidget::HandleKillFeed(const FString& KillerName, uint8 KillerTeam, const FString& VictimName, uint8 VictimTeam, UTexture2D* WeaponIcon)
{
	AddEntry(KillerName, KillerTeam, VictimName, VictimTeam, WeaponIcon);
}

void UKillFeedWidget::AddEntry(const FString& KillerName, uint8 KillerTeam, const FString& VictimName, uint8 VictimTeam, UTexture2D* WeaponIcon)
{
	if (!FeedList)
	{
		FeedList = Cast<UPanelWidget>(GetWidgetFromName(TEXT("FeedList")));
	}
	if (!FeedList)
	{
		UE_LOG(LogTemp, Warning, TEXT("[KillFeed] FeedList is missing on %s. Name the Vertical Box 'FeedList'."), *GetName());
		return;
	}
	if (!EntryClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[KillFeed] EntryClass is not set on %s."), *GetName());
		return;
	}

	UKillFeedEntry* Entry = CreateWidget<UKillFeedEntry>(this, EntryClass);
	if (!IsValid(Entry)) return;
	Entry->Setup(KillerName, KillerTeam, VictimName, VictimTeam, WeaponIcon, GetLocalPlayerName());
	FeedList->InsertChildAt(0, Entry);

	FFeedItem Item;
	Item.Entry = Entry;
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UKillFeedEntry> WeakEntry = Entry;
		World->GetTimerManager().SetTimer(Item.Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakEntry]()
		{
			if (WeakEntry.IsValid()) RemoveEntry(WeakEntry.Get());
		}), EntryLifetime, false);
	}
	Items.Insert(Item, 0);

	while (FeedList->GetChildrenCount() > MaxEntries)
	{
		if (UWidget* Oldest = FeedList->GetChildAt(FeedList->GetChildrenCount() - 1))
		{
			RemoveEntry(Cast<UKillFeedEntry>(Oldest));
		}
		else
		{
			break;
		}
	}
}

void UKillFeedWidget::RemoveEntry(UKillFeedEntry* Entry)
{
	if (!IsValid(Entry)) return;
	if (UWorld* World = GetWorld())
	{
		for (int32 i = Items.Num() - 1; i >= 0; --i)
		{
			if (Items[i].Entry.Get() == Entry)
			{
				World->GetTimerManager().ClearTimer(Items[i].Handle);
				Items.RemoveAt(i);
			}
		}
	}
	if (FeedList)
	{
		FeedList->RemoveChild(Entry);
	}
	Entry->RemoveFromParent();
}
