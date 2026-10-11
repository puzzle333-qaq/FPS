#include "AnimNotify/AnimNotify_WeaponGrip.h"

#include "Weapon/Weapon.h"
#include "Interface/PlayerInterface.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_WeaponGrip::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp) || GripSocket.IsNone())
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UPlayerInterface>())
	{
		return;
	}

	AWeapon* Weapon = IPlayerInterface::Execute_GetCurrentWeapon(Owner);
	if (!IsValid(Weapon))
	{
		return;
	}

	Weapon->AttachToPawnSocket(MeshComp, GripSocket);
}

FString UAnimNotify_WeaponGrip::GetNotifyName_Implementation() const
{
	return GripSocket.IsNone() ? FString(TEXT("Weapon Grip")) : FString::Printf(TEXT("Grip %s"), *GripSocket.ToString());
}
