#include "Ui/ControlHintWidget.h"
#include "Ui/OperationUIStyle.h"
#include "CombatComponent.h"
#include "ShooterCharacter.h"
#include "Weapon/Weapon.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

UControlHintWidget::UControlHintWidget(const FObjectInitializer& O):Super(O) { bHasScriptImplementedTick=true; }
TSharedRef<SWidget> UControlHintWidget::RebuildWidget()
{
 IconTextures.Reset();
 const TCHAR* Names[]={TEXT("Glowstick"),TEXT("NVG"),TEXT("Flashlight"),TEXT("Laser"),TEXT("Flashlight")};
 auto Overlay=SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible);
 Overlay->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(48,0,0,40))
 [SAssignNew(Root,SHorizontalBox)];
 for(int32 i=0;i<5;++i)
 {
  UTexture2D* T=LoadObject<UTexture2D>(nullptr,*FString::Printf(TEXT("/Game/FPS/UI/Icons/T_Icon_%s.T_Icon_%s"),Names[i],Names[i]));
  IconTextures.Add(T);Icons[i].SetResourceObject(T);Icons[i].DrawAs=ESlateBrushDrawType::Image;Icons[i].ImageSize=FVector2D(20,20);
  TSharedPtr<SHorizontalBox> Group;
  Root->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(i?24:0,0,0,0))
  [SAssignNew(Group,SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
   [SNew(SBox).WidthOverride(20).HeightOverride(20)[SNew(SImage).Image(&Icons[i])]]
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8,0,0,0))
   [SAssignNew(Labels[i],STextBlock).Font(FOperationUIStyle::MakeFont(13)).ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,0.6f))]
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8,0,0,0))
   [SAssignNew(Dots[i],STextBlock).Text(FText::FromString(TEXT("•"))).Font(FOperationUIStyle::MakeFont(13)).ColorAndOpacity(FOperationUIStyle::Text)
    .ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))]
  ];
  Groups[i]=Group;
 }
 return Overlay;
}
void UControlHintWidget::NativeConstruct()
{
 Super::NativeConstruct();
 if(auto* PC=GetOwningPlayer()) { PC->GetOnNewPawnNotifier().AddUObject(this,&ThisClass::BindPawn);BindPawn(PC->GetPawn()); }
}
void UControlHintWidget::Unbind()
{
 if(Character.IsValid()) Character->OnEquipmentHudChanged.RemoveAll(this);
 if(Combat.IsValid()) Combat->OnInspectionHudChanged.RemoveAll(this);
 if(Weapon.IsValid()) Weapon->OnDeviceHudChanged.RemoveAll(this);
 Character.Reset();Combat.Reset();Weapon.Reset();
}
void UControlHintWidget::BindPawn(APawn* P)
{
 Unbind();bInitialized=false;Character=Cast<AShooterCharacter>(P);
 if(Character.IsValid())
 {
  Character->OnEquipmentHudChanged.AddUObject(this,&ThisClass::RefreshEquipment);
  Combat=Character->FindComponentByClass<UCombatComponent>();
  if(Combat.IsValid()) Combat->OnInspectionHudChanged.AddUObject(this,&ThisClass::RefreshEquipment);
 }
 RefreshEquipment();
}
void UControlHintWidget::BindWeapon()
{
 AWeapon* Next=Combat.IsValid()?Combat->CurrentWeapon.Get():nullptr;
 if(Weapon.Get()==Next) return;
 if(Weapon.IsValid()) Weapon->OnDeviceHudChanged.RemoveAll(this);
 Weapon=Next;
 if(Weapon.IsValid()) Weapon->OnDeviceHudChanged.AddUObject(this,&ThisClass::RefreshEquipment);
}
void UControlHintWidget::RefreshEquipment()
{
 if(!Root) return;
 Root->SetVisibility(Character.IsValid()?EVisibility::HitTestInvisible:EVisibility::Collapsed);
 if(!Character.IsValid()) return;
 BindWeapon();
 bool Next[]={Character->GetGlowStickCount()>0,Character->GetTacticalVisionMode()==ETacticalVisionMode::NVG,
  Weapon.IsValid()&&Weapon->HasFlashlight()&&Weapon->IsFlashlightOn(),Weapon.IsValid()&&Weapon->HasLaser()&&Weapon->IsLaserOn(),Character->IsNVGInfraredEnabled()};
 const FString Texts[]={FString::Printf(TEXT("×%d"),Character->GetGlowStickCount()),(Character->GetNVGHousing()==ENVGHousing::Quad?TEXT("夜视 ····"):TEXT("夜视 ··")),TEXT("手电"),TEXT("镭射"),TEXT("IR")};
 for(int32 i=0;i<5;++i)
 {
  if(bInitialized&&Next[i]!=States[i]) FlashRemaining[i]=0.3f;
  States[i]=Next[i];Labels[i]->SetText(FText::FromString(Texts[i]));
  Labels[i]->SetColorAndOpacity(States[i]?FOperationUIStyle::Text:FOperationUIStyle::Dim);
  Dots[i]->SetVisibility(States[i]&&i>0?EVisibility::HitTestInvisible:EVisibility::Collapsed);
  Icons[i].TintColor=States[i]?FOperationUIStyle::Text:FOperationUIStyle::Dim;
 }
 bInitialized=true;
}
void UControlHintWidget::NativeTick(const FGeometry& G,float D)
{
 Super::NativeTick(G,D);
 // Only animate a received state change; no gameplay-state polling.
 for(int32 i=0;i<5;++i) if(Groups[i])
 {
  FlashRemaining[i]=FMath::Max(0.f,FlashRemaining[i]-D);
  const FLinearColor C=(States[i]||FlashRemaining[i]>0.f)?FOperationUIStyle::Text:FOperationUIStyle::Dim;
  Labels[i]->SetColorAndOpacity(C);Icons[i].TintColor=C;
 }
}
void UControlHintWidget::NativeDestruct()
{
 if(auto* PC=GetOwningPlayer()) PC->GetOnNewPawnNotifier().RemoveAll(this);
 Unbind();Super::NativeDestruct();
}
void UControlHintWidget::ReleaseSlateResources(bool B)
{
 Super::ReleaseSlateResources(B);Root.Reset();
 for(int32 i=0;i<5;++i) {Labels[i].Reset();Dots[i].Reset();Groups[i].Reset();}
}
