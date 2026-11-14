# UE5 模块化室内套件

由 build_interior_kit.py 程序生成。尺寸单位均为厘米，表中顺序为 X长×Y厚/宽×Z高。

| FBX 文件 | 尺寸（cm） | Pivot |
|---|---|---|
| SM_IntWall_100.fbx | 100×20×300 | 墙段左端底部中心线 (0,0,0)；X=0，Y=±10cm |
| SM_IntWall_200.fbx | 200×20×300 | 墙段左端底部中心线 (0,0,0)；X=0，Y=±10cm |
| SM_IntWall_400.fbx | 400×20×300 | 墙段左端底部中心线 (0,0,0)；X=0，Y=±10cm |
| SM_IntWall_Door_200.fbx | 200×20×300；居中洞口 110×220，洞底 0 | 墙段左端底部中心线 (0,0,0) |
| SM_IntWall_DoubleDoor_400.fbx | 400×20×300；居中洞口 180×220，洞底 0 | 墙段左端底部中心线 (0,0,0) |
| SM_IntWall_Window_200.fbx | 200×20×300；居中洞口 100×100，洞底 100 | 墙段左端底部中心线 (0,0,0) |
| SM_IntWall_Corner.fbx | 20×20×300 | 左端底部中心线 (0,0,0)；X=0，Y=±10cm |
| SM_IntCeiling_200x200.fbx | 200×200×10 | 角落底面 (0,0,0)；沿 +X、+Y 延伸 |
| SM_IntCeiling_400x400.fbx | 400×400×10 | 角落底面 (0,0,0)；沿 +X、+Y 延伸 |
| SM_IntDoorFrame_110.fbx | 外径126×24×228；内径110×220；框宽8，无底槛 | 门框外侧左端底部中心线 (0,0,0)；装到200墙门洞时 X偏移37cm |

## 建模与导入

- Blender 内按米建模，单位比例1；FBX apply_unit_scale=True、global_scale=1、FBX_SCALE_NONE，将米到厘米的100倍写入坐标。前向 -Y，上 Z。UE 导入缩放设1。
- 墙段沿 +X，厚度关于 Y=0 对称；所有对象位置/旋转为0，缩放为1。天花板底面 Z=0，安装高度由关卡放置决定。
- 渲染网格为焊接、封闭、全四边形表面；无内部重复面。FBX/UE 渲染时仍可能三角化。
- UV0 为局部原点与世界坐标一致的轴向 box 投影，1 UV=2米；放置后纹理相位随模块起点重置。UV1 每个面独立打包到0–1，互不重叠，留白为单元宽度的4%；建议 Lightmap Resolution 256。
- M_Wall_Plaster 用于 Y正负墙面；M_Wall_Trim 用于端面、洞口内侧、顶底边。材质仅为预览色，请在UE中替换为平铺PBR材质。
- 每个 FBX 仅含一个渲染模型及其 UCX_<模型名>_00 等凸盒。关闭自动生成碰撞，启用导入自定义碰撞；每个UCX一个凸包。洞口没有碰撞遮挡。
- UE 中保留导入 UV，关闭 Generate Lightmap UVs，Light Map Coordinate Index=1。建议关闭 Combine Meshes；导入材质槽但不自动创建材质也可。
- 门框内径110×220，外径126×228，因此不能完整塞入110×220的墙洞；它是覆盖洞口边缘的门框套。与200墙对齐时平移X=37cm，Y=0，Z=0；两侧各覆盖墙面8cm，深24cm比墙体两侧各突出2cm。
- interior_kit.blend 保存可编辑模型；validation_report.json 记录拓扑检查与碰撞数量。脚本可重复执行。
- 已做 Blender 几何与FBX文件检查；尚未在UE编辑器中实机验证。资产为本地脚本原创程序几何，无外部素材。

运行：`& "C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe" --background --python build_interior_kit.py`

## Gameplay Props 导出与重新导入验证

由 build_gameplay_props.py 生成，原创程序几何。Blender米建模，FBX厘米坐标；导出设置与 build_interior_kit.py 相同：scale_length=1、global_scale=1、apply_unit_scale=True、FBX_SCALE_NONE、前向-Y／上Z。

| 模型 | 重新导入总包围盒 X×Y×Z (cm) | Pivot | UCX盒数 | 结果 |
|---|---|---|---|---|
| SM_Door_Panel_105 | 105×14×215 | 铰链侧底边厚度中心；X=0，沿+X延伸；板体105×5×215，把手总厚14，原点(0,0,0) | 1 | PASS |
| SM_Door_Frame_110 | 130×24×230 | 门洞底边中心；净空X=±55、Z=0至220，无底槛，原点(0,0,0) | 3 | PASS |
| SM_Boiler_Blockout | 180×300×280 | 底面中心；圆筒沿Y；前端-Y；主体实心盒碰撞，3个UCX，原点(0,0,0) | 3 | PASS |
| SM_ElecCabinet_Tall | 120×60×200 | 底面中心；柜门、把手、铭牌朝-Y，原点(0,0,0) | 1 | PASS |
| SM_ElecCabinet_Low | 80×60×110 | 底面中心；柜门、把手、铭牌朝-Y，原点(0,0,0) | 1 | PASS |

- 门板尺寸105×5×215指板体；双面把手中心离地100、安装X=95，含把手总厚14cm。门框内径110×220，条宽10、深24，外径130×24×230。
- UV0沿用轴向投影，1 UV=2米；UV1每面独立0–1网格打包，单元留白4%。灯光贴图坐标索引1，关闭自动生成UV。圆筒封盖为多边形，FBX/UE可能三角化。
- 每个FBX含单个SM及UCX_<模型名>_00等凸盒。锅炉主体盒覆盖完整圆筒截面，无弹道穿透空隙；附属细管并非逐根精确碰撞。UE启用自定义碰撞、关闭自动碰撞，每UCX一个凸包。
- 材质槽：门M_Door_Wood／M_Door_Metal；门框M_Wall_Trim；锅炉M_Boiler_Metal／M_Pipe_Metal；柜体M_Cabinet_Metal。仅预览材质。
- 验证实际重新导入五个FBX，断言尺寸误差<0.01cm、原点位置、底面Z=0、中心/铰链定位、单位缩放、UV0/UV1及UCX数量与8顶点盒结构。详细数据见gameplay_props_validation.json；可编辑源文件gameplay_props.blend。未在UE中验证弹道或视觉效果。
- 运行：`& "C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe" --background --python build_gameplay_props.py`
