#include "Animation/PSRArmsAnimInstance.h"
#include "UObject/ConstructorHelpers.h"

UPSRArmsAnimInstance::UPSRArmsAnimInstance()
{
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Idle(TEXT("/Game/PrecisionSniperRifle/Character/Animations/Anim_IdleLoop.Anim_IdleLoop"));
	if (Idle.Succeeded()) IdleAnim = Idle.Object;
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
