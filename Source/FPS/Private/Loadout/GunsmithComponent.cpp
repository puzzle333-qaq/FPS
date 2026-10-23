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
void UGunsmithComponent::RotatePreview(float DeltaYaw, float DeltaPitch)
{
	if (!IsValid(PreviewRig)) return;
	FRotator R = PreviewRig->GetViewRotation();
	R.Yaw += DeltaYaw;
	R.Pitch += DeltaPitch;
	PreviewRig->SetViewRotation(R);
}

void UGunsmithComponent::ZoomPreview(float Delta)
{
	if (!IsValid(PreviewRig) || FMath::IsNearlyZero(Delta)) return;
	const float Width = PreviewRig->GetViewOrthoWidth() * FMath::Pow(0.9f, Delta);
	PreviewRig->SetViewOrthoWidth(FMath::Clamp(Width, MinOrthoWidth, FMath::Max(MaxOrthoWidth, MinOrthoWidth)));
}

bool UGunsmithComponent::FocusSlot(FGameplayTag SlotTag)
{
	FVector World;
	if (!IsValid(PreviewRig) || !PreviewRig->GetSlotWorldLocation(PreviewWeapon, SlotTag, World)) return false;
	PreviewRig->SetViewPan(PreviewRig->WorldToPan(World));
	PreviewRig->SetViewOrthoWidth(FocusOrthoWidth);
	return true;
}

void UGunsmithComponent::ResetPreviewView()
{
	if (!IsValid(PreviewRig)) return;
	PreviewRig->SetViewRotation(FRotator::ZeroRotator);
	PreviewRig->SetViewPan(FVector2D::ZeroVector);
	PreviewRig->SetViewOrthoWidth(PreviewOrthoWidth);
}

bool UGunsmithComponent::GetSlotScreenUV(FGameplayTag SlotTag, FVector2D& OutUV) const
{
	FVector World;
	return IsValid(PreviewRig)
		&& PreviewRig->GetSlotWorldLocation(PreviewWeapon, SlotTag, World)
		&& PreviewRig->ProjectWorldToUV(World, OutUV);
}

void UGunsmithComponent::ComputeStats(const TArray<UAttachmentData*>& Attachments, TArray<FGunsmithStatLine>& Out) const
{
	Out.Reset();
	if (!IsValid(PreviewWeapon)) return;

	float Damage = PreviewWeapon->Damage * FMath::Max(PreviewWeapon->PelletsPerShot, 1);
	float Accuracy = 50.f, Control = 50.f, Range = 50.f, Mobility = 50.f;
	int32 Mag = PreviewWeapon->AuthoredMagCapacity > 0 ? PreviewWeapon->AuthoredMagCapacity : PreviewWeapon->MagCapacity;
	for (const UAttachmentData* A : Attachments)
	{
		if (!IsValid(A)) continue;
		Damage += A->StatDamage;
		Accuracy += A->StatAccuracy + (1.f - A->HipFireSpreadMultiplier) * 100.f;
		Control += A->StatControl + (1.f - A->RecoilMultiplier) * 100.f;
		Range += A->StatRange;
		Mobility += A->StatMobility;
		if (A->MagazineCapacity > 0) Mag = A->MagazineCapacity;
	}

	auto Add = [&Out](const TCHAR* Id, const FText& Label, float Value, float Max)
	{
		FGunsmithStatLine Line;
		Line.StatId = Id;
		Line.Label = Label;
		Line.Current = Line.Preview = Value;
		Line.Max = Max;
		Out.Add(Line);
	};
	Add(TEXT("Damage"), NSLOCTEXT("Gunsmith", "Damage", "伤害"), FMath::Max(Damage, 0.f), StatMaxDamage);
	Add(TEXT("Accuracy"), NSLOCTEXT("Gunsmith", "Accuracy", "精准"), FMath::Clamp(Accuracy, 0.f, 100.f), 100.f);
	Add(TEXT("Control"), NSLOCTEXT("Gunsmith", "Control", "操控"), FMath::Clamp(Control, 0.f, 100.f), 100.f);
	Add(TEXT("Range"), NSLOCTEXT("Gunsmith", "Range", "射程"), FMath::Clamp(Range, 0.f, 100.f), 100.f);
	Add(TEXT("Mobility"), NSLOCTEXT("Gunsmith", "Mobility", "机动"), FMath::Clamp(Mobility, 0.f, 100.f), 100.f);
	Add(TEXT("Magazine"), NSLOCTEXT("Gunsmith", "Magazine", "弹匣"), float(Mag), FMath::Max(100.f, float(Mag)));
}

TArray<FGunsmithStatLine> UGunsmithComponent::GetCurrentStats() const
{
	TArray<FGunsmithStatLine> Lines;
	if (IsValid(PreviewWeapon)) ComputeStats(PreviewWeapon->GetEquippedAttachments(), Lines);
	return Lines;
}

TArray<FGunsmithStatLine> UGunsmithComponent::GetStatPreview(FGameplayTag SlotTag, UAttachmentData* Candidate) const
{
	TArray<FGunsmithStatLine> Lines;
	if (!IsValid(PreviewWeapon)) return Lines;
	const TArray<UAttachmentData*> Current = PreviewWeapon->GetEquippedAttachments();
	ComputeStats(Current, Lines);

	const FGameplayTag Slot = IsValid(Candidate) ? Candidate->SlotTag : SlotTag;
	TArray<UAttachmentData*> Next;
	for (UAttachmentData* A : Current)
	{
		if (!IsValid(A) || A->SlotTag.MatchesTagExact(Slot)) continue;
		// 候选配件会锁住的槽（短护木 -> 握把）在对比里也去掉
		if (IsValid(Candidate) && Candidate->BlockedSlots.HasTagExact(A->SlotTag)) continue;
		Next.Add(A);
	}
	if (IsValid(Candidate)) Next.Add(Candidate);

	TArray<FGunsmithStatLine> After;
	ComputeStats(Next, After);
	for (int32 i = 0; i < Lines.Num() && i < After.Num(); ++i)
	{
		Lines[i].Preview = After[i].Current;
		Lines[i].Max = FMath::Max(Lines[i].Max, After[i].Max);
	}
	return Lines;
}