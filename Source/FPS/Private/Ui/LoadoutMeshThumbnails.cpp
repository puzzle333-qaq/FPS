#include "Ui/LoadoutMeshThumbnails.h"

#include "Ui/GunsmithScreen.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"
#include "UObject/SoftObjectPath.h"
#include "GameFramework/PlayerController.h"

void ULoadoutMeshThumbnails::EnsureDefaultRefs()
{
	// 默认贴图路径（先按 BakeMeshThumbs_HOWTO 烘焙/导入到这些名字）
	if (MainWeaponTexture.IsNull())
	{
		MainWeaponTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
			TEXT("/Game/FPS/UI/Thumbs/T_Thumb_RAR.T_Thumb_RAR")));
	}
	if (PistolTexture.IsNull())
	{
		PistolTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
			TEXT("/Game/FPS/UI/Thumbs/T_Thumb_Pistol.T_Thumb_Pistol")));
	}
	if (VestTexture.IsNull())
	{
		VestTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
			TEXT("/Game/FPS/UI/Thumbs/T_Thumb_Vest.T_Thumb_Vest")));
	}
	if (MagTexture.IsNull())
	{
		MagTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
			TEXT("/Game/FPS/UI/Thumbs/T_Thumb_Mag.T_Thumb_Mag")));
	}
	if (HelmetTexture.IsNull())
	{
		HelmetTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
			TEXT("/Game/FPS/UI/Thumbs/T_Thumb_Helmet.T_Thumb_Helmet")));
	}

	if (GunsmithWidgetClass.IsNull())
	{
		GunsmithWidgetClass = TSoftClassPtr<UGunsmithScreen>(FSoftObjectPath(
			TEXT("/Game/FPS/UI/WBP_Gunsmith.WBP_Gunsmith_C")));
	}

	// Mesh 对照表（不加载）
	if (!MainWeaponMeshRef.IsValid())
	{
		MainWeaponMeshRef = FSoftObjectPath(
			TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Weapon/Meshes/SK_RAR_AssaultRifle.SK_RAR_AssaultRifle"));
	}
	if (!PistolMeshRef.IsValid())
	{
		PistolMeshRef = FSoftObjectPath(
			TEXT("/Game/InfimaGames/ModernGunsBundle/ModernPistol/Meshes/SK_Pistol_Default_Combined.SK_Pistol_Default_Combined"));
	}
	if (!VestMeshRef.IsValid())
	{
		VestMeshRef = FSoftObjectPath(
			TEXT("/Game/Bandits/Mesh/Cloth/Bomber/SKM_Bomber_Vest.SKM_Bomber_Vest"));
	}
	if (!MagMeshRef.IsValid())
	{
		MagMeshRef = FSoftObjectPath(
			TEXT("/Game/InfimaGames/RealisticAssaultRifle/Art/Weapon/Meshes/SM_RAR_AssaultRifle_Magazine_Default_Full.SM_RAR_AssaultRifle_Magazine_Default_Full"));
	}
}

void ULoadoutMeshThumbnails::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureDefaultRefs();
	RefreshAllThumbnails();
	BindSlotButtons();
}

void ULoadoutMeshThumbnails::NativeDestruct()
{
	CloseActiveGunsmith();
	Super::NativeDestruct();
}

void ULoadoutMeshThumbnails::ApplyTextureToImage(UImage* Image, const TSoftObjectPtr<UTexture2D>& SoftTex)
{
	if (!IsValid(Image) || SoftTex.IsNull()) return;

	UTexture2D* Tex = SoftTex.LoadSynchronous();
	if (!IsValid(Tex)) return;

	FSlateBrush Brush = Image->GetBrush();
	Brush.SetResourceObject(Tex);
	const int32 W = Tex->GetSizeX();
	const int32 H = Tex->GetSizeY();
	Brush.ImageSize = FVector2D(
		W > 0 ? static_cast<float>(W) : 256.f,
		H > 0 ? static_cast<float>(H) : 256.f);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Image->SetBrush(Brush);
}

void ULoadoutMeshThumbnails::RefreshAllThumbnails()
{
	ApplyTextureToImage(MainWeaponImage, MainWeaponTexture);
	ApplyTextureToImage(PistolWeaponImage, PistolTexture);
	ApplyTextureToImage(VestImage, VestTexture);
	ApplyTextureToImage(MagImage, MagTexture);
	ApplyTextureToImage(HelmetImage, HelmetTexture);
}

UButton* ULoadoutMeshThumbnails::FindButtonByNames(const TArray<FName>& Names) const
{
	if (!WidgetTree) return nullptr;
	TArray<UButton*> Buttons;
	WidgetTree->ForEachWidget([&Buttons](UWidget* W)
	{
		if (UButton* B = Cast<UButton>(W))
		{
			Buttons.Add(B);
		}
	});
	for (const FName& Wanted : Names)
	{
		for (UButton* B : Buttons)
		{
			if (B && B->GetFName() == Wanted) return B;
		}
	}
	// 宽松：名字包含关键词（避免误绑到 Image 控件）
	for (UButton* B : Buttons)
	{
		if (!B) continue;
		const FString N = B->GetName();
		for (const FName& Wanted : Names)
		{
			if (N.Contains(Wanted.ToString(), ESearchCase::IgnoreCase)) return B;
		}
	}
	return nullptr;
}

UButton* ULoadoutMeshThumbnails::ResolveMainWeaponButton() const
{
	if (Btn_MainWeapon) return Btn_MainWeapon;
	if (Button_MainWeapon) return Button_MainWeapon;
	if (MainWeaponButton) return MainWeaponButton;
	return FindButtonByNames({
		TEXT("Btn_MainWeapon"), TEXT("Button_MainWeapon"), TEXT("MainWeaponButton"),
		TEXT("MainWeapon"), TEXT("Btn_Primary"), TEXT("PrimaryWeapon")
	});
}

UButton* ULoadoutMeshThumbnails::ResolvePistolButton() const
{
	if (Btn_Pistol) return Btn_Pistol;
	if (Button_Pistol) return Button_Pistol;
	if (PistolButton) return PistolButton;
	if (Btn_Sidearm) return Btn_Sidearm;
	return FindButtonByNames({
		TEXT("Btn_Pistol"), TEXT("Button_Pistol"), TEXT("PistolButton"),
		TEXT("Btn_Sidearm"), TEXT("Pistol"), TEXT("Sidearm")
	});
}

void ULoadoutMeshThumbnails::BindSlotButtons()
{
	if (UButton* MainBtn = ResolveMainWeaponButton())
	{
		MainBtn->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMainWeaponClicked);
		UE_LOG(LogTemp, Log, TEXT("[Loadout] Bound MainWeapon button: %s"), *MainBtn->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Loadout] No MainWeapon button found (name Btn_MainWeapon / Button_MainWeapon / MainWeaponButton)."));
	}

	if (UButton* PistolBtn = ResolvePistolButton())
	{
		PistolBtn->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePistolClicked);
		UE_LOG(LogTemp, Log, TEXT("[Loadout] Bound Pistol button: %s"), *PistolBtn->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Loadout] No Pistol button found (name Btn_Pistol / Button_Pistol / PistolButton)."));
	}
}

void ULoadoutMeshThumbnails::HandleMainWeaponClicked()
{
	OpenGunsmithForKind(EWeaponKind::LongGun);
}

void ULoadoutMeshThumbnails::HandlePistolClicked()
{
	OpenGunsmithForKind(EWeaponKind::Pistol);
}

void ULoadoutMeshThumbnails::OpenGunsmithForKind(EWeaponKind Kind)
{
	EnsureDefaultRefs();
	CloseActiveGunsmith();

	UClass* Class = GunsmithWidgetClass.LoadSynchronous();
	if (!Class)
	{
		UE_LOG(LogTemp, Error, TEXT("[Loadout] GunsmithWidgetClass failed to load. Set Class Defaults or create /Game/FPS/UI/WBP_Gunsmith."));
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!IsValid(PC))
	{
		UE_LOG(LogTemp, Error, TEXT("[Loadout] OpenGunsmithForKind: no owning PlayerController."));
		return;
	}

	UGunsmithScreen* Screen = CreateWidget<UGunsmithScreen>(PC, Class);
	if (!IsValid(Screen))
	{
		UE_LOG(LogTemp, Error, TEXT("[Loadout] CreateWidget Gunsmith failed (parent must be UGunsmithScreen)."));
		return;
	}

	ActiveGunsmith = Screen;
	Screen->OnClosed.AddUniqueDynamic(this, &ThisClass::HandleGunsmithClosed);
	Screen->OpenForKind(Kind);
	Screen->AddToViewport(GunsmithZOrder);
	UE_LOG(LogTemp, Log, TEXT("[Loadout] Opened Gunsmith for Kind=%d"), static_cast<int32>(Kind));
}

void ULoadoutMeshThumbnails::HandleGunsmithClosed()
{
	CloseActiveGunsmith();
}

void ULoadoutMeshThumbnails::CloseActiveGunsmith()
{
	if (!IsValid(ActiveGunsmith))
	{
		ActiveGunsmith = nullptr;
		return;
	}
	ActiveGunsmith->OnClosed.RemoveDynamic(this, &ThisClass::HandleGunsmithClosed);
	ActiveGunsmith->RemoveFromParent();
	ActiveGunsmith = nullptr;
}
