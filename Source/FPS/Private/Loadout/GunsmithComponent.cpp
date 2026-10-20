#include "Loadout/GunsmithComponent.h"

#include "ShooterGameInstance.h"
#include "Ui/LoadoutPreviewRig.h"
#include "Weapon/AttachmentData.h"
#include "Engine/World.h"

UGunsmithComponent::UGunsmithComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UGunsmithComponent::EnsureRig()
{
	if (IsValid(PreviewRig)) return;
	UWorld* World = GetWorld();
	if (!World) return;

	UClass* RigClass = PreviewRigClass ? PreviewRigClass.Get() : ALoadoutPreviewRig::StaticClass();
	const FTransform Xf(FRotator::ZeroRotator, FVector(0.f, 0.f, -100000.f));
	PreviewRig = World->SpawnActorDeferred<ALoadoutPreviewRig>(RigClass, Xf);
	if (!IsValid(PreviewRig)) return;
	PreviewRig->RenderTargetWidth = PreviewWidth;
	PreviewRig->RenderTargetHeight = PreviewHeight;
	PreviewRig->OrthoWidth = PreviewOrthoWidth;
	PreviewRig->RowSpacing = 0.f; // 只放一把，摆在正中
	PreviewRig->FinishSpawning(Xf);
}

bool UGunsmithComponent::SetPreviewWeapon(TSubclassOf<AWeapon> WeaponClass)
{
	UWorld* World = GetWorld();
	if (!World || !WeaponClass) return false;

	if (IsValid(PreviewWeapon))
	{
		PreviewWeapon->Destroy();
		PreviewWeapon = nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = GetOwner();
	// 菜单是单机世界，本地就是权威，枪的 BeginPlay 会装默认配件，EquipAttachment 也能用
	PreviewWeapon = World->SpawnActor<AWeapon>(WeaponClass, FVector(0.f, 0.f, -101000.f), FRotator::ZeroRotator, Params);
	if (!IsValid(PreviewWeapon)) return false;
	PreviewWeapon->SetActorHiddenInGame(true);
	PreviewWeapon->SetReplicates(false);

	// 套用已保存的预设：先卸掉预设里没有的槽，再装预设里的
	if (const UShooterGameInstance* GI = Cast<UShooterGameInstance>(World->GetGameInstance()))
	{
		FWeaponPreset Saved;
		if (GI->FindWeaponPreset(WeaponClass, Saved))
		{
			for (const FWeaponAttachmentSlot& Slot : PreviewWeapon->GetAttachmentSlots())
			{
				const bool bInPreset = Saved.Attachments.ContainsByPredicate([&Slot](const UAttachmentData* A)
				{
					return IsValid(A) && A->SlotTag == Slot.SlotTag;
				});
				if (!bInPreset)
				{
					PreviewWeapon->RemoveAttachment(Slot.SlotTag);
				}
			}
			for (UAttachmentData* Data : Saved.Attachments)
			{
				if (IsValid(Data) && PreviewWeapon->CanEquipAttachment(Data))
				{
					PreviewWeapon->EquipAttachment(Data);
				}
			}
		}
	}

	RefreshPreview();
	return true;
}

TArray<FWeaponAttachmentSlot> UGunsmithComponent::GetSlots() const
{
	return IsValid(PreviewWeapon) ? PreviewWeapon->GetAttachmentSlots() : TArray<FWeaponAttachmentSlot>();
}

TArray<FGameplayTag> UGunsmithComponent::GetSlotTags() const
{
	TArray<FGameplayTag> Tags;
	for (const FWeaponAttachmentSlot& Slot : GetSlots())
	{
		Tags.Add(Slot.SlotTag);
	}
	return Tags;
}

TArray<UAttachmentData*> UGunsmithComponent::GetAllowedForSlot(FGameplayTag SlotTag) const
{
	return IsValid(PreviewWeapon) ? PreviewWeapon->GetAllowedAttachmentsForSlot(SlotTag) : TArray<UAttachmentData*>();
}

UAttachmentData* UGunsmithComponent::GetEquippedInSlot(FGameplayTag SlotTag) const
{
	return IsValid(PreviewWeapon) ? PreviewWeapon->GetAttachmentInSlot(SlotTag) : nullptr;
}

bool UGunsmithComponent::EquipPreview(UAttachmentData* Attachment)
{
	if (!IsValid(PreviewWeapon) || !PreviewWeapon->EquipAttachment(Attachment)) return false;
	RefreshPreview();
	return true;
}

bool UGunsmithComponent::RemovePreview(FGameplayTag SlotTag)
{
	if (!IsValid(PreviewWeapon) || !PreviewWeapon->RemoveAttachment(SlotTag)) return false;
	RefreshPreview();
	return true;
}

bool UGunsmithComponent::SaveCurrent()
{
	UShooterGameInstance* GI = GetWorld() ? Cast<UShooterGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
	if (!IsValid(GI) || !IsValid(PreviewWeapon))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Gunsmith] Save failed: no preview weapon, or Game Instance Class is not UShooterGameInstance."));
		return false;
	}

	FWeaponPreset Preset;
	Preset.WeaponClass = PreviewWeapon->GetClass();
	for (UAttachmentData* Data : PreviewWeapon->GetEquippedAttachments())
	{
		if (IsValid(Data)) Preset.Attachments.Add(Data);
	}
	GI->EquipPrimaryWeapon(Preset);
	const bool bSaved = GI->SaveLoadout();
	UE_LOG(LogTemp, Log, TEXT("[Gunsmith] Save+equip %s %s"),
		*GetNameSafe(Preset.WeaponClass),
		bSaved ? TEXT("ok") : TEXT("failed"));
	return bSaved;
}

UTextureRenderTarget2D* UGunsmithComponent::GetPreviewRenderTarget() const
{
	return IsValid(PreviewRig) ? PreviewRig->GetRenderTarget() : nullptr;
}

void UGunsmithComponent::RefreshPreview()
{
	EnsureRig();
	if (IsValid(PreviewRig))
	{
		TArray<AWeapon*> List;
		if (IsValid(PreviewWeapon)) List.Add(PreviewWeapon);
		PreviewRig->ShowWeapons(List);
	}
	OnPreviewChanged.Broadcast();
}

void UGunsmithComponent::ClearPreview()
{
	if (IsValid(PreviewWeapon))
	{
		PreviewWeapon->Destroy();
		PreviewWeapon = nullptr;
	}
	if (IsValid(PreviewRig))
	{
		PreviewRig->ShowWeapons({});
	}
}

void UGunsmithComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearPreview();
	if (IsValid(PreviewRig))
	{
		PreviewRig->Destroy();
		PreviewRig = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}