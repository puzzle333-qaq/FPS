#include "Range/RangeLoadoutBench.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimationAsset.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "ShooterPlayerController.h"
#include "Weapon/AttachmentData.h"
#include "Weapon/Weapon.h"

ARangeLoadoutBench::ARangeLoadoutBench()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
	InteractBox->SetupAttachment(SceneRoot);
	InteractBox->SetBoxExtent(FVector(80.f, 80.f, 90.f));
	InteractBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractBox->SetCanEverAffectNavigation(false);

	CharacterMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh"));
	CharacterMesh->SetupAttachment(SceneRoot);
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CharacterMesh->SetCanEverAffectNavigation(false);

	Prompt = NSLOCTEXT("RangeBench", "Prompt", "按 F 改枪");
}

void ARangeLoadoutBench::BeginPlay()
{
	Super::BeginPlay();

	USkeletalMeshComponent* Mesh = GetPreviewMesh();
	if (!IsValid(Mesh)) return;
	if (PreviewAnimClass)
	{
		Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		Mesh->SetAnimInstanceClass(PreviewAnimClass);
	}
	else if (PreviewIdle)
	{
		Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Mesh->PlayAnimation(PreviewIdle, true);
	}
}

AActor* ARangeLoadoutBench::GetBenchViewTarget() const
{
	if (IsValid(ViewCamera))
	{
		return ViewCamera.Get();
	}
	return const_cast<ARangeLoadoutBench*>(this);
}

USkeletalMeshComponent* ARangeLoadoutBench::GetPreviewMesh() const
{
	return IsValid(ExternalPreviewMesh) ? ExternalPreviewMesh.Get() : CharacterMesh.Get();
}

void ARangeLoadoutBench::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	USkeletalMeshComponent* Mesh = GetPreviewMesh();
	if (!IsValid(PreviewWeapon) || !IsValid(Mesh)) return;
	const USceneComponent* Root = PreviewWeapon->GetRootComponent();
	if (!Root || Root->GetAttachParent() != Mesh)
	{
		AttachPreview();
	}
}

bool ARangeLoadoutBench::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return IsValid(InteractingPawn);
}

FText ARangeLoadoutBench::GetInteractPrompt_Implementation(APawn* InteractingPawn) const
{
	return IsValid(InteractingPawn) ? Prompt : FText::GetEmpty();
}

void ARangeLoadoutBench::Interact_Implementation(APawn* InteractingPawn)
{
	if (!HasAuthority() || !IsValid(InteractingPawn)) return;
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(InteractingPawn->GetController()))
	{
		PC->Auth_OpenRangeBench(this);
	}
}

void ARangeLoadoutBench::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARangeLoadoutBench, PreviewWeapon);
}

void ARangeLoadoutBench::Auth_SetPreview(TSubclassOf<AWeapon> WeaponClass, const TArray<UAttachmentData*>& Attachments, APawn* Viewer)
{
	if (!HasAuthority()) return;
	Auth_ClearPreview();
	if (!WeaponClass || !WeaponClass->IsChildOf(AWeapon::StaticClass()) || WeaponClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return;
	}
	if (!IsValid(GetWorld())) return;

	FActorSpawnParameters Params;
	Params.Owner = IsValid(Viewer) ? static_cast<AActor*>(Viewer) : this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWeapon* Weapon = GetWorld()->SpawnActor<AWeapon>(WeaponClass, GetActorTransform(), Params);
	if (!IsValid(Weapon))
	{
		UE_LOG(LogTemp, Warning, TEXT("[RangeBench] preview spawn failed for %s"), *GetNameSafe(WeaponClass));
		return;
	}
	// 枪默认跟着 Owner 的相关性走。预览要让站在台子前的人看见，改成按距离同步。
	Weapon->bNetUseOwnerRelevancy = false;
	Weapon->SetNetCullDistanceSquared(FMath::Square(8000.f));

	for (const FWeaponAttachmentSlot& Slot : Weapon->GetAttachmentSlots())
	{
		const bool bInPreset = Attachments.ContainsByPredicate([Weapon, &Slot](const UAttachmentData* Data)
		{
			return IsValid(Data) && IsValid(Weapon) && Weapon->GetEquipSlot(Data) == Slot.SlotTag;
		});
		if (!bInPreset)
		{
			Weapon->RemoveAttachment(Slot.SlotTag);
		}
	}
	for (UAttachmentData* Data : Attachments)
	{
		if (IsValid(Data))
		{
			Weapon->EquipAttachment(Data);
		}
	}

	PreviewWeapon = Weapon;
	AttachPreview();
	ForceNetUpdate();
}

void ARangeLoadoutBench::Auth_ClearPreview()
{
	if (!HasAuthority()) return;
	if (IsValid(PreviewWeapon))
	{
		PreviewWeapon->Destroy();
	}
	PreviewWeapon = nullptr;
}

void ARangeLoadoutBench::OnRep_PreviewWeapon()
{
	AttachPreview();
}

FName ARangeLoadoutBench::ResolveHandSocket() const
{
	USkeletalMeshComponent* Mesh = GetPreviewMesh();
	if (!IsValid(Mesh)) return NAME_None;
	const FName Candidates[] = {
		HandSocket,
		FName(TEXT("weapon_r")),
		FName(TEXT("Weapon_r")),
		FName(TEXT("hand_r")),
		FName(TEXT("ik_hand_gun")),
		FName(TEXT("RifleSocket"))
	};
	for (const FName& Name : Candidates)
	{
		if (!Name.IsNone() && Mesh->DoesSocketExist(Name))
		{
			return Name;
		}
	}
	return NAME_None;
}

void ARangeLoadoutBench::AttachPreview()
{
	USkeletalMeshComponent* Mesh = GetPreviewMesh();
	if (!IsValid(PreviewWeapon) || !IsValid(Mesh)) return;
	PreviewWeapon->SetActorHiddenInGame(false);
	PreviewWeapon->SetActorEnableCollision(false);
	PreviewWeapon->AttachCosmeticThirdPerson(Mesh, ResolveHandSocket(), PreviewWeaponOffset);
}
