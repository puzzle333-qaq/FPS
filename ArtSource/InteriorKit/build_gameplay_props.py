"""Gameplay props; run Blender --background --python build_gameplay_props.py.
All design coordinates below are cm; stored meshes use meters, like interior kit.
"""
from pathlib import Path
import bpy
import math
import json
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'FBX'
OUT.mkdir(exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0
COLORS = {'M_Door_Wood':(.24,.12,.055,1), 'M_Door_Metal':(.24,.26,.28,1),
          'M_Wall_Trim':(.32,.31,.29,1), 'M_Boiler_Metal':(.22,.28,.30,1),
          'M_Pipe_Metal':(.12,.15,.16,1), 'M_Cabinet_Metal':(.29,.34,.30,1)}
MATS = {}
for name,color in COLORS.items():
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.diffuse_color = color
    mat.use_nodes = True
    shader = next((n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
    if shader is None:
        shader = mat.node_tree.nodes.new('ShaderNodeBsdfPrincipled')
        output = mat.node_tree.nodes.new('ShaderNodeOutputMaterial')
        mat.node_tree.links.new(shader.outputs['BSDF'], output.inputs['Surface'])
    shader.inputs['Base Color'].default_value = color
    shader.inputs['Roughness'].default_value = .65
    MATS[name] = mat

def box(center, size, mat=None):
    bpy.ops.mesh.primitive_cube_add(size=1, location=[v/100 for v in center])
    obj = bpy.context.object
    obj.dimensions = [v/100 for v in size]
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if mat: obj.data.materials.append(MATS[mat])
    return obj

def cylinder(center, radius, length, axis, mat):
    rotation = {'Y':(math.pi/2,0,0), 'Z':(0,0,0)}[axis]
    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=radius/100,
        depth=length/100, location=[v/100 for v in center], rotation=rotation)
    obj = bpy.context.object
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    obj.data.materials.append(MATS[mat])
    return obj

def join(parts, name, slots):
    bpy.ops.object.select_all(action='DESELECT')
    for p in parts: p.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = name
    # Bake all component transforms into mesh and place origin at the design pivot.
    scene.cursor.location = (0,0,0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    indices = [slots.index(s.name) for s in obj.data.materials]
    face_slots = [indices[p.material_index] for p in obj.data.polygons]
    obj.data.materials.clear()
    for s in slots: obj.data.materials.append(MATS[s])
    for p,idx in zip(obj.data.polygons, face_slots): p.material_index = idx
    while obj.data.uv_layers:
        obj.data.uv_layers.remove(obj.data.uv_layers[0])
    uv0 = obj.data.uv_layers.new(name='UV0')
    uv1 = obj.data.uv_layers.new(name='UV1')
    n = math.ceil(math.sqrt(len(obj.data.polygons)))
    cell,pad = 1/n,.04/n
    for index,poly in enumerate(obj.data.polygons):
        axis = max(range(3), key=lambda a: abs(poly.normal[a]))
        axes = [(1,2),(0,2),(0,1)][axis]
        coords = [(obj.data.vertices[obj.data.loops[li].vertex_index].co[axes[0]],
                   obj.data.vertices[obj.data.loops[li].vertex_index].co[axes[1]]) for li in poly.loop_indices]
        lo = [min(p[a] for p in coords) for a in range(2)]
        span = max(max(p[a] for p in coords)-lo[a] for a in range(2))
        assert span > 0
        for li,p in zip(poly.loop_indices,coords):
            uv0.data[li].uv = (p[0]/2,p[1]/2)
            uv1.data[li].uv = ((index%n)*cell+pad+(p[0]-lo[0])/span*(cell-2*pad),
                                  (index//n)*cell+pad+(p[1]-lo[1])/span*(cell-2*pad))
    obj.data.uv_layers.active_index = 0
    uv0.active_render = True
    return obj

assets = []
def register(name, parts, slots, collision_specs, expected, pivot):
    obj = join(parts,name,slots)
    collisions = []
    for i,(center,size) in enumerate(collision_specs):
        col = box(center,size)
        col.name = f'UCX_{name}_{i:02d}'
        scene.cursor.location = (0,0,0)
        bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
        collisions.append(col)
    assets.append((obj,collisions,expected,pivot))

wood,metal = 'M_Door_Wood','M_Door_Metal'
parts = [box((52.5,0,107.5),(105,5,215),wood)]
for sign in (-1,1):
    parts += [box((95,sign*3,100),(4,1,16),metal),
              box((91,sign*5,100),(12,4,2),metal)]
register('SM_Door_Panel_105',parts,[wood,metal],
    [((52.5,0,107.5),(105,5,215))],(105,14,215),'铰链侧底边厚度中心；X=0，沿+X延伸；板体105×5×215，把手总厚14')
trim = 'M_Wall_Trim'
specs = [((-60,0,110),(10,24,220)),((60,0,110),(10,24,220)),((0,0,225),(130,24,10))]
register('SM_Door_Frame_110',[box(c,s,trim) for c,s in specs],[trim],specs,
    (130,24,230),'门洞底边中心；净空X=±55、Z=0至220，无底槛')
bm,pm = 'M_Boiler_Metal','M_Pipe_Metal'
parts = [box((0,0,10),(180,300,20),bm)]
for y in (-95,95):
    parts += [box((0,y,35),(140,20,50),bm)]
parts += [cylinder((0,0,135),85,280,'Y',bm),
          cylinder((0,-145,135),48,10,'Y',bm)]
for x in (-43,43):
    parts += [cylinder((x,-145,190),12,10,'Y',pm),
              box((x,-150.5,190),(1,1,16),pm)]
# Front gauge indicators stay within the prescribed -150 cm front plane.
for p in parts[-4:]:
    if p.location.y < -1.5: p.location.y = -1.495
for x in (-45,0):
    parts += [cylinder((x,35,230),8,60,'Z',pm),
              cylinder((x,15,255),8,40,'Y',pm)]
parts += [cylinder((48,85,240),16,80,'Z',pm)]
register('SM_Boiler_Blockout',parts,[bm,pm],
    [((0,0,25),(180,300,50)),((0,0,135),(170,300,170)),
     ((0,35,250),(130,150,60))],(180,300,280),'底面中心；圆筒沿Y；前端-Y；主体实心盒碰撞，3个UCX')
for name,w,d,h in [('SM_ElecCabinet_Tall',120,60,200),('SM_ElecCabinet_Low',80,60,110)]:
    m = 'M_Cabinet_Metal'
    parts = [box((0,1,h/2),(w,58,h),m)]
    # Slightly raised split doors with visible gap, recessed into total depth envelope.
    for sign in (-1,1):
        parts += [box((sign*w/4,-28.8,h*.47),(w/2-2,1.6,h*.86),m),
                  box((sign*5,-29.5,h*.5),(2,1,14),m)]
    parts += [box((0,-29.5,h-7),(w*.35,1,7),m)]
    register(name,parts,[m],[((0,0,h/2),(w,d,h))],(w,d,h),'底面中心；柜门、把手、铭牌朝-Y')

for obj,cols,expected,pivot in assets:
    bpy.ops.object.select_all(action='DESELECT')
    for item in [obj]+cols: item.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(OUT/(obj.name+'.fbx')),use_selection=True,object_types={'MESH'},
        global_scale=1.0,apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',
        axis_forward='-Y',axis_up='Z',use_mesh_modifiers=False,mesh_smooth_type='FACE',
        add_leaf_bones=False,bake_anim=False,use_custom_props=False)
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'gameplay_props.blend'))

# Reimport every FBX into a clean scene, checking actual exported data.
report = []
metadata = [(o.name,len(c),e,p,[m.name for m in o.data.materials]) for o,c,e,p in assets]
for name,expected_cols,expected,pivot,slots in metadata:
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for material in list(bpy.data.materials):
        bpy.data.materials.remove(material, do_unlink=True)
    bpy.ops.import_scene.fbx(filepath=str(OUT/(name+'.fbx')))
    obj = bpy.data.objects[name]
    points = [obj.matrix_world @ v.co for v in obj.data.vertices]
    low = [min(p[a] for p in points)*100 for a in range(3)]
    high = [max(p[a] for p in points)*100 for a in range(3)]
    dims = [high[a]-low[a] for a in range(3)]
    assert all(abs(a-b)<.01 for a,b in zip(dims,expected)), (name,dims,expected)
    assert obj.location.length < 1e-5, (name,'pivot',list(obj.location))
    assert all(abs(v-1)<1e-5 for v in obj.scale)
    assert abs(low[2])<.01
    if name == 'SM_Door_Panel_105': assert abs(low[0])<.01 and abs(low[1]+high[1])<.01
    else: assert abs(low[0]+high[0])<.01 and abs(low[1]+high[1])<.01
    imported_cols = [o for o in scene.objects if o.name.startswith('UCX_'+name+'_')]
    assert len(imported_cols)==expected_cols
    assert [u.name for u in obj.data.uv_layers] == ['UV0','UV1']
    assert [m.name for m in obj.data.materials] == slots
    # Each collision remains a closed 8-vertex box after round trip.
    assert all(len(c.data.vertices)==8 for c in imported_cols)
    report.append(dict(name=name,dimensions_cm=[round(v,4) for v in dims],
        bounds_cm=[[round(v,4) for v in low],[round(v,4) for v in high]],
        pivot_cm=[round(v*100,6) for v in obj.location],pivot_description=pivot,
        uv_layers=[u.name for u in obj.data.uv_layers],material_slots=[m.name for m in obj.data.materials],
        collision_boxes=expected_cols,status='PASS'))
(ROOT/'gameplay_props_validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
readme = ROOT/'README.md'
marker = '\n## Gameplay Props 导出与重新导入验证\n'
original = readme.read_text(encoding='utf-8') if readme.exists() else ''
original = original.split(marker)[0]
lines = [marker,'由 build_gameplay_props.py 生成，原创程序几何。Blender米建模，FBX厘米坐标；导出设置与 build_interior_kit.py 相同：scale_length=1、global_scale=1、apply_unit_scale=True、FBX_SCALE_NONE、前向-Y／上Z。',
    '', '| 模型 | 重新导入总包围盒 X×Y×Z (cm) | Pivot | UCX盒数 | 结果 |','|---|---|---|---|---|']
for r in report:
    lines.append(f"| {r['name']} | {'×'.join(f'{v:g}' for v in r['dimensions_cm'])} | {r['pivot_description']}，原点(0,0,0) | {r['collision_boxes']} | PASS |")
lines += ['', '- 门板尺寸105×5×215指板体；双面把手中心离地100、安装X=95，含把手总厚14cm。门框内径110×220，条宽10、深24，外径130×24×230。',
    '- UV0沿用轴向投影，1 UV=2米；UV1每面独立0–1网格打包，单元留白4%。灯光贴图坐标索引1，关闭自动生成UV。圆筒封盖为多边形，FBX/UE可能三角化。',
    '- 每个FBX含单个SM及UCX_<模型名>_00等凸盒。锅炉主体盒覆盖完整圆筒截面，无弹道穿透空隙；附属细管并非逐根精确碰撞。UE启用自定义碰撞、关闭自动碰撞，每UCX一个凸包。',
    '- 材质槽：门M_Door_Wood／M_Door_Metal；门框M_Wall_Trim；锅炉M_Boiler_Metal／M_Pipe_Metal；柜体M_Cabinet_Metal。仅预览材质。',
    '- 验证实际重新导入五个FBX，断言尺寸误差<0.01cm、原点位置、底面Z=0、中心/铰链定位、单位缩放、UV0/UV1及UCX数量与8顶点盒结构。详细数据见gameplay_props_validation.json；可编辑源文件gameplay_props.blend。未在UE中验证弹道或视觉效果。',
    '- 运行：`& "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Blender\\blender.exe" --background --python build_gameplay_props.py`','']
readme.write_text(original+'\n'.join(lines),encoding='utf-8')
print('GAMEPLAY_PROPS_SUCCESS: 5 FBX round trips PASS')
