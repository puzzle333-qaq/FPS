# 装备页与结算页（2026-11-13）

本次按用户确认的两份规格实现。原规格保留在同目录。入口仍为靶场装备台、任务结束后的 MissionFlow；没有新增任务或装备配置管道。

## 装备界面
- `UFullScreenLoadoutWidget` 继承原 `URangeLoadoutWidget`，复用枪械目录、配件白名单、弹药/投掷物计数、服务器提交与 Warehouse 存档。
- 左栏总览、详情下钻、定制页、本地三套可命名预设；Esc 在详情返回总览，在总览离开装备台。
- 右侧局部 SceneCapture：黑底、白色正面光、红色轮廓光。关闭界面即销毁预览和捕获定时器；不另开动画蓝图。
- 玩家指定姿势来源 `/Game/Police/MMC/Demo/Animations/A_Pose_1`，复制为 `/Game/FPS/Characters/Police/A_Police_LoadoutPose`。角色继续用现有 Bandit 骨骼，通过 Compatible Skeletons 播放 Police 姿势。
- 配件图标直接填对应 `AttachmentData` 资产的 **配件图标 / Icon**。已装备行与展开候选条共用此字段；空值保留图框，不用改代码。行内图框 96×40，建议透明背景纹理，UI 保持比例。后续替换资产后重新打开界面即可。
- 不显示不存在的 AP/JHP 弹种、防具材质或覆盖数值。
- 切换查看队友时只读；预览读取队友的真实武器、已装配件与防护，携行数量通过现有 Pouch 组件复制。

## 防护装备代价（用户追加）
- 基础携行容量 13；重型防弹衣占用 1 格；防弹面罩（眼镜＋面罩）占用 1 格；叠加后 11 格。防毒面具不继承防弹面罩代价。
- **不扣武器配件槽**。预设和配件白名单不受影响。
- 重甲保持项目既有移速倍率 0.78；面罩新增倍率 0.95；叠乘 0.741。均在 ShooterCharacter 的 Armor 默认参数中可调整。
- 界面步进器、Pouch 入格及服务器提交共同限制容量；换装导致超限时先减少末尾投掷物，再减少副武器/主武器备用弹匣，界面提示调整。保留已装武器和配件。

## 结算界面
- 读取复制的 FMissionReport；黑底、中央评级弧线、左侧明细、用时与本地个人最佳。
- 完成的隐藏记录只在最终报告揭晓；未完成项仅显示数量，任务中不复制其名称。
- 动画约 2.2 秒，任意键或点击跳过；失败显示实际 Grade 和得分率。
- 主菜单只退出自己；设置复用已有界面；重试/返回总部由房主授权执行全队 Travel。已取消自动 20 秒返回。
- `MissionPersonalBest.sav` 按地图包名保存成功局最佳用时和最高得分；首次显示无纪录。

## 维护位置
- 界面布局：`Source/FPS/Private/Ui/FullScreenLoadoutWidget.cpp`、`MissionDebriefSlate.cpp`。
- 换装资产：`Content/FPS/Characters/Police/DA_PoliceAppearance`。
- 姿势配置脚本：`Scripts/SetupPoliceLoadoutPose.py`；原模型材质/模块配置：`Scripts/SetupPoliceAppearance.py`。
- 运行验证结果与截图：`Saved/ScreenRefresh`；先前模块换装的网络验证：`Saved/PoliceCustomization/Validation.json`。

## 局内第三人称握持修正（用户追加）
- 9 把 KINEMATION 枪补入从原始手臂 idle 姿势计算的左手握持目标；保留已经存在的握把/模型插槽的优先级。没有插槽时才用校准的武器局部坐标。
- X24 从缺失插槽后的 `ik_hand_gun` 回退改为 `hand_r`，偏移复用原有第一人称 PistolSocketX24 的手腕相对校准，左手目标使用原始 X24 idle。
- 局内第三人称切枪时，正确隐藏第一人称枪模并显示第三人称枪模；避免侧面出现浮空的第一人称副本。
- 角色预览复用实际第三人称挂载参数，避免 X24 在预览中再出现另一套偏移。
- `Scripts/CalibrateThirdPersonHandGrips.py` 可在编译后、PIE 外重跑。修改前备份在 `Saved/ScreenRefresh/GripBackups`，不修改源模型和动画。

## 本轮收尾验证
- Development Editor 编译成功；12 项自动化全部通过，含换装组合、携行容量、预设持久化和第三人称握持回归。
- 双人 PIE：服务器/客户端装备与容量一致，队友查看只读；结算 Report 同步；队员不能发起返回；房主重试后两人重连，返回后两人均在 Lvl_Range。
- 局内 Mk14/X24 均直接挂 hand_r，第一人称副本正确隐藏；左手与目标误差分别约 0.37 cm / 0.03 cm。
- 已停止 PIE，恢复原来的联机测试偏好、后台节流、Python 远程执行设置，编辑器回到靶场。测试新增存档备份在 Saved/ScreenRefresh/TestSaveBackups；既有 WarehouseLoadout 存档保留。
- 汇总结果：Saved/ScreenRefresh/Validation.json。未执行提交或推送。

任务文案、地图说明、武器/配件图标等内容仍从现有配置资产读取；当前两份界面的布局由原生 Slate 文件实现，布局调整在上述 cpp 中进行。

## 工厂 GM 持久化补修
工厂关卡 World Settings 的 GameMode Override 已显式保存为 `/Script/FPS.CoopCqbGameMode`，直接打开关卡运行时也会使用任务 GM，无需依赖测试启动参数。保存前保留了当前关卡改动；修改前的关卡备份在 `Saved/FactoryGMFix/Lvl_Factory_Whitebox.before-gm.umap`。

## 工厂直接进图的举枪等待修正
排查确认 Controller/Pawn 已正常生成并占有；真正阻塞为开场 Raise 等待。当地第一人称手臂在侦察镜头中不可见时，OnlyTickPoseWhenRendered 会停止动画更新，使举枪与视角恢复流程无法正常推进。
- 当地玩家装备手臂时保持 AlwaysTickPoseAndRefreshBones；远端不强制常驻更新。
- 部署举枪请求区分“已受理”和“蒙太奇正在播放”：可选举枪动画缺失时正常完成，不在每帧重复请求并无限锁输入。
- 新增 FPS.Mission.DeploymentRaiseFallback 回归检查；直接工厂 PIE 的自然进图验证存于 Saved/FactoryGMFix。
