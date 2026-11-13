#pragma once
#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

/** Scoped to deployment and in-mission UI; lobby/loadout/settlement keep their style. */
struct FPS_API FOperationUIStyle
{
 static const FLinearColor Text;
 static const FLinearColor TextMuted;
 static const FLinearColor Dim;
 static const FLinearColor Panel;
 static const FLinearColor Outline;
 static const FLinearColor Warning;
 static const FLinearColor Ready;
 static FSlateFontInfo MakeFont(int32 Size, bool bBold=false, bool bMagazine=false);
 static FSlateBrush MakePanelBrush(const FLinearColor& Tint, float Radius=0.f, float Width=1.f, const FLinearColor* Line=nullptr);
};
