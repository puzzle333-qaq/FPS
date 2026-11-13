#include "Ui/OperationUIStyle.h"
#include "Fonts/CompositeFont.h"
#include "Misc/Paths.h"

const FLinearColor FOperationUIStyle::Text=FLinearColor::FromSRGBColor(FColor(237,238,240));
const FLinearColor FOperationUIStyle::TextMuted=FLinearColor::FromSRGBColor(FColor(155,161,166));
const FLinearColor FOperationUIStyle::Dim=FLinearColor::FromSRGBColor(FColor(92,97,101));
const FLinearColor FOperationUIStyle::Panel=FLinearColor(0.003f,0.0034f,0.0037f,0.75f);
const FLinearColor FOperationUIStyle::Outline(1,1,1,0.12f);
const FLinearColor FOperationUIStyle::Warning=FLinearColor::FromSRGBColor(FColor(195,59,46));
const FLinearColor FOperationUIStyle::Ready=FLinearColor::FromSRGBColor(FColor(127,163,107));

FSlateFontInfo FOperationUIStyle::MakeFont(int32 Size,bool bBold,bool /*bMagazine*/)
{
 static TSharedPtr<FCompositeFont> Composite=[] {
  auto F=MakeShared<FCompositeFont>();
  const FString Dir=FPaths::ProjectContentDir()/TEXT("FPS/UI/Fonts");
  F->DefaultTypeface.Fonts.Emplace(TEXT("Regular"),Dir/TEXT("BarlowCondensed-Regular.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
  F->DefaultTypeface.Fonts.Emplace(TEXT("Bold"),Dir/TEXT("BarlowCondensed-SemiBold.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
  F->FallbackTypeface.Typeface.Fonts.Emplace(TEXT("Regular"),Dir/TEXT("SourceHanSansSC-Regular.otf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
  F->FallbackTypeface.Typeface.Fonts.Emplace(TEXT("Bold"),Dir/TEXT("SourceHanSansSC-Regular.otf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
  return F;
 }();
 FFontOutlineSettings Line;
 // Widgets supply the single 0/1 px shadow. Do not stack a font outline on it.
 Line.OutlineSize=0;
 Line.OutlineColor=FLinearColor(0,0,0,0.6f);
 return FSlateFontInfo(Composite,Size,bBold?TEXT("Bold"):TEXT("Regular"),Line);
}
FSlateBrush FOperationUIStyle::MakePanelBrush(const FLinearColor& Tint,float Radius,float Width,const FLinearColor* Line)
{
 FSlateBrush B;B.DrawAs=ESlateBrushDrawType::RoundedBox;B.TintColor=Tint;
 const float R=FMath::Min(Radius,2.f);B.OutlineSettings.CornerRadii=FVector4(R,R,R,R);
 B.OutlineSettings.RoundingType=ESlateBrushRoundingType::FixedRadius;
 B.OutlineSettings.Width=Width;B.OutlineSettings.Color=Line?*Line:Outline;return B;
}
