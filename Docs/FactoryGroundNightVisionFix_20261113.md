# 工厂地面与全黑夜视修复（2026-11-13）

## 地面

保留关卡中 Floor_0 用户调整后的 Z=20。修正以下网格朝内的三角形朝向和法线，保留原有 UCX 碰撞、材质和模型尺寸：

- /Game/LevelRepairs/Factory/SM_FactoryFloor_Stairwell
- /Game/LevelRepairs/Factory/SM_FactoryDeployment_Apron

修复前向下追踪，出生区简单碰撞 Z=0、可见三角面 Z=-20；厂房内分别 Z=20、Z=0。玩家站在碰撞面上，从上方却看到地板底面，产生约 20 cm 浮空。修复后两种追踪高度一致，厂房内实际蒙皮靴底 Z=19.729，地面 Z=20。未调整胶囊体、玩家相机或角色网格偏移。

Scripts/FixFactoryGroundNormals.py 是带备份和元数据防重复翻转的修复脚本。原资产备份在 Saved/FactoryGMFix/GroundBefore。ArtSource/FactoryRepair/build_repairs.py 的 box 三角面顺序也已修正，避免未来重建再次产生反面地板；本次仅更新上述两个现有资产。

## 夜视

AShooterCharacter 新增 NVGIlluminator（相机附属的本机聚光补光）。现有绿色后期材质保留。全黑时提供实际可被材质成像的场景照明，不改动电闸控制的灯具或手电开关。

- 3 cd，40 m 衰减半径，内/外锥角 50/65 度，动态阴影保留墙体遮挡。
- 间接光和体积雾散射强度为零，避免照明残留和雾中光束。
- 组件不复制，只由本机控制角色在 NVG 且电池有电时开启。关闭、热成像、失去本机控制或电池耗尽时关闭。
- 现有电池时长及夜视强光过曝效果保留。

## 验证

FPSEditor Development 编译通过。13 项既有自动测试全部成功，见 Saved/FactoryGMFix/GroundNVGAutomation.json。

关闭关卡中所有常规灯光的 PIE 极暗测试：关闭夜视画面为黑，开启后可辨认走廊、门与障碍物。截图位于 Saved/FactoryGMFix/BlackoutOff.png、CorridorNVG3.png。测试灯光变更仅发生在 PIE 世界。

双人主机/客户端检查见 Saved/FactoryGMFix/NVGNetworkQA.json。地面测量工具 fps.DebugPlayerGround 仅编辑器可用，输出 Saved/PlayerGround.txt；包含实际 CPU 蒙皮最低点、胶囊底部、简单/复杂地面追踪高度。
