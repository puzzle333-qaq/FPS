# 工厂美术替换与装饰交接 · 2026-11-11

已按 `05_美术白盒替换与装饰.md` 完成三个阶段，保存于 `/Game/Maps/Lvl_Factory_Whitebox`。本轮通过端口 8001 的 Unreal MCP 操作当前编辑器，未重跑旧白盒生成脚本。

## 已落地内容

1. **正式墙体**：车间、仓库、装卸区使用 Factory；办公室隔墙、转角和天花使用 InteriorKit；地下室使用 UndergroundTunnel 混凝土模块。保留 110×220cm 原门洞及已修复墙端边界。89 个旧灰盒墙、门楣和天花对象仅隐藏渲染，原网格、变换、材质和碰撞保留。
2. **厂房外形**：中央高跨双坡屋顶、两翼外屋面、立面封板、办公室窗和车间高窗、装卸铺台及 12×4m 雨棚、钢柱、通风设施、围栏、两处道路入口和原尺寸 PortaCabin。低翼外屋面提高到 Z=500cm，原室内天花和灯具位置保留，利用封闭夹层容纳原吊灯。中央车间墙顶缺面已补原尺寸 InteriorKit 内衬。
3. **分区装饰**：160 件室内现成包装饰，以及 4 件庭院托盘/桶。包括办公室桌面纸张、键盘、杯子、白板和书架；工业区高位管线、电缆和阀门；地下室管道、墙挂电箱、铁架和碎物。与旧物穿插或靠近门口的候选摆放已舍弃。

地图共 1,829 个 Actor，其中本轮新增 1,225 个（包含建筑拼块，1,203 个可见）；阶段 1/2/3 分别为 280/781/164 个。所有新增 Actor 均为 Scale=(1,1,1)。

## 原资产与碰撞策略

Factory 原生墙约 50cm 厚，无法直接装入已验收的 20cm 墙体。本轮将墙、屋顶、天花、窗墙和场地地面制作独立裁切副本，导入 `/Game/FactoryArt/WallAdaptations` 与 `/Game/FactoryArt/ArchitectureAdaptations`，共 116 个新结构网格资产；保留源 UV 和原包材质，未覆盖外购资产。装饰道具全部使用现成包网格，没有新做道具网格。

室内视觉替换层为 NoCollision，由原已验收碰撞承担门洞、墙体、屋顶和楼梯通行。新增地面、围栏、活动房、雨棚立柱和落地家具按用途设置碰撞。窗沿用原墙碰撞，本轮没有新增可穿越窗洞；门卫房和其门为外观布景，没有增加交互逻辑。

30 扇原门、三处楼梯及扶手/侧碰撞、钢走廊和井口护栏、现有掩体及灯光状态均保持。43 组约 80–86cm 半身掩体继续沿用用户批准的高度，不替换、不缩放；灯光位置、亮度和颜色保持本轮开工基线（含上一轮已移动的 FD_094）。

## 验证结果

- 地图保存并重新加载后，604 个原对象的记录状态均与开工实时基线一致（比较变换、网格引用、组件变换、材质引用、碰撞配置、门参数/合页、灯光位置/颜色/亮度；旧灰盒组件渲染隐藏为预期改动）。
- PIE 中 30 扇门分别完成推开、关闭、踢开三轮测试，角度为 90°/0°/90°。
- 三处楼梯双向行走共 6 项全部通过。测试 CharacterMovement 胶囊半径 42cm、半高 96cm、最大台阶 45cm。
- 新装饰和实心配套物共 265 项进行 AABB 净距检查：距完整门口 120cm、出生/刷怪/撤离点 100cm，告警 0。正式墙层未侵入现有门洞。
- 新增建筑位于原场地范围；仅地下室替换墙皮沿用已验收外墙面 X=6210 / Y=-4610cm 的原边界。
- 20cm 网格、34cm 胶囊半径的平面路径预检：A 到 B、L1、L2、L3 均可达。该结果是静态路径预检，不冒充完整 AI 或多人实战验证。
- 外观、三间办公室、车间及地下室透视已检查，所查墙顶缺口已封闭。截图采用 Unlit 方便检查材质与几何；本轮未调灯、未做最终光照烘焙或性能验收。

## 清单与结果文件

- [逐物件改动清单（1,292 行，含修改原因）](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/factory_art_changes.csv>)
- [全部对象包围盒（1,829 行）](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/factory_art_all_actor_bounds.csv>)
- [摆设包围盒（509 行，含原有摆设）](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/factory_art_dressing_placed.csv>)
- [604 个原对象状态复核](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/FactoryArt_AllOriginalsQA.json>)
- [净距检查](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/FactoryArt_SpatialQA.json>)
- [门运行时结果](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/FactoryArt_RuntimeDoors.json>)
- [楼梯运行时结果](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/FactoryArt_RuntimeStairs.json>)
- [平面路径预检](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/FactoryArt_RoutePrecheck.json>)
- [资产来源、结构适配说明和输出 SHA-256](<C:/Users/ASUS/Documents/Unreal Projects/FPS/ArtSource/FactoryArt/AssetProvenance.json>)

## 截图

- [厂房外观](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/FactoryArt_Views/01_exterior.png>)
- [财务室](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/FactoryArt_Views/02_finance.png>)
- [档案室](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/FactoryArt_Views/03_archives.png>)
- [经理办公室](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/FactoryArt_Views/04_manager.png>)
- [主车间](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/FactoryArt_Views/05_workshop.png>)
- [地下室](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/FactoryArt_Views/06_basement.png>)
- [场地俯视](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/FactoryArt_Views/07_top.png>)
- [车间反向检查](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/FactoryArt_Views/08_workshop_reverse.png>)
- [西侧楼梯与走廊](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/FactoryArt_Views/09_workshop_west.png>)

## 备份与后续使用

开工前地图备份：[FactoryArtStart](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/AssetBackup_20261111_053615_FactoryArtStart/Lvl_Factory_Whitebox.umap>)。阶段 2、3 前的备份位于 `Saved/AssetBackup_20261111_063733_FactoryArtStage2` 与 `Saved/AssetBackup_20261111_064726_FactoryArtStage3`。

[实时开工基线](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/FactoryArt_LiveBaseline.json>)；[结构适配源文件与重建脚本](<C:/Users/ASUS/Documents/Unreal Projects/FPS/ArtSource/FactoryArt>)。脚本属于本轮制作记录，继续编辑应以当前已保存地图为准，不要整体重跑生成脚本。未提交 Git，也未推送；工程内原有代码和其它地图改动未纳入本轮。
