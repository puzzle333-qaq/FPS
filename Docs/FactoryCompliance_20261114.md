# 工厂人质、拘捕与门交互（2026-11-14）

## 当前行为

- F 警告由玩家视线射线选择一名人质/嫌疑人，遮挡会阻断。警告没有 UI 提示。
- 人质先举手、再跪下；警员在其背后 155 cm 内、无遮挡时显示捆绑提示。F 开始，再按 F 可取消，离开、死亡或切换战斗动作也会中断。完成后才计入解救。
- 捆绑使用 Hostage_Set 的实际双人动作，FK 重定向到现有 Police 骨骼，固定跪姿下半身。操作者 1P、3P 与被绑者同速播放，默认总时长 3.6 秒。没有合成弯腰或手臂 IK 拉伸。
- 未被警告的人质受枪声/爆炸惊吓后寻找可达、未占用的躲藏点；桌下警告会先爬出，再举手跪下。
- 有效闪光使嫌疑人立即投降并生成一把有物理、可复制的掉落枪，重复投降不重复生成。
- 开门、关门、踹门均只播放 Mesh3P；所有门动作均原地播放，不吸附、不转向。踹门由服务器再次校验视线射线：250 cm 内直接命中门才能开始；不做强制脚部贴合。服务器在手/脚接触时驱动门并发送声音，动作期间锁定移动、视角和战斗输入，结束恢复。视线被其他物体挡住或没有瞄准门时不触发踹门。

## 默认动画与参数

资源目录：`/Game/FPS/Animations/Compliance`。

- `A_OperatorKneelBind1P`、`A_OperatorKneelBind3P`、`A_TargetKneelBind` 为当前捆绑三条资源。旧的 `A_OperatorBind*`、`A_TargetBind`、`SKM_Police_BindSleeves` 不再作为默认资源。
- `URestraintActionComponent.OperatorAlignment`：目标局部坐标 (-45.6933, 1.8707, 0) cm。
- `A_DoorOpenLeft3P` / `A_DoorOpenRight3P` / `A_DoorKick3P` 为门动作。普通开关门默认 1 秒，踹门默认 2.2 秒。
- `UDoorGestureComponent.HandleContactTime=0.8`、`KickContactTime=2.166667` 是原动画时间，实际按播放速度换算。关门倒放开门动作，相应倒算接触时刻。

## 关卡

工厂保存了 `Civilian_Office`、`Civilian_Workshop` 两名人质及各自 `HostageHidePoint`。对应两张桌子的碰撞副本位于 `/Game/FPS/Props/Hostage`，保留桌下空间。`FactoryNavigationBounds` 与 Recast 导航覆盖其路线。改动桌子/地面后需检查躲藏点与 Approach 的地面高度，并重新构建导航。

## 资源重建

编辑器停止 PIE 后执行：

1. `Scripts/SetupDangerComplianceAnimations.py`
2. `Scripts/SetupHostagePairedRestraint.py`（生成基础重定向器）
3. `Scripts/SetupKneelingFKRestraint.py`（当前安全跪姿版本）
4. `Scripts/SetupDoorGestureAnimations.py`

关卡需要重新布置时才使用 `SetupFactoryHostageCover.py` 和 `SetupFactoryNavigation.py`，避免覆盖手动摆放。`SetupStandingRestraint.py`、`SetupKneelingRestraintVictim.py`、`SetupBindSleeves.py` 是已弃用实验，不要作为当前制作流程执行。

## 验证入口

自动测试：`FPS.Compliance`（定向警告/遮挡/捆绑取消与完成、闪光弃枪幂等）和 `FPS.Mission.ObjectiveFlow`（完成捆绑后任务计分）。实机记录位于 `Saved/DangerPack`，不作为生产资源依赖。
