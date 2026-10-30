
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Loadout/LoadoutTypes.h"
#include "Ui/BuyShopTypes.h"
#include "Game/SearchDestroyTypes.h"
#include "Game/PoliceCriminalTypes.h"
#include "ShooterPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UAttachmentMenu;
class AWeapon;
class UPauseMenuScreen;
class UBuyShopScreen;
class UModeHudWidget;
class UMatchHUD;
class UCombatHudKeepAlive;
class UCombatStatusHud;
class AGameStateBase;
class AWeaponPickup;
class UScoreboardScreen;
class URadarWidget;
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
	UFUNCTION(BlueprintPure, Category = "FPS|Movement") bool IsTacticalSprinting() const { return bIsSprinting && bIsTacticalSprinting; }

	// 冲刺中按开火到第一枪的缓冲（战术冲刺更长）
	float GetSprintToFireDelay() const { return IsTacticalSprinting() ? TacticalSprintToFireDelay : SprintToFireDelay; }

	// 冲刺后摇 + Option A 滑铲前半段门控：要再等这么久才能开镜（Combat 预输入复用）
	UFUNCTION(BlueprintPure, Category = "FPS|Movement")
	bool CanStartAds() const;

	/** 距离可以开镜还剩多久（冲刺后摇与滑铲门控取较大值；都过了则为 0） */
	UFUNCTION(BlueprintPure, Category = "FPS|Movement")
	float GetAdsSettleRemaining() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Movement")
	float GetSprintToAdsDelay() const { return SprintToAdsDelay; }

	UFUNCTION(BlueprintCallable)
	void StopSprinting();

	// PIE/debug: console `Armor 0|1|2` (None/Light/Heavy).
	UFUNCTION(Exec)
	void Armor(int32 Level);

	// PIE/debug: console `FactionLook 0|1|2` (DefaultManny/Criminal/Police).
	UFUNCTION(Exec)
	void FactionLook(int32 Level);

	// PIE/debug: console `ToggleThirdPerson` or `ThirdPerson 0|1` — see own 3P body / FactionLook.
	UFUNCTION(Exec)
	void ToggleThirdPerson();

	UFUNCTION(Exec)
	void ThirdPerson(int32 Enabled);

	// PIE/debug: console `Vision 0|1|2` (Off/NVG/Thermal), `CycleVision`, `RefillVision`, `FlashWash`.
	UFUNCTION(Exec)
	void Vision(int32 Mode);

	UFUNCTION(Exec)
	void CycleVision();

	UFUNCTION(Exec)
	void RefillVision(float Amount = -1.f);

	UFUNCTION(Exec)
	void FlashWash(float Amount = 2.f);

	// PIE/debug: console `SpawnTestDoor` — wooden breakable door in front of you.
	UFUNCTION(Exec)
	void SpawnTestDoor();

	// PIE/debug traps: Spawn* ignore setup; Place* require criminal setup (or non-PC mode).
	UFUNCTION(Exec)
	void SpawnTripwire();

	UFUNCTION(Exec)
	void SpawnGasCan();

	UFUNCTION(Exec)
	void PlaceTripwire();

	UFUNCTION(Exec)
	void PlaceGasCan();

	// ---- 警匪战术 PIE ----
	// PickFaction 0=警察 1=匪；VanPick / ArmoryPick；SkipSetup 跳过布置
	UFUNCTION(Exec)
	void PickFaction(int32 Faction);

	UFUNCTION(Exec)
	void VanPick(FString WeaponName);

	UFUNCTION(Exec)
	void ArmoryPick(FString WeaponName);

	UFUNCTION(Exec)
	void SkipSetup();

	UFUNCTION(Exec)
	void SpawnTestHostage();

	// MvpRound 测试：KillSelf / ForceWin 0|1 / SetRoundTime 秒
	UFUNCTION(Exec)
	void KillSelf();

	UFUNCTION(Exec)
	void ForceWin(int32 Team);

	UFUNCTION(Exec)
	void SetRoundTime(float Seconds);

	/** 面包车/军械库关卡测切图：TravelToMatch [/Game/.../MapName] */
	UFUNCTION(Exec)
	void TravelToMatch(FString MapName);

	// Polish: watch time, radar equip, barrel debug, blood footprints PIE.
	UFUNCTION(Exec)
	void CheckWatch();

	UFUNCTION(Exec)
	void GrantRadar(int32 Enabled = 1);

	UFUNCTION(Exec)
	void DebugBarrel(int32 Enabled = 1);

	UFUNCTION(Exec)
	void MarkBlood(int32 Steps = 10);

	UFUNCTION(BlueprintPure, Category = "FPS|HUD")
	UMatchHUD* GetMatchHUD() const { return MatchHUD; }


	/** Server helper: write SubmittedLoadout + rebuild (GameMode calls this). */
	void Auth_SetSubmittedLoadout(const FLoadoutPreset& Loadout, bool bStripAttachments);



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

	// ---- 战术大地图（M 开关；Esc 优先关地图） ----
	UFUNCTION(BlueprintCallable, Category = "FPS|HUD")
	void ToggleFullMap();

	UFUNCTION(BlueprintCallable, Category = "FPS|HUD")
	void ShowFullMap(bool bShow);

	UFUNCTION(BlueprintPure, Category = "FPS|HUD")
	bool IsFullMapOpen() const;

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
	void Auth_SpectatePrev(bool bOnlyIfInvalid = false);

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

	// NVG toggle (runtime Alt+K). Default Off; Thermal remains console-only for now.
	void EnsureNVGInput();
	void EnsureWatchInput();
	void Input_CheckWatch();
	void Input_ToggleNVG();

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> NVGAction;
	UPROPERTY()
	TObjectPtr<UInputAction> WatchAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> NVGIMC;
	UPROPERTY()
	TObjectPtr<UInputMappingContext> WatchIMC;

	bool bNVGBound = false;


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

	UPROPERTY(Transient)
	TObjectPtr<UCombatHudKeepAlive> CombatHudKeepAlive;

	UPROPERTY(Transient)
	TObjectPtr<UCombatStatusHud> CombatStatusHud;

	public:
	/** Creates/re-pins CombatStatusHud + KeepAlive for local player. Safe to call repeatedly. */
	void EnsureCombatHudKeepAlive();

	private:
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
	// Repeated crouch/stand gets progressively slower.
	int32 CrouchSpamCount = 0;
	double LastCrouchToggleTime = -100.0;
	double CrouchReadyTime = 0.0;
	UPROPERTY(EditAnywhere, Category = "FPS|Movement|CrouchSpam", meta = (ClampMin = "0.1"))
	float CrouchSpamWindow = 1.0f;
	UPROPERTY(EditAnywhere, Category = "FPS|Movement|CrouchSpam", meta = (ClampMin = "0"))
	float CrouchSpamBaseCooldown = 0.15f;
	UPROPERTY(EditAnywhere, Category = "FPS|Movement|CrouchSpam", meta = (ClampMin = "0"))
	float CrouchSpamStep = 0.2f;
	UPROPERTY(EditAnywhere, Category = "FPS|Movement|CrouchSpam", meta = (ClampMin = "0"))
	float CrouchSpamMaxCooldown = 1.2f;
	bool CanToggleCrouchNow() const;
	void NoteCrouchToggle();
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
	void EnsureFullMapInput();
	void Input_ToggleFullMap();

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FullMapAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> FullMapIMC;

	UPROPERTY(Transient)
	TObjectPtr<URadarWidget> FullMapWidget;

	bool bFullMapBound = false;
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

	UFUNCTION(Server, Reliable)
	void Server_SpectatePrev();

	void EnsureSpectateInput();
	void Input_SpectateNext();
	void Input_SpectatePrev();
	void Auth_SpectateCycle(int32 Direction, bool bOnlyIfInvalid);
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
	TObjectPtr<UInputAction> SpectatePrevAction;

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
	float WalkSpeed = 120.f; // quiet-walk reference (Apply uses RunSpeed * WalkSpeedMultiplier)

	UPROPERTY(EditAnywhere, Category = "FPS|Movement")
	float RunSpeed = 220.f; // was 500 — small CQB maps

	UPROPERTY(EditAnywhere, Category = "FPS|Movement")
	float SprintSpeed = 320.f; // was 700

	// ---------- 战术冲刺：冲刺中快速再按一次冲刺键（双击 Shift） ----------
	UPROPERTY(EditAnywhere, Category = "FPS|Movement|TacticalSprint")
	bool bEnableTacticalSprint = false; // off for tactical mode

	UPROPERTY(EditAnywhere, Category = "FPS|Movement|TacticalSprint", meta = (ClampMin = "0"))
	float TacticalSprintSpeed = 380.f; // was 800

	UPROPERTY(EditAnywhere, Category = "FPS|Movement|TacticalSprint", meta = (ClampMin = "0.1"))
	float TacticalSprintDuration = 3.f;

	// 战术冲刺结束后多久才能再用
	UPROPERTY(EditAnywhere, Category = "FPS|Movement|TacticalSprint", meta = (ClampMin = "0"))
	float TacticalSprintCooldown = 5.f;

	// 两次按冲刺键间隔小于这个算双击
	UPROPERTY(EditAnywhere, Category = "FPS|Movement|TacticalSprint", meta = (ClampMin = "0.05"))
	float SprintDoubleTapWindow = 0.3f;

	// 冲刺转开火缓冲（秒）
	UPROPERTY(EditAnywhere, Category = "FPS|Movement", meta = (ClampMin = "0"))
	float SprintToFireDelay = 0.12f;

	UPROPERTY(EditAnywhere, Category = "FPS|Movement|TacticalSprint", meta = (ClampMin = "0"))
	float TacticalSprintToFireDelay = 0.22f;

	// 松开冲刺（或滑铲取消冲刺）后，多久才能开镜
	UPROPERTY(EditAnywhere, Category = "FPS|Movement", meta = (ClampMin = "0"))
	float SprintToAdsDelay = 0.05f;


	bool bIsTacticalSprinting = false;
	double LastSprintPressTime = -10.0;
	double TacticalSprintReadyTime = 0.0;
	FTimerHandle TacticalSprintTimer;
	void StartTacticalSprint();
	void EndTacticalSprint();

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
	double SprintEndedAt = -1000.0;
	void NoteSprintEnded();
	bool bIsWalking = false;

	void UpdateMovementSpeed();
	class UStatsComponent* GetStats();

	UFUNCTION()  
	void HandleStaminaDepleted();

	// 只改本地速度，不发 RPC
	void ApplyMovementSpeed();

	// 客户端 -> 服务器：告诉服务器我现在的移动状态
	UFUNCTION(Server, Reliable)
	void Server_SetMovementState(bool bNewSprinting, bool bNewWalking, bool bNewTactical);

	// 服务器 -> 客户端：耐力耗尽，退出冲刺
	UFUNCTION(Client, Reliable)
	void Client_StopSprint();

	UFUNCTION(Server, Reliable)
	void Server_SpawnTestDoor();

	UFUNCTION(Server, Reliable)
	void Server_SpawnTripwire(bool bIgnoreSetup);

	UFUNCTION(Server, Reliable)
	void Server_SpawnGasCan(bool bIgnoreSetup);

	UFUNCTION(Server, Reliable)
	void Server_PickFaction(uint8 FactionByte);

	UFUNCTION(Server, Reliable)
	void Server_VanPickByName(const FString& WeaponName);

	UFUNCTION(Server, Reliable)
	void Server_ArmoryPickByName(const FString& WeaponName);

	UFUNCTION(Server, Reliable)
	void Server_SkipSetup();

	UFUNCTION(Server, Reliable)
	void Server_MvpDebug(int32 Op, float Value);

	UFUNCTION(Server, Reliable)
	void Server_TravelToMatch(const FString& MapName);

	UFUNCTION(Server, Reliable)
	void Server_SpawnTestHostage();

};
