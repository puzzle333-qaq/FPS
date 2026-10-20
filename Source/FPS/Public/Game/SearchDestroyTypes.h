#pragma once

#include "CoreMinimal.h"
#include "SearchDestroyTypes.generated.h"

UENUM(BlueprintType)
enum class EBombSiteId : uint8
{
	None UMETA(DisplayName = "None"),
	A    UMETA(DisplayName = "A"),
	B    UMETA(DisplayName = "B")
};

UENUM(BlueprintType)
enum class EBombState : uint8
{
	Idle     UMETA(DisplayName = "地上"),
	Carried  UMETA(DisplayName = "携带"),
	Planting UMETA(DisplayName = "安放中"),
	Planted  UMETA(DisplayName = "已安放"),
	Defusing UMETA(DisplayName = "拆除中"),
	Exploded UMETA(DisplayName = "爆炸"),
	Defused  UMETA(DisplayName = "已拆除")
};

UENUM(BlueprintType)
enum class EBombRoundPhase : uint8
{
	Warmup    UMETA(DisplayName = "准备"),
	Freeze    UMETA(DisplayName = "冻结"),
	Live      UMETA(DisplayName = "回合中"),
	PostRound UMETA(DisplayName = "回合结束")
};

UENUM(BlueprintType)
enum class ERoundWinReason : uint8
{
	None         UMETA(DisplayName = "无"),
	Elimination  UMETA(DisplayName = "歼灭"),
	BombExploded UMETA(DisplayName = "爆炸"),
	BombDefused  UMETA(DisplayName = "拆除"),
	TimeExpired  UMETA(DisplayName = "超时")
};

UENUM(BlueprintType)
enum class EBombPrompt : uint8
{
	None   UMETA(DisplayName = "无"),
	Pickup UMETA(DisplayName = "拾取"),
	Plant  UMETA(DisplayName = "安放"),
	Defuse UMETA(DisplayName = "拆除")
};
