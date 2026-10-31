#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Weapon/Weapon.h"
#include "LoadoutMeshThumbnails.generated.h"

class UImage;
class UTexture2D;
class UButton;
class UGunsmithScreen;

/**
 * WBP_Loadout 父类：预烘焙缩略图 + 槽位按钮进 Gunsmith（按 Kind 过滤）。
 * Image：MainWeaponImage / PistolWeaponImage / VestImage / MagImage / HelmetImage
 * Button（任选一名即可）：Btn_MainWeapon / Button_MainWeapon / MainWeaponButton
 *                 Btn_Pistol / Button_Pistol / PistolButton / Btn_Sidearm
 * Class Defaults 可填 GunsmithWidgetClass（默认 /Game/FPS/UI/WBP_Gunsmith）。
 */
UCLASS()
class FPS_API ULoadoutMeshThumbnails : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Thumbnails|Textures")
	TSoftObjectPtr<UTexture2D> MainWeaponTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Thumbnails|Textures")
	TSoftObjectPtr<UTexture2D> PistolTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Thumbnails|Textures")
	TSoftObjectPtr<UTexture2D> VestTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Thumbnails|Textures")
	TSoftObjectPtr<UTexture2D> MagTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Thumbnails|Textures")
	TSoftObjectPtr<UTexture2D> HelmetTexture;

	/** 仅作对照：对应网格软路径，方便烘焙时对照；运行时不加载、不拍照。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Thumbnails|MeshRefs")
	FSoftObjectPath MainWeaponMeshRef;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Thumbnails|MeshRefs")
	FSoftObjectPath PistolMeshRef;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Thumbnails|MeshRefs")
	FSoftObjectPath VestMeshRef;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Thumbnails|MeshRefs")
	FSoftObjectPath MagMeshRef;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Thumbnails|MeshRefs")
	FSoftObjectPath HelmetMeshRef;

	/** WBP_Gunsmith（父类 UGunsmithScreen）。留空用默认路径。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Loadout|Gunsmith")
	TSoftClassPtr<UGunsmithScreen> GunsmithWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Loadout|Gunsmith")
	int32 GunsmithZOrder = 50;

	UFUNCTION(BlueprintCallable, Category = "FPS|Thumbnails")
	void RefreshAllThumbnails();

	UFUNCTION(BlueprintCallable, Category = "FPS|Thumbnails")
	void ApplyTextureToImage(UImage* Image, const TSoftObjectPtr<UTexture2D>& SoftTex);

	UFUNCTION(BlueprintCallable, Category = "FPS|Loadout|Gunsmith")
	void OpenGunsmithForKind(EWeaponKind Kind);

	UFUNCTION(BlueprintCallable, Category = "FPS|Loadout|Gunsmith")
	void CloseActiveGunsmith();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> MainWeaponImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> PistolWeaponImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> VestImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> MagImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> HelmetImage;

	// 槽位整行按钮（名字对上就绑；否则 Construct 时按名字搜索）
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_MainWeapon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_MainWeapon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> MainWeaponButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Pistol;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Pistol;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> PistolButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Sidearm;

	void EnsureDefaultRefs();
	void BindSlotButtons();
	UButton* ResolveMainWeaponButton() const;
	UButton* ResolvePistolButton() const;
	UButton* FindButtonByNames(const TArray<FName>& Names) const;

	UFUNCTION()
	void HandleMainWeaponClicked();

	UFUNCTION()
	void HandlePistolClicked();

	UFUNCTION()
	void HandleGunsmithClosed();

	UPROPERTY(Transient)
	TObjectPtr<UGunsmithScreen> ActiveGunsmith;
};
