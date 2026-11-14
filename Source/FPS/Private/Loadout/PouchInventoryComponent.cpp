#include "Loadout/PouchInventoryComponent.h"
#include "Weapon/Weapon.h"
#include "Net/UnrealNetwork.h"
#include "ShooterCharacter.h"
#include "Character/PoliceAppearanceComponent.h"

int32 UPouchInventoryComponent::GetReservedSlotCount() const
{
 const auto* C=Cast<AShooterCharacter>(GetOwner());
 return C?int32(C->GetBodyArmor()==EBodyArmorType::Heavy)+int32(C->PoliceAppearance&&C->PoliceAppearance->FaceProtection==EPoliceFaceProtection::GogglesAndMask):0;
}
int32 UPouchInventoryComponent::GetSlotCount() const { return SlotCount-GetReservedSlotCount(); }
void UPouchInventoryComponent::ReconcileCapacity()
{
 TArray<FPouchSlot> Packed;for(const auto& Entry:Slots)if(!Entry.IsEmpty())Packed.Add(Entry);
 for(int32 I=0;I<Slots.Num();++I)Slots[I]=I<GetSlotCount()&&Packed.IsValidIndex(I)?Packed[I]:FPouchSlot();
}

UPouchInventoryComponent::UPouchInventoryComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
    Slots.SetNum(SlotCount);
}

void UPouchInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(UPouchInventoryComponent,Slots);
}

const FPouchSlot& UPouchInventoryComponent::GetEmptySlot()
{
    static const FPouchSlot Empty;
    return Empty;
}

FPouchSlot UPouchInventoryComponent::GetSlot(int32 Index) const
{
    return Slots.IsValidIndex(Index) ? Slots[Index] : GetEmptySlot();
}

int32 UPouchInventoryComponent::FindEmptySlot() const
{
    for(int32 i = 0;i< GetSlotCount(); ++i)
    {
        if (Slots[i].IsEmpty()) return i;
    }
    return INDEX_NONE;
}

int32 UPouchInventoryComponent::CountUsed() const
{
    int32 Used = 0;
    for (const FPouchSlot& Slot : Slots)
    {
        if(!Slot.IsEmpty()) ++Used;
    }
    return Used;
}

int32 UPouchInventoryComponent::TryAddMagzine(FGameplayTag WeaponType,int32 Rounds,int32 Capacity)
{
    if (!WeaponType.IsValid() || Rounds <= 0 || Capacity <= 0) return INDEX_NONE;
    const int32 Index = FindEmptySlot();
    if(Index == INDEX_NONE) return INDEX_NONE;

    FPouchSlot& Slot = Slots[Index];
    Slot.Kind = EPouchSlotKind::Magazine;
    Slot.MagWeaponType = WeaponType;
    Slot.Rounds = FMath::Min(Rounds,Capacity);
    Slot.MagCapacity = Capacity;
    Slot.ThrowableClass = nullptr;
    return Index;
}

int32 UPouchInventoryComponent::TryAddThrowable(TSubclassOf<AWeapon> ThrowableClass)
{
    const AWeapon* CDO = ThrowableClass ? ThrowableClass->GetDefaultObject<AWeapon>() : nullptr;
    if(!CDO || CDO->WeaponKind != EWeaponKind::Throwable) return INDEX_NONE;
    const int32 Index = FindEmptySlot();
    if(Index == INDEX_NONE) return INDEX_NONE;

    FPouchSlot& Slot = Slots[Index];
    Slot = FPouchSlot();
    Slot.Kind = EPouchSlotKind::Throwable;
    Slot.ThrowableClass = ThrowableClass;
    return Index;
}

void UPouchInventoryComponent::ClearSlot(int32 Index)
{
    if(Slots.IsValidIndex(Index))
    {
        Slots[Index] = FPouchSlot();
    }
}
