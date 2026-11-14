**Language / 语言：** [English](README.md) ｜ **中文**

---

# FPS — 合作 CQB FPS（Unreal Engine 5.8，C++）

一款 **4 人合作近距离战斗（CQB）FPS**，使用 **Unreal Engine 5.8 + C++** 开发，对标《Ready or Not》。小队从靶场大厅部署进入任务地图，在士气驱动的嫌疑人 AI 面前清房，完成随机化目标，撤离后获得评级结算。

本项目脱胎于早期的战术 PvP 框架（角色、武器与战斗核心沿用自那时）。当前的合作任务循环是正式游戏内容，位于默认分支 `cqb/strip-arena-modes`。旧的竞技场模式（TDM / 爆破 / BDM）只保留在 `master` 分支，**已不属于本游戏**。

> 本文档于 2026-11-14 对照现行代码（`cqb/strip-arena-modes` @ `925df4f`）重写。
> 系统级深挖（任务流、AI、配音、网络）见 [DOCUMENTATION.zh-CN.md](DOCUMENTATION.zh-CN.md)。
> 英文版：[README.md](README.md) ｜ [DOCUMENTATION.md](DOCUMENTATION.md)

---

## 1. 项目概览

| 项目 | 值 |
| --- | --- |
| 引擎版本 | Unreal Engine 5.8（[FPS.uproject](FPS.uproject) 中 `EngineAssociation: "5.8"`） |
| 主模块 | `FPS`，类型 Runtime（[FPS.Build.cs](Source/FPS/FPS.Build.cs)） |
| 语言 | C++（189 个源码文件，约 33,600 行），玩法按需暴露给蓝图 |
| 输入 | Enhanced Input（`UEnhancedInputComponent` + `UInputMappingContext`） |
| 游戏默认地图 | `/Game/FPS/Level/MainMenu/MainMenu` —— 主菜单关卡 |
| GameInstance | `BP_ShooterGameInstance`（`UShooterGameInstance`）—— 负责小队跨图旅行 |
| 全局默认 GameMode | `/Script/FPS.ShooterGameModeBase`（菜单）；任务使用 `ACoopCqbGameMode` |
| 在线服务 | Epic Online Services（EOS）组队 + 会话；房主监听服务器权威 |
| 构建目标 | `FPS`（Game）、`FPSEditor`（Editor） |

模块依赖（[FPS.Build.cs](Source/FPS/FPS.Build.cs)）：

- Public：`Core`、`CoreUObject`、`Engine`、`InputCore`、`EnhancedInput`、`GameplayTags`、`PhysicsCore`、`UMG`、`Niagara`、`AnimGraphRuntime`、`AIModule`、`NavigationSystem`
- Private：`Slate`、`SlateCore`、`MoviePlayer`、`AssetRegistry`、`LevelSequence`、`MovieScene`、`OnlineSubsystem`、`OnlineSubsystemUtils`、`Sockets`

启用的主要插件（[FPS.uproject](FPS.uproject)）：`OnlineSubsystemEOS`、`EOSShared`、`SocketSubsystemEOS`、`OnlineServicesEOS`、`GameplayStateTree`、`AnimationWarping`、`ModelContextProtocol`（编辑器工具）。

## 2. 功能一览

| 系统 | 已实现内容 | 主要代码 |
| --- | --- | --- |
| 任务循环 | 主菜单 → 靶场大厅 → 选关（房主）→ 整队切图 → 部署门禁 → 任务目标 → 撤离 → 评级结算 → 返回大厅 | [CoopCqbGameMode.cpp](Source/FPS/Private/Game/CoopCqbGameMode.cpp)、[MissionDeployPoint.cpp](Source/FPS/Private/World/MissionDeployPoint.cpp) |
| 部署链 | 房主在任务台选关，小队就绪后 `UShooterGameInstance::TravelSquadTo` 把全员以服务器旅行带走，人数经 travel URL 传递；目标图等全员加载完毕才进入简报 | [ShooterGameInstance.cpp](Source/FPS/Private/ShooterGameInstance.cpp)、[ShooterPlayerController_MissionFlow.cpp](Source/FPS/Private/ShooterPlayerController_MissionFlow.cpp) |
| 工厂任务（「锈蚀车间」） | 夜间突袭废弃工厂的地下军火作坊：账本随机藏在三间办公室之一，查封三台工作台，撤离；4 个隐藏任务（无声行动、断电者、出货单、锅炉房的秘密） | [FactoryMissionObjective.cpp](Source/FPS/Private/World/FactoryMissionObjective.cpp) |
| 嫌疑人 AI | 脑状态 Idle / Investigate / Combat / TakeCover / Retreat / Flee / Surrender；听觉（枪声 4500、脚步 900）、找掩体、丢失视线宽限 3.5 秒、首击士气判定（投降 0.30 / 困兽 0.25 / 逃跑 0.45）、小队角色（压制/侧翼）、命中部位（腿 70% / 胸 25% / 头 5%） | [HostileAIController.cpp](Source/FPS/Private/Game/HostileAIController.cpp)、[HostileSpawner.cpp](Source/FPS/Private/Game/HostileSpawner.cpp)、[HostileSquadSubsystem.cpp](Source/FPS/Private/Game/HostileSquadSubsystem.cpp) |
| 配音系统 | 19 句英文无线电台词（指挥官 + 队员）：简报、开场、4 个隐藏任务触发、7 句上报、6 句喝令；cue 枚举 + 电台排队播放组件 | [MissionVoiceTypes.h](Source/FPS/Public/Game/MissionVoiceTypes.h)、[MissionRadioComponent.cpp](Source/FPS/Private/Game/MissionRadioComponent.cpp) |
| 角色与战斗 | 第一人称手臂 + 第三人称身体、探头、下蹲/滑铲/匍匐、体力、按身体部位的伤害与受击反应——沿用自框架时期，仍是现行核心 | [ShooterCharacter.cpp](Source/FPS/Private/ShooterCharacter.cpp)、[CombatComponent.cpp](Source/FPS/Private/CombatComponent.cpp) |
| 武器与配件 | 数据驱动武器（`UWeaponData`）、配件系统（枪口/瞄具/握把/枪托/战术/弹匣等）与改装界面、13 格携行具背包、仓库配装存档、配装预览台 | [Weapon.cpp](Source/FPS/Private/Weapon/Weapon.cpp)、[RangeLoadoutWidget.cpp](Source/FPS/Private/Ui/RangeLoadoutWidget.cpp) |
| 门与世界物件 | 可破坏门（把手/推/踹三种开门方式）、荧光棒、梯子、绊线陷阱、油桶、人质 | [BreakableDoor](Source/FPS/Public/World/BreakableDoor.h)、[MissionObjective.cpp](Source/FPS/Private/World/MissionObjective.cpp) |
| HUD 与 UI | 极简 CQB HUD（设备状态条；弹匣队列仅在长按 R 检视时显示）、任务平板（Tab）、任务流程/结算界面、无人机侦察加载演出 | [MissionFlowWidget.cpp](Source/FPS/Private/Ui/MissionFlowWidget.cpp)、[MissionTabletWidget](Source/FPS/Public/Ui/MissionTabletWidget.h) |
| 组队与会话 | 基于 EOS 的组队（最多 4 人）、房主权威监听服务器、跨图保持小队完整 | [PartySubsystem.cpp](Source/FPS/Private/Online/PartySubsystem.cpp) |

## 3. 目录结构

```
FPS/
├─ Config/                 引擎/游戏 ini（默认值、EOS、输入）
├─ Content/
│  ├─ FPS/
│  │  ├─ Level/            MainMenu、Lvl_Range（大厅）、Lvl_Boat、Testmap
│  │  ├─ Coop/             合作系统 HOWTO 笔记
│  │  ├─ Range/            靶场时期 HOWTO 笔记（工作日志）
│  │  ├─ Weapon/           武器蓝图、Kinemation 动画集
│  │  ├─ Data/             WeaponData / 配件数据资产
│  │  └─ Input/            输入动作与映射上下文
│  └─ Maps/                Lvl_Factory_Whitebox（工厂任务）
├─ Docs/                   设计与交接文档（中文）
├─ Source/FPS/
│  ├─ Public/  Private/    Game、World、Ui、Weapon、Loadout、Online 等
│  └─ Private/Tests/       自动化测试（任务流、工厂配音）
├─ DOCUMENTATION.zh-CN.md  系统参考（本次重写）
└─ README.zh-CN.md
```

## 4. 快速开始

1. 安装 **Unreal Engine 5.8**。
2. 克隆本仓库并切换到 `cqb/strip-arena-modes`（默认分支）。
3. 右键 `FPS.uproject` → **生成 Visual Studio 项目文件**（项目使用 Cursor/VS + UE 构建工具开发）。
4. 构建 `FPSEditor` 目标，然后打开 `FPS.uproject`。
5. 游戏从主菜单开始；进入**靶场大厅**（`Lvl_Range`）进行配装与选关。任务台仅房主可用，且只在靶场大厅工作。

快速自检部署链有一个可选的 QA 捷径：以 `-UIQA -UIQATravel` 参数启动，游戏会自动从靶场旅行到工厂白盒图，预期人数为 1。

## 5. 操作说明

默认按键（Enhanced Input；权威列表见 `Content/FPS/Input` 下的映射上下文）：

| 输入 | 动作 |
| --- | --- |
| WASD / 鼠标 | 移动 / 视角 |
| 鼠标左键 | 开火 |
| 鼠标右键（按住） | 瞄准（ADS） |
| R | 换弹 —— **长按 R** 检视弹匣并显示弹匣队列 |
| Shift | 疾跑 |
| Ctrl（按住）/ C（切换） | 下蹲（高速时触发滑铲） |
| 空格 | 跳跃 |
| Q / E | 左 / 右探头 |
| F | 互动（任务台、门、押解等） |
| Y | 在任务台就绪 |
| Tab | 任务平板 |
| 1 / 2 / 3 … | 武器槽（主武器 / 副武器 / 手枪 / 近战） |
| 开火模式键 | 切换开火模式 |
| 设备键 | 开关手电 / 镭射 / 夜视（状态显示在 HUD 状态条上） |

## 6. 关键地图与资产

| 地图 | 作用 |
| --- | --- |
| `/Game/FPS/Level/MainMenu/MainMenu`（及 `Lvl_MainMenu`） | 主菜单 |
| `/Game/FPS/Level/Lvl_Range` | 靶场大厅：配装台、任务台、组队集结（**任务前地图**） |
| `/Game/Maps/Lvl_Factory_Whitebox` | 任务一 —— 工厂（「锈蚀车间」） |
| `/Game/FPS/Level/Lvl_Boat` | 早期测试任务图 |
| `/Game/FPS/Level/Testmap` | 沙盒 / 旧测试图 |

关键蓝图：`BP_ShooterGameInstance`、`BP_MainMenuGM`（`AMainMenuGameMode` 的蓝图子类）、`BP_ShooterCharacter`、`WBP_ShooterOverlay`（HUD），任务与配装界面在 `Content/FPS/UI` 下。

## 7. 系统架构（速览）

游戏的脊柱是一条链（按此顺序读代码）：

```
MissionDeployPoint::Auth_SelectMission   房主选关（仅限靶场大厅）
  → 小队就绪 + 名单复验
  → MissionDeployPoint::DeploySquad
  → UShooterGameInstance::TravelSquadTo  拼 travel URL（?listen?game=…?ExpectedOperators=N）
  → World::ServerTravel                  所有客户端被一并带走
  → ACoopCqbGameMode::InitGame           读 URL：是不是靶场？预期几人？
  → InitGameState                        两者抄进 ACoopCqbGameState（可复制）
  → 就绪门                               全员连上 + 加载完成（0.25 秒轮询）
  → 简报门                               简报定时器 —— 工厂图改为等部署视角就绪
  → ACoopCqbGameMode::StartMission
  → SpawnHostilesFromLevel               关卡内每个 AHostileSpawner 刷出自己那一排人
```

两条设计规则撑起整条链：**GameInstance 是唯一能活过切图的对象**，所以跨图数据只能塞进 travel URL；**GameMode 只存在于服务器**，所以客户端要看的东西必须抄一份进 GameState。

## 8. 网络模型要点

- 监听服务器、房主权威：玩法判定（选关、刷怪、士气、计分）全部只在服务器执行（全代码贯穿 `HasAuthority` 守卫）。
- 组队/会话走 EOS（`UPartySubsystem`）；小队通过 `ServerTravel` 整体旅行（刻意不用无缝旅行，让新图的控制器重新确认占有与流送）。
- 就绪是显式的：客户端上报 `Server_MissionLoaded`；连上人数达到预期且全员就绪，任务才开始。

## 9. 已知限制（2026-11-14）

- 游戏处于测试/验证阶段，尚未做分发打包；Development/Shipping 打包验证待完成。
- 当前内容的纹理流送池超标，性能专项尚未做。
- 嫌疑人 AI 可用，但其在工厂图中的行为调校仍在进行。
- 树里还有少量旧系统残留（如 `UTraversalComponent`，是已移除的翻越功能的遗留）。
- 两句隐藏任务配音仍是旧台词版本，重录待进行。

## 10. 武器配件与整备界面

- 配装在**靶场配装台**（`ARangeLoadoutBench` / `RangeLoadoutWidget`）完成：武器、配件、装备。
- 配件是数据驱动的（`AttachmentData`、`Gunsmith` 定义），应用到实装武器上，并有预览台展示效果。
- 携带的弹匣构成一个**队列**：换弹取队列中的下一个，卸下的（未打空的）弹匣回到队尾；HUD 仅在检视（长按 R）时按队列顺序显示。
- 配装预设保存在 `UWarehouseLoadoutSaveGame`；任务最佳成绩保存在 `UMissionBestSaveGame`。

## 11. 延伸阅读

- [DOCUMENTATION.zh-CN.md](DOCUMENTATION.zh-CN.md) —— 系统参考：任务流、AI、配音、网络、数据。
- [DOCUMENTATION.md](DOCUMENTATION.md) —— 英文版。
- `Docs/` —— 设计与交接文档（中文）。
- `Content/FPS/Coop/*_HOWTO.txt` —— 合作系统工作笔记；`Content/FPS/Range/*_HOWTO.txt` —— 早期工作日志（部分已成历史）。
