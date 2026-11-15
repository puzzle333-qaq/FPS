#include "Game/HostileAICharacter.h"
#include "Combat/HitZoneVitalsComponent.h"
#include "World/BloodPool.h"
#include "ShooterCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"

#include "Game/CoopCqbGameMode.h"
#include "Game/CoopCqbGameState.h"
#include "Game/MissionRadioComponent.h"
#include "Game/HostileAIController.h"
#include "Stats/StatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Weapon/Weapon.h"
#include "Game/CompliancePoseComponent.h"
#include "Game/DroppedHostileWeapon.h"
#include "Game/RestraintActionComponent.h"

FName AHostileAICharacter::HostileTag(TEXT("FPS_Hostile"));

AHostileAICharacter::AHostileAICharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
 CompliancePose=CreateDefaultSubobject<UCompliancePoseComponent>(TEXT("CompliancePose"));
	SetReplicateMovement(true);
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AHostileAIController::StaticClass();

	Tags.AddUnique(HostileTag);

	Stats = CreateDefaultSubobject<UStatsComponent>(TEXT("Stats"));
	Vitals = CreateDefaultSubobject<UHitZoneVitalsComponent>(TEXT("HitZoneVitals"));
	Vitals->SetIsReplicated(true);
	Vitals->bOutOfCombatTorsoRecovery = false;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
 GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 480.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 320.f;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 160.f;
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(60.f);
	bUseControllerRotationYaw = false;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshFinder(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (MeshFinder.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MeshFinder.Object);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}
}

void AHostileAICharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHostileAICharacter, bDead);
	DOREPLIFETIME(AHostileAICharacter, Morale);
	DOREPLIFETIME(AHostileAICharacter, bArrested);
 DOREPLIFETIME(AHostileAICharacter,bBeingRestrained);
	DOREPLIFETIME(AHostileAICharacter, bArmInjured);
	DOREPLIFETIME(AHostileAICharacter, bLegInjured);
	DOREPLIFETIME(AHostileAICharacter, BodyArmor);
	DOREPLIFETIME(AHostileAICharacter, BodyMesh);
	DOREPLIFETIME(AHostileAICharacter, CarriedWeapon);
}

void AHostileAICharacter::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && Stats)
	{
		Stats->MaxHealth = MaxHealth;
		Stats->Health = MaxHealth;
		Stats->DeathState = EDeathState::NotDead;
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		BaseWalkSpeed = Move->MaxWalkSpeed;
		BaseWalkSpeedCrouched = Move->MaxWalkSpeedCrouched;
	}
	RefreshInjuryMovement();
	if (HasAuthority())
	{
		ChooseDefaultLookAndWeapon();
		SpawnCarriedWeapon();
	}
	ApplyBodyMesh();
	AttachCarriedWeapon();
 CompliancePose->SetMesh(GetMesh());
 OnRep_Morale();
 RefreshDeploymentVisibility();
}

USkeletalMeshComponent* AHostileAICharacter::GetMesh3P_Implementation() const
{
	return GetMesh();
}

void AHostileAICharacter::OnRep_BodyMesh()
{
	ApplyBodyMesh();
	AttachCarriedWeapon();
 CompliancePose->SetMesh(GetMesh());
}

void AHostileAICharacter::OnRep_CarriedWeapon()
{
	AttachCarriedWeapon();
}

void AHostileAICharacter::ChooseDefaultLookAndWeapon()
{
	const FString Existing = BodyMesh.ToSoftObjectPath().ToString();
	const bool bNeedsLook = BodyMesh.IsNull() || Existing.Contains(TEXT("Manny")) || Existing.Contains(TEXT("Mannequin"));
	if (bNeedsLook)
	{
		TArray<FSoftObjectPath> Presets;
		Presets.Reserve(24);
		for (int32 Index = 1; Index <= 24; ++Index)
		{
			const FString Name = Index == 3
				? TEXT("SKM_Bandit_Preset_3")
				: FString::Printf(TEXT("SKM_Bandit_Preset_%02d"), Index);
			Presets.Add(FSoftObjectPath(FString::Printf(TEXT("/Game/Bandits/Mesh/Preset/%s.%s"), *Name, *Name)));
		}
		BodyMesh = TSoftObjectPtr<USkeletalMesh>(Presets[FMath::RandRange(0, Presets.Num() - 1)]);
	}

	if (WeaponClass) return;
	static const TCHAR* Guns[] = {
		TEXT("/Game/FPS/Weapon/BP_Weapon_AK.BP_Weapon_AK_C"),
		TEXT("/Game/FPS/Weapon/RAR/BP_FPS_RAR.BP_FPS_RAR_C"),
		TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_AK105.BP_Weapon_AK105_C"),
		TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_TR15.BP_Weapon_TR15_C"),
		TEXT("/Game/FPS/Weapon/Kinemation/BP_Weapon_Viper.BP_Weapon_Viper_C"),
		TEXT("/Game/FPS/Weapon/BP_Weapon_X24.BP_Weapon_X24_C"),
	};
	TArray<UClass*> Loaded;
	for (const TCHAR* Path : Guns)
	{
		if (UClass* GunClass = LoadClass<AWeapon>(nullptr, Path))
		{
			Loaded.Add(GunClass);
		}
	}
	if (Loaded.Num() > 0)
	{
		WeaponClass = Loaded[FMath::RandRange(0, Loaded.Num() - 1)];
	}
}

void AHostileAICharacter::ApplyBodyMesh()
{
	if (BodyMesh.IsNull() || !GetMesh()) return;
	if (USkeletalMesh* Loaded = BodyMesh.LoadSynchronous())
	{
		GetMesh()->SetSkeletalMesh(Loaded);
	}
}

void AHostileAICharacter::SpawnCarriedWeapon()
{
	if (!HasAuthority() || IsValid(CarriedWeapon) || !WeaponClass) return;
	UWorld* World = GetWorld();
	if (!World) return;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	CarriedWeapon = World->SpawnActor<AWeapon>(WeaponClass, GetActorTransform(), Params);
	AttachCarriedWeapon();
}

void AHostileAICharacter::AttachCarriedWeapon()
{
	if (!IsValid(CarriedWeapon) || !GetMesh()) return;
	static const FName Grips[] = {
		TEXT("RifleSocketak"), TEXT("RifleSocket"), TEXT("hand_r"), TEXT("weapon_r")
	};
	FName Grip = TEXT("hand_r");
	for (const FName& Name : Grips)
	{
		if (GetMesh()->DoesSocketExist(Name) || GetMesh()->GetBoneIndex(Name) != INDEX_NONE)
		{
			Grip = Name;
			break;
		}
	}
	CarriedWeapon->SetInstigator(this);
	CarriedWeapon->AttachToPawnSocket(GetMesh(), Grip);
	if (USkeletalMeshComponent* Gun3P = CarriedWeapon->GetMesh3P())
	{
		Gun3P->SetHiddenInGame(false);
	}
	if (USkeletalMeshComponent* Gun1P = CarriedWeapon->GetMesh1P())
	{
		Gun1P->SetHiddenInGame(true);
	}
}

void AHostileAICharacter::SetMorale(EHostileMorale NewMorale)
{
	if (!HasAuthority()) return;
	if (bArrested) return;
 if(!IsHostileAlive()||Morale==NewMorale||Morale==EHostileMorale::Surrender)return;
 Morale = NewMorale;
 if (Morale == EHostileMorale::Surrender)
 {
  DropCarriedWeapon();
  CompliancePose->SetPose(ECompliancePose::HandsUp);
  GetCharacterMovement()->StopMovementImmediately();
 }
 ForceNetUpdate();
}

void AHostileAICharacter::Auth_ReceiveWarning(APawn* Speaker)
{
 if(HasAuthority()&&IsValid(Speaker)&&IsInFlashSurrenderWindow())SetMorale(EHostileMorale::Surrender);
}
void AHostileAICharacter::DropCarriedWeapon()
{
 if(!HasAuthority()||!CarriedWeapon)return;
 auto* GunVisual=CarriedWeapon->GetMesh3P();
 if(GunVisual&&GunVisual->GetSkeletalMeshAsset())
 {
  FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  if(auto* Drop=GetWorld()->SpawnActor<ADroppedHostileWeapon>(GunVisual->GetComponentLocation()+GetActorForwardVector()*35.f,GunVisual->GetComponentRotation(),P))Drop->Initialize(GunVisual,GetActorForwardVector());
 }
 CarriedWeapon->Destroy();CarriedWeapon=nullptr;ForceNetUpdate();
}
void AHostileAICharacter::OnRep_Morale()
{
 if(Morale!=EHostileMorale::Surrender)return;
 GetCharacterMovement()->StopMovementImmediately();
 if(auto* Brain=Cast<AHostileAIController>(GetController()))Brain->StopMovement();
}

void AHostileAICharacter::Multicast_HostileFired_Implementation(FVector Muzzle, FVector Impact)
{
	OnHostileFired(Muzzle, Impact);
}

bool AHostileAICharacter::IsHostileAlive() const
{
	if (bDead) return false;
	if (Stats && Stats->DeathState != EDeathState::NotDead) return false;
	return true;
}

bool AHostileAICharacter::DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator)
{
	if (!HasAuthority() || !IsHostileAlive() || DamageAmout <= 0.f) return false;
	if (!Stats) return false;
 if (const auto* GS = GetWorld()->GetGameState<ACoopCqbGameState>())
 {
  if (!GS->IsMissionLive() || GS->bRangeHub) return false;
 }

	if (AHostileAIController* Brain = Cast<AHostileAIController>(GetController()))
	{
		const FVector From = IsValid(DamageInstigator) ? DamageInstigator->GetActorLocation() : GetActorLocation();
		Brain->NotifyTookDamage(DamageInstigator, From);
	}
	if (Vitals)
	{
		return Vitals->ApplyUnspecifiedHit(DamageInstigator, 1.f);
	}
	Auth_Die(DamageInstigator);
	return true;
}

float AHostileAICharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Actual = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	AActor* Inst = DamageCauser ? DamageCauser : (EventInstigator ? EventInstigator->GetPawn() : nullptr);
	DoDamage_Implementation(Actual > 0.f ? Actual : DamageAmount, Inst);
	return Actual;
}

void AHostileAICharacter::Auth_Die(AActor* Killer)
{
	if (!HasAuthority() || bDead) return;
	bDead = true;
 CompliancePose->SetPose(ECompliancePose::None);
	if (Stats)
	{
		Stats->DeathState = EDeathState::DeadFinished;
		Stats->Health = 0.f;
	}
	ApplyDeadVisual();
	ForceNetUpdate();

	if (AController* C = GetController())
	{
		C->UnPossess();
	}

	if (ACoopCqbGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ACoopCqbGameMode>() : nullptr)
	{
		GM->NotifyHostileKilled(this);
	}
	UE_LOG(LogTemp, Log, TEXT("[Hostile] %s died (killer=%s)"), *GetNameSafe(this), *GetNameSafe(Killer));
}

void AHostileAICharacter::OnRep_Dead()
{
	ApplyDeadVisual();
}

void AHostileAICharacter::ApplyDeadVisual()
{
	if (!bDead) return;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
		Move->SetComponentTickEnabled(false);
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	USkeletalMeshComponent* Skel = GetMesh();
	if (!Skel) return;

	if (!Skel->GetPhysicsAsset())
	{
		if (UPhysicsAsset* Asset = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin")))
		{
			Skel->SetPhysicsAsset(Asset);
		}
	}

	Skel->SetCollisionProfileName(TEXT("Ragdoll"));
	Skel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Skel->SetAllBodiesSimulatePhysics(true);
	Skel->SetSimulatePhysics(true);
	Skel->WakeAllRigidBodies();
	Skel->bBlendPhysics = true;
	Skel->bPauseAnims = true;
}


bool AHostileAICharacter::CanInteract_Implementation(APawn* Pawn) const
{
 const auto* GS = GetWorld()->GetGameState<ACoopCqbGameState>();
 return CanOfficerRestrain(Pawn) && IsHostileAlive() && !bArrested && !bBeingRestrained && Morale == EHostileMorale::Surrender
  && CompliancePose->IsHandsUpSettled() && GS && GS->IsMissionLive() && !GS->bRangeHub;
}
FText AHostileAICharacter::GetInteractPrompt_Implementation(APawn* Pawn) const
{
 if(bArrested||!IsHostileAlive())return FText::GetEmpty();
 if(bBeingRestrained)return NSLOCTEXT("Mission","ArrestProgress","正在捆绑嫌疑人");
 if(Morale!=EHostileMorale::Surrender||!CompliancePose->IsHandsUpSettled())return FText::GetEmpty();
 if(!CanOfficerRestrain(Pawn))return FText::GetEmpty();
 return NSLOCTEXT("Mission", "Arrest", "按 F 拘捕嫌疑人");
}
void AHostileAICharacter::Interact_Implementation(APawn* Pawn)
{
 if (!HasAuthority() || !CanInteract_Implementation(Pawn)) return;
 float Duration=3.6f;
 if(auto* Action=Pawn->FindComponentByClass<URestraintActionComponent>()){if(!Action->BeginRestraint(this,Duration))return;Duration=Action->GetDuration();}
 RestrainingOfficer=Pawn;bBeingRestrained=true;RestraintCompleteAt=GetWorld()->GetTimeSeconds()+Duration;CompliancePose->SetPose(ECompliancePose::Binding,CompliancePose->GetBindingDuration()/Duration);ForceNetUpdate();
}
bool AHostileAICharacter::CanOfficerRestrain(const APawn* Pawn)const
{
 if(!IsValid(Pawn)||FVector::DistSquared2D(Pawn->GetActorLocation(),GetActorLocation())>FMath::Square(155.f)||FMath::Abs(Pawn->GetActorLocation().Z-GetActorLocation().Z)>120.f)return false;
 if(FVector::DotProduct(GetActorForwardVector(),(Pawn->GetActorLocation()-GetActorLocation()).GetSafeNormal2D())>-.35f)return false;
 if(const auto* S=UStatsComponent::FindStatsComponent(Pawn);S&&S->DeathState!=EDeathState::NotDead)return false;
 FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(SuspectBind),false,this);Q.AddIgnoredActor(Pawn);
 return !GetWorld()->LineTraceSingleByChannel(Hit,Pawn->GetActorLocation(),GetActorLocation(),ECC_Visibility,Q);
}
void AHostileAICharacter::UpdateRestraint()
{
 if(!HasAuthority()||!bBeingRestrained)return;
 auto* Officer=RestrainingOfficer.Get();auto* Action=Officer?Officer->FindComponentByClass<URestraintActionComponent>():nullptr;
 const auto* GS=GetWorld()->GetGameState<ACoopCqbGameState>();
 if(!IsHostileAlive()||!GS||!GS->IsMissionLive()||!CanOfficerRestrain(Officer)||(Action&&!Action->IsActive()))
 {
  if(Action)Action->EndRestraint();RestrainingOfficer.Reset();bBeingRestrained=false;CompliancePose->SetPose(IsHostileAlive()?ECompliancePose::Kneel:ECompliancePose::None);ForceNetUpdate();return;
 }
 if(GetWorld()->GetTimeSeconds()<RestraintCompleteAt)return;
 if(Action)Action->EndRestraint();RestrainingOfficer.Reset();bBeingRestrained=false;
 bArrested = true;
 DropCarriedWeapon();CompliancePose->SetPose(ECompliancePose::Bound);
 if(GS&&GS->MissionRadio)GS->MissionRadio->QueueCue(EMissionVoiceCue::TargetSecured);
 Crouch();
 OnRep_Arrested();
 ForceNetUpdate();
 if (auto* GM = GetWorld()->GetAuthGameMode<ACoopCqbGameMode>()) GM->Auth_RefreshObjectives();
}
void AHostileAICharacter::OnRep_Arrested()
{
 if (!bArrested) return;
 GetCharacterMovement()->StopMovementImmediately();
 if (auto* Brain = Cast<AHostileAIController>(GetController()))
 {
  Brain->StopMovement();
  Brain->SetActorTickEnabled(false);
 }
 // Keep the pawn and its crouching animation in place after restraint.
}


void AHostileAICharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
 UpdateRestraint();
 RefreshDeploymentVisibility();
	if (GetNetMode() == NM_DedicatedServer) return;
	if (bDead || BloodyFootstepsRemaining <= 0) return;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const float Speed2D = Move ? Move->Velocity.Size2D() : 0.f;
	if (Speed2D < 20.f) return;
	FootstepDistanceAccum += Speed2D * DeltaSeconds;
	if (FootstepDistanceAccum < 70.f) return;
	FootstepDistanceAccum = 0.f;
	bFootstepLeft = !bFootstepLeft;
	SpawnBloodFootprint();
	--BloodyFootstepsRemaining;
}

void AHostileAICharacter::Auth_ApplyHitZoneInjury(EHitZone Zone)
{
	if (!HasAuthority() || !IsHostileAlive()) return;
	bool bChanged = false;
	if (Zone == EHitZone::Arm && !bArmInjured)
	{
		bArmInjured = true;
		bChanged = true;
		OnRep_ArmInjured();
	}
	else if (Zone == EHitZone::Leg && !bLegInjured)
	{
		bLegInjured = true;
		bChanged = true;
		OnRep_LegInjured();
	}
	if (bChanged)
	{
		RefreshInjuryMovement();
		ForceNetUpdate();
	}
}

void AHostileAICharacter::OnRep_ArmInjured()
{
}

void AHostileAICharacter::OnRep_LegInjured()
{
	RefreshInjuryMovement();
}

void AHostileAICharacter::RefreshInjuryMovement()
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		const float Mul = bLegInjured ? LegInjurySpeedMultiplier : 1.f;
		Move->MaxWalkSpeed = BaseWalkSpeed * Mul;
		Move->MaxWalkSpeedCrouched = BaseWalkSpeedCrouched * Mul;
	}
}

void AHostileAICharacter::MarkBloodyFeet(int32 ExtraSteps)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	const int32 Add = ExtraSteps > 0 ? ExtraSteps : BloodyFootstepsOnHit;
	BloodyFootstepsRemaining = FMath::Max(BloodyFootstepsRemaining, Add);
}

void AHostileAICharacter::SpawnBloodFootprint()
{
	UWorld* World = GetWorld();
	if (!World || GetNetMode() == NM_DedicatedServer) return;
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/FPS/FX/Blood/MI_BloodPuddle_Wet.MI_BloodPuddle_Wet"));
	if (!Mat)
	{
		Mat = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/FPS_Controller/Environment/Materials/MI_Oil_Puddle_Decal.MI_Oil_Puddle_Decal"));
	}
	if (!Mat) return;

	const UCapsuleComponent* Cap = GetCapsuleComponent();
	const float Half = IsValid(Cap) ? Cap->GetScaledCapsuleHalfHeight() : 90.f;
	const FVector Start = GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, Half + 60.f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AIBloodFootprint), false, this);
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		FCollisionObjectQueryParams Obj;
		Obj.AddObjectTypesToQuery(ECC_WorldStatic);
		Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
		if (!World->LineTraceSingleByObjectType(Hit, Start, End, Obj, Params)) return;
	}
	const float Side = bFootstepLeft ? -10.f : 10.f;
	const FVector Loc = Hit.ImpactPoint + Hit.ImpactNormal * 1.5f + GetActorRightVector() * Side;
	const FRotator DecalRot = FRotationMatrix::MakeFromZX(Hit.ImpactNormal, GetActorForwardVector()).Rotator();
	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
		World, Mat, FVector(18.f * 0.35f, 18.f, 18.f), Loc, DecalRot, 14.f);
	if (IsValid(Decal))
	{
		Decal->SetFadeScreenSize(0.001f);
		Decal->SetFadeOut(14.f * 0.55f, 14.f * 0.45f, false);
	}
}

void AHostileAICharacter::Auth_BeginFlashSurrenderWindow(float WindowMinSeconds, float WindowMaxSeconds)
{
	if (!HasAuthority()) return;
	const float MinS = FMath::Max(1.f, FMath::Min(WindowMinSeconds, WindowMaxSeconds));
	const float MaxS = FMath::Max(MinS, FMath::Max(WindowMinSeconds, WindowMaxSeconds));
	const float Duration = FMath::FRandRange(MinS, MaxS);
	const UWorld* World = GetWorld();
	if (!World) return;
	const float Now = World->GetTimeSeconds();
	FlashSurrenderWindowUntil = FMath::Max(FlashSurrenderWindowUntil, Now + Duration);
 SetMorale(EHostileMorale::Surrender);
}

bool AHostileAICharacter::IsInFlashSurrenderWindow() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < FlashSurrenderWindowUntil;
}

void AHostileAICharacter::ApplyStingerEffect(float Strength, float LockDuration, float RecoverDuration)
{
	Strength = FMath::Clamp(Strength, 0.f, 1.f);
	if (Strength < 0.08f) return;
	const UWorld* World = GetWorld();
	if (!World) return;
	const float Now = World->GetTimeSeconds();
	StingerStrength = Strength;
	const float Lock = LockDuration > 0.f ? LockDuration : StingerLockDuration;
	const float Recover = RecoverDuration >= 0.f ? RecoverDuration : StingerRecoverDuration;
	StingerLockUntil = Now + Lock;
	StingerRecoverUntil = StingerLockUntil + Recover;
}

float AHostileAICharacter::GetStingerLookScale() const
{
	const UWorld* World = GetWorld();
	if (!World) return 1.f;
	const float Now = World->GetTimeSeconds();
	if (Now < StingerLockUntil) return FMath::Clamp(1.f - StingerStrength, 0.f, 1.f);
	if (Now < StingerRecoverUntil)
	{
		const float Window = FMath::Max(0.01f, StingerRecoverUntil - StingerLockUntil);
		const float T = FMath::Clamp((Now - StingerLockUntil) / Window, 0.f, 1.f);
		const float Ease = T * T * (3.f - 2.f * T);
		return FMath::Lerp(1.f - StingerStrength, 1.f, Ease);
	}
	return 1.f;
}

float AHostileAICharacter::GetArmInjurySpreadScale() const
{
	return bArmInjured ? ArmInjurySpreadMultiplier : 1.f;
}

float AHostileAICharacter::GetArmInjuryTurnScale() const
{
	return bArmInjured ? ArmInjuryTurnMultiplier : 1.f;
}

void AHostileAICharacter::Multicast_FleshHit_Implementation(FVector ImpactPoint, FVector ImpactNormal)
{
	AShooterCharacter::SpawnBloodImpactFX(this, ImpactPoint, ImpactNormal, this);
}

void AHostileAICharacter::RefreshDeploymentVisibility()
{
 const auto* GS=GetWorld()->GetGameState<ACoopCqbGameState>();
 const bool Hide=GS&&!GS->bRangeHub&&GetWorld()->GetMapName().Contains(TEXT("Factory"))&&(GS->GetCoopPhase()==ECoopCqbPhase::Waiting||GS->GetCoopPhase()==ECoopCqbPhase::Briefing);
 if(Hide||bDeploymentHidden)
 {
  SetActorHiddenInGame(Hide);if(CarriedWeapon)CarriedWeapon->SetActorHiddenInGame(Hide);
  TArray<AActor*> Attached;GetAttachedActors(Attached,true,true);
  for(AActor* Actor:Attached)if(Cast<AWeapon>(Actor))Actor->SetActorHiddenInGame(Hide);
  bDeploymentHidden=Hide;
 }
}
