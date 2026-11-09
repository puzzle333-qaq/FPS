#include "Loadout/PouchInventoryComponent.h"
#include "Weapon/Weapon.h"

UPouchInventoryComponent::UPouchInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    Slots.SetNum(SlotCount);
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
    for(int32 i = 0;i< Slots.Num(); ++i)
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