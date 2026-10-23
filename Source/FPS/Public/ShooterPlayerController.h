
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Loadout/LoadoutTypes.h"
#include "Ui/BuyShopTypes.h"
#include "Game/SearchDestroyTypes.h"
#include "ShooterPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UAttachmentMenu;
class AWeapon;
class UPauseMenuScreen;
class UBuyShopScreen;
class UModeHudWidget;
class UMatchHUD;
class AGameStateBase;
class AWeaponPickup;
class UScoreboardScreen;
class AShooterCharacter;
class USettingsScreen;
struct FInputActionValue;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShooterSpectateChanged, bool, bIsSpectating, APlayerState*, Target);
/**
 * 
 */
UCLASS()
class FPS_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AShooterPlayerController();

	// 只按当前状态重新算速度（不发 RPC），角色在开镜/下蹲变化时调用
	void RefreshMovementSpeed() { ApplyMovementSpeed(); }
	UFUNCTION(BlueprintPure, Category = "FPS|Movement") bool IsSprinting() const { return bIsSprinting; }
	
	UFUNCTION(BlueprintCallable)
	void StopSprinting();

	bool bPawnAlive;

	// ---------- 局内头顶 HUD（计分条 + 击杀播报） ----------
	// 填 WBP_MatchHUD（父类 MatchHUD）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|HUD")
	TSubclassOf<UMatchHUD> MatchHUDClass;

	// ---------- 局内暂停菜单 ----------
	// 菜单控件（WBP_PauseMenu，父类 PauseMenuScreen）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PauseMenu")
	TSubclassOf<UPauseMenuScreen> PauseMenuWidgetClass;

	UFUNCTION(BlueprintCallable, Category = "FPS|PauseMenu")
	void TogglePauseMenu();

	UFUNCTION(BlueprintCallable, Category = "FPS|PauseMenu")
	void OpenPauseMenu();

	UFUNCTION(BlueprintCallable, Category = "FPS|PauseMenu")
	void ClosePauseMenu();

	// ---- 设置（暂停菜单里的“设置”按钮调用） ----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Settings")
	TSubclassOf<USettingsScreen> SettingsWidgetClass;

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings")
	void ShowSettings();

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings")
	void HideSettings();

	// 把保存的操作设置（FOV 等）套到当前角色上
	void ApplyControlSettings();

	bool bPauseHiddenForSettings = false;

	UPROPERTY()
	TObjectPtr<USettingsScreen> SettingsScreen;

	UFUNCTION(BlueprintPure, Category = "FPS|PauseMenu")
	bool IsPauseMenuOpen() const;

	// ---------- 爆破购买商店 ----------
	UFUNCTION(BlueprintCallable, Category = "FPS|Buy")
	void ToggleBuyShop();

	UFUNCTION(BlueprintCallable, Category = "FPS|Buy")
	void OpenBuyShop();

	UFUNCTION(BlueprintCallable, Category = "FPS|Buy")
	void CloseBuyShop();

	UFUNCTION(BlueprintPure, Category = "FPS|Buy")
	bool IsBuyShopOpen() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Buy")
	bool CanOpenBuyShop() const;

	UFUNCTION(BlueprintCallable, Category = "FPS|Buy")
	bool RequestBuyShopItem(int32 CatalogIndex);

	UFUNCTION(BlueprintPure, Category = "FPS|Buy")
	TArray<FBuyShopItem> GetCachedBuyCatalog() const;

	bool Auth_ApplyBoughtWeapon(const FWeaponPreset& Preset);

	// ---- F 交互 / 武器掉落 ----
	// 相机射线长度（cm）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Interact")
	float InteractDistance = 250.f;

	// 掉落物类，可填 AWeaponPickup 的蓝图子类；为空用 C++ 默认
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Interact")
	TSubclassOf<AWeaponPickup> WeaponPickupClass;

	// 本地：准星对着的可交互物（没有返回 nullptr）
	UFUNCTION(BlueprintPure, Category = "FPS|Interact")
	AActor* TraceInteractable() const;

	// HUD 绑定：当前交互提示（门/枪 > 爆破拾取/安放/拆除），没有则为空
	UFUNCTION(BlueprintPure, Category = "FPS|Interact")
	FText GetInteractPromptText() const;

	// 服务器：死亡掉落主枪/手枪（刀和炸弹不在这里）
	void Auth_DropWeaponsOnDeath(APawn* DeadPawn);

	// 服务器：捡枪，同槽位旧枪丢地上。成功返回 true（调用方销毁掉落物）
	bool Auth_PickupWeapon(AWeaponPickup* Pickup);

	// ---- 计分板（按住 Tab） ----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|HUD")
	TSubclassOf<UScoreboardScreen> ScoreboardWidgetClass;

	UFUNCTION(BlueprintCallable, Category = "FPS|HUD")
	void ShowScoreboard(bool bShow);

	// 填 WBP_BuyShop（父类 BuyShopScreen）。只在爆破局使用；TDM 可留空。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Buy")
	TSubclassOf<UBuyShopScreen> BuyShopWidgetClass;



	// ---------- 局外配装 ----------
	// 服务器上已校验的配装；SpawnInventory 读它。控制器跨重生存在，所以重生也会用
	const FLoadoutPreset* GetSubmittedLoadout() const { return bHasSubmittedLoadout ? &SubmittedLoadout : nullptr; }

	// 允许进局使用的枪（空 = 不限制）
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Loadout")
	TArray<TSubclassOf<AWeapon>> AllowedLoadoutWeapons;

	UFUNCTION(Server, Reliable)
	void Server_SubmitLoadout(const FLoadoutPreset& Loadout);

	UFUNCTION(Server, Reliable)
	void Server_BuyShopItem(int32 CatalogIndex);

	// ---------- 比赛 ----------
	// 比赛结束：本地停掉输入（结算界面由 HUD 监听 GameState.OnMatchEnded 显示）
	UFUNCTION(Client, Reliable)
	void Client_OnMatchEnded(uint8 WinningTeam);

	// 服务器让这个客户端回主菜单
	UFUNCTION(Client, Reliable)
	void Client_GoToMainMenu();

	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	bool IsMatchOver() const { return bMatchOver; }

	// ---------- 观战（爆破：死亡/回合中途加入） ----------
	UFUNCTION(BlueprintPure, Category = "FPS|Spectate")
	bool IsSpectating() const { return bSpectating; }

	// 正在看的玩家（复制给自己，HUD 显示“正在观战: xxx”）
	UFUNCTION(BlueprintPure, Category = "FPS|Spectate")
	APlayerState* GetSpectateTarget() const { return SpectateTarget; }

	UFUNCTION(BlueprintPure, Category = "FPS|Spectate")
	FString GetSpectateTargetName() const;

	// 观战开始/结束/切换目标时（本地）触发
	UPROPERTY(BlueprintAssignable, Category = "FPS|Spectate")
	FShooterSpectateChanged OnSpectateTargetChanged;

	// ---- 服务器 ----
	void Auth_StartSpectating();
	void Auth_StopSpectating();
	// bOnlyIfInvalid：当前目标还活着就不换
	void Auth_SpectateNext(bool bOnlyIfInvalid = false);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* P) override;

private:

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputMappingContext> ShooterIMC;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction>SprintAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction>WalkAction;

	// 战术配件开关（例如 F 键）：手电和激光一起开关（第 12 步）
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> FlashlightAction;

	void Input_ToggleFlashlight();

	// 爆破交互（安放 / 拆除）。空着时运行时绑 F
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> InteractAction;

	// 配件菜单开关（例如 B 键）
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> ToggleAttachmentMenuAction;

	// 填 WBP_AttachmentMenu
	UPROPERTY(EditAnywhere, Category = "FPS|UI")
	TSubclassOf<UAttachmentMenu> AttachmentMenuClass;

	// 只创建一次，之后反复加到屏幕/移除
	UPROPERTY(Transient)
	TObjectPtr<UAttachmentMenu> AttachmentMenu;

	void Input_ToggleAttachmentMenu();

	// Tab 打开爆破购买商店（仅购买阶段有效）。按键运行时注册。
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BuyShopAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> BuyShopIMC;

	UPROPERTY(Transient)
	TObjectPtr<UBuyShopScreen> BuyShop;

	void EnsureBuyShopInput();
	bool bBuyShopBound = false;
	void Input_ToggleBuyShop();

	UFUNCTION()
	void HandleBuyPhaseChanged(EBombRoundPhase NewPhase);

	void HandleGameStateSetForBuy(AGameStateBase* NewGameState);


	// 暂停菜单按键（运行时创建：Esc + P，P 方便 PIE 里测，因为 PIE 默认 Esc 是停止）
	void EnsurePauseInput();
	void Input_TogglePauseMenu();

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PauseMenuAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> PauseMenuIMC;

	UPROPERTY(Transient)
	TObjectPtr<UPauseMenuScreen> PauseMenu;

	UPROPERTY(Transient)
	TObjectPtr<UMatchHUD> MatchHUD;

	void EnsureMatchHUD();

	void EnsureModeHud();

	UPROPERTY(Transient)
	TObjectPtr<UModeHudWidget> ModeHud;

	bool bMatchOver = false;

	FLoadoutPreset SubmittedLoadout;
	bool bHasSubmittedLoadout = false;

	void Input_Crouch();          // C: toggle crouch / stand (prone -> crouch); slides when fast
	void Input_CrouchHold_Started();   // Left Ctrl pressed: crouch (or slide when fast)
	void Input_CrouchHold_Completed(); // Left Ctrl released: stand (CMC waits for headroom)
	void EnsureCrouchInput();

	// Empty = runtime IAs: C -> toggle, Left Ctrl -> hold. Runtime IMC has priority 2 and consumes those keys.
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> CrouchToggleAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> CrouchHoldAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> CrouchRuntimeIMC;

	bool bCrouchHeld = false;
	void Input_Jump();
	void Input_Sprint();
	void Input_Walk();
	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Look(const FInputActionValue& InputActionValue);
	void Input_Interact_Started();
	void Input_Interact_Completed();

	UFUNCTION(Server, Reliable)
	void Server_Interact(AActor* Target);

	AWeaponPickup* Auth_SpawnPickupFromWeapon(AWeapon* Weapon, APawn* FromPawn, const FVector& Offset);

	UPROPERTY()
	TObjectPtr<UInputAction> ScoreboardAction;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> ScoreboardIMC;

	UPROPERTY()
	TObjectPtr<UScoreboardScreen> Scoreboard;

	bool bScoreboardBound = false;
	void EnsureScoreboardInput();
	void Input_Scoreboard_Started();
	void Input_Scoreboard_Completed();
	void EnsureInteractInput();

	// ---- 观战 ----
	UPROPERTY(ReplicatedUsing = OnRep_Spectate)
	bool bSpectating = false;

	UPROPERTY(ReplicatedUsing = OnRep_Spectate)
	TObjectPtr<APlayerState> SpectateTarget;

	UFUNCTION()
	void OnRep_Spectate();

	UFUNCTION(Server, Reliable)
	void Server_SpectateNext();

	void EnsureSpectateInput();
	void Input_SpectateNext();
	void ApplySpectateLocal();
	FTimerHandle SpectateFadeTimer;
	// ---- 客户端校时（RTT/2 补偿） ----
	UFUNCTION(Server, Unreliable)
	void Server_RequestServerTime(double ClientSendTime);
	UFUNCTION(Client, Unreliable)
	void Client_ReportServerTime(double ClientSendTime, double ServerTime);
	void RequestServerTimeSync();
	FTimerHandle TimeSyncTimer;
	double BestTimeSyncRTT = -1.0;
	TWeakObjectPtr<AShooterCharacter> Viewed1PCharacter;
	void SyncSpectatedCharacter();
	bool bSpectateBound = false;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SpectateNextAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> SpectateIMC;

	bool IsMovementLocked() const;

	UFUNCTION(Server, Reliable)
	void Server_BeginBombInteract();

	UFUNCTION(Server, Reliable)
	void Server_EndBombInteract();

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> InteractIMC;

	UPROPERTY(EditAnywhere, Category = "FPS|Movement")
	float WalkSpeed = 250.f;

	UPROPERTY(EditAnywhere, Category = "FPS|Movement")
	float RunSpeed = 500.f;

	UPROPERTY(EditAnywhere, Category = "FPS|Movement")
	float SprintSpeed = 800.f;

	// 下面三个倍率会相乘叠加，例如 下蹲+静步+开镜 = 500 * 0.6 * 0.5 * 0.7
	UPROPERTY(EditAnywhere, Category = "FPS|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrouchSpeedMultiplier = 0.6f;

	// 趴行速度 = RunSpeed * 这个倍率（再乘静步/开镜倍率）。不能冲刺
	UPROPERTY(EditAnywhere, Category = "FPS|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ProneSpeedMultiplier = 0.25f;

	UPROPERTY(EditAnywhere, Category = "FPS|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WalkSpeedMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, Category = "FPS|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimSpeedMultiplier = 0.7f;

	bool bIsSprinting = false;
	bool bIsWalking = false;

	void UpdateMovementSpeed();
	class UStatsComponent* GetStats();

	UFUNCTION()  
	void HandleStaminaDepleted();

	// 只改本地速度，不发 RPC
	void ApplyMovementSpeed();

	// 客户端 -> 服务器：告诉服务器我现在的移动状态
	UFUNCTION(Server, Reliable)
	void Server_SetMovementState(bool bNewSprinting, bool bNewWalking);

	// 服务器 -> 客户端：耐力耗尽，退出冲刺
	UFUNCTION(Client, Reliable)
	void Client_StopSprint();
};
