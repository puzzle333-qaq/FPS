# RAR 单身体俯仰修复与动作差距交付

依据：《单身体试点：身体俯仰耦合（低头弯腰）修复方案》（用户附件，日期 2026-11-16）。用户明确试点武器为 **RAR**。

第一包已修改身体 ABP、保存并编译。第二包只完成静态盘点，遵循原文“第一包验收后再动本包”，尚未改持枪姿势或动作表。PIE 视觉验收仍由 puzzle 完成，下面没有把静态检查写成 PIE 通过。

## 第 0 步：四项结论（修改前）

1. **实际身体资产**：BP_ShooterCharacter 的 Mesh 使用 `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple`，Anim Class 为 `/Game/FPS/Character/ABP_ThirdPerson.ABP_ThirdPerson_C`。试点开关为 true。
2. **Aim Offset 接线**：身体 ABP 的 `Aim Offset` 状态存在 RotationOffsetBlendSpace 节点。BlendSpace 输入来自 `CurrentAnim.AimOffset_Hip`，不是写死资产；`AimOffset_Aim` 没有消费者。Y 接拆分后的 `AimRotation.Pitch`，X 未暴露/未连接。另一个关键断点在 AnimGraph 的 `AnimGraphNode_LayeredBoneBlend_1`：把 UpperBody 混入 LowerBody 时，只包含 `clavicle_l`、`clavicle_r`，脊柱变换被排除，无法保留弯腰。
3. **俯仰数据写入**：不是无人写入。`BlueprintThreadSafeUpdateAnimation` 的 Property Access 读取 `Shooter Character.GetFixedAimRotation` 并写入 `AimRotation`，后续更新 IsInAir。该路径没有按文档求 BaseAimRotation 与 ActorRotation 的标准化差值。已有 `AO_Yaw` 来自角色转身偏移，还用于 OrientationWarping，不能直接改作新的视线相对 Yaw 而影响行走。
4. **RAR 数据**：RAR 的 WeaponType 为 `Weapon.type.rar`；ThirdPersonAnims 已有同名条目，ResolveThirdPersonPoseTag 直接命中，无须退回 AK。`AimOffset_Hip`、`AimOffset_Aim` 均为 `/Game/FPS_Controller/Animations/TPP_Anims/AR/Turn/AO_TPP_Ak`，都非空，因此未执行第 1 步补数据。

四项在实际保存修改之前已向用户报告。完整节点和数据快照位于 `Saved/BodyPitchAudit/audit.json`；修改前文本导出为 `Saved/BodyPitchAudit/ABP_Before.t3d`。

## 第一包实际修改

唯一修改的既有资产：`Content/FPS/Character/ABP_ThirdPerson.uasset`。

| 位置 | 改前 | 改后 |
| --- | --- | --- |
| EventGraph 每帧更新 | 更新 Gait、Lean、Prone 后结束 | 在现有 Shooter Character 有效分支末尾，取 GetBaseAimRotation、GetActorRotation，经 Delta(Rotator) 标准化后写入 AimRotation |
| ThreadSafeUpdate 中 AimRotation 写入 | PropertyAccess → Set AimRotation → Set IsInAir | 保留后续 IsInAir 更新，移除旧 Set AimRotation，避免覆盖新相对值 |
| Aim Offset.BlendSpace | 只读 AimOffset_Hip | Select 按 IsAiming 在 CurrentAnim.AimOffset_Hip/AimOffset_Aim 中选择 |
| Aim Offset.Y | 原始 AimRotation.Pitch | 标准化后的 AimRotation.Pitch，经 [-90,90] Clamp |
| Aim Offset.X | 隐藏、默认 0 | 暴露并连接标准化 AimRotation.Yaw，经当前 RAR TPP 资产的 [0,100] 范围 Clamp |
| UpperBody → LowerBody 的现有 Layered Blend | clavicle_l + clavicle_r，BlendDepth=0 | spine_01，BlendDepth=0，上半身完整保留，腿部仍取 LowerBody |

复用已有的 `AimRotation` 旋转变量存放 Pitch/Yaw 两个分量，没有再加重复的 AO_Pitch/AO_Yaw 变量；保留原 `AO_Yaw` 及 OrientationWarping 逻辑。没有重做状态机、FABRIK 或姿势数据。

原 AO_TPP_Ak 的 Y 轴范围为 [-90,90]，三个样本分别位于俯仰 -90/0/90；所有样本 X 都为 50。因此这份现有资产有俯仰样本，没有左右瞄准的独立样本。本轮修正 Yaw 数据与输入范围，不声称新增左右扭腰动画。

新增实施脚本：`Scripts/FixBodyPitchCoupling.py`。该脚本面向修改前基线，已执行完成，不要在修复后重复执行；脚本会在节点基线不符时停止，不自动覆盖其他结构。

## 验证与保护范围

- 修改后身体蓝图编译：0 节点错误、0 节点警告，保存成功。
- 在另一次 UE Python commandlet 中重新加载资产，创建临时角色与控制器，调用实际 BlueprintUpdateAnimation：输入 Pitch -60/0/60/300，AimRotation.Pitch 分别为 -60/0/60/-60，断言通过。这是变量驱动验证，不是骨骼最终求值、渲染或 PIE 视觉验收。
- 没有新增或修改 C++。本轮前后 SHA-256 核对一致的文件：ShooterCharacter.cpp/.h、CombatComponent.cpp、ABP_FirstPerson.uasset、BP_ShooterCharacter.uasset、DA_WeaponData.uasset。
- 未修改 RefreshHybridBodyVisibility、SetMeshVisibility；未修改 1P 动画资产或数据，也未修改其他武器条目。
- 原身体 ABP 备份：`Saved/BodyPitchAudit/Before/ABP_ThirdPerson.uasset`。
- 编译及修改结果：`Saved/BodyPitchAudit/fix_result.json`。
- 数值验证与蒙太奇骨骼审计：`Saved/BodyPitchAudit/runtime_after.json`。
- 日志：`Saved/Logs/BodyPitchFix.log`、`Saved/Logs/BodyPitchVerify.log`。

## 第 3 步：PIE 逐项状态

| 验收项 | 状态 | 当前证据 / 需观察现象 |
| --- | --- | --- |
| 第三人称 -60 到 +60 俯仰、枪口对齐、Yaw 不跳 | 待 puzzle PIE 验收 | 已验证相对 Pitch 数值；脊柱、枪口最终姿态未做视觉判定 |
| 单身体第一人称低头、抬头 | 待 puzzle PIE 验收 | spine_01 层接线已保存；需确认相机附近没有穿模 |
| 开镜切换、不抖不跳 | 待 puzzle PIE 验收 | IsAiming 选择已连接；RAR Hip/Aim 当前引用同一 AO，属于文档允许的试点用法 |
| 关闭试点后的 1P、行走/侧移、队友视角 | 待 puzzle PIE 验收 | 1P 与角色可见性文件哈希未变；未运行双客户端 |

## 第 5 步：RAR 完整动作差距表（只盘点，尚未执行第二包）

这里“本地播放”区分代码确有本地身体调用与实际可见动作；没有运行 PIE 的项目均明确标记待验证。类别：(a) 有资产但接线/槽位/路由有问题；(b) 需要填正确的现有 TPP 或已重定向资产；(c) 只有 1P 来源，需要先确认骨骼并考虑上半身复用/重定向；(d) 没有确认可用的身体资产，暂跳过。非缺失条目记为“有数据”。

| 动作 | 1P 来源 | 当前 3P 条目/图 | 本地身体是否实际播 | 差距与后续路线 |
| --- | --- | --- | --- | --- |
| 持枪 Idle | FirstPersonAnims.rar：A_RAR_FP_PCH_AssaultRifle_Idle_Pose；Grab 为 Grip_Vertical 版 | ThirdPersonAnims.rar：A_TPP_AR_Idle_Pose_G1；Grab 为 G2 | 有连续身体姿势路径；PIE 待验证 | 有数据，但与 1P 家族观感不同；第二包第 6 步选姿势并验 FABRIK |
| 瞄准 Idle | FirstPersonAnims.rar：A_RAR_FP_PCH_AssaultRifle_Aim_Pose，另有 Grab | 3P AimIdleAnim 为 A_TPP_AR_Ironsight_Pose_G1；当前 Aim Offset 基础 SequencePlayer 仍只接 IdleAnim | AimIdle 字段在该状态图未被消费；PIE 待验证 | (a) 已有 TPP 姿势，后续补按 IsAiming 选择基础姿势；本轮只修 AO 选择 |
| 首次出枪 | FirstPersonMontages.rar.FirstEquipMontage：AM_RAR_Equip | 3P FirstEquipMontage 空，回退通用 EquipMontage | Local_CycleWeapon 明确会调本地身体 PlayEquipMontage；PIE 待验证 | (b) 区分首次/常规缺口；库有 AM_TPP_AK_Equip，可先作试点回退，不声称有独立首次版 |
| 常规出枪 | FirstPersonMontages.rar.EquipMontage：AM_RAR_Equip_Quick；武器 ArmsEquipSequence 也有源序列 | 3P EquipMontage：Anim_Arms_AssaultRiffle_EQUIP_Montage，DefaultSlot | 本地身体调用存在，是否正确姿态待 PIE | (b) 可改成实际 TPP AM_TPP_AK_Equip；需要手枪路径/时序验证 |
| 腰射开火 | FirstPersonMontages.rar.FireMontage：AM_RAR_Fire；ArmsFireSequence 有 A_RAR_*_Fire | 3P FireMontage 同为 AM_RAR_Fire，骨骼为 SKEL_IG_Mannequin | Local_FireWeapon 调 PlayOwnerThirdPersonFireMontage，调用存在；PIE 待验证 | (b) 库有 Manny 同族 AM_TPP_AR_Shot；现引用的兼容性与表现不能仅凭名称判定 |
| 开镜开火 | 主蒙太奇仍 AM_RAR_Fire；仅蒙太奇缺失才用 ArmsFireAimSequence 的 Fire_Aimed | 3P 仍共用 FireMontage，没有独立 ADS 字段 | 本地身体调用存在；PIE 待验证 | (b) 可以共享 TPP Shot 并叠加 AimOffset；不凭空增加现有 FMontageData 没有的字段 |
| 空仓换弹 | FirstPersonMontages.rar.EmptyReloadMontage：/Game/FPS/Animation/RAR/AM_RAR_Reload_Empty，已是 FPS_Controller 的 SK_Mannequin | 3P EmptyReloadMontage 指向 Infima 原始 AM_RAR_FP_PCH_AssaultRifle_Reload_Empty；其槽为 Action Standing/Aiming/Running/Lowered，身体图没有对应 Slot | 有身体换弹解析路径，但这些槽无对应身体消费节点；PIE 未测 | (a)/(b) 修槽/引用；优先审计已有已重定向 Manny 换弹或 TPP Quick_Reload，避免生搬 RAR 源 |
| 战术换弹 | FirstPersonMontages.rar.TacticalReloadMontage：/Game/FPS/Animation/RAR/AM_RAR_Reload，SK_Mannequin | 3P TacticalReloadMontage 误填 AM_RAR_Fire | 代码会解析到开火资产；正确换弹表现不能成立，PIE 未测 | (b) 明确数据错误；第二包只修 rar 这一条目的对应字段 |
| 普通检视 | FirstPersonMontages.rar.InspectMontage1P：AM_RAR_Inspect；当前自由检视入口已经取消 | 3P InspectMontage1P 空；没有身体检视调用 | 当前代码没有普通检视身体路由，PIE 未测 | (c)/(d) 有 1P 来源但未确认安全的 TPP 检视；不能通过填一个字段就声称完成；不擅自恢复已取消的按键 |
| 弹匣检视（长按 R） | Initiate_VersionWeapon：RAR 走 ResolveMagCheckMontage，优先 RAR MagCheck 蒙太奇/序列 | 不读 ThirdPersonMontages 检视字段；只取得玩家 Mesh1P AnimInstance | 当前没有本地身体检视调用 | (a)/(c) 有 1P 动作但身体路由缺失；先审骨骼，RAR 源不得直接套 Manny；需可用 Manny 重定向版本才可上半身分层 |
| 冲刺姿势 | FirstPersonAnims.rar.SprintAnim：A_RAR_FP_PCH_AssaultRifle_Sprint_F_Loop | 3P SprintAnim：A_TPP_AR_Sprint；现有 bIsSprinting 控制分支 | 有连续身体冲刺路径；PIE 待验证 | 有数据，第二包第 6 步对齐观感并验握把，无需新建播放系统 |
| High/Low Ready | 本次没有在 FPlayerAnim 中确认对应专用字段；武器通用 Ready 行为不能当作已配姿势 | 3P 条目同样未确认独立姿势字段 | 未确认，PIE 未测 | (d) 暂不改，不伪造现有数据结构没有的槽 |

路径缩写：A_TPP_AR_*、AM_TPP_AK_Equip、AM_TPP_AR_Shot、AM_TPP_AK_Quick_Reload_G1 均来自 `/Game/FPS_Controller/Animations/TPP_Anims/AR`。上述三个 TPP 蒙太奇实际检查到的骨骼均是 FPS_Controller 的 SK_Mannequin。

特别注意：本次实际读取的若干 RAR 角色动作骨骼名称为 `SKEL_IG_Mannequin`，并非文档举例中的武器骨骼名 `SKEL_RAR_AssaultRifle`。不能仅据“RAR”字样判定一定可用或不可用；第二包必须逐个检查源序列、重定向和兼容性。文档关于禁止把 RAR 武器骨骼动作写进 Manny 身体的红线仍完整保留。

## 第 6–8 步状态

未执行修改，等待第一包 PIE 验收。RAR 姿势、Equip/Fire/Reload/Inspect 数据以及武器系统均保持本轮修改前状态。

第 8 步的出枪、腰射开火、开镜开火、空仓换弹、战术换弹、普通检视、弹匣检视、冲刺，以及关闭试点/队友视角/其他武器回归，均未执行 PIE，不标记为通过。第一包验收通过后，按上表逐项选择接线、TPP、已确认安全的 Manny 上半身分层或无资产跳过，并记录手与枪的全程对齐结果。

## 后续修复：相机随身体弯腰移动

用户反馈“会弯腰，但是相机锁在原地转而不是跟随头部”后，补充了相机位置跟随。这是后续相机修改，与上文第一包零 C++ 的历史记录区分。

- 原因：FirstPersonCamera 挂在胶囊体下的 SpringArm，相机位置没有读取动画骨骼；原先只转视线。
- 在 ShooterCharacter 的 CalcCamera 中读取已求值的身体锚点，更新实际相机组件位置，然后交由原相机逻辑计算视图。更新发生在 CameraManager 取视角时，避免从角色较早的 Tick 读取上一帧骨骼。
- 仅对单身体试点的本地第一人称/第一人称观战生效。调试第三人称、关闭试点时恢复原相机相对位置；死亡脱离相机和绑人质动作保留对相机的控制。
- 默认锚点 neck_01，角色空间眼位偏移 (0,0,14) cm。试点隐藏 neck_01 后，后代头骨会受零缩放影响，因此只读取锚点位置，不继承骨骼缩放或旋转。视线旋转仍由原 SpringArm/鼠标路径控制，保留侧倾 Roll。
- 原 UpdateLean 在跟随激活时不再把相机平移写回胶囊体原位；旋转逻辑继续执行。
- 添加 4 cm 球形相机碰撞检测，忽略本人和附着装备，限制眼位穿入阻挡几何。
- 可在 BP 默认值的 FPS → SingleBody → Camera 中调整 SingleBodyCameraAnchorBone、SingleBodyCameraEyeOffset、SingleBodyCameraProbeRadius。

变更文件：Source/FPS/Private/ShooterCharacter.cpp、Source/FPS/Public/ShooterCharacter.h、新增 Source/FPS/Private/Tests/SingleBodyCameraTests.cpp。没有再改动画蓝图、1P 资产或武器条目。

验证：FPSEditor Win64 Development 构建通过；FPS.SingleBody.CameraFollowsEvaluatedAnchor 自动化测试通过，覆盖隐藏颈部后的锚点跟随、三轴移动、保持视线旋转、第三人称/关闭试点恢复、阻挡几何和脱离身体后的相机。测试日志为 Saved/Logs/SingleBodyCameraTests.log，报告在 Saved/Automation/SingleBodyCamera。此测试不替代 RAR 在 PIE 中的视觉与手、枪对齐验收。

本次增加了原生类字段，需要重开编辑器加载新构建，再进入 PIE 检查低头/抬头时眼位是否随身体一起移动。若高度需要微调，调整 SingleBodyCameraEyeOffset.Z；不通过改 1P 动画来补偿相机位置。

## 第二包：RAR 身体持枪、ADS 与动作接入（2026-11-16 后续）

用户确认视角可用后，反馈开镜无效、动作未接、持枪与 Mesh1P 不贴合，随后实施本节。上文“第 6–8 步未执行”为先前阶段记录，本节取代该状态；视觉 PIE 验收仍单独保留。

### 实施结果

- 用 RAR 原始角色动作（SKEL_IG_Mannequin）经 IK Retargeter 重定向至实际身体 Manny Simple 的骨骼，生成 Content/FPS/Animation/RARBody 下的专用序列与蒙太奇；没有把武器骨骼动画直接写到身体。持枪/瞄准/垂直握把/冲刺姿势保留原 TPP 下半身骨骼轨道。
- 仅更新 DA_WeaponData 的 rar 身体姿势与身体蒙太奇：首次出枪、快速出枪、开火、空仓换弹、战术换弹、普通检视；新增对应 MagCheck 资源供长按 R 解析。修复原战术换弹误填开火资产。
- ABP_ThirdPerson 基础姿势按 IsAiming 选择 Idle/AimIdle；BodyUpperAction 槽从 spine_01 分层，按该槽的实际蒙太奇权重启用，未播放时保留完整旧图输出。
- 按原 Mesh1P 的 RifleSocket、hand_r 与挂枪规则标定 RAR 的身体挂枪变换；身体武器仍附着真实 hand_r。无前握把时使用标定左手目标，有前握把时读取实际 3P 配件的原作者 LH 握持点。
- 最终骨骼链增加右手 ADS FABRIK，再做左手握把 FABRIK（精度 0.01 cm，迭代上限 32）；换弹/检视让出手部控制，开火期间保留 ADS。
- ADS 读取可见 3P 枪/瞄具的真实瞄准 Socket，与眼距参数求右手目标。相机取景时对同帧动画求值产生的剩余横向偏差进行随 ADS 权重淡入的校正，保留原腰射头部跟随与眼位设置，最后仍经过相机碰撞检测。
- 镜内画面选择实际身体上的 3P 光学配件；关闭单身体试点时保留原 Mesh1P 路径。
- 身体、3P 枪械动作补上开火/换弹/检视路由。长按 R 保留弹匣检查；增加仅试点生效的 I 普通检视入口。检视记录实际播放实例用于打断，并增加服务器/代理的身体检视路由。

### 校验与边界

- FPSEditor Win64 Development 原生构建成功。动画蓝图编译无错误。
- 逐字段对照先前审计：全部 FirstPersonAnims、FirstPersonMontages 与非 rar 的 ThirdPersonAnims、ThirdPersonMontages 共 29 行未变；报告 Saved/RARBodyActions/isolation.json。
- 自动化加载真实身体动画图与 RAR 配件，验证蒙太奇骨骼/槽位/可播放性、ADS 输入与手部目标、实际瞄具位置、左手握把、检视/打断、开火期间 ADS 与退出试点。最终运行结果见 Saved/Logs/RARBodyTests.log、Saved/Automation/RARBody。
- 此次为 NullRHI 自动化，未进行渲染 PIE、移动/侧倾/全部俯仰角的视觉验收或双客户端联机验收；不能据此声称所有动作全程无穿模。镜内渲染与联机代理也需 PIE 验收。
- 重开编辑器加载新原生构建后，使用 RAR 检查右键开镜、腰射/开镜开火、空仓/战术换弹、I 检视、长按 R 弹匣检查、冲刺与打断；其他武器数据未随本包替换。
- 新资源位于仓库当前忽略的 Content/FPS/Animation/ 目录；资源已在本地磁盘生成。以后提交或迁移本包时需要明确包含 RARBody 子目录。

最终自动化结果：FPS.SingleBody.CameraFollowsEvaluatedAnchor 与 FPS.SingleBody.RARBodyPresentation 均成功。测试姿势中右手目标误差 0.0077 cm，最终瞄具相机空间 (20.002, 0.000, 0.000) cm，左手握点误差 0.0001 cm。代理检视打断同时停止身体与枪械蒙太奇并清理引用；双客户端视觉仍待验收。

## 2026-11-16 检查点后的左臂、弹匣修复

检查点提交 242c3bf：用户确认 RAR 左手手位、开镜正确，但左臂变形、换弹和弹匣检视仍不完整。提交只包含本任务相关代码、蓝图及生成的 RARBody 资源，未包含门、任务平板、TR15 等其他工作；仅本地提交，未推送。

后续工作区变更：
- 复制 1P 的范围缩小到 hand_l/hand_r 及手指，保留身体上臂、前臂与扭转骨的局部变换。左臂新增 TwoBoneIK，使用本帧 1P 肘部位置作为 Joint Target；禁止拉伸和无约束扭转，再单独还原正确的 1P 手腕组件空间旋转。右手目标与已确认的 ADS 逻辑不变。
- 单身体以隐藏 Mesh1P 枪为动画源时，强制该枪与骨骼配件 AlwaysTickPoseAndRefreshBones，避免可见 3P 枪和弹匣读取停滞的骨骼。
- RAR 备用弹匣原先只给 1P 创建，现同时为有备用插槽的 3P 枪创建。显示备用弹匣时按照父枪隐藏状态过滤，避免把隐藏 1P 的弹匣重新显示到单身体画面。
- 构建日志 Saved/RARVisual/arm_build.log；动画图检查 Saved/RARVisual/arm_graph.json。此节不以编译或定位误差代替左臂外观、换弹和弹匣检视完整视觉验收。

追加：检视选择 1P 时同时检查其动画实例是否存在；缺失时回退身体实例。最终原生构建通过；FPS.SingleBody.CameraFollowsEvaluatedAnchor、FPS.SingleBody.RARBodyPresentation 回归检查均通过（Saved/Logs/RARArmTests.log）。这两项不覆盖左臂蒙皮外观与全部换弹中间帧，视觉验收状态另行记录。

本次实际 PIE 检查：运行警察身体，启用试点后观察腰射左臂外形、空仓换弹中间帧及 AM_RAR_MagCheck 中间帧。手位测量：腰射左手相对 1P 误差约 0.00006 cm；弹匣检视约 0.00011 cm。主/备用弹匣 Socket 的 1P/3P 组件空间姿势一致。报告在 Saved/RARVisual/arm_hip_check.json、arm_reload_mid.json、arm_magcheck_mid.json。数值只说明目标一致，不覆盖所有视角、服装蒙皮与联机远端表现；不声明所有中间帧零穿模。

动作结束结果（Saved/RARVisual/arm_finish.json）：Ammo=30，IsInspecting=false，IsReloading=false，恢复持枪；隐藏源枪为 AlwaysTickPoseAndRefreshBones，1P 与 3P 均有备用弹匣实例。修复保留在检查点之后的工作区，未追加提交或推送。
