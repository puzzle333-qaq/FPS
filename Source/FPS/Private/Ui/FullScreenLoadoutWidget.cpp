#include "Ui/FullScreenLoadoutWidget.h"
#include "Ui/OperationUIStyle.h"
#include "Character/PoliceAppearanceComponent.h"
#include "ShooterCharacter.h"
#include "ShooterPlayerController.h"
#include "CombatComponent.h"
#include "Range/RangeLoadoutBench.h"
#include "Loadout/PouchInventoryComponent.h"
#include "Loadout/WarehouseLoadoutSaveGame.h"
#include "Weapon/AttachmentData.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PointLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Animation/AnimationAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Images/SImage.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "TimerManager.h"

namespace {
class SPouchMagazine : public SLeafWidget {
public: SLATE_BEGIN_ARGS(SPouchMagazine){} SLATE_ARGUMENT(bool,Filled) SLATE_ARGUMENT(bool,Throwable) SLATE_ARGUMENT(bool,Reserved) SLATE_END_ARGS()
 void Construct(const FArguments& A){Filled=A._Filled;Throwable=A._Throwable;Reserved=A._Reserved;}
 virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(18,44);}
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& E,int32 L,const FWidgetStyle&,bool)const override{
  FVector2D Z=G.GetLocalSize();TArray<FVector2D> P={{0,0},{Z.X,0},{Z.X,Z.Y},{0,Z.Y},{0,0}};
  FSlateDrawElement::MakeLines(E,L,G.ToPaintGeometry(),P,ESlateDrawEffect::None,FOperationUIStyle::Text,true,1.5f);
  if(Reserved){TArray<FVector2D> Slash={{0,Z.Y},{Z.X,0}};FSlateDrawElement::MakeLines(E,L+1,G.ToPaintGeometry(),Slash,ESlateDrawEffect::None,FOperationUIStyle::Dim,true,1.5f);}
  if(Filled&&!Reserved)FSlateDrawElement::MakeBox(E,L+1,G.ToPaintGeometry(FVector2f(Z.X-4,Z.Y-4),FSlateLayoutTransform(FVector2f(2,2))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Throwable?FOperationUIStyle::TextMuted:FOperationUIStyle::Text.CopyWithNewOpacity(.65f));
  return L+1;
 }
 bool Filled=false,Throwable=false,Reserved=false;
};
FString FaceName(EPoliceFaceProtection F){switch(F){case EPoliceFaceProtection::FlashGoggles:return TEXT("闪光防护眼镜");case EPoliceFaceProtection::GogglesAndMask:return TEXT("闪光眼镜 + 防弹面罩");case EPoliceFaceProtection::GasMask:return TEXT("防毒面具");default:return TEXT("无面部装备");}}
FString PlayerWeaponName(FString S){if(S.Contains(TEXT("WeaponFlash")))return TEXT("闪光弹");if(S.Contains(TEXT("WeaponSting")))return TEXT("震撼弹");if(S.Contains(TEXT("Molotov")))return TEXT("燃烧瓶");return S;}
}
TSharedRef<SWidget> UFullScreenLoadoutWidget::Text(const FString& S,int32 Size,FLinearColor Color){return SNew(STextBlock).Text(FText::FromString(S)).Font(FOperationUIStyle::MakeFont(Size,Size>=28)).ColorAndOpacity(Color).AutoWrapText(false);}
TSharedRef<SWidget> UFullScreenLoadoutWidget::Button(const FString& S,TFunction<void()> Action,bool Enabled,int32 Size){
 const bool ActiveTab=Size==28&&S.Contains(TEXT("━"));
 auto Control=SNew(SButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(12,8)).IsEnabled(Enabled).OnClicked_Lambda([Action=MoveTemp(Action)]{Action();return FReply::Handled();})[Text(Size==28?S.Replace(TEXT("━"),TEXT("")).TrimEnd():S,Size)];
 if(Size==28)return SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Control]+SVerticalBox::Slot().AutoHeight().Padding(12,0)[SNew(SBox).HeightOverride(2)[SNew(SBorder).Padding(0).BorderImage(&TabUnderlineBrush).Visibility(ActiveTab?EVisibility::HitTestInvisible:EVisibility::Hidden)]];
 return Control;
}
TSharedRef<SWidget> UFullScreenLoadoutWidget::Icon(UTexture2D* Texture,FVector2D Size){if(!Texture)return SNew(SBox).WidthOverride(Size.X).HeightOverride(Size.Y)[SNew(SBorder).BorderImage(&EmptyIconBrush).Padding(0)];auto B=MakeShared<FSlateBrush>();B->SetResourceObject(Texture);B->ImageSize=FVector2D(Texture->GetSizeX(),Texture->GetSizeY());ImageBrushes.Add(B);return SNew(SBox).WidthOverride(Size.X).HeightOverride(Size.Y)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&B.Get())]];}
TSharedRef<SWidget> UFullScreenLoadoutWidget::RebuildWidget(){
 TabUnderlineBrush=FOperationUIStyle::MakePanelBrush(FOperationUIStyle::Warning,0,0);
 EmptyIconBrush=FOperationUIStyle::MakePanelBrush(FLinearColor::Transparent,0,1,&FOperationUIStyle::Outline);
 BlackBrush=FOperationUIStyle::MakePanelBrush(FLinearColor::Black,0,0);LineBrush=FOperationUIStyle::MakePanelBrush(FOperationUIStyle::Outline,0,0);
 ButtonStyle.SetNormal(FOperationUIStyle::MakePanelBrush(FLinearColor::Transparent,0,0));ButtonStyle.SetHovered(FOperationUIStyle::MakePanelBrush(FLinearColor(.035,.035,.035),0,1,&FOperationUIStyle::Warning));ButtonStyle.SetPressed(ButtonStyle.Hovered);
 for(int32 I=0;I<3;++I)PresetOptions.Add(MakeShared<FString>(FString::Printf(TEXT("行动配装 %d"),I+1)));
 auto Root=SNew(SBorder).BorderImage(&BlackBrush).Padding(0)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1920).HeightOverride(1080)[SAssignNew(Canvas,SCanvas)]]];
 Canvas->AddSlot().Position(FVector2D(700,40)).Size(FVector2D(1172,960))[SNew(SImage).Image(&PreviewBrush)];
 Canvas->AddSlot().Position(FVector2D(48,40)).Size(FVector2D(620,1000))[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Content,SVerticalBox)]];
 Canvas->AddSlot().Position(FVector2D(1592,1000)).Size(FVector2D(280,40))[SNew(STextBlock).Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("%d/%d 槽位"),UsedSlots(),ViewedOperator&&ViewedOperator->GetPouch()?ViewedOperator->GetPouch()->GetSlotCount():PouchLimit()));}).Font(FOperationUIStyle::MakeFont(13)).ColorAndOpacity_Lambda([this]{return UsedSlots()>=(ViewedOperator&&ViewedOperator->GetPouch()?ViewedOperator->GetPouch()->GetSlotCount():PouchLimit())?FOperationUIStyle::Warning:FOperationUIStyle::TextMuted;})];
 Canvas->AddSlot().Position(FVector2D(1652,40)).Size(FVector2D(220,40))[Button(TEXT("离开并装备  /  Esc"),[this]{CloseBench();})];
 RefreshScreen();return Root;
}
void UFullScreenLoadoutWidget::NativeConstruct(){Super::NativeConstruct();RefreshStage();GetWorld()->GetTimerManager().SetTimer(CaptureTimer,this,&ThisClass::CaptureStage,.1f,true);}
void UFullScreenLoadoutWidget::NativeDestruct(){GetWorld()->GetTimerManager().ClearTimer(CaptureTimer);if(StageWeapon)StageWeapon->Destroy();if(Stage)Stage->Destroy();StageWeapon=nullptr;Stage=nullptr;Capture=nullptr;StageBody=nullptr;StageAppearance=nullptr;RenderTarget=nullptr;Super::NativeDestruct();}
void UFullScreenLoadoutWidget::ReleaseSlateResources(bool Children){Super::ReleaseSlateResources(Children);Content.Reset();Canvas.Reset();PresetName.Reset();ImageBrushes.Reset();PresetOptions.Reset();}
FReply UFullScreenLoadoutWidget::NativeOnPreviewKeyDown(const FGeometry& G,const FKeyEvent& E){if(E.GetKey()==EKeys::Escape&&Detail){SetDetail(nullptr);return FReply::Handled();}return Super::NativeOnPreviewKeyDown(G,E);}
void UFullScreenLoadoutWidget::RefreshNativePresentation(){
 int32 Excess=FMath::Max(0,PouchUsed()-PouchLimit());
 auto Trim=[&](FWeaponPreset& P){int32 Take=FMath::Min(Excess,P.StackCount);P.StackCount-=Take;Excess-=Take;};
 if(Excess>0){for(int32 I=Throwables.Num()-1;I>=0;--I)Trim(Throwables[I]);Trim(Secondary);Trim(Primary);Status=TEXT("防护装备占用携行槽位，超出的携行数量已调整");}
 const FString Now=SelectionSignature();if(SavedSignature.IsEmpty())SavedSignature=Now;Unsaved=Now!=SavedSignature;RefreshScreen();RefreshStage();}
void UFullScreenLoadoutWidget::AddTitle(const FString& S){Content->AddSlot().AutoHeight().Padding(0,24,0,8)[Text(S,13,FOperationUIStyle::TextMuted)];}
void UFullScreenLoadoutWidget::SetDetail(TSubclassOf<AWeapon> W){Detail=W;ExpandedSlot=FGameplayTag();RefreshScreen();RefreshStage();}
int32 UFullScreenLoadoutWidget::UsedSlots()const{if(ViewedOperator){if(auto* P=ViewedOperator->FindComponentByClass<UPouchInventoryComponent>())return P->CountUsed();return 0;}return PouchUsed();}
void UFullScreenLoadoutWidget::AddWeapon(const FRangeWeaponChoice& C,const FString& Type){Content->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(120)[SNew(SButton).ButtonStyle(&ButtonStyle).ContentPadding(0).IsEnabled(!ViewedOperator).OnClicked_Lambda([this,C]{SetDetail(C.WeaponClass);return FReply::Handled();})[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Icon(C.Icon,{240,84})]+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(20,0)[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Text(Type,12,FOperationUIStyle::TextMuted)]+SVerticalBox::Slot().AutoHeight()[Text(PlayerWeaponName(C.DisplayName.ToString()),28)]]]]];Content->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(1)[SNew(SBorder).BorderImage(&LineBrush)]];}
void UFullScreenLoadoutWidget::Step(TSubclassOf<AWeapon> W,int32 Delta,bool Throwable){if(ViewedOperator)return;const bool OK=Throwable?AdjustThrowableCount(W,Delta):AdjustMagCount(W,Delta);if(!OK&&Delta>0){OverflowAt=FPlatformTime::Seconds();Status=TEXT("可用携行槽位已占满");RefreshScreen();}else if(OK){Unsaved=true;Status.Empty();}}
void UFullScreenLoadoutWidget::AddStepper(const FRangeWeaponChoice& C,bool Throwable){const int32 Count=GetStackCount(C.WeaponClass);Content->AddSlot().AutoHeight().Padding(0,4)[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(PlayerWeaponName(C.DisplayName.ToString()),16)]+SHorizontalBox::Slot().AutoWidth()[Button(TEXT("‹"),[this,C,Throwable]{Step(C.WeaponClass,-1,Throwable);},!ViewedOperator&&Count>0)]+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Text(FString::FromInt(Count),20)]+SHorizontalBox::Slot().AutoWidth()[Button(TEXT("›"),[this,C,Throwable]{Step(C.WeaponClass,1,Throwable);},!ViewedOperator&&UsedSlots()<PouchLimit())]];}
void UFullScreenLoadoutWidget::AddProtection(){auto* C=ViewedOperator?ViewedOperator.Get():GetOwningPlayerPawn<AShooterCharacter>();if(!C)return;AddTitle(TEXT("ARMOR & MUNITIONS  /  防护与弹药"));Content->AddSlot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Button((C->GetBodyArmor()==EBodyArmorType::Light?TEXT("● "):TEXT("○ "))+FString(TEXT("轻型防弹衣")),[this]{SelectBodyArmor(EBodyArmorType::Light);},!ViewedOperator)]+SHorizontalBox::Slot().FillWidth(1)[Button((C->GetBodyArmor()==EBodyArmorType::Heavy?TEXT("● "):TEXT("○ "))+FString(TEXT("重型防弹衣")),[this]{SelectBodyArmor(EBodyArmorType::Heavy);},!ViewedOperator)]];
 if(C->GetBodyArmor()==EBodyArmorType::Heavy)Content->AddSlot().AutoHeight()[Text(TEXT("重甲：携行槽位 −1 · 移速 −22%"),13,FOperationUIStyle::TextMuted)];
 AddTitle(TEXT("HEADWEAR  /  头部装备"));Content->AddSlot().AutoHeight()[Text(TEXT("战术头盔                    挂载：夜视仪"),16)];if(C->PoliceAppearance&&C->PoliceAppearance->FaceProtection==EPoliceFaceProtection::GogglesAndMask)Content->AddSlot().AutoHeight()[Text(TEXT("防弹面罩：携行槽位 −1 · 移速 −5%"),13,FOperationUIStyle::TextMuted)];
 if(C->PoliceAppearance)Content->AddSlot().AutoHeight().Padding(0,8)[Text(TEXT("面部：")+FaceName(C->PoliceAppearance->FaceProtection),16,FOperationUIStyle::TextMuted)];
 if(!Customization){Content->AddSlot().AutoHeight()[Button(TEXT("更换面部装备 ›"),[this]{Customization=true;RefreshScreen();},!ViewedOperator)];return;}
 for(auto F:{EPoliceFaceProtection::FlashGoggles,EPoliceFaceProtection::GogglesAndMask,EPoliceFaceProtection::GasMask})Content->AddSlot().AutoHeight()[Button(FaceName(F),[this,F]{SelectFaceProtection(F);},!ViewedOperator)];
}
void UFullScreenLoadoutWidget::ChangeOperator(int32 Delta){TArray<AShooterCharacter*> Operators;Operators.Add(GetOwningPlayerPawn<AShooterCharacter>());if(auto* GS=GetWorld()->GetGameState())for(const auto& PS:GS->PlayerArray)if(PS)if(auto* C=Cast<AShooterCharacter>(PS->GetPawn());C&&C!=Operators[0])Operators.Add(C);int32 I=ViewedOperator?Operators.IndexOfByKey(ViewedOperator.Get()):0;I=(FMath::Max(0,I)+Delta+Operators.Num())%Operators.Num();ViewedOperator=I==0?nullptr:Operators[I];Detail=nullptr;RefreshScreen();RefreshStage();}
void UFullScreenLoadoutWidget::SavePreset(){if(ViewedOperator)return;if(PresetName)CurrentPresetName=PresetName->GetText().ToString().Left(48);auto L=BuildLoadout();L.Name=FText::FromString(CurrentPresetName);if(auto* C=GetOwningPlayerPawn<AShooterCharacter>()){L.bIncludesProtection=true;L.BodyArmor=C->GetBodyArmor();if(C->PoliceAppearance)L.FaceProtection=C->PoliceAppearance->FaceProtection;}if(UWarehouseLoadoutSaveGame::SaveLoadout(L,FString::Printf(TEXT("WarehousePreset%d"),PresetIndex))){Unsaved=false;SavedSignature=SelectionSignature();Status=TEXT("预设已保存");}else Status=TEXT("保存失败，请重试");RefreshScreen();}
void UFullScreenLoadoutWidget::LoadPreset(int32 Index){if(ViewedOperator)return;PresetIndex=Index;FLoadoutPreset L;if(UWarehouseLoadoutSaveGame::LoadLoadout(L,FString::Printf(TEXT("WarehousePreset%d"),Index))){Primary=FWeaponPreset();Secondary=FWeaponPreset();Throwables.Reset();for(const auto& W:L.Weapons){if(!W.WeaponClass)continue;switch(W.WeaponClass->GetDefaultObject<AWeapon>()->WeaponKind){case EWeaponKind::LongGun:Primary=W;break;case EWeaponKind::Pistol:Secondary=W;break;case EWeaponKind::Throwable:Throwables.Add(W);break;default:break;}}CurrentPresetName=L.Name.ToString();if(L.bIncludesProtection){SelectFaceProtection(L.FaceProtection);SelectBodyArmor(L.BodyArmor);}bDirty=true;PushPreview(Primary.WeaponClass?&Primary:nullptr);Status=TEXT("预设已载入");}else{CurrentPresetName=*PresetOptions[Index];Status=TEXT("空预设，可保存当前配装");}Unsaved=false;SavedSignature=SelectionSignature();Detail=nullptr;RefreshScreen();RefreshStage();}
void UFullScreenLoadoutWidget::RefreshScreen(){if(!Content)return;Content->ClearChildren();ImageBrushes.Reset();
 Content->AddSlot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[Button(Customization?TEXT("火力配置"):TEXT("火力配置 ━"),[this]{Customization=false;Detail=nullptr;RefreshScreen();},true,28)]+SHorizontalBox::Slot().AutoWidth()[Button(Customization?TEXT("定制 ━"):TEXT("定制"),[this]{Customization=true;Detail=nullptr;RefreshScreen();},true,28)]];
 auto* View=ViewedOperator?ViewedOperator.Get():GetOwningPlayerPawn<AShooterCharacter>();const FString Name=ViewedOperator&&View->GetPlayerState()?View->GetPlayerState()->GetPlayerName():TEXT("SELF");
 Content->AddSlot().AutoHeight().Padding(0,16)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[Button(TEXT("‹"),[this]{ChangeOperator(-1);})]+SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Center).VAlign(VAlign_Center)[Text(Name,20)]+SHorizontalBox::Slot().AutoWidth()[Button(TEXT("›"),[this]{ChangeOperator(1);})]];
 if(ViewedOperator)Content->AddSlot().AutoHeight()[Text(TEXT("只读 · 队友配装"),13,FOperationUIStyle::TextMuted)];
 else{Content->AddSlot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Text(TEXT("PRESET"),13,FOperationUIStyle::TextMuted)]+SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&PresetOptions).OnGenerateWidget_Lambda([this](TSharedPtr<FString> S){return Text(*S,16);}).OnSelectionChanged_Lambda([this](TSharedPtr<FString> S,ESelectInfo::Type){if(S)LoadPreset(PresetOptions.IndexOfByKey(S));})[Text(FString::Printf(TEXT("%d"),PresetIndex+1),20)]]+SHorizontalBox::Slot().FillWidth(1)[SAssignNew(PresetName,SEditableTextBox).Text(FText::FromString(CurrentPresetName)).Font(FOperationUIStyle::MakeFont(20)).OnTextCommitted_Lambda([this](const FText& T,ETextCommit::Type){CurrentPresetName=T.ToString().Left(48);Unsaved=true;})]+SHorizontalBox::Slot().AutoWidth()[Text(Unsaved?TEXT(" ● "):TEXT(""),20,FOperationUIStyle::Warning)]+SHorizontalBox::Slot().AutoWidth()[Button(TEXT("保存"),[this]{SavePreset();})]];}
 if(Detail&&!ViewedOperator){Content->AddSlot().AutoHeight().Padding(0,16)[Button(TEXT("‹ 返回装备总览"),[this]{SetDetail(nullptr);})];auto Choices=GetPrimaryChoices();Choices.Append(GetSecondaryChoices());for(const auto& C:Choices)if(C.WeaponClass==Detail)Content->AddSlot().AutoHeight()[Text(C.DisplayName.ToString(),40)];Content->AddSlot().AutoHeight()[Text(TEXT("━ 已装备"),13,FOperationUIStyle::Ready)];Content->AddSlot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("主武器"),[this]{SetDetail(Primary.WeaponClass);})]+SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("随身武器"),[this]{SetDetail(Secondary.WeaponClass);})]];
 auto Slots=GetWeaponSlots(Detail);for(const auto& AttachmentSlot:Slots){Content->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(72)[SNew(SButton).ButtonStyle(&ButtonStyle).IsEnabled(!AttachmentSlot.bLocked).OnClicked_Lambda([this,AttachmentSlot]{ExpandedSlot=ExpandedSlot==AttachmentSlot.SlotTag?FGameplayTag():AttachmentSlot.SlotTag;RefreshScreen();return FReply::Handled();})[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[Icon(AttachmentSlot.EquippedIcon,{96,40})]+SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Text(AttachmentSlot.SlotName.ToString(),12,FOperationUIStyle::TextMuted)]+SVerticalBox::Slot().AutoHeight()[Text(AttachmentSlot.EquippedName.IsEmpty()?TEXT("未安装"):AttachmentSlot.EquippedName.ToString(),20)]]]]];if(ExpandedSlot==AttachmentSlot.SlotTag){TSharedPtr<SHorizontalBox> Strip;Content->AddSlot().AutoHeight()[SNew(SScrollBox).Orientation(Orient_Horizontal)+SScrollBox::Slot()[SAssignNew(Strip,SHorizontalBox)]];if(AttachmentSlot.bCanClear)Strip->AddSlot().AutoWidth()[Button(TEXT("卸下"),[this,AttachmentSlot]{ClearAttachmentSlot(Detail,AttachmentSlot.SlotTag);})];for(const auto& C:AttachmentSlot.Choices)Strip->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(144)[SNew(SButton).ButtonStyle(&ButtonStyle).OnClicked_Lambda([this,C]{EquipAttachment(Detail,C.Attachment);return FReply::Handled();})[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Icon(C.Icon,{96,40})]+SVerticalBox::Slot().AutoHeight()[Text((C.bEquipped?TEXT("━ "):TEXT(""))+C.DisplayName.ToString(),13,C.bEquipped?FOperationUIStyle::Warning:FOperationUIStyle::Text)]]]];}}
 AddTitle(TEXT("选择武器"));for(const auto& C:(Detail==Primary.WeaponClass?GetPrimaryChoices():GetSecondaryChoices()))Content->AddSlot().AutoHeight()[Button((C.bSelected?TEXT("● "):TEXT(""))+C.DisplayName.ToString(),[this,C]{if(Detail==Primary.WeaponClass)SelectPrimary(C.WeaponClass);else SelectSecondary(C.WeaponClass);Detail=C.WeaponClass;PushPreview(FindPreset(Detail));})];
 Content->AddSlot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("弹药"),[this]{Deployables=false;RefreshScreen();})]+SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("可部署物品"),[this]{Deployables=true;RefreshScreen();})]];if(Deployables)for(const auto& C:GetThrowableChoices())AddStepper(C,true);else for(const auto& C:Choices)if(C.WeaponClass==Detail)AddStepper(C,false);
 }else if(Customization){AddProtection();}else{
 AddTitle(TEXT("WEAPONS  /  武器"));if(ViewedOperator){if(auto* Combat=ViewedOperator->FindComponentByClass<UCombatComponent>())for(auto* W:Combat->GetInventory())if(W)AddWeapon(MakeWeaponChoice(W->GetClass(),true),W->WeaponKind==EWeaponKind::LongGun?TEXT("主武器"):W->WeaponKind==EWeaponKind::Pistol?TEXT("副武器"):TEXT("战术装备"));}
 else{for(const auto& C:GetPrimaryChoices())if(C.bSelected)AddWeapon(C,TEXT("主武器"));for(const auto& C:GetSecondaryChoices())if(C.bSelected)AddWeapon(C,TEXT("副武器"));Content->AddSlot().AutoHeight()[Button(TEXT("战术装备 ›"),[this]{Deployables=true;SetDetail(Primary.WeaponClass);})];for(const auto& C:GetThrowableChoices())if(C.bSelected)AddStepper(C,true);}
 AddProtection();
 }
 AddTitle(TEXT("弹药槽位"));TSharedPtr<SHorizontalBox> Slots;Content->AddSlot().AutoHeight()[SAssignNew(Slots,SHorizontalBox).RenderTransform_Lambda([this]{float T=FPlatformTime::Seconds()-OverflowAt;return FSlateRenderTransform(FVector2D(T<.3?FMath::Sin(T*80)*4:0,0));})];int32 N=UsedSlots();for(int32 I=0;I<13;++I)Slots->AddSlot().AutoWidth().Padding(0,0,I==FMath::Max(0,Primary.StackCount-1)?16:8,0)[SNew(SBox).WidthOverride(18).HeightOverride(44)[SNew(SPouchMagazine).Filled(I<N).Reserved(I>=(ViewedOperator&&ViewedOperator->GetPouch()?ViewedOperator->GetPouch()->GetSlotCount():PouchLimit())).Throwable(I>=Primary.StackCount+Secondary.StackCount)]];
 if(!ViewedOperator&&!Detail){for(const auto& C:GetPrimaryChoices())if(C.bSelected)AddStepper(C,false);for(const auto& C:GetSecondaryChoices())if(C.bSelected)AddStepper(C,false);}
 if(!Status.IsEmpty())Content->AddSlot().AutoHeight().Padding(0,8)[Text(Status,13,FOperationUIStyle::TextMuted)];
}
void UFullScreenLoadoutWidget::RefreshStage(){
 if(!GetWorld()||!Bench)return;
 auto* Operator=ViewedOperator?ViewedOperator.Get():GetOwningPlayerPawn<AShooterCharacter>();if(!Operator)return;
 if(!Stage){
  FActorSpawnParameters P;P.Owner=GetOwningPlayer();P.ObjectFlags|=RF_Transient;Stage=GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),FVector(0,0,50000),FRotator::ZeroRotator,P);if(!Stage)return;Stage->SetReplicates(false);
  StageBody=NewObject<USkeletalMeshComponent>(Stage);Stage->SetRootComponent(StageBody);Stage->AddInstanceComponent(StageBody);StageBody->RegisterComponent();StageBody->SetWorldLocation(FVector(0,0,50000));StageBody->SetWorldRotation(FRotator(0,-90,0));StageBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);StageBody->SetCanEverAffectNavigation(false);StageBody->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  StageBody->SetSkeletalMesh(Operator->GetMesh()->GetSkeletalMeshAsset());
  if(auto* Idle=LoadObject<UAnimationAsset>(nullptr,TEXT("/Game/FPS/Characters/Police/A_Police_LoadoutPose.A_Police_LoadoutPose")))StageBody->PlayAnimation(Idle,true);
  StageAppearance=NewObject<UPoliceAppearanceComponent>(Stage);StageAppearance->SetIsReplicated(false);Stage->AddInstanceComponent(StageAppearance);StageAppearance->RegisterComponent();StageAppearance->InitializePreview(StageBody,Operator->PoliceAppearance?Operator->PoliceAppearance->FaceProtection:EPoliceFaceProtection::FlashGoggles,Operator->GetBodyArmor());
  RenderTarget=NewObject<UTextureRenderTarget2D>(this);RenderTarget->ClearColor=FLinearColor::Black;RenderTarget->InitAutoFormat(1172,960);RenderTarget->UpdateResourceImmediate(true);PreviewBrush.SetResourceObject(RenderTarget);PreviewBrush.ImageSize=FVector2D(1172,960);
  Capture=NewObject<USceneCaptureComponent2D>(Stage);Stage->AddInstanceComponent(Capture);Capture->RegisterComponent();Capture->TextureTarget=RenderTarget;Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;Capture->FOVAngle=45;Capture->ShowFlags.SetAtmosphere(false);Capture->ShowFlags.SetFog(false);Capture->ShowFlags.SetMotionBlur(false);Capture->ShowFlags.SetDepthOfField(false);Capture->ShowFlags.SetBloom(false);Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;Capture->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;Capture->PostProcessSettings.bOverride_AutoExposureBias=true;Capture->PostProcessSettings.AutoExposureBias=1;Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
  auto Light=[this](FVector Offset,FLinearColor Color,float Intensity){auto* L=NewObject<UPointLightComponent>(Stage);Stage->AddInstanceComponent(L);L->RegisterComponent();L->SetWorldLocation(StageBody->GetComponentLocation()+Offset);L->SetLightColor(Color);L->SetIntensity(Intensity);L->SetAttenuationRadius(600);L->SetCastShadows(false);};Light({220,-140,190},FLinearColor(.9,.94,1),20000);Light({-120,100,160},FLinearColor(1,.025,.01),10000);
 }
 if(StageAppearance&&Operator->PoliceAppearance)StageAppearance->SetPreviewProtection(Operator->PoliceAppearance->FaceProtection,Operator->GetBodyArmor());
 if(StageWeapon){StageWeapon->Destroy();StageWeapon=nullptr;}
 FWeaponPreset Selection;if(ViewedOperator){if(auto* Combat=Operator->FindComponentByClass<UCombatComponent>())for(auto* W:Combat->GetInventory())if(W&&W->WeaponKind==EWeaponKind::LongGun){Selection.WeaponClass=W->GetClass();for(auto* A:W->GetEquippedAttachments())if(A)Selection.Attachments.Add(A);break;}}
 else if(const FWeaponPreset* P=FindPreset(Detail?Detail:Primary.WeaponClass))Selection=*P;
 if(Selection.WeaponClass){FTransform X(StageBody->GetComponentTransform());StageWeapon=GetWorld()->SpawnActorDeferred<AWeapon>(Selection.WeaponClass,X,Stage,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);if(StageWeapon){StageWeapon->SetReplicates(false);StageWeapon->FinishSpawning(X);StageWeapon->SetActorEnableCollision(false);for(const auto& AttachmentSlot:StageWeapon->GetAttachmentSlots())if(!Selection.Attachments.ContainsByPredicate([this,AttachmentSlot](const UAttachmentData* A){return A&&StageWeapon->GetEquipSlot(A)==AttachmentSlot.SlotTag;}))StageWeapon->RemoveAttachment(AttachmentSlot.SlotTag);for(const auto& A:Selection.Attachments)if(A)StageWeapon->EquipAttachment(A);FName Mount=Bench->HandSocket;
 if(!StageBody->DoesSocketExist(Mount))for(FName Candidate:{FName(TEXT("weapon_r")),FName(TEXT("Weapon_r")),FName(TEXT("hand_r")),FName(TEXT("ik_hand_gun"))})if(StageBody->DoesSocketExist(Candidate)){Mount=Candidate;break;}
 StageWeapon->AttachCosmeticThirdPerson(StageBody,Mount,Bench->PreviewWeaponOffset);}}
 Capture->ShowOnlyActors.Reset();Capture->ShowOnlyComponents.Reset();Capture->ShowOnlyActorComponents(Stage,true);if(StageWeapon)Capture->ShowOnlyActorComponents(StageWeapon,true);
 const FVector Focus=StageBody->GetComponentLocation()+FVector(0,0,95);const FVector Camera=Focus+FVector(290,-45,0);Capture->SetWorldLocationAndRotation(Camera,(Focus-Camera).Rotation());CaptureStage();
}
void UFullScreenLoadoutWidget::CaptureStage(){if(Capture)Capture->CaptureScene();}

FString UFullScreenLoadoutWidget::SelectionSignature()const{FString S=CurrentPresetName;for(const auto& P:BuildLoadout().Weapons){S+=GetPathNameSafe(P.WeaponClass.Get())+FString::FromInt(P.StackCount);for(const auto& A:P.Attachments)S+=GetPathNameSafe(A);}if(auto* C=GetOwningPlayerPawn<AShooterCharacter>()){S+=FString::FromInt(int32(C->GetBodyArmor()));if(C->PoliceAppearance)S+=FString::FromInt(int32(C->PoliceAppearance->FaceProtection));}return S;}
