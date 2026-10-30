#pragma once

#include "CoreMinimal.h"
#include "PoliceCriminalTypes.generated.h"

// Team 0 = Police (进攻), Team 1 = Criminal (防守/布置)
UENUM(BlueprintType)
enum class EPoliceCriminalFaction : uint8
{
	Police    UMETA(DisplayName = "警察"),
	Criminal  UMETA(DisplayName = "匪徒")
};

UENUM(BlueprintType)
enum class EPoliceCriminalPhase : uint8
{
	Waiting      UMETA(DisplayName = "等待"),
	CriminalSetup UMETA(DisplayName = "匪徒布置"),
	Assault      UMETA(DisplayName = "突击/对局中"),
	PostMatch    UMETA(DisplayName = "结算")
};

// 本批：剿匪 Elimination；人质救援后续再接完整规则
UENUM(BlueprintType)
enum class EPoliceCriminalMatchType : uint8
{
	Elimination UMETA(DisplayName = "剿匪"),
	HostageRescue UMETA(DisplayName = "解救人质")
};

// 剿匪结算副标题用：歼灭 / 超时 / 掉线认输 / 平局
UENUM(BlueprintType)
enum class EPoliceCriminalWinReason : uint8
{
	None        UMETA(DisplayName = "无"),
	Elimination UMETA(DisplayName = "歼灭"),
	Timeout     UMETA(DisplayName = "时间到"),
	Forfeit     UMETA(DisplayName = "对方掉线"),
	Draw        UMETA(DisplayName = "平局")
};

namespace PoliceCriminalTeams
{
	constexpr uint8 Police = 0;
	constexpr uint8 Criminal = 1;
}

inline uint8 FactionToTeamId(EPoliceCriminalFaction F)
{
	return F == EPoliceCriminalFaction::Police ? PoliceCriminalTeams::Police : PoliceCriminalTeams::Criminal;
}

inline EPoliceCriminalFaction TeamIdToFaction(uint8 TeamId)
{
	return TeamId == PoliceCriminalTeams::Police ? EPoliceCriminalFaction::Police : EPoliceCriminalFaction::Criminal;
}
