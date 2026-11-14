#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "GameFramework/Actor.h"
#include "Interface/Interactable.h"
#include "Ui/RangeLoadoutWidget.h"
#include "RangeLoadoutBench.generated.h"

class USceneComponent;
class UBoxComponent;
class USkeletalMeshComponent;
class UAnimationAsset;
class UAnimInstance;
class AWeapon;
class UAttachmentData;

// 靶场改枪台的逻辑锚点。
// 机位用关卡里单独放的 Camera / CineCamera，拖到细节里的 ViewCamera。
// 人物用 ExternalPreviewMesh；没填就用本 Actor 上的 CharacterMesh。
UCLASS(meta = (DisplayName = "靶场改枪台"))
class FPS_API ARangeLoadoutBench : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ARangeLoadoutBench();

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	AActor* GetBenchViewTarget() const;
 virtual void CalcCamera(float DeltaTime,struct FMinimalViewInfo& OutResult) override;
	USkeletalMeshComponent* GetPreviewMesh() const;

	// 服务器：把预览枪挂到人物手上。配件由武器自己同步。
	void Auth_SetPreview(TSubclassOf<AWeapon> WeaponClass, const TArray<UAttachmentData*>& Attachments, APawn* Viewer);
	void Auth_ClearPreview();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Bench")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Bench")
	TObjectPtr<UBoxComponent> InteractBox;

	// 关卡里放一台 Camera 或 CineCamera，拖进来。位置、朝向、FOV 都在那台摄像机上改。
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "FPS|Bench")
	TObjectPtr<ACameraActor> ViewCamera;

	// 没填 ExternalPreviewMesh 时，预览枪挂在这里。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Bench")
	TObjectPtr<USkeletalMeshComponent> CharacterMesh;

	// 用关卡里已经摆好的人物网格。空 = 用上面的 CharacterMesh。
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "FPS|Bench")
	TObjectPtr<USkeletalMeshComponent> ExternalPreviewMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Bench")
	FText Prompt;

	// 你做的改枪 WBP。父类必须是 RangeLoadoutWidget。空着按 F 不会打开界面。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Bench", meta = (DisplayName = "改枪界面"))
	TSubclassOf<URangeLoadoutWidget> LoadoutWidgetClass;

	// 这些枪不进改枪列表。选一个父类时，它的子类也一起隐藏。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Bench", meta = (DisplayName = "隐藏武器"))
	TArray<TSubclassOf<AWeapon>> HiddenWeapons;

	// 枪挂在人物的哪个插槽上。找不到时会再试 weapon_r / hand_r。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Bench")
	FName HandSocket = TEXT("weapon_r");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Bench")
	FTransform PreviewWeaponOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Bench")
	TSubclassOf<UAnimInstance> PreviewAnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Bench")
	TObjectPtr<UAnimationAsset> PreviewIdle;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(ReplicatedUsing = OnRep_PreviewWeapon)
	TObjectPtr<AWeapon> PreviewWeapon;

	UFUNCTION()
	void OnRep_PreviewWeapon();

	void AttachPreview();
	FName ResolveHandSocket() const;
};
