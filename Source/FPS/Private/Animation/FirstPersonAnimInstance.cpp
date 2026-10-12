#include "Animation/FirstPersonAnimInstance.h"

#include "ShooterCharacter.h"

void UFirstPersonAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const AShooterCharacter* Shooter = Cast<AShooterCharacter>(TryGetPawnOwner());
	if (!IsValid(Shooter))
	{
		FABRIK_Alpha1P = 0.f;
		return;
	}

	FABRIK_Socket1P = Shooter->FABRIK_SocketTransform1P;
	FABRIK_Alpha1P = Shooter->FABRIK_Alpha1P;
}
