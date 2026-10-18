#include "Ui/GunsmithScreen.h"

#include "Loadout/GunsmithComponent.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"

UGunsmithComponent* UGunsmithScreen::GetGunsmith() const
{
	const APlayerController* PC = GetOwningPlayer();
	return IsValid(PC) ? PC->FindComponentByClass<UGunsmithComponent>() : nullptr;
}

void UGunsmithScreen::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_Back) Button_Back->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBack);
	if (Button_Save) Button_Save->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSave);
	if (UGunsmithComponent* Gunsmith = GetGunsmith())
	{
		Gunsmith->OnPreviewChanged.AddUniqueDynamic(this, &ThisClass::HandlePreviewChanged);
	}
	HandlePreviewChanged();
}

void UGunsmithScreen::NativeDestruct()
{
	if (UGunsmithComponent* Gunsmith = GetGunsmith())
	{
		Gunsmith->OnPreviewChanged.RemoveDynamic(this, &ThisClass::HandlePreviewChanged);
	}
	Super::NativeDestruct();
}

void UGunsmithScreen::ShowWeapon(TSubclassOf<AWeapon> WeaponClass)
{
	if (UGunsmithComponent* Gunsmith = GetGunsmith())
	{
		Gunsmith->SetPreviewWeapon(WeaponClass);
	}
}

void UGunsmithScreen::SelectSlot(FGameplayTag SlotTag)
{
	SelectedSlot = SlotTag;
	UGunsmithComponent* Gunsmith = GetGunsmith();
	if (!Gunsmith) return;
	OnSlotSelected(SlotTag, Gunsmith->GetAllowedForSlot(SlotTag), Gunsmith->GetEquippedInSlot(SlotTag));
}

bool UGunsmithScreen::PickAttachment(UAttachmentData* Attachment)
{
	UGunsmithComponent* Gunsmith = GetGunsmith();
	if (!Gunsmith) return false;
	return IsValid(Attachment) ? Gunsmith->EquipPreview(Attachment) : Gunsmith->RemovePreview(SelectedSlot);
}

bool UGunsmithScreen::Save()
{
	UGunsmithComponent* Gunsmith = GetGunsmith();
	const bool bOk = Gunsmith && Gunsmith->SaveCurrent();
	OnSaved(bOk);
	return bOk;
}

void UGunsmithScreen::HandleBack()
{
	OnBackRequested();
}

void UGunsmithScreen::HandleSave()
{
	Save();
}

void UGunsmithScreen::HandlePreviewChanged()
{
	UGunsmithComponent* Gunsmith = GetGunsmith();
	if (Image_Preview && Gunsmith)
	{
		if (UTextureRenderTarget2D* RT = Gunsmith->GetPreviewRenderTarget())
		{
			Image_Preview->SetBrushResourceObject(RT);
			Image_Preview->SetDesiredSizeOverride(FVector2D(RT->SizeX, RT->SizeY));
		}
	}
	OnPreviewUpdated();
}