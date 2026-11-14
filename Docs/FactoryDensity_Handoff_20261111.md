# 室内加密度交接 · 2026-11-11

已按用户最新顺序完成：先接五个区域的连续管路并单独保存，再摆地面、桌面和墙面挂件，最后用 Unreal 实际网格包围盒做 QA。任务依据为 `06_室内加密度.md` 与 `factory_dressing_density_v1.csv`；配套说明中“先地面、后接管”的旧顺序由本次用户指令覆盖。

保存地图：`/Game/Maps/Lvl_Factory_Whitebox`。输入 **361/361 行全部落实**，没有漏项。共新增 **470 个原尺寸 Actor**：管线、管件和支架 143 个；地面、桌面及墙挂 327 个，含原尺寸支承、叠放、成组挂件等 109 个补充件。地图现有 2,299 个 Actor。全部新件 Scale=(1,1,1)，未新建道具网格，未覆盖外购资产。

## 管路

- 锅炉房：FD_178 延长到锅炉西侧，接口保留约 5cm；FD_179 沿北墙接到东墙，再通过原尺寸斜段接 FD_180，西侧增加贴墙供给端。该区域包含进水和沿墙检修两个回路。
- 主车间南墙：完整主管、三通及阀门，三条高位折返支管终止在 FD_062/063/064 料斗顶部上方约 3.2cm，不穿入料斗。
- 泵房：FD_198 西墙管经过西北角接入北墙 FD_199，并对接新增储液/机泵服务单元。实测发现的 5cm 管间隙已用原尺寸三通补接。
- 配电室：FD_210 向东西连续延长，两处分支停在现有高柜后接口前，不穿柜体，末端封帽。
- 楼梯间：FD_191 沿南墙向东西连续延长，托臂贴墙，端部封帽。

厂房管卡采用 Factory 原生钢支柱落地支承；地下室使用原生短托臂贴墙。厂房 4m 支柱保持原尺寸，低层顶段位于既有封闭屋面夹层内；支架为视觉配套，不改变旧通行碰撞。主管均使用原尺寸段，非模数短距用原生短管、法兰和管件套接，未拉伸。

## 地面、桌面和墙面

九间办公室、八个厂房区域和三间地下室均已按行补齐。开放办公室补入完整工位及台面物件；会议室补椅子、白板；休息室补沙发、茶几和杂志；仓库、作坊补货架、托盘、箱桶、工具车和工具箱；地下室补铁架、杂物、挂墙电箱和原生碎石网格贴花。办公走廊只加墙面件，保持地面空置。

实际尺寸与预估不符时，只移动新件。桌面物件使用实际台面高度，纸箱、托盘上的叠放按真实承托面落位。地面布置同时检查房间内部 110cm 通道，避免“离门够远但成组封路”。新件的最终坐标和原清单坐标都在实际摆放 CSV 中。

部分功能提示在指定包中没有对应的小型同名网格，采用现有原尺寸替代：Factory 小木箱提示使用 FPS_Controller 木箱，工具车使用 Factory 紧凑推车，线盘使用现成延长线卷盘，警示牌使用现成禁止吸烟牌，公告使用办公室原生夹板成组；小型泵提示使用 Factory 储液容器配机械管件，未强塞 636cm 的大型 Gas_Pump。具体网格逐行导出。线槽采用实测 150cm 原生段，没有将其放大成 2m。

新增 31 件全身掩体实高均 ≥160cm。新增半身标签的实际分组高度另列 QA：休息室沙发原生高约 71.6cm，按原尺寸作低矮遮挡，没有宣称达到 90cm 标准；原已批准的 43 组约 80–86cm 半身掩体保持原状态。弹药箱及托盘组用原尺寸有意叠放，不以缩放补高度。

## 验证

- 实际包围盒 QA：**0 告警**。检查新件 Scale=1、31 件全身掩体高度、门口 120cm（含 E1）、出生/刷怪/撤离点 100cm、地面墙距 10cm、账本柜周边 100cm、主车间旧掩体旁 120cm、楼梯口/头部空间、非有意穿插和窗面避让。
- 管路检查：真实渲染 AABB 接触/套接连通预检，加五区透视复核；锅炉房为两个设计回路，其余四区各一个连通组件，没有孤立管件。该方法不冒充流体仿真或逐三角形管腔验证。
- 110cm 通道：10cm 网格、55cm 半径、打开门的保守 AABB 路径预检通过，20 区、A/B 部署点和三处账本柜附近可达。目标可达判据为标记 150cm 内有可达点；这是路径预检，不替代完整 AI/多人实测。
- PIE：30 扇门分别推开、关闭、踢开，角度 90°/0°/90°；三处楼梯双向 **6/6 通过**。测试胶囊半径42cm、半高96cm、最大台阶45cm。
- 最终保存、重新加载并在交付前再次比较：**1,829 个原对象无差异**，包括门、楼梯、钢走廊、护栏、账本柜、掩体、旧管道和灯光。过程中曾检测到 Floor_0 偏到 Z=19cm，已备份该状态并恢复开工冻结基线 Z=0；最终复核通过。
- 灯光位置、亮度和颜色未作本轮调整。截图为 Unlit 材质/几何检查，编辑器已恢复 Lit、退出 PIE，并恢复原后台节流设置。本轮未做最终灯光烘焙或性能验收。

## 交付文件

- [实际新摆设清单及真实包围盒（470 行）](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/factory_dressing_density_placed.csv>)
- [全图实际包围盒（2,299 行）](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/factory_dressing_density_all_bounds.csv>)
- [逐行资产映射、坐标与原因（361 行）](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/asset_mapping_and_adjustments.csv>)
- [实际包围盒 QA](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/FactoryDensity_QA.json>)
- [110cm 通道预检](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/FactoryDensity_RouteQA.json>)
- [门运行结果](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/FactoryDensity_RuntimeDoors.json>)
- [楼梯运行结果](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/FactoryDensity_RuntimeStairs.json>)
- [冻结原对象交付复核](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/FactoryDensity_Protected_Delivery.json>)
- [保存地图及输入文档 SHA-256](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/manifest.json>)

## 截图

- [财务室（同机位）](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/views/02_finance.png>)
- [档案室（同机位）](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/views/03_archives.png>)
- [经理室（同机位）](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/views/04_manager.png>)
- [主车间（同机位）](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/views/05_workshop.png>)
- [地下室（同机位）](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/views/06_basement.png>)
- [前台](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/views/07_reception.png>)
- [开放办公室](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/views/08_open_office.png>)
- [货物仓](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/views/09_warehouse.png>)
- [锅炉房连续管路](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/views/P1_boiler.png>)
- [主车间到料斗的管路](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/views/P2_workshop.png>)
- [泵房管路与设备](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/views/P3_pump.png>)
- [配电室](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/views/P4_electrical.png>)
- [楼梯间连续管路](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Docs/FactoryDensity_20261111/views/P5_stair_room.png>)

## 备份和制作记录

开工备份：[FactoryDensityStart](<C:/Users/ASUS/Documents/Unreal Projects/FPS/Saved/AssetBackup_20261111_100536_FactoryDensityStart/Lvl_Factory_Whitebox.umap>)。管路单独保存后再进行地面阶段，阶段备份见 `Saved/AssetBackup_*_FactoryDensityDressing`。结构冻结基线为 `Saved/FactoryDensity_Baseline.json`。

最终原生资产与摆放计划在 [ArtSource/FactoryDensity](<C:/Users/ASUS/Documents/Unreal Projects/FPS/ArtSource/FactoryDensity>)；只读 FBX 用于测量原管件端口和玻璃区域，没有重新导入或覆盖源资产。实际包围盒导出和 QA 脚本在 `Saved/density_editor_common.py`、`Saved/qa_factory_dressing_density.py`、`Saved/density_route_qa.py`。不要整体重跑旧白盒或上一轮美术生成器。

未提交 Git、未推送；本轮没有修改 C++ 或其它地图。
