#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Texture2D.h"
#include "KillFeedWidget.generated.h"

class UTextBlock;
class UImage;
class UPanelWidget;
class AShooterGameState;

// 一条击杀播报。WBP_KillFeedEntry 选它当父类。布局：击杀者 | 武器图标 | 被击杀者
UCLASS()
class FPS_API UKillFeedEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FString& KillerName, uint8 KillerTeam, const FString& VictimName, uint8 VictimTeam, UTexture2D* WeaponIcon, const FString& LocalPlayerName);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Killer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Victim;

	// 旧的“击杀”文字，有就藏掉
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Separator;

	// 中间武器图标，名字必须是 Image_Weapon
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Weapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|KillFeed")
	FLinearColor TeamAColor = FLinearColor(0.2f, 0.55f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|KillFeed")
	FLinearColor TeamBColor = FLinearColor(1.f, 0.25f, 0.2f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|KillFeed")
	FLinearColor NeutralColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|KillFeed")
	FLinearColor LocalPlayerColor = FLinearColor(1.f, 0.85f, 0.2f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|KillFeed", meta = (ClampMin = "40"))
	float NameWrapWidth = 120.f;

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|HUD|KillFeed")
	void OnEntryReady(const FString& KillerName, uint8 KillerTeam, const FString& VictimName, uint8 VictimTeam, bool bSuicide);

private:
	void ResolveWidgets();
	void ApplyNameStyle(UTextBlock* Text, const FString& Name, const FLinearColor& Color);
	FLinearColor ColorFor(uint8 TeamId, bool bIsLocalName) const;
};

// 通用击杀播报列表。所有模式共用，WBP_KillFeed 选它当父类。
UCLASS()
class FPS_API UKillFeedWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|KillFeed")
	TSubclassOf<UKillFeedEntry> EntryClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|KillFeed", meta = (ClampMin = "1", ClampMax = "12"))
	int32 MaxEntries = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|HUD|KillFeed", meta = (ClampMin = "0.5"))
	float EntryLifetime = 5.f;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 必需：Vertical Box，名字必须是 FeedList
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> FeedList;

private:
	void BindGameState(AShooterGameState* GS);
	void UnbindGameState();
	void AddEntry(const FString& KillerName, uint8 KillerTeam, const FString& VictimName, uint8 VictimTeam, UTexture2D* WeaponIcon);
	void RemoveEntry(UKillFeedEntry* Entry);
	FString GetLocalPlayerName() const;

	UFUNCTION()
	void HandleKillFeed(const FString& KillerName, uint8 KillerTeam, const FString& VictimName, uint8 VictimTeam, UTexture2D* WeaponIcon);

	void HandleGameStateSet(AGameStateBase* NewGameState);

	TWeakObjectPtr<AShooterGameState> BoundGameState;

	struct FFeedItem
	{
		TWeakObjectPtr<UKillFeedEntry> Entry;
		FTimerHandle Handle;
	};
	TArray<FFeedItem> Items;
};
