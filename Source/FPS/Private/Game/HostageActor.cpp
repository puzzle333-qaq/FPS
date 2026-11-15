#include "Game/HostageActor.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Game/ShooterPlayerState.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Net/UnrealNetwork.h"
#include "Game/CoopCqbGameMode.h"
#include "Game/CoopCqbGameState.h"
#include "Game/MissionRadioComponent.h"
#include "Game/CompliancePoseComponent.h"
#include "Game/HostageHidePoint.h"
#include "Game/RestraintActionComponent.h"
#include "Stats/StatsComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AHostageActor::AHostageActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
 SetReplicateMovement(true);
 CompliancePose=CreateDefaultSubobject<UCompliancePoseComponent>(TEXT("CompliancePose"));

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SetRootComponent(Capsule);
	Capsule->InitCapsuleSize(34.f, 88.f);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
 Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Capsule->SetCanEverAffectNavigation(false);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Capsule);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeMesh.Object);
		BodyMesh->SetRelativeScale3D(FVector(0.45f, 0.45f, 1.6f));
		BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	}

	CivilianMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CivilianMesh"));
 CivilianMesh->SetupAttachment(Capsule);
 CivilianMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 CivilianMesh->SetRelativeLocation(FVector(0, 0, -88));
 CivilianMesh->SetRelativeRotation(FRotator(0, -90, 0));
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
 if (Manny.Succeeded()) { CivilianMesh->SetSkeletalMesh(Manny.Object); BodyMesh->SetVisibility(false); }
 RescuedCrouchAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/ThirdPerson/Animations/Locomotion/Pistol/MM_Pistol_Crouch_Idle.MM_Pistol_Crouch_Idle")));
 Tags.AddUnique(FName(TEXT("Hostage")));
	Health = MaxHealth;
}

void AHostageActor::BeginPlay()
{
	Super::BeginPlay();
	Tags.AddUnique(FName(TEXT("Hostage")));
	if (HasAuthority()) { Health = MaxHealth; bAlive = true; }
 CompliancePose->SetMesh(CivilianMesh);
 if(HasAuthority())
 {
  CompliancePose->SetPose(bRescued?ECompliancePose::Bound:ECompliancePose::Calm);
  GetWorldTimerManager().SetTimer(BehaviorTimer,this,&ThisClass::UpdateBehavior,.1f,true);
 }
 OnRep_RescueState();
}

bool AHostageActor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	const auto* GS = GetWorld()->GetGameState<ACoopCqbGameState>();
 return bAlive && !bRescued && Behavior==EHostageBehavior::Compliant && IsBehindAndClose(InteractingPawn) && GS && GS->IsMissionLive() && !GS->bRangeHub;
}

FText AHostageActor::GetInteractPrompt_Implementation(APawn* InteractingPawn) const
{
 if(!bAlive||bRescued)return FText::GetEmpty();
 if(Behavior==EHostageBehavior::Binding)return BindingOfficer.Get()==InteractingPawn?NSLOCTEXT("Hostage","BindingCancel","正在捆绑人质（F 取消）"):NSLOCTEXT("Hostage","Binding","正在捆绑人质");
 if(CanInteract_Implementation(InteractingPawn))return NSLOCTEXT("Hostage","Bind","按 F 捆绑并保护人质");
 return FText::GetEmpty();
}
void AHostageActor::Interact_Implementation(APawn* InteractingPawn)
{
 if (!HasAuthority() || !CanInteract_Implementation(InteractingPawn)) return;
 float Duration=BindDuration;
 if(auto* Action=InteractingPawn->FindComponentByClass<URestraintActionComponent>())
 {if(!Action->BeginRestraint(this,Duration))return;Duration=Action->GetDuration();}
 BindingOfficer=InteractingPawn;StateDeadline=GetWorld()->GetTimeSeconds()+Duration;SetBehavior(EHostageBehavior::Binding);
 CompliancePose->SetPose(ECompliancePose::Binding,CompliancePose->GetBindingDuration()/Duration);
}
void AHostageActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(AHostageActor, bRescued);
 DOREPLIFETIME(AHostageActor, bAlive);
 DOREPLIFETIME(AHostageActor, Health);
 DOREPLIFETIME(AHostageActor, Behavior);
}
void AHostageActor::OnRep_RescueState()
{
 SetActorHiddenInGame(!bAlive);
 if (!bAlive) Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 if (bRescued && bAlive)
 {
  // Only change pose. Never move the actor or attach it to the rescuing operator.
  CompliancePose->SetPose(ECompliancePose::Bound);
 }
}

bool AHostageActor::DoDamage_Implementation(float DamageAmout, AActor* DamageInstigator)
{
	if (!HasAuthority() || !bAlive || DamageAmout <= 0.f) return false;
	if (const auto* GS = GetWorld()->GetGameState<ACoopCqbGameState>())
 {
  if (!GS->IsMissionLive() || GS->bRangeHub) return false;
 }
 uint8 Team = 255;
	if (const APawn* P = Cast<APawn>(DamageInstigator))
	{
		if (const AShooterPlayerState* PS = P->GetPlayerState<AShooterPlayerState>())
		{
			Team = PS->GetTeamId();
		}
	}
	else if (const AController* C = Cast<AController>(DamageInstigator))
	{
		if (const AShooterPlayerState* PS = C->GetPlayerState<AShooterPlayerState>())
		{
			Team = PS->GetTeamId();
		}
	}
	LastDamageTeam = Team;
	Health = FMath::Max(0.f, Health - DamageAmout);
 Auth_Frighten(IsValid(DamageInstigator)?DamageInstigator->GetActorLocation():GetActorLocation());
	UE_LOG(LogTemp, Log, TEXT("[Hostage] %s dmg=%.1f hp=%.1f team=%d"), *GetName(), DamageAmout, Health, Team);
	if (Health <= 0.f)
	{
		Die(DamageInstigator);
	}
	return true;
}

void AHostageActor::Die(AActor* Killer)
{
	if (!bAlive) return;
	bAlive = false;
 if(BindingOfficer.IsValid())if(auto* Action=BindingOfficer->FindComponentByClass<URestraintActionComponent>())Action->EndRestraint();
 CompliancePose->SetPose(ECompliancePose::None);
	bFollowing = false;
	FollowTarget = nullptr;
	if (Capsule)
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetActorHiddenInGame(true);
 ForceNetUpdate();
 if (auto* GM = GetWorld()->GetAuthGameMode<ACoopCqbGameMode>()) GM->Auth_RefreshObjectives();
	UE_LOG(LogTemp, Warning, TEXT("[Hostage] %s killed by %s (team=%d)"), *GetName(), *GetNameSafe(Killer), LastDamageTeam);
}

void AHostageActor::EndPlay(const EEndPlayReason::Type Reason)
{
 if(BindingOfficer.IsValid())if(auto* Action=BindingOfficer->FindComponentByClass<URestraintActionComponent>())Action->EndRestraint();
 if(GetWorld())GetWorldTimerManager().ClearTimer(BehaviorTimer);
 if(ActiveHidePoint.IsValid()&&ActiveHidePoint->Occupant==this)ActiveHidePoint->Occupant.Reset();
 Super::EndPlay(Reason);
}
bool AHostageActor::IsBehindAndClose(const APawn* Pawn)const
{
 if(!IsValid(Pawn)||FVector::DistSquared2D(Pawn->GetActorLocation(),GetActorLocation())>FMath::Square(155.f)||FMath::Abs(Pawn->GetActorLocation().Z-GetActorLocation().Z)>120.f)return false;
 if(const auto* Stats=UStatsComponent::FindStatsComponent(Pawn);Stats&&Stats->DeathState!=EDeathState::NotDead)return false;
 if(FVector::DotProduct(GetActorForwardVector(),(Pawn->GetActorLocation()-GetActorLocation()).GetSafeNormal2D())>-.35f)return false;
 FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(HostageBind),false,this);Q.AddIgnoredActor(Pawn);
 return !GetWorld()->LineTraceSingleByChannel(Hit,Pawn->GetActorLocation(),GetActorLocation(),ECC_Visibility,Q);
}
void AHostageActor::SetLowProfile(bool bLow)
{
 if(bLowProfile==bLow)return;
 const float Floor=GetActorLocation().Z-Capsule->GetUnscaledCapsuleHalfHeight();
 bLowProfile=bLow;const float Half=bLow?28.f:88.f;
 Capsule->SetCapsuleSize(bLow?20.f:34.f,Half);
 CivilianMesh->SetRelativeLocation(FVector(0,0,-Half));
 if(HasAuthority())SetActorLocation(FVector(GetActorLocation().X,GetActorLocation().Y,Floor+Half));
}
void AHostageActor::OnRep_Behavior()
{
 SetLowProfile(Behavior==EHostageBehavior::EnteringCover||Behavior==EHostageBehavior::Hiding||Behavior==EHostageBehavior::Emerging);
}
void AHostageActor::SetBehavior(EHostageBehavior Next)
{
 Behavior=Next;OnRep_Behavior();ForceNetUpdate();
 switch(Next){
 case EHostageBehavior::Calm:CompliancePose->SetPose(ECompliancePose::Calm);break;
 case EHostageBehavior::Scared:CompliancePose->SetPose(ECompliancePose::Scared);break;
 case EHostageBehavior::SeekingCover:CompliancePose->SetPose(ECompliancePose::Flee);break;
 case EHostageBehavior::EnteringCover:case EHostageBehavior::Hiding:
  CompliancePose->SetPose(ActiveHidePoint.IsValid()&&ActiveHidePoint->bUnderTable?ECompliancePose::TableHide:ECompliancePose::ProneHide);break;
 case EHostageBehavior::Emerging:CompliancePose->SetPose(ECompliancePose::Crawl);break;
 case EHostageBehavior::Complying:CompliancePose->SetPose(ECompliancePose::HandsUp);break;
 case EHostageBehavior::Secured:CompliancePose->SetPose(ECompliancePose::Bound);break;
 default:break;}
}
void AHostageActor::Auth_Frighten(const FVector& Threat)
{
 if(!HasAuthority()||!bAlive||bRescued||Behavior!=EHostageBehavior::Calm)return;
 LastThreat=Threat;StateDeadline=GetWorld()->GetTimeSeconds()+WarningGraceSeconds;SetBehavior(EHostageBehavior::Scared);
}
void AHostageActor::Auth_ReceiveWarning(APawn* Speaker)
{
 if(!HasAuthority()||!bAlive||bRescued||!IsValid(Speaker))return;
 if(Behavior==EHostageBehavior::Complying||Behavior==EHostageBehavior::Compliant||Behavior==EHostageBehavior::Binding||Behavior==EHostageBehavior::Emerging)return;
 WarningSpeaker=Speaker;EscapePath.Reset();
 if(bLowProfile&&ActiveHidePoint.IsValid())
 {
  EscapePath.Add(ActiveHidePoint->Approach->GetComponentLocation());PathIndex=0;PathBlockedSeconds=0;SetBehavior(EHostageBehavior::Emerging);
 }
 else StartCompliance();
}
void AHostageActor::StartCompliance()
{
 SetLowProfile(false);
 if(ActiveHidePoint.IsValid()&&ActiveHidePoint->Occupant==this)ActiveHidePoint->Occupant.Reset();
 ActiveHidePoint.Reset();EscapePath.Reset();
 if(WarningSpeaker.IsValid())SetActorRotation(FRotator(0,(WarningSpeaker->GetActorLocation()-GetActorLocation()).Rotation().Yaw,0));
 StateDeadline=GetWorld()->GetTimeSeconds()+CompliancePose->GetHandsUpDuration();SetBehavior(EHostageBehavior::Complying);
}
bool AHostageActor::FindCoverPath()
{
 TArray<AHostageHidePoint*> Points;
 for(TActorIterator<AHostageHidePoint> It(GetWorld());It;++It)if((!It->Occupant.IsValid()||It->Occupant==this)&&FVector::DistSquared2D(It->GetActorLocation(),GetActorLocation())<FMath::Square(It->SearchRadius))Points.Add(*It);
 Points.Sort([this](const AHostageHidePoint& A,const AHostageHidePoint& B){return (&A==PreferredHidePoint)?true:(&B==PreferredHidePoint)?false:FVector::DistSquared2D(A.GetActorLocation(),GetActorLocation())<FVector::DistSquared2D(B.GetActorLocation(),GetActorLocation());});
 for(auto* Point:Points)
 {
  auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),GetActorLocation(),Point->Approach->GetComponentLocation());
  if(!Path||!Path->IsValid()||Path->IsPartial()||Path->PathPoints.Num()<2)continue;
  EscapePath=Path->PathPoints;PathIndex=1;PathBlockedSeconds=0;ActiveHidePoint=Point;Point->Occupant=this;return true;
 }
 return false;
}
bool AHostageActor::AdvancePath(float Speed)
{
 if(!EscapePath.IsValidIndex(PathIndex))return true;
 FVector Delta=EscapePath[PathIndex]-GetActorLocation();Delta.Z=0;
 if(Delta.Size2D()<12.f){++PathIndex;return !EscapePath.IsValidIndex(PathIndex);}
 const FVector Old=GetActorLocation();FVector Next=Old+Delta.GetClampedToMaxSize(Speed*.1f);
 FHitResult Ground;FCollisionQueryParams Q(SCENE_QUERY_STAT(HostageGround),false,this);
 if(GetWorld()->LineTraceSingleByChannel(Ground,Next+FVector(0,0,30),Next-FVector(0,0,200),ECC_WorldStatic,Q))Next.Z=Ground.ImpactPoint.Z+Capsule->GetUnscaledCapsuleHalfHeight()+1.f;
 FHitResult Hit;SetActorLocation(Next,true,&Hit);SetActorRotation(FRotator(0,Delta.Rotation().Yaw,0));
 PathBlockedSeconds=(GetActorLocation()-Old).Size2D()<1.f?PathBlockedSeconds+.1f:0.f;
 if(PathBlockedSeconds>2.f)
 {
  EscapePath.Reset();
  if(bLowProfile){SetBehavior(EHostageBehavior::Hiding);return false;}
  SetBehavior(EHostageBehavior::Scared);StateDeadline=GetWorld()->GetTimeSeconds()+3.f;
 }
 return false;
}
void AHostageActor::UpdateBehavior()
{
 if(!HasAuthority())return;
 const auto* GS=GetWorld()->GetGameState<ACoopCqbGameState>();
 if(!bAlive||bRescued||!GS||!GS->IsMissionLive()||GS->bRangeHub)
 {
  if(BindingOfficer.IsValid())if(auto* Action=BindingOfficer->FindComponentByClass<URestraintActionComponent>())Action->EndRestraint();
  BindingOfficer.Reset();return;
 }
 const float Now=GetWorld()->GetTimeSeconds();
 switch(Behavior){
 case EHostageBehavior::Scared:
  if(Now>=StateDeadline){if(FindCoverPath())SetBehavior(EHostageBehavior::SeekingCover);else StateDeadline=Now+3.f;}break;
 case EHostageBehavior::SeekingCover:
  if(AdvancePath(200.f)&&ActiveHidePoint.IsValid())
  {EscapePath={ActiveHidePoint->GetActorLocation()};PathIndex=0;PathBlockedSeconds=0;StateDeadline=Now+3.5f;SetActorRotation(ActiveHidePoint->GetActorRotation());SetBehavior(EHostageBehavior::EnteringCover);}break;
 case EHostageBehavior::EnteringCover:
  if(Now>=StateDeadline&&AdvancePath(45.f)){if(ActiveHidePoint.IsValid())SetActorRotation(ActiveHidePoint->GetActorRotation());SetBehavior(EHostageBehavior::Hiding);}break;
 case EHostageBehavior::Emerging:
  if(AdvancePath(65.f))
  {
   FCollisionQueryParams Q(SCENE_QUERY_STAT(HostageStand),false,this);const FVector Stand=GetActorLocation()+FVector(0,0,60);
   if(!GetWorld()->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,87),Q))StartCompliance();
  }break;
 case EHostageBehavior::Complying:if(Now>=StateDeadline)SetBehavior(EHostageBehavior::Compliant);break;
 case EHostageBehavior::Binding:
  if(!BindingOfficer.IsValid()||!IsBehindAndClose(BindingOfficer.Get())||(BindingOfficer->FindComponentByClass<URestraintActionComponent>()&&!BindingOfficer->FindComponentByClass<URestraintActionComponent>()->IsActive()))
  {
   if(BindingOfficer.IsValid())if(auto* Action=BindingOfficer->FindComponentByClass<URestraintActionComponent>())Action->EndRestraint();
   BindingOfficer.Reset();SetBehavior(EHostageBehavior::Compliant);CompliancePose->SetPose(ECompliancePose::Kneel);break;
  }
  if(Now>=StateDeadline)
  {
   if(auto* Action=BindingOfficer->FindComponentByClass<URestraintActionComponent>())Action->EndRestraint();
   bRescued=true;bFollowing=false;FollowTarget.Reset();SetBehavior(EHostageBehavior::Secured);OnRep_RescueState();
   if(auto* Flow=GetWorld()->GetGameState<ACoopCqbGameState>();Flow&&Flow->MissionRadio)Flow->MissionRadio->QueueCue(EMissionVoiceCue::CivilianSecured);
   if(auto* GM=GetWorld()->GetAuthGameMode<ACoopCqbGameMode>())GM->Auth_RefreshObjectives();
  }break;
 default:break;}
}
