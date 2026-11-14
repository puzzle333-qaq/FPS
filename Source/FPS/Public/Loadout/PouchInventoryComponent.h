#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "PouchInventoryComponent.generated.h"

class AWeapon;

UENUM(BlueprintType)
enum class EPouchSlotKind : uint8
{
    Empty UMETA(DisplayName = "Empty"),
    Magazine UMETA(DisplayName = "Magazine"),
    Throwable UMETA(DisplayName = "Throwable"),
};

USTRUCT(BlueprintType)
struct FPouchSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Pouch")
    EPouchSlotKind Kind = EPouchSlotKind::Empty;

    //用武器的WeaponType来判断弹匣属于哪吧枪
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Pouch")
    FGameplayTag MagWeaponType;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Pouch")
    int32 Rounds = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Pouch")
    int32 MagCapacity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Pouch")
    TSubclassOf<AWeapon> ThrowableClass;

    bool IsEmpty() const { return Kind == EPouchSlotKind::Empty; }
};

UCLASS(ClassGroup = (FPS), meta = (BlueprintSpawnableComponent))
class FPS_API UPouchInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPouchInventoryComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    static constexpr int32 SlotCount = 13;

    UFUNCTION(BlueprintCallable, Category = "FPS|Pouch")
    int32 GetSlotCount() const;
    UFUNCTION(BlueprintPure,Category="FPS|Pouch") int32 GetReservedSlotCount() const;
    void ReconcileCapacity();

    UFUNCTION(BlueprintCallable, Category = "FPS|Pouch")
    FPouchSlot GetSlot(int32 Index) const;
    
    UFUNCTION(BlueprintCallable ,Category = "FPS|Pouch")
    int32 CountUsed() const;

    UFUNCTION(BlueprintCallable, Category = "FPS|Pouch")
    int32 TryAddMagzine(FGameplayTag WeaponType,int32 Rounds,int32 Capacity);

    UFUNCTION(BlueprintCallable, Category = "FPS|Pouch")
    int32 TryAddThrowable(TSubclassOf<AWeapon> ThrowableClass);

    UFUNCTION(BlueprintCallable, Category = "FPS|Pouch")
    void ClearSlot(int32 Index);

private:

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "FPS|Pouch", meta = (AllowPrivateAccess = "true"))
    TArray<FPouchSlot> Slots;

    int32 FindEmptySlot() const;
    static const FPouchSlot& GetEmptySlot();

};
