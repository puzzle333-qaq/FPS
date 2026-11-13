#include "Ui/MagInspectWidget.h"
#include "Ui/OperationUIStyle.h"
#include "CombatComponent.h"
#include "Weapon/Weapon.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace {
 TAutoConsoleVariable<int32> MagDark(TEXT("fps.UI.MagDark"),0,TEXT("Magazine comparison: 0 light fill, 1 dark fill."));
 class SOperationMagazine:public SLeafWidget {
 public:
  SLATE_BEGIN_ARGS(SOperationMagazine){} SLATE_ARGUMENT(float,Fill) SLATE_ARGUMENT(bool,Current) SLATE_END_ARGS()
  void Construct(const FArguments& A){Fill=A._Fill;Current=A._Current;}
  virtual FVector2D ComputeDesiredSize(float)const override{return Current?FVector2D(16,40):FVector2D(14,34);}
  virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& E,int32 L,const FWidgetStyle& S,bool)const override {
   const FVector2D Z=G.GetLocalSize();const auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
   const bool Warning=Current&&Fill<=.25f;
   FLinearColor C=FOperationUIStyle::Text;
   C*=S.GetColorAndOpacityTint();
   TArray<FVector2D> P={FVector2D(0,0),FVector2D(Z.X,0),FVector2D(Z.X,Z.Y),FVector2D(0,Z.Y),FVector2D(0,0)};
   TArray<FVector2D> Shadow=P;
   for(FVector2D& Point:Shadow)Point.Y+=1;
   FSlateDrawElement::MakeLines(E,L,G.ToPaintGeometry(),Shadow,ESlateDrawEffect::None,FLinearColor(0,0,0,.6f)*S.GetColorAndOpacityTint(),true,1.5f);
   FSlateDrawElement::MakeLines(E,L+1,G.ToPaintGeometry(),P,ESlateDrawEffect::None,C,true,1.5f);
   const float H=FMath::Max(0.f,(Z.Y-4)*Fill);
   if(H>0) {
    FLinearColor FillColor=Warning?FOperationUIStyle::Warning*S.GetColorAndOpacityTint():MagDark.GetValueOnGameThread()?FLinearColor(.04f,.045f,.05f,.9f)*S.GetColorAndOpacityTint():C.CopyWithNewOpacity(C.A*.65f);
    FSlateDrawElement::MakeBox(E,L+2,G.ToPaintGeometry(FVector2f(Z.X-4,H),FSlateLayoutTransform(FVector2f(2,Z.Y-2-H))),Brush,ESlateDrawEffect::None,FillColor);
    const TArray<FVector2D> Surface={FVector2D(2,Z.Y-2-H),FVector2D(Z.X-2,Z.Y-2-H)};
    FSlateDrawElement::MakeLines(E,L+3,G.ToPaintGeometry(),Surface,ESlateDrawEffect::None,C,true,1.f);
   }
   return L+3;
  }
 private:float Fill=0;bool Current=false;
 };
}
UMagInspectWidget::UMagInspectWidget(const FObjectInitializer& O):Super(O){bHasScriptImplementedTick=true;}
TSharedRef<SWidget> UMagInspectWidget::RebuildWidget()
{
 return SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
 +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0,0,0,40))
 [SAssignNew(Panel,SVerticalBox).Visibility(EVisibility::Collapsed)
  +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,0,0,8))
  [SAssignNew(FireMode,STextBlock).Font(FOperationUIStyle::MakeFont(12,false,true)).ColorAndOpacity(FOperationUIStyle::Text)
   .ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))]
  +SVerticalBox::Slot().AutoHeight()[SAssignNew(Row,SHorizontalBox)]];
}
void UMagInspectWidget::NativeConstruct()
{
 Super::NativeConstruct();if(auto* PC=GetOwningPlayer()){PC->GetOnNewPawnNotifier().AddUObject(this,&ThisClass::BindPawn);BindPawn(PC->GetPawn());}
}
void UMagInspectWidget::BindPawn(APawn* P)
{
 if(Combat.IsValid()){Combat->OnInspectionChanged.RemoveAll(this);Combat->OnInspectionHudChanged.RemoveAll(this);}
 Combat=P?P->FindComponentByClass<UCombatComponent>():nullptr;
 Opacity=0;Linger=0;bInspecting=false;if(Panel)Panel->SetVisibility(EVisibility::Collapsed);
 if(Combat.IsValid()){
  Combat->OnInspectionChanged.AddUObject(this,&ThisClass::InspectionChanged);
  Combat->OnInspectionHudChanged.AddUObject(this,&ThisClass::RefreshMagazine);
  if(Combat->IsInspecting())InspectionChanged(true);
 }
}
void UMagInspectWidget::InspectionChanged(bool On)
{
 bInspecting=On;
 if(On){Linger=0;RefreshMagazine();}
 else {Linger=.5f;}
}
void UMagInspectWidget::RefreshMagazine()
{
 if(!bInspecting||!Combat.IsValid()||!Row||!FireMode) return;
 AWeapon* W=Combat->CurrentWeapon;if(!IsValid(W)||!W->UsesAmmo())return;
 Combat->GetHudMagazineFills(Fills);CurrentRounds=W->Ammo;Row->ClearChildren();
 FString Mode=W->FireType==EFireType::Automatic?TEXT("全自动"):W->FireType==EFireType::Burst?TEXT("点射"):TEXT("单发");
 bool Hint=false;if(GConfig)GConfig->GetBool(TEXT("/Script/FPS.SettingsScreen"),TEXT("bLowAmmoHint"),Hint,GGameUserSettingsIni);
 if(Hint&&Fills.Num()>0&&Fills[0]<=.25f)Mode+=TEXT(" · 弹量偏低");
 FireMode->SetText(FText::FromString(Mode));
 for(int32 i=0;i<Fills.Num();++i)
 {
  auto Item=SNew(SVerticalBox);
  Item->AddSlot().AutoHeight().HAlign(HAlign_Center)[SNew(SOperationMagazine).Fill(Fills[i]).Current(i==0)];
  if(i==0)Item->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0,8,0,0))
   [SNew(STextBlock).Text(FText::AsNumber(CurrentRounds)).Font(FOperationUIStyle::MakeFont(12,false,true))
    .ColorAndOpacity(FOperationUIStyle::Text)
    .ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))];
  Row->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(i?6:0,0,0,0))[Item];
 }
}
void UMagInspectWidget::NativeTick(const FGeometry& G,float D)
{
 Super::NativeTick(G,D);if(!Panel)return;
 const bool Show=bInspecting||Linger>0;
 if(!bInspecting)Linger=FMath::Max(0.f,Linger-D);
 Opacity=FMath::FInterpConstantTo(Opacity,Show?1.f:0.f,D,1.f/.15f);
 Panel->SetRenderOpacity(Opacity);
 Panel->SetVisibility(Opacity>0?EVisibility::HitTestInvisible:EVisibility::Collapsed);
}
void UMagInspectWidget::NativeDestruct()
{
 if(auto* PC=GetOwningPlayer())PC->GetOnNewPawnNotifier().RemoveAll(this);
 if(Combat.IsValid()){Combat->OnInspectionChanged.RemoveAll(this);Combat->OnInspectionHudChanged.RemoveAll(this);}
 Combat.Reset();Super::NativeDestruct();
}
void UMagInspectWidget::ReleaseSlateResources(bool B){Super::ReleaseSlateResources(B);Panel.Reset();Row.Reset();FireMode.Reset();}
