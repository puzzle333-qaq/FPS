#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TraversalComponent.generated.h"

class ACharacter;
class ALadder;
class UAnimMontage;
class UAnimSequenceBase;

UENUM(BlueprintType)
enum class ETraversalMode : uint8
{
	None,
	Vault,
	Mantle,
	LadderAttach,
	Ladder
};

/**
 * VaultMantle: Space-driven vault / mantle / ladder. Created by AShooterCharacter's constructor.
 * Owning client predicts, server re-validates traces and is authoritative (rejects snap back).
 * During a move the CMC is put in MOVE_Flying with MaxFlySpeed 0 and the position is driven here.
 * 3P body plays FreeAnimationLibrary Vault/Mantle/Ladder sequences via BodySlot dynamic montages.
 */
UCLASS(ClassGroup = (FPS), meta = (BlueprintSpawnableComponent))
class FPS_API UTraversalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTraversalComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Owning client: Space pressed. Returns true if it consumed the press. */
	UFUNCTION(BlueprintCallable, Category = "Traversal")
	bool TryTraversal();

	UFUNCTION(BlueprintPure, Category = "Traversal")
	bool IsBusy() const { return Mode != ETraversalMode::None; }

	UFUNCTION(BlueprintPure, Category = "Traversal")
	bool IsOnLadder() const { return Mode == ETraversalMode::Ladder || Mode == ETraversalMode::LadderAttach; }

	UFUNCTION(BlueprintPure, Category = "Traversal")
	ETraversalMode GetMode() const { return Mode; }

	// ---- Tunables (cm, measured from capsule bottom / feet) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float MinObstacleHeight = 40.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float WaistHeight = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float EyeHeight = 165.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float MaxReach = 75.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float MaxVaultDepth = 110.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float VaultDuration = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float MantleDurationLow = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float MantleDurationHigh = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float LadderSpeed = 150.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float LadderTopExitDuration = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	float VaultNoiseLoudness = 1.0f;

	/** Use Mantle 2M clip when obstacle height (cm) is at/above this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Anim")
	float MantleHighAnimHeight = 130.f;

	/** ABP_ThirdPerson montage slot (same as slide). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Anim")
	FName TraversalSlotName = TEXT("BodySlot");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Anim")
	TSoftObjectPtr<UAnimSequenceBase> VaultAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Anim")
	TSoftObjectPtr<UAnimSequenceBase> MantleAnimLow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Anim")
	TSoftObjectPtr<UAnimSequenceBase> MantleAnimHigh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Anim")
	TSoftObjectPtr<UAnimSequenceBase> LadderUpAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Anim")
	TSoftObjectPtr<UAnimSequenceBase> LadderDownAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Anim")
	TSoftObjectPtr<UAnimSequenceBase> LadderIdleAnim;

	/** Optional override for ladder top exit; falls back to MantleAnimLow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Anim")
	TSoftObjectPtr<UAnimSequenceBase> LadderTopExitAnim;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Mode)
	ETraversalMode Mode = ETraversalMode::None;

	UPROPERTY(Replicated)
	TObjectPtr<ALadder> CurrentLadder;

	bool bInterp = false;
	FVector P0, P1, P2;
	float Elapsed = 0.f;
	float Duration = 0.f;
	ETraversalMode AfterMode = ETraversalMode::None;
	float LadderFeetZ = 0.f;
	float LastObstacleHeight = 0.f;
	int8 LadderAnimDir = 0; // -1 down, 0 idle, +1 up

	bool bControlling = false;
	float SavedMaxFlySpeed = 0.f;
	float RestoreFlagsTime = -1.f;
	FVector StartLocation;
	bool bSavedRootMotion = false;
	uint8 SavedRootMotionMode = 0;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveTraversalMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveTraversalMontage1P;

	ACharacter* GetChar() const;
	bool ComputeVaultMantle(ETraversalMode& OutMode, FVector& OutP1, FVector& OutP2, float& OutDuration, float& OutHeight) const;
	ALadder* FindLadder() const;
	bool CapsuleFits(const FVector& At) const;
	bool CapsuleSweepClear(const FVector& From, const FVector& To) const;

	void BeginControl();
	void EndControl();
	void StartInterp(const FVector& InP1, const FVector& InP2, float InDuration, ETraversalMode InMode, ETraversalMode InAfter);
	void FinishInterp();
	bool StartLadder(ALadder* Ladder);
	bool ExitLadderTop();
	void ExitLadderDrop();
	void TickLadderLocal(float DeltaTime);

	void EnsureSkeletonCompat();
	void PlayTraversalAnim(ETraversalMode InMode, float ObstacleHeight, float InDuration, int8 LadderDir);
	void StopTraversalAnim(float BlendOut = 0.2f);
	void UpdateLadderAnim(int8 Dir);

	UFUNCTION()
	void OnRep_Mode();

	UFUNCTION(Server, Reliable)
	void ServerTryVaultMantle(FVector ClientStart);
	UFUNCTION(Server, Reliable)
	void ServerAttachLadder(ALadder* Ladder);
	UFUNCTION(Server, Reliable)
	void ServerLadderExit(bool bTop);
	UFUNCTION(Client, Reliable)
	void ClientRejected(FVector ServerLocation);
};
