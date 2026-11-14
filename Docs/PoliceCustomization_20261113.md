# Police 换装 · SKM_Preset_07

入口：靶场改枪台 → 仓库 → 角色防护。可选择闪光防护眼镜、眼镜＋防弹面罩、防毒面具，以及轻甲／重甲。选择立即更新玩家与仓库预览，服务器同步给其他队员；本机选择保存在 Saved/SaveGames/PoliceAppearance.sav，重新进入和生成角色后恢复。

角色资产位于 `/Game/FPS/Characters/Police`，配置为 `DA_PoliceAppearance`。基础模型来自 Police 的 SKM_Preset_07，模块绑定到现用 `/Game/Bandits/Mesh/SKM_Bandit_Skeleton`。骨骼名称和层级保持一致，现有持枪动画、第一人称手套及武器插槽沿用原流程；袖子匹配黑色警服。

轻甲使用轻型背心＋轻型护板，重甲使用重型背心、护板、护颈与护肩。基础预设中自带的眼镜和护甲材质区段会隐藏，防止重复叠戴。模块使用 Leader Pose 跟随身体，不单独运行动画蓝图；外观切换不重置身体动画。盔甲数值继续走原 BodyArmor；两种眼镜继续连接原防闪光效果。防毒面具本轮连接外观，未新增毒气伤害系统或面罩防弹数值。

原包仍引用 `/Game/MMC`，FPS 模块启动时将其映射到实际 `/Game/Police/MMC`。已复制并接好本项目使用的材质，不删除原始 Police 资产。`Scripts/SetupPoliceAppearance.py` 可重建项目资产与配置。原始编辑器未保存的改动在重启前备份、保存，记录在 `Saved/PoliceCustomization/Before`。

验证记录与截图：`Saved/PoliceCustomization`。换装组合测试及旧任务回归共 6 项自动测试通过；双客户端独立选择、复制同步与持枪姿态通过。最终报告见 `Saved/PoliceCustomization/Validation.json`。
