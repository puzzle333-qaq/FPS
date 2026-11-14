# FPS 项目代码解释文档（合作 CQB 版）

## 1. 文档说明与阅读顺序

本文档描述的是代码的**当前状态**：`cqb/strip-arena-modes` 分支 @ `925df4f`（2026-11-14）上的合作 CQB 游戏。它取代上一版——上一版写的是已退役的竞技场 PvP 代码库（TDM / 爆破 / BDM、房间大厅、购买商店），那套代码现在只存在于 `master` 分支，本文不再覆盖。

阅读顺序：第 2 节给出代码地图和最重要的一条数据流。第 3–5 节是沿用自早期游戏的框架核心（角色、战斗、武器）与新旧交替说明。第 6–9 节是合作游戏本体：任务框架、端到端部署链、嫌疑人 AI、配音系统。第 10–13 节是组队/在线、配装、世界可交互物与 UI。第 14–16 节是配置层、测试与如实记录的注意事项。

约定：类名均为准确名称。行号极少出现（行号容易过期），函数名才是稳定的锚点。“服务器”指房主监听服务器；玩法权威始终在服务器侧。

## 2. 代码地图与数据流

源码布局（`Source/FPS`，189 个文件，约 33,600 行）：

| 目录 | 内容 |
| --- | --- |
| `Game/` | `ACoopCqbGameMode`、`ACoopCqbGameState`、敌对 AI（`AHostileAIController`、`AHostileAICharacter`、`AHostileSpawner`、`UHostileSquadSubsystem`）、`UMissionRadioComponent`、`MissionVoiceTypes`、人质、噪声事件 |
| `World/` | `AMissionDeployPoint`、`AMissionObjective`、`AFactoryMissionObjective`、`ABreakableDoor`、荧光棒、梯子、陷阱、油桶 |
| `Ui/` | 26 个界面类：HUD 家族、`UMissionFlowWidget`、结算幕、配装、平板、加载屏 |
| `Weapon/` | `AWeapon`、`UWeaponData`、配件 |
| `Loadout/` | `UPouchInventoryComponent`、`UWarehouseLoadoutSaveGame` |
| `Online/` | `UPartySubsystem`（EOS 组队/会话衔接） |
| `Character/`、`Combat/`、`Stats/` | `AShooterCharacter` 支撑、`UCombatComponent`、`UStatsComponent` |
| `Menu/`、`Range/`、`Settings/`、`Vision/` | 主菜单模式、靶场配装台/预览台、设置、夜视 |
| `Tests/`（Private） | 自动化测试：任务流、工厂配音 |

最先要学的一条流是部署链（第 7 节）。其余系统几乎都挂在它身上：它选定的 GameMode、它填充的 GameState、它等待的就绪、它触发的刷怪点。

## 3. 模块与构建层

- 单一运行时模块 `FPS`。Public 依赖：Core、CoreUObject、Engine、InputCore、EnhancedInput、GameplayTags、PhysicsCore、UMG、Niagara、AnimGraphRuntime、AIModule、NavigationSystem。Private 依赖包括 MoviePlayer（加载屏）、LevelSequence/MovieScene（侦察演出）、OnlineSubsystem(Utils)、Sockets。
- 引擎关联 5.8。游戏目标为 `FPS` / `FPSEditor`。
- 值得注意的插件：OnlineSubsystemEOS + EOSShared + SocketSubsystemEOS + OnlineServicesEOS（在线）、GameplayStateTree、AnimationWarping、ModelContextProtocol（编辑器）。

## 4. 沿用至今的框架核心

这些系统早于合作游戏，是它的地基。此处只作简述；它们内部的变化比周围一切都少。

### 4.1 AShooterCharacter —— 玩家角色

第一人称手臂网格 + 第三人称身体，相机挂在零长弹簧臂上并带延迟。管辖移动模式（步行 / 带体力的疾跑、Ctrl 按住或 C 切换的下蹲、速度触发的滑铲、匍匐爬行）、Q/E 探头（可复制、带墙体探测、武器射线跟随探头后的相机），并逐帧把运动数据推给动画蓝图。全项目最大的文件（约 5,540 行）——用 IDE 的符号搜索导航，不要滚动翻。

### 4.2 UCombatComponent —— 战斗核心

开火/换弹/瞄准的编排、开火模式、装备槽、互动入口。服务器验证 + 客户端预测/驳回的模式沿用自框架时期。约 3,930 行。

### 4.3 AWeapon + UWeaponData —— 武器

`AWeapon`（约 4,370 行）是武器 Actor：带命中判定的射击、后坐力模式与视角回正、ADS、瞄具（含画中画倍镜）、手电/镭射设备、近战横扫。全部调校都在 `UWeaponData` 资产里 —— 换模型、换动画、改数值都是数据变更，不是代码变更。配件（第 11 节）在运行时修改数据。

### 4.4 伤害、命中部位、生命值

伤害按身体部位结算（与嫌疑人 AI 瞄准用的是同一套部位模型：腿/胸/头）。`UStatsComponent` 管辖生命值与服务器权威的体力模型（消耗/恢复、跳跃消耗、耗尽事件）。`ShooterPlayerController`（及其任务流分片 `ShooterPlayerController_MissionFlow.cpp`，见 7.5）管辖输入路由与移动模式仲裁。

## 5. 从框架到合作：变了什么

给读过上一版的读者：竞技场那一代是**退役，不是移植**。TDM / 爆破 / BDM 模式、购买商店、计分板、房间大厅及其会话浏览器、旧配装存档，都只存在于 `master`。沿用的是第 4 节的核心（角色、战斗、武器、伤害、体质）与服务器权威的整体风格。第 6–13 节的一切 —— 任务框架、部署链、嫌疑人 AI、配音、组队、平板、结算 —— 都是为合作游戏写的新代码。靶场以变形的方式活了下来：旧的自由练习靶场图变成了小队大厅（`Lvl_Range`），配装与选关都在那里。

## 6. 合作任务框架

### 6.1 ACoopCqbGameMode / ACoopCqbGameState

同一个 GameMode 既跑靶场大厅又跑任务图；它扮演哪种角色，在 `InitGame` 里按图决定：

```cpp
bRangeHub = MapName.Contains(TEXT("Lvl_Range")) || UGameplayStatics::HasOption(Options, TEXT("RangeHub"));
ExpectedOperators = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("ExpectedOperators"), 1), 1, MaxOperators);
```

已知的脆弱点（如实记录）：大厅身份有一半靠**地图名猜**。travel 参数 `RangeHub` 是显式覆盖通道 —— 返回大厅的旅行总是带上它。给靶场图改名而不更新这处检查，选关会**静默失效**。

`InitGameState` 把 `bRangeHub` 与 `ExpectedOperators` 抄进 `ACoopCqbGameState`，因为 GameMode 只存在于服务器，而 GameState 会复制给所有人（任务台的检查、HUD 的计数、就绪判断读的都是 GameState 那份）。

GameState 还承载任务阶段（`ECoopCqbPhase`：Waiting / Briefing / InProgress / …）、对局状态、主目标，以及任务结束的 `FMissionReport`（6.3）。

### 6.2 任务阶段

`Waiting`（队员加载中）→ `Briefing`（7.4 的两道门）→ `InProgress`（目标生效）→ 结算（GameState 上的 `bDebriefOpen`；结算界面读取报告）→ 旅行返回大厅。

### 6.3 计分与结算报告

`FMissionReport`（位于 GameState）累积结算屏要显示的一切：目标计数、分组得分、`Score` / `MaximumScore`、字母 `Grade`、隐藏任务完成情况、惩罚项（如击杀已投降者）。结算**不需要**任何额外数据管线 —— 它只渲染这个结构体。本地 `UMissionBestSaveGame` 保存个人最佳成绩。

## 7. 部署链（端到端）

### 7.1 AMissionDeployPoint —— 任务台（靶场大厅）

摆在 `Lvl_Range` 里。互动仅限房主、仅限大厅。`Auth_SelectMission(PC, Index)` 的守卫依次为：`HasAuthority()`、`IsRoomHost(PC)`、PC/ pawn 有效、索引在范围内、**距任务台 ≤ 500**、`GameState->bRangeHub == true`、且没有其他任务台已经选了任务。随后它设定 `SelectedMission`，广播倒计时界面（`Multicast_ShowCountdown` → 各客户端打开选关界面），启动 0.25 秒一轮的名册服务定时器；若所选地图名含 "Factory"，还会立即通过它的 `BriefingRadio` 组件播放简报配音（第 9 节）。

队员按 Y 就绪。`DeploySquad` 复验名册（有任何变化即以“队伍发生变化，请重新按 Y 就绪”中止），把当前 PlayerController 数计作 `Operators`，解析目标的 `FMissionDestination`（地图 + GameMode 来自任务台的 `Missions` 数组；数组非空时，旧的单图回退字段不生效），检查地图包存在，然后调用旅行。

### 7.2 UShooterGameInstance::TravelSquadTo —— 旅行

GameInstance 是唯一能活过切图的对象，所以旅行归它管。单机游戏走简单的 `TravelTo` 路径。联机游戏拼一条 **travel URL**：

```
<LongPackageName>?listen?game=<GameMode路径>?ExpectedOperators=<N>
```

- 监听服务器时加 `listen`；`game=` 锁定目标 GameMode。
- 旅行前它强制 `bUseSeamlessTravel = false`（新图的控制器刻意重新确认占有与流送），并**清理 travel 基底 URL**，防止上一张图的参数 —— 尤其是 `RangeHub` —— 漏进下一张。
- `World->ServerTravel(url)` 把所有已连接客户端一并带走。

`UShooterGameInstance::Init` 里有一条可选 QA 路径：命令行带 `-UIQA -UIQATravel` 时，它在靶场等 8 秒后以 `ExpectedOperators=1` 旅行到工厂白盒图，让打包后的版本能自检这条链。

### 7.3 落地 —— InitGame / InitGameState

见 6.1。`InitGameState` 里还有一项额外职责：旧的工厂美术图没有 `PlayerStart`，若地图名含 "Factory" 且不存在队员出生点，GameMode 会在登录前生成最多 4 个临时部署出生点（做地面射线、检查传送落点）。

### 7.4 StartMission 之前的两道门

1. **就绪门。** 非大厅地图上，GameMode 启动 0.25 秒循环的 `ReadinessTimer`（`CheckOperatorReadiness`）。客户端上报加载完成（`AShooterPlayerController::Server_MissionLoaded`，由 PlayerController 任务流分片里的 `ServiceMissionReadiness` 服务）。当连上人数 ≥ `ExpectedOperators` 且全员就绪，阶段推进，HUD 状态行显示“等待全员加载：x / y”（就绪 / 预期）。
2. **简报门。** `StartBriefing` 把阶段设为 `Briefing`。非工厂图走 `BriefingTimer`（`BriefingDuration`）进入 `StartMission`。**工厂图刻意不用这个定时器**：它等每个队员的部署视角就绪上报（`Auth_DeploymentViewReady`）—— 无人机侦察部署演出就挂在这道门上 —— 人齐后才调用 `StartMission`。

### 7.5 AShooterPlayerController 的任务流分片

`Private/ShooterPlayerController_MissionFlow.cpp`（2026-11-14 由误导性的 `MissionFlowController.cpp` 正名 —— 并不存在那个类）装的是 PlayerController 的任务那一半：`Server_MissionLoaded`、`ServiceMissionReadiness`、`InitializeMissionInput`、结算动作（`Server_DebriefAction` —— 房主重试 / 返回大厅）、配装保护 RPC，以及 `EndPlay` 时的就绪清理。

### 7.6 刷出对面

`StartMission` → `SpawnHostilesFromLevel`：GameMode 遍历关卡内每个 `AHostileSpawner`，调用 `Auth_SpawnHostiles(HostileClass)`。Spawner 是**配置点，不是随机散布**：它沿自身右方向按 `SpacingCm` 间距一排刷出 `Count` 个敌人，朝向与 Spawner 一致，使用延迟生成，以便在 `FinishSpawning` 前把类别、小队角色（8.3）、身体网格（从 `LookPresets` 随机）、武器（从 `WeaponClasses` 随机）都设好。

### 7.7 目标与撤离

`AMissionObjective` 是可交互目标的基类；`AFactoryMissionObjective` 为工厂任务的目标类型做特化。目标只在任务 `InProgress` 且当前图不是大厅时生效（`IsMissionLive` / `bRangeHub` 检查）。工厂任务（「锈蚀车间」）：

- **账本**：任务开始时，在三间办公室（财务室 / 档案室 / 经理室）的摆放点中随机选一处生成 Ledger 类型的 `AFactoryMissionObjective`。
- **工作台**：三处查封目标（改装间 / 组装间 / 弹药间）。
- **隐藏任务**（不在目标面板显示，触发时以配音播报）：无声行动（拿到账本前不触发警报）、地下配电室拉总闸、出货单（调度室的手写出货单）、锅炉房的秘密（藏着的成品枪箱）。
- 撤离完成任务并打开结算（6.3）。

## 8. 嫌疑人 AI

### 8.1 AHostileAIController —— 大脑

正经的 `AAIController`，带显式脑状态：**Idle / Investigate / Combat / TakeCover / Retreat / Flee / Surrender**。感知为视觉 + 听觉（经 `UAINoiseEvent`）：枪声在 4500 内可闻，脚步 900。丢失视线后有 3.5 秒宽限才降级。战斗中会找掩体，并在**首次中弹**时掷士气：投降 0.30、困兽 0.25、逃跑 0.45（以代码中的调校值为准）。投降的嫌疑人可被玩家押解（计分的目标类别之一；击杀已投降者计入报告惩罚）。

### 8.2 AHostileAICharacter —— 身体

控制器驱动的 pawn：生命/部位、武器操作、投降/失能状态、押解互动。在任务中只经由刷怪点（7.6）产生。

### 8.3 小队与瞄准

`UHostileSquadSubsystem`（世界子系统）在生成时发放小队角色：**压制者**守住位置、在牵引范围内持续开火；**侧翼手**绕向玩家侧面。瞄准按部位加权 —— 腿 70%、胸 25%、头 5% —— 这也是压制火力更容易致伤而非致死的原因。

### 8.4 调校面

三处：刷怪点摆放 + `Count`/`SpacingCm`/类别池（关卡侧）、感知半径与士气概率（控制器）、小队角色配比（子系统）。行为问题通常出在摆放（刷怪点朝向、掩体可达性、NavMesh），不是架构缺失。

## 9. 配音系统

### 9.1 Cue

全部任务配音由 `EMissionVoiceCue` 枚举寻址（`MissionVoiceTypes.h`）：`Briefing`、`Opening`、`HiddenSilent`、`HiddenShipping`、`HiddenBreaker`、`HiddenCrates`、`BenchSecured`、`CivilianSecured`、`LedgerFound`、`RoomClear`、`SuspectDown`、`SuspectIncap`、`TargetSecured`，以及六句喝令（`ShoutHandsUp`、`ShoutShowHands`、`ShoutSeeHands`、`ShoutDontMove`、`ShoutDropWeapon`、`ShoutGround`）。`MissionVoice` 命名空间负责 cue → 资产名（`AssetName`）、预期时长（`Duration`）与 `LoadSound` 的映射；`NextWarning` 负责喝令变体的不重复轮换。

### 9.2 UMissionRadioComponent —— 播放

带队列的电台：指挥官台词与上报以“通话”形式播放（带优先级/打断规则 —— 如新简报会切断当前通话）；喝令则由队员角色在现场喊出，不走电台。组件的持有者：任务台（简报）、目标物（上报/隐藏触发）、敌人/人质互动（喝令与制服/击倒上报）、GameMode。

### 9.3 资产

工作语言为英文。选角（全游戏锁定）：指挥官 —— 低沉/克制的男声，电台处理（300–3200 Hz 带通、压缩、静噪音）；队员 —— 有力度的中年男声，喝令用带驱动与颗粒感的链路，上报走电台链。WAV 直接导入 UE。已知欠账：`HiddenShipping` 与 `HiddenCrates` 的音频仍是旧台词版本，重录待进行（只需换资产，不动代码）。

## 10. 组队与在线

`UPartySubsystem`（GameInstance 子系统）封装 EOS 会话/大厅，服务最多 4 人的小队：创建/加入、名册，以及让队伍在 `ServerTravel` 中保持完整的旅行协调。互联网联机基于 EOS（不依赖 Steam）；`OnlineSubsystemSteam` 出于历史原因仍在项目文件中启用，但不是游玩路径。局域网/本地测试走同一套监听服务器流程。

## 11. 配装与配件

- **靶场配装台**（`ARangeLoadoutBench` + `URangeLoadoutWidget`）：大厅里的全屏配装。`ALoadoutPreviewRig` 展示穿戴当前装备的干员。
- **携行具背包**：`UPouchInventoryComponent` —— 正好 13 格。
- **弹匣是队列。** 换弹按队列顺序取下一个弹匣；卸下的（可能未打空的）弹匣回到队尾；打空的弹匣退出队列。HUD 严格按队列顺序渲染（当前弹匣在最前），且只在检视弹匣（长按 R）时显示。
- **配件**：数据驱动（`UAttachmentData` / 枪匠定义）：枪管、护木、枪口、瞄具、握把、枪托、战术设备、弹匣。武器行为由 `UWeaponData` + 已装配件共同决定。
- **持久化**：`UWarehouseLoadoutSaveGame`（预设）、`UMissionBestSaveGame`（各任务最佳成绩）。（框架时期的 `UShooterLoadoutSave` 只存在于 `master`。）

## 12. 世界可交互物

- **ABreakableDoor**：按区域的开门方式 —— 把手、推、踹 —— 带可破坏门板。（收尾清单上的已知 bug：踹门目前朝玩家方向开；嫌疑点是 `ResolveOpenDirectionAwayFrom` 的开门方向符号。）
- **AMissionDeployPoint**（7.1）、**AMissionObjective / AFactoryMissionObjective**（7.7）、**AHostageActor**（押解/救援）、荧光棒（HUD 上计数的荧光棒）、梯子、绊线陷阱、油桶、血泊。
- `UTraversalComponent` 是已移除的翻越/攀爬功能的残留 —— 无引用，计划删除；不要在它上面搭新东西。

## 13. UI 系统

设计语言（已锁定）：近黑半透明面板、米白文字 `#EDEEF0`、单一警示红、Barlow Condensed / Rajdhani + 思源黑体、直角、无金色常驻 UI。

- **HUD**：只有左下设备状态条（荧光棒数量、夜视/手电/镭射以图标亮度 + 圆点表示状态）；弹匣队列只在检视时出现（第 11 节）。刻意**没有**常驻数字弹药计数。
- **MissionFlowWidget**：选关、状态行（等待全员加载 x/y）、目标、结算（6.3）。
- **平板**（Tab）：3D 平板，屏幕是运行简报布局的 Widget Component（浅色主题、内容在边框内满幅；LED 感来自屏幕亮度 + 泛光，不是点阵）。
- **加载**：两段式 —— 按地图的无人机侦察视频（Sequencer 渲染，经 MoviePlayer 加载框架播放，侦察 HUD 框为控件层；工厂侦察序列资产 `LS_Factory_Recon_V3`），10 秒一圈、结束于出生点正上方悬停；随后关卡内以极简状态行等 7.4 的两道门走完。
- **结算与配装界面**：是既有数据流（6.3、11）的换皮 —— 没有平行的数据管线。

## 14. 配置层

- `Config/DefaultEngine.ini`：`GameDefaultMap` = 主菜单图；`GameInstanceClass` = `BP_ShooterGameInstance`；`GlobalDefaultGameMode` = `/Script/FPS.ShooterGameModeBase`（菜单/基类模式 —— 任务在旅行时逐次覆盖）。框架时期的自定义碰撞通道保留。
- EOS 凭据/产品 ID 在项目的 EOS 配置中（不在本文档）。
- **Maps to Cook**：任何被任务台提供的任务图（含工厂图）必须进入打包的 Cook 列表，否则运行时选关会在 `DoesPackageExist` 检查处失败。

## 15. 测试

自动化测试位于 `Source/FPS/Private/Tests`，主要是 `MissionFlowTests`（部署/就绪流程，含 `ExpectedOperators` 处理）与 `FactoryVoiceTests`（cue/资产接线）。动部署链或配音表之后，从编辑器的 Automation 窗口跑一遍。

## 16. 注意事项与已知问题（2026-11-14）

- **尚未打包**：Development/Shipping 打包验证未做；当前内容的纹理流送池超标（性能专项待进行）。
- **bRangeHub 靠名字猜**（6.1）是任务流里最锋利的已知脆弱点。
- **踹门方向**（第 12 节）朝玩家开；修复方法已定位，未实施。
- **工厂图内的 AI 调校**（刷怪点朝向、掩体、NavMesh）进行中；架构（第 8 节）本身是完整的。
- **未入库的 EOS 插件**：OnlineSubsystemEOS 插件文件在本地存在但尚未提交进仓库 —— 全新克隆在它们入库前无法构建在线路径。
- **两句配音**（9.3）待按最终台词重录。
- `master` 分支是竞技场游戏的只读存档；不要把它合并进合作分支。
