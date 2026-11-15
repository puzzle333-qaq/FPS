
#pragma once

#include "CoreMinimal.h"
#include "Character/PoliceAppearanceTypes.h"
#include "GameFramework/PlayerController.h"
#include "Loadout/LoadoutTypes.h"
#include "ShooterPlayerController.generated.h"

class UDeploymentDroneComponent;
class AMissionTabletActor;
class AMissionDeployPoint;
class UMissionFlowWidget;
class ACoopCqbGameState;
class UInputMappingContext;
class UInputAction;
class AWeapon;
class UCombatHudKeepAlive;
class UScreenEffectOverlay;
class UCombatStatusHud;
class UPartyRosterWidget;
class UInteractPromptWidget;
class UMagInspectWidget;
class UControlHintWidget;
class UNVGMaskWidget;
class UTutorialPromptWidget;
class UPauseMenuScreen;
class USettingsScreen;
class AGameStateBase;
class AShooterCharacter;
class ARangeLoadoutBench;
class URangeLoadoutWidget;
class UAttachmentData;
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
 UFUNCTION(Server,Reliable) void Server_SetPoliceProtection(EPoliceFaceProtection Face,EBodyArmorType Armor);
	AShooterPlayerController();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Mission") TObjectPtr<UDeploymentDroneComponent> DeploymentDrone;
 UFUNCTION(Server,Reliable) void Server_DeploymentViewReady();
 void HideDeploymentOverlayForRaise();
 void LockDeploymentPresentation();
 virtual void ClientRestart_Implementation(APawn* NewPawn)override;
 bool IsMissionSceneReady()const{return bMissionLoadAcknowledged;}
 virtual void AutoManageActiveCameraTarget(AActor* SuggestedTarget)override;
 UFUNCTION(Client, Reliable) void Client_OpenMissionSelection(AMissionDeployPoint* Deploy);
 UFUNCTION(Server, Reliable) void Server_SelectMission(AMissionDeployPoint* Deploy, int32 Index);
 UFUNCTION(Server, Reliable) void Server_CancelMissionDeploy(AMissionDeployPoint* Deploy);
 UFUNCTION(Server,Reliable) void Server_ToggleMissionReady(AMissionDeployPoint* Deploy);
 UFUNCTION(BlueprintCallable,Category="Mission") void ToggleMissionReady();
 UFUNCTION(Server, Reliable) void Server_SubmitIdentity(const FString& DisplayName, const FString& PlatformId);
 UFUNCTION(Server, Reliable) void Server_MissionLoaded();
 void CloseMissionPanel();
 bool IsMissionTabletOpen() const { return MissionTablet != nullptr; }
 UFUNCTION() void OnMissionStateUpdated();


	// 只按当前状态重新算速度（不发 RPC），角色在开镜/下蹲变化时调用
	void RefreshMovementSpeed() { ApplyMovementSpeed(); }
	UFUNCTION(BlueprintPure, Category = "FPS|Movement") bool IsSprinting() const { return bIsSprinting; }
	bool IsCrouchHeld() const { return bCrouchHeld; }
	bool IsControlMovementLocked() const { return IsMovementLocked(); }

	float GetSprintToFireDelay() const { return SprintToFireDelay; }

	// 冲刺后摇：要再等这么久才能开镜（Combat 预输入复用）
	UFUNCTION(BlueprintPure, Category = "FPS|Movement")
	bool CanStartAds() const;

	/** 距离可以开镜还剩多久（冲刺后摇与滑铲门控取较大值；都过了则为 0） */
	UFUNCTION(BlueprintPure, Category = "FPS|Movement")
	float GetAdsSettleRemaining() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Movement")
	float GetSprintToAdsDelay() const { return SprintToAdsDelay; }

	UFUNCTION(BlueprintCallable)
	void StopSprinting();

	// PIE/debug: console `WeaponAttachDebug 1` — draw 1P attach / GripSocket / FABRIK and print pose/anim.
	UFUNCTION(Exec)
	void WeaponAttachDebug(int32 Enabled = 1);

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

	// PIE/debug: `Goggles 1` wears flash goggles (ghosting). `Goggles 0` is a normal whiteout.
	UFUNCTION(Exec)
	void Goggles(int32 Enabled);

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

	UFUNCTION(Exec)
	void SpawnTestHostage();

	// Watch, radar, barrel debug, blood footprints.
	UFUNCTION(Exec)
	void CheckWatch();

	UFUNCTION(Exec)
	void GrantRadar(int32 Enabled = 1);

	UFUNCTION(Exec)
	void DebugBarrel(int32 Enabled = 1);

	UFUNCTION(Exec)
	void MarkBlood(int32 Steps = 10);

	/** Server helper: write SubmittedLoadout + rebuild (GameMode calls this). */
	void Auth_SetSubmittedLoadout(const FLoadoutPreset& Loadout, bool bStripAttachments);

	bool bPawnAlive = true;

	// Applies saved FOV onto the current pawn.
	void ApplyControlSettings();

	// Esc / P 打开暂停菜单。邀请好友和设置都在这一层，有光标才能点。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|PauseMenu")
	TSubclassOf<UPauseMenuScreen> PauseMenuWidgetClass;

	UFUNCTION(BlueprintCallable, Category = "FPS|PauseMenu")
	void TogglePauseMenu();

	UFUNCTION(BlueprintCallable, Category = "FPS|PauseMenu")
	void OpenPauseMenu();

	UFUNCTION(BlueprintCallable, Category = "FPS|PauseMenu")
	void ClosePauseMenu();

	UFUNCTION(BlueprintPure, Category = "FPS|PauseMenu")
	bool IsPauseMenuOpen() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Settings")
	TSubclassOf<USettingsScreen> SettingsWidgetClass;

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings")
	void ShowSettings();

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings")
	void HideSettings();

	// ---- F 交互 ----
	// 相机射线长度（cm）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Interact")
	float InteractDistance = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Interact", meta = (ClampMin = "0.25"))
	float SurrenderWarningCooldown = 2.5f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="FPS|Interact") float WarningDistance=1500.f;
 UFUNCTION(BlueprintPure,Category="FPS|Interact") AActor* TraceWarningTarget() const;

	// 本地：准星对着的可交互物（没有返回 nullptr）
	UFUNCTION(BlueprintPure, Category = "FPS|Interact")
	AActor* TraceInteractable(bool bIncludeUnavailable=false) const;

	// HUD 绑定：当前交互提示（门/人质/陷阱），没有则为空
	UFUNCTION(BlueprintPure, Category = "FPS|Interact")
	FText GetInteractPromptText() const;

	bool IsTacticalDeviceLaser() const { return bTacticalDeviceIsLaser; }

	/** Local-only tutorial hint. Shown once per PromptId on this controller. */
	UFUNCTION(BlueprintCallable, Category = "FPS|Tutorial")
	void ShowTutorialPrompt(const FText& Text, FName PromptId, float Duration = 6.f);

	UFUNCTION(BlueprintPure, Category = "FPS|Tutorial")
	FText GetTutorialPromptText() const { return TutorialPromptText; }

	// 靶场改枪台。服务器在交互时调用；本地打开镜头和抽屉，离开时提交配装。
	void Auth_OpenRangeBench(ARangeLoadoutBench* Bench);
	UFUNCTION(BlueprintCallable, Category = "FPS|改枪台")
	void LeaveRangeBench();
	void UpdateRangeBenchPreview(ARangeLoadoutBench* Bench, TSubclassOf<AWeapon> WeaponClass, const TArray<UAttachmentData*>& Attachments);
	bool IsRangeBenchOpen() const { return bRangeBenchOpen; }

	// ---------- 局外配装 ----------
	// 服务器上已校验的配装；SpawnInventory 读它。控制器跨重生存在，所以重生也会用
	const FLoadoutPreset* GetSubmittedLoadout() const { return bHasSubmittedLoadout ? &SubmittedLoadout : nullptr; }

	// 允许进局使用的枪（空 = 不限制）。名单里没有的，只要类在 /Game/FPS/Weapon/ 下也会放行（改枪台新枪不用每次改这份名单）。
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Loadout")
	TArray<TSubclassOf<AWeapon>> AllowedLoadoutWeapons;

	UFUNCTION(Server, Reliable)
	void Server_SubmitLoadout(const FLoadoutPreset& Loadout);

	UFUNCTION(Client, Reliable)
	void Client_SaveWarehouseLoadout(const FLoadoutPreset& Loadout);

	void RestoreWarehouseLoadout();
	bool bRestoredWarehouseLoadout = false;

	// ---------- 比赛 ----------
	// 比赛结束：本地停掉输入（结算界面由 HUD 监听 GameState.OnMatchEnded 显示）
	UFUNCTION(Client, Reliable)
	void Client_OnMatchEnded(uint8 WinningTeam);

	// 服务器让这个客户端回主菜单
	UFUNCTION(Client, Reliable)
	void Client_GoToMainMenu();

	UFUNCTION(BlueprintPure, Category = "FPS|Match")
	bool IsMatchOver() const { return bMatchOver; }

	// ---------- 观战（死亡后看队友） ----------
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
 virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* P) override;

private:
 void InitializeMissionInput();
 void ToggleMissionTablet();
 void ScrollTabletUp();
 void ScrollTabletDown();
 void ServiceMissionReadiness();
 void ShowMissionPanel(uint8 Panel, AMissionDeployPoint* Deploy = nullptr);
public:
 UFUNCTION(Server,Reliable) void Server_DebriefAction(bool bRetry);
private:
 UPROPERTY(Transient) TObjectPtr<UMissionFlowWidget> MissionWidget;
 UPROPERTY(Transient) TObjectPtr<AMissionTabletActor> MissionTablet;
 UPROPERTY(Transient) TObjectPtr<UInputAction> TabletAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> MissionReadyAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> TabletScrollUpAction;
 UPROPERTY(Transient) TObjectPtr<UInputAction> TabletScrollDownAction;
 UPROPERTY(Transient) TObjectPtr<UInputMappingContext> TabletIMC;
 UPROPERTY(Transient) TObjectPtr<ACoopCqbGameState> BoundMissionState;
 bool bMissionInputMuted = false;
 bool bMissionLoadAcknowledged = false;
 FTimerHandle MissionServiceTimer;
 float NextSurrenderWarningTime = 0.f;
 int32 PreviousWarningIndex = -1;


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

	// 旧的 I 键战术开关。不再绑定；镭射/手电改由鼠标上侧键控制。
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> FlashlightAction;

	void EnsureTacticalDeviceInput();
	void Input_TacticalDevice_Started();
	void Input_TacticalDevice_Completed();
	void Input_TacticalDevice_Hold();

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TacticalDeviceAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> TacticalDeviceIMC;

	FTimerHandle TacticalDeviceHoldTimer;
	bool bTacticalDeviceHoldFired = false;
	// 同一战术配件的功能：false=手电，true=镭射。SideMouse2 短按开关，长按切换。
	bool bTacticalDeviceIsLaser = false;

	// N: tap toggles night vision. Hold swaps dual tubes and fullscreen quad.
	void EnsureNVGInput();
	void EnsureWatchInput();
	void Input_CheckWatch();
	void Input_ToggleNVG();
	void Input_NVG_Started();
	void Input_NVG_Completed();
	void Input_NVG_Hold();

	FTimerHandle NVGHoldTimer;
	bool bNVGHoldFired = false;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> NVGAction;
	UPROPERTY()
	TObjectPtr<UInputAction> WatchAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> NVGIMC;
	UPROPERTY()
	TObjectPtr<UInputMappingContext> WatchIMC;

	bool bNVGBound = false;


	// 世界交互（门 / 人质 / 陷阱）。空着时运行时绑 F
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(Transient)
	TObjectPtr<UCombatHudKeepAlive> CombatHudKeepAlive;

	UPROPERTY(Transient)
	TObjectPtr<UScreenEffectOverlay> ScreenEffectOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UCombatStatusHud> CombatStatusHud;

	void EnsurePauseInput();
	void Input_TogglePauseMenu();
	bool CanTogglePauseMenu() const;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PauseMenuAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> PauseMenuIMC;

	UPROPERTY(Transient)
	TObjectPtr<UPauseMenuScreen> PauseMenu;

	UPROPERTY()
	TObjectPtr<USettingsScreen> SettingsScreen;

	bool bPauseHiddenForSettings = false;

	UPROPERTY(Transient)
	TObjectPtr<UPartyRosterWidget> PartyRoster;

	UPROPERTY(Transient)
	TObjectPtr<UInteractPromptWidget> InteractPrompt;

	TObjectPtr<UMagInspectWidget> MagInspect;

	TObjectPtr<UControlHintWidget> ControlHints;

	TObjectPtr<UNVGMaskWidget> NVGMask;

	UPROPERTY(Transient)
	TObjectPtr<UTutorialPromptWidget> TutorialPrompt;

	FText TutorialPromptText;
	TSet<FName> SeenTutorialPromptIds;
	FTimerHandle TutorialPromptTimer;
	void EnsureTutorialPromptWidget();
	void ClearTutorialPrompt();

	void SubmitLocalIdentity();

	public:
	/** Creates/re-pins CombatStatusHud + KeepAlive for local player. Safe to call repeatedly. */
	void EnsureCombatHudKeepAlive();

	private:
	bool bMatchOver = false;

	FLoadoutPreset SubmittedLoadout;
	bool bHasSubmittedLoadout = false;

	void Input_Crouch();          // C: toggle crouch / stand
	void Input_CrouchHold_Started();   // Left Ctrl pressed: crouch
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

	/** F with no interactable: police surrender shout (noise). */
	UFUNCTION(Server, Reliable)
	void Server_SurrenderWarning();

	void EnsureInteractInput();

	UFUNCTION(Client, Reliable)
	void Client_OpenRangeBench(ARangeLoadoutBench* Bench);

	UFUNCTION(Client, Reliable)
	void Client_CloseRangeBench();

	UFUNCTION(Server, Reliable)
	void Server_SetRangeBenchPreview(ARangeLoadoutBench* Bench, TSubclassOf<AWeapon> WeaponClass, const TArray<UAttachmentData*>& Attachments);

	UFUNCTION(Server, Reliable)
	void Server_EndRangeBench();

	void EnterRangeBench(ARangeLoadoutBench* Bench);
	void RefreshEquippedWeapon();

	UPROPERTY()
	TObjectPtr<ARangeLoadoutBench> RangeBench;

	UPROPERTY()
	TObjectPtr<URangeLoadoutWidget> RangeLoadoutWidget;

	bool bRangeBenchOpen = false;
	bool bRangeBenchInputMuted = false;

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

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> InteractIMC;

	UPROPERTY(EditAnywhere, Category = "FPS|Movement")
	float WalkSpeed = 120.f; // quiet-walk reference (Apply uses RunSpeed * WalkSpeedMultiplier)

	UPROPERTY(EditAnywhere, Category = "FPS|Movement")
	float RunSpeed = 220.f; // was 500 — small CQB maps

	UPROPERTY(EditAnywhere, Category = "FPS|Movement")
	float SprintSpeed = 320.f; // was 700

	// 冲刺转开火缓冲（秒）
	UPROPERTY(EditAnywhere, Category = "FPS|Movement", meta = (ClampMin = "0"))
	float SprintToFireDelay = 0.12f;

	// 松开冲刺后，多久才能开镜
	UPROPERTY(EditAnywhere, Category = "FPS|Movement", meta = (ClampMin = "0"))
	float SprintToAdsDelay = 0.05f;

	// 下面三个倍率会相乘叠加，例如 下蹲+静步+开镜 = 500 * 0.6 * 0.5 * 0.7
	UPROPERTY(EditAnywhere, Category = "FPS|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrouchSpeedMultiplier = 0.6f;

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
	void Server_SetMovementState(bool bNewSprinting, bool bNewWalking);

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
	void Server_SpawnTestHostage();

};
