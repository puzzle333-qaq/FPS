#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScoreboardScreen.generated.h"

class UTextBlock;
class UPanelWidget;

USTRUCT(BlueprintType)
struct FScoreboardRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Scoreboard")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Scoreboard")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Scoreboard")
	int32 Deaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Scoreboard")
	int32 Assists = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Scoreboard")
	uint8 TeamId = 255;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Scoreboard")
	bool bIsLocalPlayer = false;
};

// 计分板一行。WBP_ScoreboardRow 选它，放 Text_Name / Text_Kills / Text_Deaths / Text_Assists（都可选）
UCLASS()
class FPS_API UScoreboardRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FPS|Scoreboard")
	void SetRow(const FScoreboardRow& InRow);

	UFUNCTION(BlueprintPure, Category = "FPS|Scoreboard")
	FScoreboardRow GetRow() const { return Row; }

	// 想自己画（高亮自己、换颜色）就在蓝图实现这个
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Scoreboard")
	void OnRowUpdated(const FScoreboardRow& InRow);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Kills;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Deaths;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Assists;

	FScoreboardRow Row;
};

// 按住 Tab 显示的计分板。WBP_Scoreboard 选它。
// 放两个 VerticalBox：TeamAList / TeamBList，并在 Class Defaults 填 RowWidgetClass，C++ 自动每 0.25 秒刷新。
// 也可以不放列表，自己在 OnScoreboardRefreshed 里用 GetTeamRows 画。
UCLASS()
class FPS_API UScoreboardScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	// 某队的行，按击杀降序（同击杀按死亡升序）
	UFUNCTION(BlueprintPure, Category = "FPS|Scoreboard")
	TArray<FScoreboardRow> GetTeamRows(uint8 TeamId) const;

	// 所有人（没有队伍的模式用这个，例如死斗）
	UFUNCTION(BlueprintPure, Category = "FPS|Scoreboard")
	TArray<FScoreboardRow> GetAllRows() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Scoreboard")
	int32 GetTeamScore(uint8 TeamId) const;

	UFUNCTION(BlueprintCallable, Category = "FPS|Scoreboard")
	void RefreshScoreboard();

	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Scoreboard")
	void OnScoreboardRefreshed();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> TeamAList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> TeamBList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TeamAScore;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TeamBScore;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Scoreboard")
	TSubclassOf<UScoreboardRowWidget> RowWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Scoreboard")
	float RefreshInterval = 0.25f;

private:
	void FillList(UPanelWidget* List, const TArray<FScoreboardRow>& Rows);
	TArray<FScoreboardRow> CollectRows(bool bFilterTeam, uint8 TeamId) const;
	FTimerHandle RefreshTimer;
};
