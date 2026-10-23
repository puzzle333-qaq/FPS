#include "AnimNotify/AnimNotifyState_KitMagInHand.h"

#include "Weapon/Weapon.h"
#include "Interface/PlayerInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

namespace
{
	struct FKitMagHome
	{
		TWeakObjectPtr<USceneComponent> Parent;
		FName Socket;
		FTransform Relative;
		FVector WorldScale;
	};
	TMap<TWeakObjectPtr<USceneComponent>, FKitMagHome> GKitMagHomes;

	USceneComponent* FindKitMag(USkeletalMeshComponent* MeshComp, const FString& Prefix)
	{
		if (!IsValid(MeshComp)) return nullptr;
		AActor* Owner = MeshComp->GetOwner();
		if (!IsValid(Owner) || !Owner->Implements<UPlayerInterface>()) return nullptr;
		AWeapon* Weapon = IPlayerInterface::Execute_GetCurrentWeapon(Owner);
		if (!IsValid(Weapon)) return nullptr;
		const bool bFirstPerson = IPlayerInterface::Execute_GetMesh1P(Owner) == MeshComp;
		const FString Wanted = Prefix + (bFirstPerson ? TEXT("_1P") : TEXT("_3P"));
		TArray<UStaticMeshComponent*> Comps;
		Weapon->GetComponents<UStaticMeshComponent>(Comps);
		for (UStaticMeshComponent* C : Comps)
		{
			if (IsValid(C) && C->GetName() == Wanted) return C;
		}
		return nullptr;
	}
}

void UAnimNotifyState_KitMagInHand::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	USceneComponent* Mag = FindKitMag(MeshComp, MagComponentPrefix);
	if (!Mag || HandBone.IsNone() || !MeshComp->DoesSocketExist(HandBone)) return;
	if (!GKitMagHomes.Contains(Mag))
	{
		GKitMagHomes.Add(Mag, { Mag->GetAttachParent(), Mag->GetAttachSocketName(), Mag->GetRelativeTransform(), Mag->GetComponentScale() });
	}
	const FVector OrigWorldScale = GKitMagHomes[Mag].WorldScale;
	if (bUseHandRelativeOffset)
	{
		Mag->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandBone);
		FTransform Offset = HandRelativeOffset;
		Offset.SetScale3D(FVector::OneVector);
		Mag->SetRelativeLocationAndRotation(Offset.GetLocation(), Offset.GetRotation());
	}
	else
	{
		Mag->AttachToComponent(MeshComp, FAttachmentTransformRules::KeepWorldTransform, HandBone);
	}
	Mag->SetWorldScale3D(OrigWorldScale);
}

void UAnimNotifyState_KitMagInHand::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	for (auto It = GKitMagHomes.CreateIterator(); It; ++It)
	{
		USceneComponent* Mag = It.Key().Get();
		if (!Mag) { It.RemoveCurrent(); continue; }
		if (Mag->GetAttachParent() != MeshComp) continue;
		USceneComponent* Parent = It.Value().Parent.Get();
		if (IsValid(Parent))
		{
			Mag->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform, It.Value().Socket);
			Mag->SetRelativeTransform(It.Value().Relative);
		}
		It.RemoveCurrent();
	}
}
