#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

class APawn;

// F 键交互接口。C++ 或蓝图（门等）都可实现：Class Settings → Interfaces → Add → Interactable。
// 控制器从相机往前打 InteractDistance 射线，命中实现此接口且 CanInteract 为 true 的 Actor 时，
// 按下 F 由服务器调用 Interact；没有命中则走爆破的安放/拆除（按住）。
UINTERFACE(MinimalAPI, Blueprintable)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

class FPS_API IInteractable
{
	GENERATED_BODY()

public:
	// 这个 Pawn 现在能不能交互（服务器和本地都会调用）
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FPS|Interact")
	bool CanInteract(APawn* InteractingPawn) const;

	// HUD 提示文字，例如“按 F 开门”
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FPS|Interact")
	FText GetInteractPrompt(APawn* InteractingPawn) const;

	// 服务器上执行交互（门：在这里开关并复制状态）
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FPS|Interact")
	void Interact(APawn* InteractingPawn);
};
