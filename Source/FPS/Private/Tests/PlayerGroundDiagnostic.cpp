#if WITH_EDITOR
#include "ShooterCharacter.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SkeletalRenderPublic.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

static FAutoConsoleCommandWithWorld GroundDiagnostic(
 TEXT("fps.DebugPlayerGround"), TEXT("Write player sole and collision heights to Saved/PlayerGround.txt"),
 FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
 {
  FString Report;
  for (TActorIterator<AShooterCharacter> It(World); It; ++It)
  {
   auto* C=*It; auto* M=C->GetMesh();
   TArray<FFinalSkinVertex> Vertices;
   M->GetCPUSkinnedVertices(Vertices,0);
   double Sole=BIG_NUMBER;
   for (const auto& V:Vertices) Sole=FMath::Min(Sole,M->GetComponentTransform().TransformPosition(FVector(V.Position)).Z);
   FHitResult Simple, Complex;
   FCollisionQueryParams Q(SCENE_QUERY_STAT(PlayerGround),false,C);
   const FVector Start=C->GetActorLocation(), End=Start-FVector(0,0,500);
   World->LineTraceSingleByChannel(Simple,Start,End,ECC_Visibility,Q);
   Q.bTraceComplex=true;
   World->LineTraceSingleByChannel(Complex,Start,End,ECC_Visibility,Q);
   Report+=FString::Printf(TEXT("%s location=%s capsuleBottom=%.3f sole=%.3f floor=%.3f simple=%s/%.3f complex=%s/%.3f\n"),
    *C->GetName(),*Start.ToString(),Start.Z-C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Sole,
    C->GetCharacterMovement()->CurrentFloor.HitResult.ImpactPoint.Z,
    *GetNameSafe(Simple.GetActor()),Simple.ImpactPoint.Z,*GetNameSafe(Complex.GetActor()),Complex.ImpactPoint.Z);
  }
  FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("PlayerGround.txt")));
 }));
#endif
