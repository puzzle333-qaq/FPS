**Language / 语言：** [English](README.md) ｜ **中文**

---

# FPS — Unreal Engine 5 C++ 第一人称射击项目

一个使用 **Unreal Engine 5.8 + C++** 实现的第一人称射击（FPS）玩法框架：角色移动与视角、静步/冲刺移动模式与耐力、**Ctrl 按住 / C 切换蹲伏、按速度触发的滑铲与趴行**、武器（长枪 / AK / 手枪 / 近战）装备/切换/切枪自动关镜、开火与命中判定（近战为**沿刀刃的扫掠**）、**后坐力弹道（Recoil Pattern）与回正**、换弹、瞄准（ADS）、狙击镜、**数据驱动的武器配件系统**（枪管 / 护木 / 枪口 / 瞄具 / 握把 / 枪托 / 战术 / 弹匣）以及游戏内配件菜单与 Tab 整备界面、**画中画倍镜**、手电、生命值与治疗、部位伤害、受击动画，以及完整的多人网络同步。

> 本文档由阅读源码整理而成，**未修改任何代码文件**。
> 逐函数、逐组件的详细说明（含每个函数的签名、作用、调用链与效果）请见 [DOCUMENTATION.zh-CN.md](<DOCUMENTATION.zh-CN.md>)。
> English version: [README.md](<README.md>) ｜ [DOCUMENTATION.md](<DOCUMENTATION.md>)

---

## 1. 项目概览

| 项目 | 内容 |
| --- | --- |
| 引擎版本 | Unreal Engine 5.8（[FPS.uproject](<FPS.uproject>) 中 `EngineAssociation: "5.8"`） |
| 主模块 | `FPS`，类型 Runtime（[FPS.Build.cs](<Source/FPS/FPS.Build.cs>)） |
| 语言 | C++，大量接口通过 `BlueprintCallable` / `BlueprintImplementableEvent` 暴露给蓝图 |
| 输入 | Enhanced Input（`UEnhancedInputComponent` + `UInputMappingContext`） |
| 默认关卡 | `/Game/FirstPerson/Lvl_FirstPerson` |
| 默认 GameMode | `/Script/FPS.ShooterGameModeBase`（见 [DefaultEngine.ini](<Config/DefaultEngine.ini>)） |
| 自定义碰撞通道 | `Weapon` = `ECC_GameTraceChannel2`（Trace 类型） |
| 构建目标 | `FPS`（Game）、`FPSEditor`（Editor），`BuildSettingsVersion.V7` |

模块依赖（[FPS.Build.cs](<Source/FPS/FPS.Build.cs>)）：

- Public：`Core`、`CoreUObject`、`Engine`、`InputCore`、`EnhancedInput`、`GameplayTags`、`PhysicsCore`、`UMG`
- Private：`Slate`、`SlateCore`

启用的插件（[FPS.uproject](<FPS.uproject>)）：`ModelingToolsEditorMode`（仅编辑器）、`GameplayStateTree`、`AnimationWarping`、`ModelContextProtocol`、`AllToolsets`。

---

## 2. 功能一览

| 系统 | 已实现内容 | 主要代码位置 |
| --- | --- | --- |
| 角色 | 第一人称相机（SpringArm 臂长 0 + 相机延迟）、1P/3P 双 SkeletalMesh、蹲伏、跳跃、移动 | [ShooterCharacter.cpp](<Source/FPS/Private/ShooterCharacter.cpp>)、[ShooterPlayerController.cpp](<Source/FPS/Private/ShooterPlayerController.cpp>) |
| 移动模式 | 静步 / 冲刺均为切换式；速度倍率相乘（静步 / 下蹲 / 开镜 / 趴下）；开镜、下蹲、趴下或**开始滑铲**都会打断冲刺；耐力耗尽自动退出冲刺；**开火也会打断冲刺**（取消用的这一下按键不会开火） | [ShooterPlayerController.cpp](<Source/FPS/Private/ShooterPlayerController.cpp>)、[CombatComponent.cpp](<Source/FPS/Private/CombatComponent.cpp>) |
| 蹲伏模式 | **按住左 Ctrl** 蹲伏（松开起身，遇到天花板会延后），**C** 键切换；两者都是优先级 2 的运行时 Enhanced Input 动作，因此旧的 `IA_Crouch` 映射会被屏蔽 | [ShooterPlayerController.cpp](<Source/FPS/Private/ShooterPlayerController.cpp>) |
| 滑铲 | 速度高于 `SlideMinSpeed`（600）时按蹲伏键会滑铲而不是下蹲：下蹲胶囊、`+150` cm/s 加速、摩擦力 0 并改用线性 `BrakingDecelerationWalking`、忽略移动输入，0.9 秒 / 低于 200 cm/s / 离地时结束；结束时保持蹲伏（按了 C 或仍按住 Ctrl）或起身（已松开 Ctrl）；跳跃会取消滑铲；服务器按 75 % 速度校验，不通过则回滚 `Client_SlideRejected` | [ShooterCharacter.cpp](<Source/FPS/Private/ShooterCharacter.cpp>)、[ShooterPlayerController.cpp](<Source/FPS/Private/ShooterPlayerController.cpp>) |
| 趴下 / 趴行 | **Z** 键切换趴下（字段为空时运行时创建输入）。趴下复用 CMC 蹲伏并把 `CrouchedHalfHeight` 设为 34，压低相机，以 `RunSpeed × 0.25` 爬行，禁止跳跃 / 探头 / 冲刺，头顶空间不足时拒绝起身，服务器权威并用 `Client_ProneRejected` 回滚。`bProneCrawling`（趴下**且**在移动）让 ABP 移动时播放全身爬行、停下时播放端枪的上半身 | [ShooterCharacter.cpp](<Source/FPS/Private/ShooterCharacter.cpp>)、[ShooterPlayerController.cpp](<Source/FPS/Private/ShooterPlayerController.cpp>) |
| 耐力 | 服务器权威的消耗/回复、跳跃消耗、`OnStaminaDepleted` 广播、耐力条 HUD | [StatsComponent.cpp](<Source/FPS/Private/Stats/StatsComponent.cpp>) |
| 移动手感 | `GroundSpeed`、慢走步态 BlendSpace 替换（`Strafe_Walking`）、每帧把 Locomotion 播放速率写进 AnimBP（趴下用 `ProneAnimReferenceSpeed` 100） | [ShooterCharacter.cpp](<Source/FPS/Private/ShooterCharacter.cpp>) |
| 武器 | 长枪 / 手枪 / 近战三类；主 / 副 / 手枪 / 近战四个槽位，用数字键选择（循环切枪已删除）；切枪 Montage（可配随机变体池）；拔枪/换弹可被打断（见 §10.2）；切枪自动关镜 | [Weapon.cpp](<Source/FPS/Private/Weapon/Weapon.cpp>)、[CombatComponent.cpp](<Source/FPS/Private/CombatComponent.cpp>) |
| 近战 | `EWeaponKind::Melee` 不能开镜、不消耗弹药（`UsesAmmo()`）、不会自动换弹，两个 FABRIK alpha 都强制为 0；命中判定改为**沿刀刃的球形扫掠**（`MeleeSocketName`、`MeleeHitRadius` 40、`MeleeHitReach` 120），并播放 3P 全身挥砍 Montage | [Weapon.cpp](<Source/FPS/Private/Weapon/Weapon.cpp>)、[CombatComponent.cpp](<Source/FPS/Private/CombatComponent.cpp>) |
| 射击 | 球形 Sweep 命中判定，带**腰射散布锥**（由 `HipFireSpread` × 配件倍率随机，开镜时为 0）、单发 / 全自动（每把枪可选**切换开火模式**：`bCanSwitchFireMode`，`FireType` 已同步）、拉栓（`BoltCycling` 状态）、**后坐力弹道 + 回正 + 配件修正接口**、服务端射速节流、蓝图枪口/命中特效、弹匣与备弹 | [Weapon.cpp](<Source/FPS/Private/Weapon/Weapon.cpp>)、[CombatComponent.cpp](<Source/FPS/Private/CombatComponent.cpp>)、[RecoilModifierInterface.h](<Source/FPS/Public/Interface/RecoilModifierInterface.h>) |
| 换弹 | 1P/3P 换弹 Montage，**分种类**（首次 / 空仓 / 战术，没配就回落到默认 Montage），换弹和拉栓时左手 IK 渐隐、动画通知填弹、空仓自动换弹、C++ 动画通知把枪重新握到手上 | [CombatComponent.cpp](<Source/FPS/Private/CombatComponent.cpp>)、[AnimNotify_WeaponGrip.cpp](<Source/FPS/Private/AnimNotify/AnimNotify_WeaponGrip.cpp>) |
| 治疗 | 治疗 Montage（播放期间隐藏武器）、服务端钳制治疗量、多播给其他玩家播 3P 动画 | [ShooterCharacter.cpp](<Source/FPS/Private/ShooterCharacter.cpp>) |
| 瞄准 ADS | `bAiming` 复制（跳过 Owner）、`OnAimStateChanged` 广播、C++ 驱动开镜：`AimAlpha` 按武器开镜时间推进（配件可乘倍率），Mesh1P 从**每把枪的腰射偏移**插值到瞄具对准位置（**眼距**可配），相机 FOV 在 C++ 里插值（瞄具 `AimFOV` → 枪 `AimFeildOfView` → 默认）；可选枪身视觉后坐（默认关）。见 §10.1 | [ShooterCharacter.cpp](<Source/FPS/Private/ShooterCharacter.cpp>)、[CombatComponent.cpp](<Source/FPS/Private/CombatComponent.cpp>) |
| 狙击镜 | **旧方案，已停用。** `USniperScopeComponent` 只会让自己保持关闭；狙击枪和其他枪一样按装的瞄具走（见 §10.1） | [SniperScopeComponent.cpp](<Source/FPS/Private/Weapon/SniperScopeComponent.cpp>) |
| 伤害 | 骨骼名 → 命中部位（头 / 躯干 / 四肢）→ 倍率（爆头 ×2、四肢 ×0.75），服务端结算 | [ShooterType.cpp](<Source/FPS/Private/ShooterType/ShooterType.cpp>)、[StatsComponent.cpp](<Source/FPS/Private/Stats/StatsComponent.cpp>) |
| 探头 | Q / E 左右探头（按住，可选切换模式），同步 `LeanTarget` + 平滑 `LeanAlpha`，1P 相机横移 + 侧倾并防穿墙，冲刺时禁止，3P 在 `ABP_ThirdPerson` 里转脊椎；射线跟随探头后的镜头 | [ShooterCharacter.cpp](<Source/FPS/Private/ShooterCharacter.cpp>)、[Weapon.cpp](<Source/FPS/Private/Weapon/Weapon.cpp>) |
| 动画驱动 | `AO_Yaw`、`MovementOffsetYaw`、`TurningStatus`、`FABRIK_SocketTransform`、`FABRIK_Alpha`、`FPlayerAnim` 结构体注入 ABP，以及趴下 / 滑铲 / 握把持姿相关值 | [ShooterCharacter.cpp](<Source/FPS/Private/ShooterCharacter.cpp>)、[WeaponData.h](<Source/FPS/Public/WeaponData.h>) |
| 受击反馈 | **分方向**受击动画（前 / 后 / 左 / 右四组，空的回落到 `HitReacts`），NetMulticast 广播；被打的人看到受伤方向环，开枪的人看到命中确认标记 | [ShooterCharacter.cpp](<Source/FPS/Private/ShooterCharacter.cpp>) |
| UI | 动态准星材质（跟随武器）、**由 `OnWeaponFired` 驱动的原生散布准星**、弹药/备弹 + 武器图标 HUD、血条、耐力条、瞄准时隐藏准星（近战除外），`UShooterReticle` 绘制**受伤方向环**和**命中确认标记** | [ShooterReticle.cpp](<Source/FPS/Private/Ui/ShooterReticle.cpp>)、[ReserveAmmo.cpp](<Source/FPS/Private/Ui/ReserveAmmo.cpp>) |
| 武器配件 | 每个槽位一份数据资产的配件、服务器校验白名单、每把枪的默认配件、网络同步、1P/3P 实时生成模型（**静态或骨骼网格体**），并重定向握把 / 枪口 / 瞄准插槽 | [Weapon.cpp](<Source/FPS/Private/Weapon/Weapon.cpp>)、[AttachmentData.h](<Source/FPS/Public/Weapon/AttachmentData.h>) |
| 握把持姿 | 每份握把配件可选 Cradle（斜角 / 无握把）或 Grab（垂直握把）：会替换待机 / 瞄准待机动画、增加 Grab 换弹变体，并暴露左手握持姿态给 ABP 做分层混合 | [AttachmentData.h](<Source/FPS/Public/Weapon/AttachmentData.h>)、[FirstPersonAnimInstance.cpp](<Source/FPS/Private/Animation/FirstPersonAnimInstance.cpp>) |
| 弹匣配件 | 弹匣配件可以是**骨骼网格体**，自带换弹动画并与手臂换弹同步播放（含空仓 / 战术变体），还可选择隐藏骨骼（`mag_02` 备用弹匣） | [Weapon.cpp](<Source/FPS/Private/Weapon/Weapon.cpp>)、[AttachmentData.h](<Source/FPS/Public/Weapon/AttachmentData.h>) |
| 配件界面 | 游戏内配件菜单（一个槽位一行）、Tab 整备界面（三把枪预览渲染 + 可拖动配件窗） | [AttachmentMenu.cpp](<Source/FPS/Private/Ui/AttachmentMenu.cpp>)、[LoadoutScreen.cpp](<Source/FPS/Private/Ui/LoadoutScreen.cpp>) |
| 瞄具与手电 | 红点 / 全息对准瞄具自己的 Socket、只改相机 FOV；只有能用的 `Magnified` / `Variable` 瞄具才做画中画放大（可变倍率可切档），手电是按配件数据生成的聚光灯 | [OpticScopeComponent.cpp](<Source/FPS/Private/Weapon/OpticScopeComponent.cpp>) |
| 第一人称扶枪 IK | 第一人称 FABRIK **有意停用**（`FABRIK_Alpha1P` 恒为 0）：持枪姿态来自每把枪的持枪动画与 Grab/Cradle 持姿。`UFirstPersonAnimInstance` 向 ABP 暴露 `GripHoldStyle`、`ActiveIdleAnim`、`LeftHandHoldPose` 与 `bUseLeftHandHoldPose` | [FirstPersonAnimInstance.cpp](<Source/FPS/Private/Animation/FirstPersonAnimInstance.cpp>) |
| 网络 | 服务器权威：生成、装备、配件、伤害、耐力、治疗、**趴下与滑铲**；客户端预测弹匣与后坐力；NetMulticast 播特效与第三人称动画；`Client_ProneRejected` / `Client_SlideRejected` 回滚 | [CombatComponent.cpp](<Source/FPS/Private/CombatComponent.cpp>)、[ShooterCharacter.cpp](<Source/FPS/Private/ShooterCharacter.cpp>) |

---

## 3. 目录结构

```
FPS/
├─ FPS.uproject                     # 引擎版本、模块、插件
├─ Config/
│  ├─ DefaultEngine.ini             # 碰撞通道（Weapon/Projectile）、默认关卡与 GameMode
│  ├─ DefaultInput.ini              # EnhancedInput 默认类、鼠标灵敏度、按键轴
│  ├─ DefaultGame.ini               # 项目名、AssetManager 扫描规则
│  └─ DefaultGameplayTags.ini       # 唯一在配置里注册的 Tag：Weapon.type.ak
├─ Content/
│  ├─ FPS/                          # 本项目自己的资产（见第 6 节）
│  └─ ...                           # 第三方素材包（FPSTP、InfimaGames、IBWP3DPistol3Pack 等）
└─ Source/
   ├─ FPS.Target.cs                 # Game 目标
   ├─ FPSEditor.Target.cs           # Editor 目标
   └─ FPS/
      ├─ FPS.Build.cs              # 模块依赖
      ├─ FPS.h / FPS.cpp           # 主游戏模块声明
      ├─ Public/
      │  ├─ FPSTraceChannels.h            # 武器 Trace 通道常量
      │  ├─ ShooterGamePlayTags.h         # 原生 GameplayTag 声明
      │  ├─ ShooterCharacter.h            # 玩家角色
      │  ├─ ShooterPlayerController.h     # 玩家控制器（移动/视角/跳跃/蹲伏/冲刺/静步）
      │  ├─ ShooterGameModeBase.h         # GameMode（空壳，供蓝图继承）
      │  ├─ CombatComponent.h             # 战斗核心组件（库存/开火/换弹/瞄准/后坐力）
      │  ├─ WeaponData.h                  # 武器数据资产 + FPlayerAnim / FMontageData
      │  ├─ AnimNotify/
      │  │  └─ AnimNotify_WeaponGrip.h    # C++ 动画通知：把武器重新挂到手上
      │  ├─ Interface/
      │  │  ├─ PlayerInterface.h          # 角色能力接口
      │  │  └─ RecoilModifierInterface.h  # IRecoilModifier + FRecoilScale（配件后坐力修正）
      │  ├─ Animation/FirstPersonAnimInstance.h   # ABP_FirstPerson 的 C++ 基类：1P 扶枪 IK + 握把持姿
      │  ├─ Ui/AttachmentMenu.h                   # 配件菜单（一个槽位一行）
      │  ├─ Ui/AttachmentSlotRow.h                # 槽位行 + 可选配件按钮
      │  ├─ Ui/AttachmentOptionEntry.h            # 单个配件按钮 / "卸下"按钮
      │  ├─ Ui/LoadoutScreen.h                    # Tab 整备界面 + 可拖动配件窗
      │  ├─ Ui/LoadoutPreviewRig.h                # 三把枪预览用的 SceneCapture 装置
      │  ├─ Ui/LoadoutWeaponCard.h                # 整备界面里的单张武器卡片
      │  ├─ Weapon/AttachmentData.h               # 配件数据资产（+ EOpticType、EGripHoldStyle）
      │  ├─ Weapon/OpticScopeComponent.h          # 画中画倍镜
      │  ├─ Stats/StatsComponent.h        # 生命值 + 耐力组件
      │  ├─ ShooterType/ShooterType.h     # 部位判定 / 伤害倍率工具函数
      │  ├─ Ui/ShooterReticle.h           # 动态准星控件
      │  ├─ Ui/ReserveAmmo.h              # 弹药 HUD 控件
      │  └─ Weapon/
      │     ├─ Weapon.h                   # 武器 Actor（+ FRecoilStep）
      │     └─ SniperScopeComponent.h     # 狙击镜组件 + 全屏遮罩控件
      └─ Private/                        # 与 Public 一一对应的实现文件
```

---

## 4. 快速开始

1. **打开工程**：用 UE 5.8 打开 [FPS.uproject](<FPS.uproject>)，首次打开时选择 "Rebuild" 或先生成工程文件再编译 `FPS` 模块。
2. **默认关卡**：`/Game/FirstPerson/Lvl_FirstPerson`（[DefaultEngine.ini](<Config/DefaultEngine.ini>) 已设为 `GameDefaultMap`）。
3. **GameMode**：全局默认为 `AShooterGameModeBase`；实际使用请继承为 `BP_ShooterGameMode`（[BP_ShooterGameMode.uasset](<Content/FPS/Game/BP_ShooterGameMode.uasset>)），并在其中指定：
   - `DefaultPawnClass = BP_ShooterCharacter`
   - `PlayerControllerClass = BP_ShooterPlayerController`
4. **角色必需配置**（`BP_ShooterCharacter`，代码里有 `checkf` 强校验）：
   - `Combat → WeaponData` 必须指向 `DA_WeaponData`，否则 `GetWeaponAttachPoint` 会触发 `checkf` 断言。
   - `Combat → DefaultWeaponClass` 至少填一把武器蓝图。
   - 输入字段（`IA_FireWeapon`、`IA_ReloadWeapon`、`IA_AimWeapon`、`IA_Heal` 以及四个 `IA_Equip*`）都要赋值。
   - `HealMontage` 必须赋值，否则按治疗键只会打一条 Warning 且没有任何效果；`HealAmout` 默认 30。
   - 蓝图变量 `AdsMesh1PLoc`（`FVector`）必须存在 —— ADS 瞄具对齐会直接写这个变量。
   - `HitReacts` 至少填一个受击 Montage，否则 `Multicast_HitReact` 的随机下标会越界（当前代码未做空数组保护）。
5. **玩家控制器**：`BP_ShooterPlayerController` 需要设置 `ShooterIMC = IMC_Shooter`，以及 `IA_Move` / `IA_Look` / `IA_Jump` / `IA_Crouch` / `IA_Sprint` / `IA_Walk`。速度与倍率（`WalkSpeed` 250、`RunSpeed` 500、`SprintSpeed` 800、`CrouchSpeedMultiplier` 0.6、`WalkSpeedMultiplier` 0.5、`AimSpeedMultiplier` 0.7）配置在控制器上。
6. **输入资产**：在 `IMC_Shooter` 里绑定 `IA_*` 与按键。`IA_FireWeapon` 建议能同时触发 `Started` 与 `Completed`（单发/全自动靠这两个事件区分）；`IA_Sprint`、`IA_Walk`、`IA_Heal` 都绑在 `Started`。
7. **武器蓝图**：基于 `BP_Weapon`（`AWeapon`）派生，设置 `WeaponType`（GameplayTag）、`WeaponKind`、`Mesh1P` / `Mesh3P` 的骨骼网格体与动画蓝图（`ABP_Rifle`、`ABP_Pistol`、`ABP_M700`），调好后坐力参数（`RecoilPattern`、`RecoilPitchScale`、`RecoilYawScale`、`AimRecoilMultiplier`、`RecoilRecoverySpeed`、`RecoilRecoveryDelay`、`RecoilResetTime`），并在 `FireEffect` 蓝图事件里做枪口火焰 / 弹孔 / 音效。
8. **数据资产**：在 `DA_WeaponData` 里按武器 Tag 填写 `GripPoints`（挂点 Socket 名）、`WeaponMontages`、`FirstPersonAnims` / `ThirdPersonAnims`（有慢走混合的话还要填 `Strafe_Walking`）、`FirstPersonMontages` / `ThirdPersonMontages`。
9. **动画蓝图**必须暴露 C++ 按名字写入的变量：3P ABP 建议有 `bUseWalkGait`、`LocomotionPlayRate`（或 `PlayRate`）、`GroundSpeed`；1P ABP 至少要有 `PlayRate`。变量名对不上时不会有任何提示，只是静默失效。
10. **武器握持通知**：在换弹/开火 Montage 里"副手抓住枪"的那一帧放一个 **Weapon Grip** 通知（`UAnimNotify_WeaponGrip`），并把 `GripSocket` 设为手部 Socket 或骨骼名。本工程的狙击枪换弹 Montage 已经用了它。
11. **HUD**：把 `WBP_ShooterOverlay` 加入视口（内部含 `WBP_ReserveAmmo` 与 `WB_Stamina`），`WB_Health` 显示血量。准星控件基于 `UShooterReticle`，其内部 Image 命名为 `Image_Reticle`（代码也兼容拼写错误的 `Image_Redicle`）。
12. **狙击镜材质**：`/Game/FPS/UI/M_SniperScope` 是硬编码路径（见 [SniperScopeComponent.cpp](<Source/FPS/Private/Weapon/SniperScopeComponent.cpp>) 中的 `SniperScopeMaterialPath`），改名或移动会导致镜筒不显示并打印 Warning。材质参数需包含 `ScopeView`(Texture)、`MaskRadius`、`RingWidth`、`ReticleThickness`、`Aspect`。

---

### 4.1 移动模式：蹲伏输入、滑铲与趴行

三个按键都是 C++ 里**运行时创建的 Enhanced Input 动作**（对应的 `UInputAction` 字段为空时创建），开箱即用：

| 模式 | 按键 | 运行时资产 | 想用自己的资产就填 |
| --- | --- | --- | --- |
| 蹲伏（按住） | **左 Ctrl** | `IMC_Crouch_Runtime`（优先级 2）里的 `IA_CrouchHold_Runtime` | `BP_ShooterPlayerController → CrouchHoldAction` |
| 蹲伏（切换） | **C** | `IA_CrouchToggle_Runtime` | `BP_ShooterPlayerController → CrouchToggleAction` |
| 趴下 | **Z** | `IMC_Prone_Runtime`（优先级 1）里的 `IA_Prone_Runtime` | `BP_ShooterCharacter → ProneAction` |

1. **旧输入**：老的 `IA_Crouch`（`CrouchAction`）**只有**在两个新蹲伏动作都没赋值时才会绑定。因为运行时 IMC 优先级是 2，`IMC_Shooter` 里残留的 `LeftControl → IA_Crouch`（或 `IA_Walk`）那一行会被屏蔽 —— 为了清晰请删掉该行（或删掉 `IA_Crouch`），如果静步原本在左 Ctrl 上就改绑到别的键。
2. **趴下动画接线**（手动，C++ 只负责把值送出来）：创建/找到 `BS_Prone`（本工程已放在 `/Game/FPS/Animation/Prone/BS_Prone`，3P 骨架上 14 个采样点），在 `ABP_ThirdPerson` 的 Locomotion 状态机里加一个 **Prone** 状态（X = `MovementOffsetYaw`，Y = `GroundSpeed`，PlayRate = `LocomotionPlayRate`），并在里面用 **`bProneCrawling`** 在 `BS_Prone` 与趴下待机之间混合。状态机之后再接一个按骨骼分层混合（`clavicle_l` / `clavicle_r`，深度 0，勾选网格体空间旋转混合），权重用 `bIsProne && !bProneCrawling`，这样趴下停住时手臂仍端着枪。
3. **滑铲动画接线**（手动）：在角色骨架上创建动画槽 **`BodySlot`**（`Anim Slot Manager → DefaultGroup`），然后在 Locomotion/瞄准姿态之后接 `Slot(BodySlot)` → 按骨骼分层混合（基础姿势 = 槽输出，混合姿势 = 缓存下来的 Locomotion 姿态，分支过滤 `clavicle_l` / `clavicle_r`，勾选网格体空间旋转混合）→ 原有的 FABRIK → `DefaultSlot` → Output。**没有 `BodySlot` 时滑铲动画会静默失效。** `SlideAnim` 默认已经是 `/Game/FreeAnimationLibrary/Animations/Slide/anim_Slide_L`；`SlideMontage1P` 请保持为空，这样手臂仍然保持持枪。
4. **FABRIK**：把 3P FABRIK 节点的 alpha 绑到 **`FABRIK_Alpha`**（不要写常量 1）。近战和爬行时为 0，其他情况为 1。1P FABRIK 是有意留空的（`FABRIK_Alpha1P` 恒为 0）。
5. **调参**：`BP_ShooterCharacter → ProneCapsuleHalfHeight`（34）、`ProneCameraZOffset`（−50）、`ProneAnimReferenceSpeed`（100）、`bHideWeapon3PWhileProne`、`SlideMinSpeed`（600）、`SlideDuration`（0.9）、`SlideSpeedBoost`（150）、`SlideDeceleration`（800）、`SlideEndSpeed`（200）、`SlideCameraZOffset`（−15）；`BP_ShooterPlayerController → ProneSpeedMultiplier`（0.25）。

### 4.2 武器配件与整备界面配置

1. **标签** —— 配件槽位类型是 GameplayTag，声明在 [ShooterGamePlayTags.h](<Source/FPS/Public/ShooterGamePlayTags.h>)：`Attachment.Slot.Barrel`、`Handguard`、`Muzzle`、`Optic`、`Grip`、`Stock`、`Tactical`、`Magazine`。新增类型只需要加 Tag。
2. **配件资产** —— 每个配件建一份 `UAttachmentData` 数据资产（右键 → Miscellaneous → Data Asset → AttachmentData）。工程自带的在 `Content/FPS/Data/Accessries/`（`DA_Barris`、`DA_HangGuard`、`DA_Grip`、`DA_Grip_Vertical`、`DA_Sight`、`DA_Sight1`、`DA_Sight2`、`DA_MAG45`、`DA_Back`），以及其 `MPC/` 子目录（`DA_MPC_AK_Mag`、`DA_MPC_VerticalGrip`、`DA_MPC_AK_Stock`、`DA_MPC_MAG`、`DA_MPC_RedDot`、`DA_MPC_Suppressor`、`DA_MPC_Flashlight`）。填 `SlotTag`、`Mesh`（或 `SkeletalMesh`，见第 7 点）、`MeshOffset`，以及可选的 后坐力倍率 / 握把持姿 / 瞄具块 / 手电块。位置相关的 Socket（`FABRIK`、`Muzzle`、`SightSocket`、`LensSocket`、`LightSocket`）都做在**配件自己的网格体**上（静态或骨骼）。。
3. **枪的槽位表** —— 在武器蓝图（`BP_RIfle` 已经配好）的 `FPS|Attachment → AttachmentSlots` 里，一个槽位一行：`SlotTag`、`SocketName`、`AllowedAttachments` 白名单、可选的 `DefaultAttachment` 和 `bRequired`。槽位 Socket 会先在枪骨骼上找，找不到再在"已经装上的配件"上找 —— 握把装到护木上就是靠这个。
4. **菜单与整备** —— 在 `BP_ShooterPlayerController` 上设置 `AttachmentMenuClass = WBP_AttachmentMenu`，并赋好 `IA_AttachmentMenu`（在 `IMC_Shooter` 中绑定）与 `IA_Flashlight`。`WBP_AttachmentMenu` 里必须有名为 `SlotList` 的 `VerticalBox`，并设置 `RowClass = WBP_AttachmentSlotRow`；行控件需要 `Text_SlotName` + `EntryContainer`，并设置 `EntryClass = WBP_AttachmentEntry`；按钮控件需要 `Button_Select` + `Text_Name`（`Image_Icon` 可选）。
5. **整备界面** —— Tab 界面和它的配件窗完全由 C++ 构建、按需创建，因此不需要 WBP；只有槽位行是按硬编码路径加载 `/Game/FPS/UI/WBP_AttachmentSlotRow`。
6. **握把持姿** —— 在每份**握把**配件上设置 `GripHoldStyle`：斜角/三角握把用 `Cradle`，垂直握把用 `Grab`（本工程 `DA_Grip` = Cradle，`DA_Grip_Vertical` / `DA_MPC_VerticalGrip` = Grab）。然后在 `DA_WeaponData` 里按武器 Tag 补上 Grab 用的动画：`IdleAnim_Grab`、`AimIdleAnim_Grab`、`LeftHandHoldPose_Grab`（以及 `_Cradle`），并在 Montage 表里补 `EmptyReloadMontage_Grab` / `TacticalReloadMontage_Grab`。⚠ `IdleAnim_Grab` 必须**不同于** `LeftHandHoldPose_Grab`：代码会忽略与左手姿态相同的 `IdleAnim_Grab`，因为这种姿态（例如 `A_FPP_AK_Pose_G2`）只是"只有左手"的一层，当作完整待机用会把枪拧歪。在 `ABP_FirstPerson` 里用按骨骼分层混合接入 `LeftHandHoldPose`，分支从 `upperarm_l` 开始（深度 0），混合权重用 `bUseLeftHandHoldPose`。
7. **骨骼弹匣** —— 需要播动画的弹匣（AK）请设置配件的 `SkeletalMesh` 而不是 `Mesh`，并填 `MagReloadAnim` / `MagEmptyReloadAnim` / `MagTacticalReloadAnim`。`MagReloadAnim` 的第 0 帧同时是组件停放的静止姿态。可选勾选 `bHideSpareMagazineBone`（隐藏 AK 的 `mag_02` 备用弹匣），或在 `BonesToHide` 里列出额外要隐藏的骨骼。
8. **近战（刀）** —— 设置 `WeaponKind = Melee`，在刀的网格体上加 `Melee` Socket（或改 `MeleeSocketName`），调 `MeleeHitRadius` / `MeleeHitReach`，填 `FireMontages`（随机挥砍变体），并确保 3P 挥砍 Montage 走的是 `DefaultSlot`/`FullBody` —— 近战挥砍是**全身**动画。近战不需要弹药、不能开镜、永不换弹。
9. **腰射散布** —— 在每把武器蓝图上设置 `HipFireSpread`（角度制，半锥角，默认 3，范围 0–30）：只要**没有**开镜，子弹方向就会在该锥体内随机（开镜时强制为 0）。配件通过 `HipFireSpreadMultiplier` 乘算（默认 1；例如垂直握把 0.8，等于把锥体收紧 20 %）。原生准星会自动把同一个角度换算成像素，因此准星与散布锥一致，每次开火的 `CrosshairSpread` 抖动叠加在它上面。⚠ 散布锥是在做射线的客户端上随机出来的，服务器直接信任由此得到的 `FHitResult` —— 这是手感 / 平衡特性，不是精度惩罚。

新增操作：

| 动作 | 输入资产 | 绑定对象 | 触发事件 | 说明 |
| --- | --- | --- | --- | --- |
| 配件菜单 | `IA_AttachmentMenu` | 控制器 → `ToggleAttachmentMenuAction` | `Started` | 开关按槽位排列的配件菜单；显示鼠标并屏蔽转视角 |
| 手电 | `IA_Flashlight` | 控制器 → `FlashlightAction` | `Started` | 通过战斗组件请求开关手电（服务器权威） |
| 整备界面 | 运行时创建的 `IA_Loadout_Runtime`（Tab） | 控制器，运行时注册 | `Started` | 打开全屏整备界面；输入动作与映射上下文都是 C++ 运行时创建，**不在** `IMC_Shooter` 里 |
| 蹲伏（按住） | 运行时创建的 `IA_CrouchHold_Runtime`（左 Ctrl） | 控制器，运行时注册 | `Started` / `Completed` | 按住蹲伏，松开起身（头顶有障碍会延后）。速度够快时改为滑铲 |
| 蹲伏（切换） | 运行时创建的 `IA_CrouchToggle_Runtime`（C） | 控制器，运行时注册 | `Started` | 切换蹲 / 站；趴下时退出到蹲伏；滑铲中忽略 |
| 趴下 | 运行时创建的 `IA_Prone_Runtime`（Z） | 角色，运行时注册 | `Started` | 切换趴下 / 趴行；进入要求着地且不在治疗中，会取消冲刺，滑铲中不允许；趴下时禁止冲刺 / 跳跃 / 探头；Z 或空格起身（头顶没空间会被拒绝），C 转为蹲伏 |

## 5. 操作说明

输入由 Enhanced Input 分发：移动与移动模式在 `AShooterPlayerController`，战斗动作在 `AShooterCharacter`。

| 动作 | 输入资产 | 绑定对象 | 触发事件 | 说明 |
| --- | --- | --- | --- | --- |
| 移动 | `IA_Move` | 控制器 | `Triggered` | 以控制旋转的 Yaw 为基准前后左右移动 |
| 视角 | `IA_Look` | 控制器 | `Triggered` | `AddYawInput` / `AddPitchInput` |
| 跳跃 | `IA_Jump` | 控制器 | `Started` | 蹲伏中先起身；滑铲中会取消滑铲并起身；趴下中站起来（不跳）；起跳会在服务器扣耐力 |
| 蹲伏（按住） | 运行时创建的 `IA_CrouchHold_Runtime`（左 Ctrl） | 控制器，运行时注册 | `Started` / `Completed` | 按住蹲伏，松开起身（头顶有障碍会延后）。趴下时按 Ctrl 会转成蹲伏；滑铲中松开则结束时起身 |
| 蹲伏（切换） | 运行时创建的 `IA_CrouchToggle_Runtime`（C） | 控制器，运行时注册 | `Started` | 切换 `bWantsToCrouch`；趴下时退出到**蹲伏**；滑铲中忽略 |
| 滑铲 | 任一蹲伏键 | 控制器 / 角色 | — | 地面速度高于 `SlideMinSpeed`（600）时改为滑铲而不是下蹲 |
| 趴下 | 运行时创建的 `IA_Prone_Runtime`（Z） | 角色，运行时注册 | `Started` | 切换趴下 / 趴行；趴下时禁止冲刺、跳跃和探头；Z 或空格起身，C 转为蹲伏 |
| 冲刺 | `IA_Sprint` | 控制器 | `Started` | 切换冲刺；被蹲伏、趴下、开镜、滑铲、开火，或耐力归零打断 |
| 静步 | `IA_Walk` | 控制器 | `Started` | 切换慢走（`WalkSpeedMultiplier`）；会打断冲刺 |
| 开火 | `IA_FireWeapon` | 角色 | `Started` / `Completed` | 按下开火，松开停止（决定全自动与后坐力回正）。**冲刺中按下开火只会取消冲刺，不会开火** —— 要再按一次才射击 |
| 瞄准 | `IA_AimWeapon` | 角色 | `Started` | 切换 ADS；通过 `OnAimStateChanged` 广播（蓝图 `OnAim`、速度、步态）。近战武器永远不能开镜 |
| 换弹 | `IA_ReloadWeapon` | 角色 | `Started` | 播放换弹 Montage，由动画通知填弹。近战忽略（没有弹药） |
| 治疗 | `IA_Heal` | 角色 | `Started` | 播放 `HealMontage`，期间隐藏武器；服务端钳制并结算治疗量 |
| 左 / 右探头 | `LeanLeftAction` / `LeanRightAction`（留空 → 运行时创建 `IA_LeanLeft_Runtime` / `IA_LeanRight_Runtime`，绑 **Q / E**） | 角色 | `Started` / `Completed` | 按住探头（`bLeanToggleMode` = 按一下切换）；冲刺移动中禁止 |
| 检视武器 | `IA_VersionWeapon` → `VersionWeaponAction` | 角色 | `Started` | 播放武器的 `InspectMontage1P`（只在本地 1P）；开火、开镜、换弹、切枪、冲刺会打断 |
| 主武器 | `IA_EquipMainWeapon` | 角色 | `Started` | 切到第一把长枪，并自动关镜 |
| 副武器 | `IA_EquipSecondaryWeapon` | 角色 | `Started` | 切到第二把长枪，并自动关镜 |
| 手枪 | `IA_EquipPistol` | 角色 | `Started` | 切到第一把手枪，并自动关镜 |
| 近战 | `IA_EquipMeleeWeapon` | 角色 | `Started` | 切到第一把近战武器，并自动关镜 |
| 切换开火模式 | 自建 IA → `SwitchFireModeAction` | 角色 | `Started` | 对 `bCanSwitchFireMode` 的枪切单发 / 全自动；换弹、拉栓时不能切；服务器 RPC + `OnFireModeChanged` |
| 切换倍率 | 自建 IA → `SwitchZoomAction` | 角色 | `Started` | 开镜且装了 `Variable` 瞄具时，在 `ZoomLevelsFOV` 几档之间切换；`OnZoomLevelChanged` 给界面用 |

> 说明：`IMC_Shooter` 中存在 `LeftMouseButton`、`RightMouseButton`、`SpaceBar`、`LeftControl`、`LeftShift`、`LeftAlt`、`One`~`Four`、`R`、`Q`、`E` 及手柄扳机等按键；上表"哪个键对应哪个动作"是常见配置，**最终以编辑器内 `IMC_Shooter` 的实际绑定为准**。

> 循环切枪已**删除**（C++ 绑定、`Input_CycleWeapon`、`Initiate_CycleWeapon`、`CycleWeaponAction`）。如果 `IMC_Shooter` 里还有 `IA_CycleWeapon` 映射（以及该资产），可以删掉。切枪只用槽位键。

---

## 6. 关键蓝图与资产（`/Game/FPS`）

| 资产 | 作用 |
| --- | --- |
| `Character/BP_ShooterCharacter` | 玩家角色蓝图（挂点、FOV、武器类、ADS 变量 `AdsMesh1PLoc`、`HealMontage` / `HealAmout`、`HitReacts`） |
| `Character/ABP_FirstPerson` / `ABP_ThirdPerson` | 1P / 3P 动画蓝图；读取 `AO_Yaw`、`MovementOffsetYaw`、`TurningStatus`、`FABRIK_SocketTransform`、`CurrenrAnim` / `CurrentAnim`，以及 3P 的 `bUseWalkGait` / `LocomotionPlayRate` / `GroundSpeed` |
| `Player/BP_ShooterPlayerController` | 输入映射、移动/视角绑定、冲刺/静步动作、速度与倍率配置 |
| `Game/BP_ShooterGameMode` | GameMode，指定默认 Pawn 与 Controller |
| `Data/DA_WeaponData` | 唯一的武器数据资产（挂点、Montage、动画结构体，含 `Strafe_Walking`） |
| `Weapon/BP_Weapon`、`BP_RIfle`、`BP_Refile`、`BP_Pistol`、`BP_M700`、`BP_Weapon_AK`、`BP_Knife` | 武器 Actor 蓝图；`BP_RIfle` 覆盖了 `RecoilPattern`，`BP_Weapon_AK` 用 `Weapon.type.ak` 并在 3P 网格体上用 `RifleSocketak` |
| `Animation/Prone/BS_Prone` | 趴行 BlendSpace（14 个采样点，3P 骨架），由 `ABP_ThirdPerson` 的 Prone 状态引用 |
| `FreeAnimationLibrary/Animations/Slide/anim_Slide_L` | 默认的滑铲身体动画（`AShooterCharacter::SlideAnim`），以动态 Montage 播在 `BodySlot` 槽里 |
| `Data/Accessries/MPC/DA_MPC_AK_Mag`、`SKM_AK_Mag` | 骨骼版 AK 弹匣配件（换弹动画 + `mag_02` 备用弹匣骨骼开关） |
| `Weapon/ABP_Rifle`、`ABP_Pistol`、`ABP_M700` | 武器骨骼网格体的动画蓝图 |
| `UI/WBP_ShooterOverlay`、`UI/WBP_ReserveAmmo`、`UI/WB_Health`、`UI/WB_Stamina` | HUD 根控件、弹药控件、血量控件、耐力控件 |
| `UI/M_SniperScope` | 狙击镜遮罩材质（代码硬编码引用） |
| `Input/IMC_Shooter`、`Input/InputAction/IA_*` | 输入映射上下文与全部输入动作（含 `IA_Sprint`、`IA_Walk`、`IA_Heal`）。**蹲伏（Ctrl / C）、趴下（Z）和整备界面（Tab）都是 C++ 运行时创建的动作**，不是资产 |
| `Animation/AnimMontage_Arms_Healing` | 治疗 Montage（赋给 `HealMontage`） |
| `Animation/AnimMontage_Arms_SniperRiffle_*` | 使用 **Weapon Grip** 通知的狙击枪换弹 / 退壳 Montage |

---

## 7. 系统架构（速览）

```mermaid
graph TD
    PC["AShooterPlayerController<br/>移动 / 视角 / 跳跃 / 蹲伏（Ctrl 按住，C 切换） / 冲刺 / 静步<br/>滑铲 + 趴行速度模型 + 耐力 RPC"] -->|输入| CH["AShooterCharacter<br/>IPlayerInterface 实现<br/>趴下 / 滑铲 / 探头（已同步）"]
    PC -->|"RefreshMovementSpeed"| CH
    CH -->|"输入转发"| CC["UCombatComponent (Replicated)<br/>装备 / 开火 / 换弹 / 瞄准 / 后坐力 / 弹药"]
    CH --> ST["UStatsComponent (Replicated)<br/>Health + Stamina（服务器 Tick）"]
    CC -->|"OnAimStateChanged"| CH
    CH -->|"治疗结算"| ST
    ST -->|"OnStaminaDepleted"| PC
    CH --> SC["USniperScopeComponent<br/>SceneCapture + UMG 遮罩"]
    CC -->|"SpawnInventory / Equip"| WP["AWeapon<br/>Mesh1P / Mesh3P / Trace / Ammo / RecoilPattern"]
    WP --> WD["UWeaponData (DataAsset)<br/>GripPoints / Montages / FPlayerAnim"]
    CC -->|"读表"| WD
    CC -->|"伤害"| DMG["ShooterType<br/>ResolveHitZone / ScaleDamageForBone"]
    DMG -->|"Execute_DoDamage"| CH
    WP -->|"IRecoilModifier（配件）"| CC
    CH -->|"广播"| UI1["UShooterReticle<br/>动态准星"]
    CC -->|"广播"| UI2["UReserveAmmo<br/>弹药 / 图标"]
    ST -->|"广播"| UI3["WB_Health / WB_Stamina"]
```

---

## 8. 网络模型要点

- **生成与装备**：`SpawnInventory` 只在 `ROLE_Authority` 执行；`UCombatComponent` 复制 `Inventory` 与 `CurrentWeapon`，客户端靠 `OnRep_CurrentWeapon` 重新挂接与播放动画。
- **开火**：本地立即预测（播 1P Montage、扣弹匣、本地特效、施加后坐力）→ `Server_FireWeapon(Hit)` 校验射速并结算伤害 → `Multicast_FireWeapon` 让远端补特效与 3P 动画，并用 `Rep_Fire(AuthAmmo)` 校正弹匣。
- **后坐力**：只作用在本地控制旋转上（`ApplyFireRecoil` / `TickRecoil`），属于客户端手感表现；服务器不会看到这段偏移，与 UE 本身"瞄准由客户端决定"的模型一致。
- **换弹**：本地播 1P Montage 并置 `Reloading` → `Server_ReloadWeapon` → `Multicast_ReloadWeapon`（远端播 3P）→ 动画通知调用 `ApplyReload` / `Notify_ReloadWeapon` 在服务端填弹 → `Client_ReloadWeapon` 回传权威数值。
- **瞄准**：`bAiming` 使用 `COND_SkipOwner` 复制（Owner 已本地预测）；`Server_Aim` 保持服务端一致，切枪时 `Server_CycleWeapon` 也会强制关镜，让其他玩家看到状态变化。
- **耐力**：**只在服务器**计算（`UStatsComponent::TickComponent` 有权威判断）后同步给 Owner；控制器用 `Server_SetMovementState` 上报冲刺/静步状态，服务器用 `Client_StopSprint` 通知客户端退出冲刺。
- **移动速度**：客户端为了手感会在本地设置 `MaxWalkSpeed`，但必须把状态镜像给服务器，否则会被服务器纠正拉回。
- **治疗**：客户端起手播动画并 `Server_BeginHeal`；服务器把治疗量钳制到自己的 `HealAmout`，设定超时兜底，多播 3P 动画，并且只在 `Server_FinishHeal(true)` 时真正加血。
- **备弹**：`CurrentReserveAmmo` 只复制给 Owner（`COND_OwnerOnly`）。
- **受击**：伤害在服务端结算，`Multicast_HitReact` 让其他客户端播放受击 Montage。
- **趴下与滑铲**：两者都由 **Owner 预测**（本地立即改变胶囊与状态），再由服务器校验 —— `Server_SetProne` 重新检查是否着地 / 是否在治疗 / 头顶空间，`Server_SetSliding` 重新检查是否着地和 75 % 的 `SlideMinSpeed`。被拒绝时回传 `Client_ProneRejected(服务器值)` 或 `Client_SlideRejected()`，而 `bIsProne` / `bIsSliding`（都是 `ReplicatedUsing`）驱动其他所有机器；如果 Owner 的结束 RPC 丢了，服务器会在 `SlideDuration` 之后半秒强制结束滑铲。
- **开火打断冲刺**完全在拥有者客户端上处理（`StopSprinting()`），随后照常通过 `Server_SetMovementState` 把状态镜像给服务器。
- **腰射散布**在拥有者客户端的 `AWeapon::WeaponTrace` 里随机（`FMath::VRandCone`），服务器直接复用客户端的 `FHitResult`，与不散布的情况完全一样 —— 服务端没有散布锥，也没有任何同步的散布状态。准星在本地重算同一个角度，所以客户端与服务器永远不需要就某个数值达成一致。

---

## 9. 已知限制

- `EWeaponStatus::Firing` 未被任何代码写入。
- `AimTimeline` 仍然声明但没用；开镜现在由 C++ 的 `AimAlpha` / `AimAlignAlpha` 驱动（把 `bDriveAdsInCode` 关掉可回到旧的蓝图 `AdsMesh1PLoc` 方案）。
- 如果给 `LeanLeftAction` / `LeanRightAction` 填了自己的资产，需要自己在 `IMC_Shooter` 里映射按键；只有字段为空时才会运行时创建 Q/E 映射。Q/E 可能和 `IMC_Shooter` 里的其他映射冲突。
- 3P 脊椎探头在 `ABP_ThirdPerson` 里（`spine_03` 上的 Transform (Modify) Bone，Pitch = `LeanAlpha` × 20）；换骨架后方向不对就把符号取反。
- 后坐力是纯本地表现，不能当作服务端的"精度惩罚"使用。
- 步态/播放速率是通过**变量名**写进 AnimInstance 的（兼容 `PlayRate` / `LocomotionPlayRate` / `BS_PlayRate` 等别名），改名后会静默失效。
- `UStatsComponent::DeathState` 现在会被治疗逻辑**读取**，但仍然从未被写入 —— 死亡流程依旧缺失；`OnRep_DeathState` 为空，`ChangeHealthByAmout` 恒返回 `false`。
- `ChangeMaxHealthByAmout` / `ChangeMaxStaminaAmout` 没有上下限约束。
- `AShooterCharacter::DoDamage_Implementation` 在 `HitReacts` 为空时会对空数组取随机数，建议在蓝图侧保证至少一个 Montage。
- `USniperScopeComponent` 只为兼容保留，永久不工作，里面的常量已经不影响效果。
- 第 7 步试过的自动镜片平面、开镜灵敏度缩放、可选第一人称视野，已经从代码里撤掉；画中画瞄具仍需要镜片材质槽（`LensMaterialIndex`）或 `LensMaterial`，开镜时鼠标灵敏度不会自动缩放。
- 配件窗的行控件是按硬编码路径加载的（`/Game/FPS/UI/WBP_AttachmentSlotRow.WBP_AttachmentSlotRow_C`），改名会让整备界面的行消失（会打印 Warning）。
- `BP_M700`（狙击枪）没有配置 `AttachmentSlots`，所以它的配件菜单会显示"这把枪没有配件槽"。
- `DA_Sight` 勾了 `bOverridesAimFOV` 但没有填镜片材质，因此目前只相当于红点类瞄具；要做倍镜必须把 `OpticType = Magnified` 那一整块（Lens Socket、Lens Material、材质槽序号、渲染目标尺寸）填全，否则画中画会打警告并且不生效。
- 整备界面只显示背包里的**前三把**武器；预览装置是一个生成在地图下方极远处（Z = −100000）的临时 Actor，控件销毁时一并销毁。
- 配件装卸在服务器校验，但菜单是基于同步状态重建的，所以被服务器拒绝的请求只会留下一条 Warning，界面不会变化。
- `AShooterGameModeBase` 目前是空壳，规则逻辑（回合、计分、复活）需要自行扩展。
- **趴下**是复用 CMC 蹲伏、通过替换 `CrouchedHalfHeight` 实现的；`DefaultCrouchedHalfHeight` 在 `BeginPlay` 时记录，而 `CanExitProne` 自己做了一次胶囊重叠检测（`ProneStandUp`）—— 如果胶囊碰撞设置和 CMC 的不一致，"能不能站起来"的判定可能和移动组件不一致。滑铲中不能进入趴下，退出趴下会清空探头输入。
- **1P FABRIK 是有意停用的**：`FABRIK_Alpha1P` 恒为 0，`bDisableLeftHandIKDuringReload` / `LeftHandIKBlendSpeed` / `DisableLeftHandIKCurveName` 都已经不起作用。1P 持枪姿态来自武器的持枪动画和握把持姿。**3P** 的 FABRIK alpha 要绑到 `FABRIK_Alpha`（近战和爬行时为 0）。
- **滑铲需要角色骨架上有一个名为 `BodySlot` 的动画槽**，以及 `ABP_ThirdPerson` 里的按骨骼分层混合；没有的话滑铲什么都不会动，而且**不会打印任何日志**。工程自带的默认 `SlideAnim` 是软引用路径（`anim_Slide_L`）—— 移动资产会让滑铲身体动画失效。
- **只有当 `IdleAnim_Grab` 与 `LeftHandHoldPose_Grab` 不同时 Grab 待机才会生效**；与左手姿态相同的 `IdleAnim_Grab` 会被有意忽略（它只是"只有左手"的一层，会把枪拧歪）。`RefreshLocomotionBlendSpaces` **不能**调用 `InitAnim()` —— 那会销毁 `DefaultSlot` 上的 Grab 待机动态 Montage。
- 运行时创建的 `IMC_Crouch_Runtime` **优先级是 2**，并且会消费左 Ctrl 和 C，所以 `IMC_Shooter` 里残留的 `LeftControl → IA_Crouch`（或 `IA_Walk`）那一行会被屏蔽。旧的 `CrouchAction` 只有在 `CrouchToggleAction` 和 `CrouchHoldAction` **都**没赋值时才会绑定。
- `FirstPersonAnimInstance.cpp` 的命名空间 `GripHoldAnim_Private` 里还留着 `NAME_DefaultSlot` 和 `GrabIdleMaxSpeedXY`，是"把 Grab 待机循环挂在 `DefaultSlot` 上"那套废弃方案的遗留物，已经没人用。
- **近战命中是在客户端扫掠**的，`FHitResult` 和子弹一样直接发给服务器（服务端不会重新扫掠）。刀刃需要有 `Melee` Socket（`MeleeSocketName`），否则扫掠会从网格体原点开始。
- **弹匣动画**用的是弹匣组件上的单节点 `PlayAnimation`（不是 Montage），所以无法按骨骼混合，也无法被打断。
- **准星散布只是表现** —— `CrosshairSpread` 不会改变 `TraceRadius`，也不会改变子弹方向。真正的散布锥是 `AWeapon::HipFireSpread`（§10.3），它在 `WeaponTrace` 里、在**拥有者客户端**上施加，然后被 `Server_FireWeapon` 直接信任；因此被改过的客户端可以把自己的散布去掉。
- ⚠ `FWeaponAttachmentSlot`（Weapon.h）声明了一个 `HipFireSpread` 成员，没有任何代码读写它，也没有默认初始化 —— 看起来是从 `AWeapon::HipFireSpread` 误粘贴过来的。请改用**武器**上的属性加上 `UAttachmentData::HipFireSpreadMultiplier`。
- 移动 / 跳跃的散布惩罚尚未实现：`GetHipFireSpread()` 只判断 `bAiming`，头文件注释里标注了应该在这里补上。
- `AWeapon::ResolveAttachPoint` 在配置的 Socket 不可用时会静默**回落**（`RifleSocketak` → `RifleSocket` → `hand_r` → `HandGrip_R`），但每次都会打 Warning；枪挂得奇怪时先看 `[WeaponAttach]` 日志。没有父级又不是根骨骼的孤立骨骼会被有意拒绝。
- `Weapon.type.ak` 是 [DefaultGameplayTags.ini](<Config/DefaultGameplayTags.ini>) 里唯一注册的 Tag；它同时也是一个原生 Tag，所以两种方式都存在。

---

## 10. 武器配件与整备界面

| 组成 | 作用 |
| --- | --- |
| `FWeaponAttachmentSlot` | 枪的槽位表里的一行：槽位 Tag、挂点 Socket、白名单、默认配件、`bRequired` |
| `UAttachmentData` | 一个配件一份数据资产：槽位 Tag、名称/图标、**静态或骨骼**网格体 + 偏移、`FabrikSocketName` / `MuzzleSocketName`、`GripHoldStyle`、弹匣块（`MagReloadAnim`、`MagEmptyReloadAnim`、`MagTacticalReloadAnim`、`bHideSpareMagazineBone`、`BonesToHide`）、后坐力倍率、`HipFireSpreadMultiplier`、瞄具块（`OpticType`、`AimFOV`、`SightSocketName`、倍率、镜片 Socket / 材质 / 材质槽、渲染目标尺寸）与手电块 |
| `AWeapon` 配件 API | `EquipAttachment` / `RemoveAttachment` / `ToggleFlashlight`（仅服务器，`BlueprintAuthorityOnly`），以及 `GetAttachmentSlots`、`GetAllowedAttachmentsForSlot`、`CanEquipAttachment`、`HasAttachmentSlot`、`GetAttachmentInSlot`、`GetEquippedAttachments`、`GetRecoilMultiplier`、`GetEffectiveAimFOV`、`GetOpticAttachment`、`GetOpticMesh1P`、`HasFlashlight`、`IsFlashlightOn` 与委托 `OnAttachmentsChanged` |
| 网络同步 | `EquippedAttachments`（用数组 —— TMap 不能同步）与 `bFlashlightOn`，都是 `ReplicatedUsing`；服务器每次改动都重建模型并 `ForceNetUpdate` |
| 运行时模型 | `RebuildAttachmentVisuals` 会销毁并重新生成 1P/3P 的 StaticMesh，按依赖顺序解析挂点（护木先装，握把才能挂到护木上），并把聚光灯重新挂到本地玩家能看到的那个模型上 |
| 与后坐力 / 开镜的衔接 | `AWeapon` 实现了 `IRecoilModifier`，配件倍率会进入后坐力系统；`GetAimSocketWorldLocation` 优先瞄具的 `SightSocket`，其次枪管配件的 `AimSocket`，最后才是枪本身；开镜 FOV 取 `GetEffectiveAimFOV()` |
| 输入与界面 | `IA_AttachmentMenu`（B 键）开关 `UAttachmentMenu`，`IA_Flashlight`（F 键）开关手电，Tab 打开 `ULoadoutScreen`（与它的输入动作/映射上下文一起在运行时创建） |
| 握把 / 枪口 / 瞄具 | 握把真正挂住时，1P 扶枪 IK 跟随握把的 `FABRIK` Socket；枪口特效按 枪口配件 → 枪管配件 → 枪本体 Socket 的顺序取点；狙击枪继续走全屏 `SniperScopeComponent`，其他武器用 `UOpticScopeComponent` 做画中画倍镜 |

网络小结：客户端只通过 `UCombatComponent` **发起请求**（`Initiate_EquipAttachment`、`Initiate_RemoveAttachment`、`Initiate_ToggleFlashlight` 以及带 `…OnWeapon` 的版本），内部转发为可靠的服务器 RPC；`AWeapon` 再校验槽位与白名单并应用改动，最后由同步的数组触发 `OnRep_EquippedAttachments` → 重建模型 + 广播 `OnAttachmentsChanged` 让界面刷新。这个 Rep 还会重新应用 Locomotion 动画集（`RefreshLocomotionBlendSpaces`），所以换握把会立刻改变持枪姿态；趴下和滑铲走的是同一套"预测 → 校验 → 回滚"模式，回滚用 `Client_ProneRejected` / `Client_SlideRejected`。

### 10.1 开镜、瞄具与武器手感（武器逻辑第 1–8 步）

完整改动记录：`Saved/武器逻辑改动记录.md`。

| 方面 | 行为 | 在哪里配置 |
| --- | --- | --- |
| 换弹分种类 | `ChooseReloadType()` 选 `First`（这把枪第一次换弹）、`Empty`（空仓）或 `Tactical`；`FMontageData::ResolveReloadMontage()` 某种没配时回落到 `ReloadMontage` | `UWeaponData` → `FirstReloadMontage`、`EmptyReloadMontage`、`TacticalReloadMontage` |
| 拉栓 | 配了 `BoltMontage` 的枪开火后进入 `EWeaponStatus::BoltCycling`，Montage 结束前不能开火、换弹、切枪、治疗；服务器用 `ServerBoltEndTime` 校验 | `UWeaponData` → `BoltMontage` |
| 左手 IK | 换弹 / 拉栓时（或动画曲线 `DisableLeftHandIKCurveName` > 0）`FABRIK_Alpha1P` 渐变到 0，结束后按 `LeftHandIKBlendSpeed` 回来 | 角色 → `bDisableLeftHandIKDuringReload`、`LeftHandIKBlendSpeed` |
| 开火模式 | `Initiate_SwitchFireMode()` 本地切 `FireType` 并走 `Server_SetFireMode`；`FireType` 同步（跳过 Owner） | 武器 → `bCanSwitchFireMode`；绑定 `SwitchFireModeAction` |
| 开镜 FOV | 只在本地用 C++ 插值：可变倍率当前档 → 瞄具 `AimFOV`（勾了 `bOverridesAimFOV`）→ 枪 `AimFeildOfView` → `DefaultFOV`；关镜、切枪、死亡时回到默认 | 角色 → `bDriveAimFOVInCode`、`AimFOVInterpSpeed`、`DefaultFOV` |
| 开镜时间 | `AimAlpha` 在 `GetEffectiveAdsTime()` 内从 0 到 1；= `AimDownSightTime` × 所有已装配件的 `AdsTimeMultiplier` | 武器 → `AimDownSightTime`；配件 → `AdsTimeMultiplier` |
| 瞄具类型 | `None`、`RedDot`、`Holographic` 只改相机 FOV。`Magnified` / `Variable` 在瞄具模型有 `LensSocket` 或 `SightSocket`、并且有画面材质（`LensMaterial`，Magnified 也可用 `LensMaterialIndex` 槽上现成的材质）时，由 `UOpticScopeComponent` 做画中画。拍摄机和玩家镜头重合，画面中心就是子弹线；不开镜时不拍 | 配件 → `OpticType`、`AimFOV`、`Magnification`、`ZoomLevelsFOV`、`LensSocketName`、`LensMaterial`、`LensTextureParameterName`、`LensMaterialIndex`、`RenderTargetSize` |
| 可变倍率 | `Initiate_CycleZoom()` 让 `CurrentZoomIndex` 在 `ZoomLevelsFOV` 里轮换，画中画拍摄 FOV 平滑过渡到新档 | 配件 → `ZoomLevelsFOV`；绑定 `SwitchZoomAction` |
| 狙击 | 不再写死：装红点就和步枪一样；没装瞄具就对准自己的 `AimSocket`、用 `AimFeildOfView`；只有 Magnified / Variable 才画中画 | — |
| 腰射位置 | 腰射目标 = Mesh1P 默认位置（BeginPlay 记下）+ 武器偏移；旋转偏移在开镜时过渡回默认旋转；每帧读当前武器，切枪即生效 | 武器 → `HipMesh1POffset`、`HipMesh1PRotationOffset` |
| 眼距 | 开镜时把瞄准点放到镜头正前方的眼距处：瞄具 `EyeDistance`（勾了 `bOverridesEyeDistance`）→ 武器 `AdsEyeDistance` → 0 = 保持瞄准点本来的距离（按腰射姿态算，不会漂）。之前的 `AdsOffset` 已**删除** | 武器 → `AdsEyeDistance`；配件 → `bOverridesEyeDistance`、`EyeDistance` |
| 视觉后坐 | 只在本地给 Mesh1P 加的枪身后坐，叠在图案后坐上（子弹仍跟准星），受配件和开镜倍率影响。**默认关** | 角色 → `bEnableVisualRecoil`；武器 → `VisualRecoilLocation`、`VisualRecoilRotation`、`VisualRecoilAimMultiplier`、`VisualRecoilKickSpeed`、`VisualRecoilRecoverySpeed` |

### 10.2 战斗反馈、探头、整备界面与切枪规则（第 8 步之后）

中文改动记录：`Saved/武器逻辑改动记录.md`；整备界面 WBP 说明：`Saved/背包UI自定义说明.md`。

| 方面 | 行为 | 在哪里配置 |
| --- | --- | --- |
| 画中画曝光 | 倍镜拍摄机自己用 **Histogram 自动曝光**测光（亮度范围 0.03–8，低/高百分比 10/90），不继承主视角后处理，关掉泛光 / 局部曝光 / 胶片颗粒 / 物理相机曝光，默认用 **Lumen** GI + 反射。抗锯齿默认 **FXAA**，勾选后才用 TAA。项目默认自动曝光已关（`r.DefaultFeature.AutoExposure=False`） | 配件 → `PiPExposureBias`（EV，−4…4）、`bPiPUseLumen`（默认开）、`bPiPTemporalAA`（默认关 = FXAA） |
| 弹孔贴花 | 命中没有物理材质时，传给 `FireEffect` 的表面类型默认是 `SurfaceType2`（**Concrete**，原来是 `SurfaceType1`） | 物理材质 / 蓝图特效表 |
| 分方向受击 | 服务器按攻击者位置算前 / 后 / 左 / 右（±45° 扇区），多播 `(方向, 下标)`；某组为空时回落到 `HitReacts` | 角色 → `HitReactsFront`、`HitReactsBack`、`HitReactsLeft`、`HitReactsRight`、`HitReacts` |
| 受伤方向环 | 被打的人收到 `Client_ShowDamageIndicator(攻击者位置)` → `OnDamageIndicator`；`UShooterReticle` 按视角朝向点亮朝向攻击者的那一象限弧（前 / 右 / 后 / 左），每段独立淡出。该控件要放在 HUD 里（例如 `WBP_ShooterOverlay` 里的 `HitFeedbackReticle`） | 准星 → `DamageRingRadius`、`DamageRingThickness`、`DamageRingColor`、`DamageRingFadeTime`、`DamageRingGapDegrees`、`DamageRingSegments` |
| 命中确认标记 | 开枪的人收到 `Client_ConfirmHit()` → `OnHitConfirmed`；准星在中心画四条斜线（可选音效） | 准星 → `HitMarkerInnerOffset`、`HitMarkerLength`、`HitMarkerThickness`、`HitMarkerColor`、`HitMarkerDuration`、`HitMarkerSound` |
| 整备预览 | 整备界面里每把枪的预览姿态按枪单独配置；预览台的渲染目标和间距可调 | 武器 → `PreviewRotationOffset`（0,90,0）、`PreviewLocationOffset`（0,−28,0）、`PreviewScale`；预览台 → `RenderTargetWidth/Height`、`OrthoWidth`、`RowSpacing` |
| 整备界面 WBP | `ULoadoutScreen`、`ULoadoutAttachmentWindow` 和新的 `ULoadoutWeaponCard` 都可以用 WBP 设计（按名字绑定，见下表）；类字段留空就用 C++ 默认布局。配件弹窗默认关闭，并有关闭按钮 | 控制器 → `LoadoutScreenClass`；界面 → `CardClass`、`AttachmentWindowClass`、`PreviewRigClass`、`WindowDefaultPosition`、`NumCards`；弹窗 → `RowClass`、`RowPadding` |
| 探头（Q / E） | 按住探头（或切换模式）。`LeanTarget` 通过 `Server_SetLeanTarget` 同步（跳过拥有者）；各端都插值 `LeanAlpha`（−1…1）。本地 1P 相机横移 `LeanCameraOffset` 并侧倾 `LeanRollDegrees`，球形扫描在墙边缩短横移。冲刺移动中禁止。3P：`ABP_ThirdPerson` 读 `LeanAlpha` 转 `spine_03` | 角色 → `LeanCameraOffset`（30）、`LeanRollDegrees`（12）、`LeanInterpSpeed`（10）、`LeanSpineDegrees`（20）、`bLeanToggleMode`、`LeanWallProbeRadius`（12）、`LeanLeftAction`、`LeanRightAction` |
| 射线跟随探头 | `AWeapon::WeaponTrace` 从角色当前激活的相机出发（位置 + 旋转，即玩家实际画面，含探头和开镜），找不到再用 `GetPlayerViewPoint`；不再使用 `GetActorEyesViewPoint` | — |
| 切枪打断 | 拔枪中按**另一把**的键：停掉旧拔枪动画，新枪完整走一遍正常拔枪（时长一样）；拔枪中按**同一把**的键：忽略。换弹中切枪：停掉所有换弹动画（手臂、身体、枪身），**不给子弹**。拉栓中仍不能切枪 | — |
| 换弹 | 换弹中再按换弹：忽略。`Notify_ReloadWeapon` 只有武器仍在 `Reloading` 才填弹；`Client_ReloadWeapon` 现在带上武器，迟到的结果不会落到新枪上 | — |
| 检视、侧瞄、激光 | `VersionWeaponAction` 播放 `InspectMontage1P`；带 `bProvidesCantedSight` / `bProvidesLaser` 的配件提供 45° 侧瞄（切倍率越过主镜最后一档进入，每把枪记住），以及用手电键开关的激光束 + 光点（进侧瞄自动开） | `FMontageData::InspectMontage1P`；配件 → `CantedSightSocketName`、`CantedAimFOV`、`CantedEyeDistance`、`LaserSocketName`、`LaserColor`、`LaserBrightness`、`LaserRange`、`LaserDotSize`、`bHideBeamWhenAiming` |

整备界面 WBP 必须的控件名（`BindWidget` = 必须存在且名字完全一致，可选 = 可以不放）：

| 控件类 | 必需 | 可选 |
| --- | --- | --- |
| `ULoadoutScreen` | `PopupLayer`（CanvasPanel，铺满全屏）、`CardContainer`（任意面板） | `PreviewImage`（Image）、`Text_Title`、`Text_Hint`（TextBlock） |
| `ULoadoutAttachmentWindow` | `DragHandle`（任意控件）、`SlotList`（面板，如 VerticalBox / ScrollBox） | `CloseButton`（Button）、`Text_WeaponName`、`Text_Empty`（TextBlock） |
| `ULoadoutWeaponCard` | `Button`（Button） | `Text_Name`（TextBlock）、`Image_Icon`（Image）；蓝图事件 `OnCardStateChanged(bSelected, bEmpty, Weapon)` |

### 10.3 移动模式、握把持姿、近战与弹匣配件（最新修订）

中文改动记录：`Saved/武器逻辑改动记录.md`；报告：`Saved/Prone_REPORT.md`、`Saved/CrouchSlide_REPORT.md`、`Saved/GripGrab_Prone_REPORT.md`、`Saved/AK_*_REPORT.md`。

| 方面 | 行为 | 在哪里配置 |
| --- | --- | --- |
| 趴下 | `bIsProne`（已同步，`OnRep_IsProne`）由 Z 键切换；Owner 先预测，`Server_SetProne` 再校验是否着地 / 是否在治疗，退出时还要校验头顶空间（`CanExitProne` 做重叠检测）；不通过时用 `Client_ProneRejected` 把 Owner 拉回。复用 CMC 蹲伏并把 `CrouchedHalfHeight` 设为 34，压低 1P 相机，禁止跳跃 / 探头 / 冲刺，并写出 `ProneAlpha` 供混合使用 | 角色 → `ProneAction`、`ProneCapsuleHalfHeight`（34）、`ProneCameraZOffset`（−50）、`ProneCameraInterpSpeed`（6）、`ProneAnimReferenceSpeed`（100）、`ProneCrawlSpeedThreshold`（10）、`bHideWeapon3PWhileProne`；控制器 → `ProneSpeedMultiplier`（0.25） |
| 趴行 vs 趴下静止 | 每台机器都自己计算 **`bProneCrawling`** = 趴下**且**地面速度 > 阈值。`ABP_ThirdPerson` 应该用它来切换：`BS_Prone`（爬行、全身、`FABRIK_Alpha` 0）与"趴下待机身体 + 分层端枪上半身"（静止、`FABRIK_Alpha` 1） | ABP → `bIsProne`、`bProneCrawling`、`FABRIK_Alpha`；BlendSpace `BS_Prone` |
| 滑铲 | 速度高于 `SlideMinSpeed`（600）时按蹲伏键即滑铲：下蹲胶囊、+150 cm/s、`GroundFriction` 0 且 `BrakingDecelerationWalking` 800（结束后都会还原）、忽略移动输入、下蹲速度上限提到 1.5 × `SprintSpeed`。0.9 秒、低于 200 cm/s 或离地时结束 → 蹲伏（按了 C 或仍按住 Ctrl）或起身（已松开 Ctrl）；跳跃会取消。服务器按 75 % 校验，不通过回 `Client_SlideRejected` | 角色 → `SlideMinSpeed`、`SlideDuration`、`SlideSpeedBoost`、`SlideDeceleration`、`SlideEndSpeed`、`SlideCameraZOffset`、`SlideMontage`、`SlideAnim`、`SlideSlotName`（BodySlot）、`SlideMontage1P`（保持为空） |
| 蹲伏输入 | 左 Ctrl = 按住，C = 切换，都是 `IMC_Crouch_Runtime` 里**优先级 2** 的运行时 Enhanced Input 动作；旧的 `IA_Crouch` 只在两个新动作都没赋值时才绑定。起身交给 CMC 处理，所以头顶有障碍会自动延后 | 控制器 → `CrouchHoldAction`、`CrouchToggleAction` |
| 握把持姿 | 握把配件上的 `EGripHoldStyle`（`Cradle` / `Grab`）。没装握把时 `GetGripHoldStyle()` 返回 `Cradle`。Grab 会把 `IdleAnim_Grab` / `AimIdleAnim_Grab` 换进注入的 `FPlayerAnim`，并优先使用 `EmptyReloadMontage_Grab` / `TacticalReloadMontage_Grab`。`UFirstPersonAnimInstance` 暴露 `GripHoldStyle`、`ActiveIdleAnim`、`LeftHandHoldPose`、`bUseLeftHandHoldPose` 给 ABP 做分层混合。⚠ 与 `LeftHandHoldPose_Grab` 相同的 `IdleAnim_Grab` 会被当作完整待机而忽略 | 配件 → `GripHoldStyle`；`DA_WeaponData` → `IdleAnim_Grab`、`AimIdleAnim_Grab`、`LeftHandHoldPose_Cradle/_Grab`、`…ReloadMontage_Grab` |
| 骨骼弹匣 | 配了 `SkeletalMesh` 的弹匣配件会生成 `USkeletalMeshComponent`（停在 `MagReloadAnim` 第 0 帧），`PlayMagazineReload` 会播放对应的换弹变体并与手臂换弹同步；`bHideSpareMagazineBone` 隐藏 AK 的 `mag_02` 备用弹匣，`BonesToHide` 隐藏其他骨骼 | 配件 → `SkeletalMesh`、`MagReloadAnim`、`MagEmptyReloadAnim`、`MagTacticalReloadAnim`、`bHideSpareMagazineBone`、`BonesToHide` |
| 近战 | `UsesAmmo()` 为 false → 不扣弹匣、不自动换弹、禁止换弹；本地和服务器都强制关镜；命中判定改为**沿刀刃的球形扫掠**（`WeaponMeleeHit`）而不是相机射线；两个 FABRIK alpha 都强制为 0；3P 挥砍 Montage 走全身；`FireMontages` / `EquipMontages` 支持随机变体 | 武器 → `MeleeSocketName`（Melee）、`MeleeHitRadius`（40）、`MeleeHitReach`（120）、`FireMontages`、`EquipMontages` |
| 开火打断冲刺 | `Initiate_FireWeapon_Pressed` 先判断 `AShooterPlayerController::IsSprinting()`：是就调用 `StopSprinting()` 直接返回，所以这一下按键被消耗掉，真正开火要等下一次按下 | 控制器 → `IsSprinting()`（BlueprintPure）、`StopSprinting()`（BlueprintCallable） |
| 散布准星 | `OnWeaponFired`（在 `Local_FireWeapon` 末尾广播）让 `CrosshairSpread` 增加 4，上限 28；`UShooterReticle` 以 40/s 衰减，并用原生绘制在 `CrosshairBaseGap + CrosshairSpread` 处画四条正交线。开镜时隐藏（近战除外）。面板还会把**真实**的散布锥换算成像素一并加上 | 准星 → `bDrawSpreadingCrosshair`、`CrosshairBaseGap`、`CrosshairLineLength`、`CrosshairLineThickness`、`CrosshairLineColor`、`CrosshairSpreadKick`、`CrosshairSpreadMax`、`CrosshairSpreadRecovery` |
| 腰射散布 | `AWeapon::GetHipFireSpread()` = `HipFireSpread` × 所有已装配件的 `HipFireSpreadMultiplier`，开镜时为 **0**。`WeaponTrace` 用 `FMath::VRandCone` 在该锥体内随机方向，`UShooterReticle` 把同一个角度投影成像素（`HalfWidth / tan(FOV/2) × tan(spread)`），所以准星与散布锥一致。在负责射线的客户端上随机；移动 / 跳跃惩罚尚未实现 | 武器 → `HipFireSpread`（3，0–30°）；配件 → `HipFireSpreadMultiplier`（1） |
| AK 武器 | 用自己独立的 Tag `Weapon.type.ak`（也是 `DefaultGameplayTags.ini` 里唯一注册的 Tag），在 `DA_WeaponData` 里有一套单独的条目，并有一条硬编码的 3P 挂接路径：`RifleSocketak`（回落 `RifleSocket`），而 1P 网格体走 `GripPoints`（`weapon_r`）。`ResolveAttachPoint` 会拒绝孤立骨骼，并且每次回落都会打印一条 `[WeaponAttach]` 日志 | 武器蓝图 `WeaponType`；`DA_WeaponData` → `GripPoints` |

## 11. 延伸阅读

- 逐函数 / 逐组件详解：[DOCUMENTATION.zh-CN.md](<DOCUMENTATION.zh-CN.md>)
- English: [README.md](<README.md>) ｜ [DOCUMENTATION.md](<DOCUMENTATION.md>)
- UE 官方参考：Gameplay Framework、Enhanced Input、`UDataAsset`、`USceneCaptureComponent2D`、`UAnimNotify`
