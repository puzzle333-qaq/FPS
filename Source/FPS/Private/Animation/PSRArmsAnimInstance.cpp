#include "Animation/PSRArmsAnimInstance.h"
#include "Misc/PackageName.h"

UPSRArmsAnimInstance::UPSRArmsAnimInstance()
{
	// The optional source pack is absent in this checkout. Keep authored overrides,
	// and register the legacy default only when its package is actually installed.
	const FString Package = TEXT("/Game/PrecisionSniperRifle/Character/Animations/Anim_IdleLoop");
	if (FPackageName::DoesPackageExist(Package))
		IdleAnimSoft = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(Package + TEXT(".Anim_IdleLoop")));
}

void FPSRArmsAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	const UPSRArmsAnimInstance* Inst = Cast<UPSRArmsAnimInstance>(InAnimInstance);
	UAnimSequenceBase* Seq = nullptr;
	if (Inst)
	{
		if (!Inst->IdleAnimSoft.IsNull())
		{
			Seq = Inst->IdleAnimSoft.LoadSynchronous();
		}
		if (!Seq)
		{
			Seq = Inst->IdleAnim.Get();
		}
	}
	IdlePlayer.SetSequence(Seq);
	IdlePlayer.SetLoopAnimation(true);
	Slot.SlotName = Inst ? Inst->SlotName : FName(TEXT("DefaultSlot"));
	Slot.Source.SetLinkNode(&IdlePlayer);
	FAnimInstanceProxy::Initialize(InAnimInstance); // picks up GetCustomRootNode()
}
