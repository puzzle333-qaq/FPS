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

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void EnsureRig();

	UPROPERTY()
	TObjectPtr<AWeapon> PreviewWeapon;

	UPROPERTY()
	TObjectPtr<ALoadoutPreviewRig> PreviewRig;
};