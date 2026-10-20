#include "AnimNotify/AnimNotify_ThrowProjectile.h"

#include "Interface/PlayerInterface.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_ThrowProjectile::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp)) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UPlayerInterface>()) return;

	IPlayerInterface::Execute_Notify_ThrowWeapon(Owner);
}

FString UAnimNotify_ThrowProjectile::GetNotifyName_Implementation() const
{
	return FString(TEXT("Throw Projectile"));
}
