#include "Ui/NVGMaskWidget.h"
#include "GameFramework/PlayerController.h"
#include "ShooterCharacter.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
UNVGMaskWidget::UNVGMaskWidget(const FObjectInitializer& O):Super(O){bHasScriptImplementedTick=true;}
TSharedRef<SWidget> UNVGMaskWidget::RebuildWidget()
{
 if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/FPS/Materials/M_NVG_Housing.M_NVG_Housing")))
 {
  MaskMaterial=UMaterialInstanceDynamic::Create(Material,this);MaskBrush.SetResourceObject(MaskMaterial);
  MaskBrush.DrawAs=ESlateBrushDrawType::Image;
 }
 return SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
  +SOverlay::Slot()[SNew(SImage).Image(&MaskBrush)];
}
void UNVGMaskWidget::NativeConstruct()
{
 Super::NativeConstruct();
 if(auto* PC=GetOwningPlayer()){PC->GetOnNewPawnNotifier().AddUObject(this,&ThisClass::BindPawn);BindPawn(PC->GetPawn());}
}
void UNVGMaskWidget::BindPawn(APawn* Pawn)
{
 if(Character.IsValid())Character->OnEquipmentHudChanged.RemoveAll(this);
 Character=Cast<AShooterCharacter>(Pawn);
 if(Character.IsValid())Character->OnEquipmentHudChanged.AddUObject(this,&ThisClass::RefreshVisibility);
 RefreshVisibility();
}
void UNVGMaskWidget::RefreshVisibility()
{
 SetVisibility(Character.IsValid()&&Character->GetTacticalVisionMode()==ETacticalVisionMode::NVG?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
}
void UNVGMaskWidget::NativeDestruct()
{
 if(auto* PC=GetOwningPlayer())PC->GetOnNewPawnNotifier().RemoveAll(this);
 if(Character.IsValid())Character->OnEquipmentHudChanged.RemoveAll(this);
 Character.Reset();Super::NativeDestruct();
}
void UNVGMaskWidget::NativeTick(const FGeometry& Geometry,float Delta)
{
 Super::NativeTick(Geometry,Delta);
 const auto* C=Cast<AShooterCharacter>(GetOwningPlayerPawn());
 const bool Active=C&&C->GetTacticalVisionMode()==ETacticalVisionMode::NVG;
 if(!Active||!MaskMaterial)return;
 const FVector2D Size=Geometry.GetAbsoluteSize();
 MaskMaterial->SetScalarParameterValue(TEXT("Quad"),C->GetNVGHousing()==ENVGHousing::Quad?1.f:0.f);
 MaskMaterial->SetScalarParameterValue(TEXT("Aspect"),Size.X/FMath::Max(1.f,Size.Y));
 MaskMaterial->SetScalarParameterValue(TEXT("ScreenHeight"),FMath::Max(1.f,Size.Y));
}
