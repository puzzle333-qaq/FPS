#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Weapon/Weapon.h"
#include "GunsmithComponent.generated.h"

class ALoadoutPreviewRig;
class UAttachmentData;
class UTextureRenderTarget2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGunsmithPreviewChanged);

// 改枪台属性条的一行：当前值 / 换上候选配件后的值 / 满格值
USTRUCT(BlueprintType)
struct FGunsmithStatLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Gunsmith")
	FName StatId;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Gunsmith")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Gunsmith")
	float Current = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Gunsmith")
	float Preview = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Gunsmith")
	float Max = 100.f;
};

// 改枪台（局外，只在本地）：生成一把预览枪，装/卸配件，画进贴图，保存到 GameInstance + 存档。
// 不联网、不复制；进局后由 AShooterPlayerController 把配装发给服务器重新校验。
UCLASS(ClassGroup = (FPS), meta = (BlueprintSpawnableComponent))
class FPS_API UGunsmithComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGunsmithComponent();

	// 改枪台可选的枪（武器列表按钮用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith")
	TArray<TSubclassOf<AWeapon>> AvailableWeapons;

	// 预览用的拍照台，可以做蓝图子类调相机/灯光
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith")
	TSubclassOf<ALoadoutPreviewRig> PreviewRigClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith", meta = (ClampMin = "16"))
	int32 PreviewWidth = 1024;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith", meta = (ClampMin = "16"))
	int32 PreviewHeight = 576;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith", meta = (ClampMin = "1"))
	float PreviewOrthoWidth = 120.f;

	// 换一把预览枪；会套用已保存的配件
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith")
	bool SetPreviewWeapon(TSubclassOf<AWeapon> WeaponClass);

	UFUNCTION(BlueprintPure, Category = "FPS|Gunsmith")
	AWeapon* GetPreviewWeapon() const { return PreviewWeapon; }

	UFUNCTION(BlueprintPure, Category = "FPS|Gunsmith")
	TArray<FWeaponAttachmentSlot> GetSlots() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Gunsmith")
	TArray<FGameplayTag> GetSlotTags() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Gunsmith")
	TArray<UAttachmentData*> GetAllowedForSlot(FGameplayTag SlotTag) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Gunsmith")
	UAttachmentData* GetEquippedInSlot(FGameplayTag SlotTag) const;

	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith")
	bool EquipPreview(UAttachmentData* Attachment);

	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith")
	bool RemovePreview(FGameplayTag SlotTag);

	// 保存当前预览枪的配件，长枪同时装备成第一把。并存盘。
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith")
	bool SaveCurrent();

	// 给 UMG Image 当笔刷
	UFUNCTION(BlueprintPure, Category = "FPS|Gunsmith")
	UTextureRenderTarget2D* GetPreviewRenderTarget() const;

	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith")
	void RefreshPreview();

	// 退出改枪台时可调，清掉预览枪
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith")
	void ClearPreview();

	UPROPERTY(BlueprintAssignable, Category = "FPS|Gunsmith")
	FGunsmithPreviewChanged OnPreviewChanged;

	// ---- 预览视角 ----
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith|View")
	void RotatePreview(float DeltaYaw, float DeltaPitch);

	// Delta > 0 拉近。按比例缩放正交宽度
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith|View")
	void ZoomPreview(float Delta);

	// 镜头平移并拉近到这个槽位（装了配件看配件，没装看 Socket）
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith|View")
	bool FocusSlot(FGameplayTag SlotTag);

	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith|View")
	void ResetPreviewView();

	// 槽位在预览贴图上的位置（UV 0~1）
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith|View")
	bool GetSlotScreenUV(FGameplayTag SlotTag, FVector2D& OutUV) const;

	// 对准槽位时的正交宽度（越小越近）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith|View", meta = (ClampMin = "5"))
	float FocusOrthoWidth = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith|View", meta = (ClampMin = "5"))
	float MinOrthoWidth = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith|View", meta = (ClampMin = "5"))
	float MaxOrthoWidth = 220.f;

	// ---- 属性 ----
	// 这个槽换成 Candidate 后的属性（Candidate 为空 = 卸下这个槽）。Current 是现在的配装
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith|Stats")
	TArray<FGunsmithStatLine> GetStatPreview(FGameplayTag SlotTag, UAttachmentData* Candidate) const;

	// 不对比，只看当前
	UFUNCTION(BlueprintCallable, Category = "FPS|Gunsmith|Stats")
	TArray<FGunsmithStatLine> GetCurrentStats() const;

	// 伤害条满格值（单发伤害 × 弹丸数）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Gunsmith|Stats", meta = (ClampMin = "1"))
	float StatMaxDamage = 150.f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void EnsureRig();
	void ComputeStats(const TArray<UAttachmentData*>& Attachments, TArray<FGunsmithStatLine>& Out) const;

	UPROPERTY()
	TObjectPtr<AWeapon> PreviewWeapon;

	UPROPERTY()
	TObjectPtr<ALoadoutPreviewRig> PreviewRig;
};