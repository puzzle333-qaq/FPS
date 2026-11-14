"""Deterministic UE interior kit. Run with Blender --background --python this_file."""
from pathlib import Path
import bpy
import bmesh
import json
import math

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'FBX'
OUT.mkdir(exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0
materials = []
for name, color in [('M_Wall_Plaster', (.65, .63, .59, 1)), ('M_Wall_Trim', (.32, .31, .29, 1))]:
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.diffuse_color = color
    mat.use_nodes = True
    shader = next((n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
    if shader is None:
        shader = mat.node_tree.nodes.new('ShaderNodeBsdfPrincipled')
        output = mat.node_tree.nodes.new('ShaderNodeOutputMaterial')
        mat.node_tree.links.new(shader.outputs['BSDF'], output.inputs['Surface'])
    shader.inputs['Base Color'].default_value = color
    shader.inputs['Roughness'].default_value = .8
    materials.append(mat)


def surface(name, xs, ys, zs, occupied):
    """Boundary of a rectilinear cell union: welded quads, no hidden faces/T junctions."""
    verts, faces, lookup = [], [], {}
    def vertex(p):
        if p not in lookup:
            lookup[p] = len(verts)
            verts.append(p)
        return lookup[p]
    directions = [(-1,0,0), (1,0,0), (0,-1,0), (0,1,0), (0,0,-1), (0,0,1)]
    for i,j,k in sorted(occupied):
        x,X = xs[i:i+2]; y,Y = ys[j:j+2]; z,Z = zs[k:k+2]
        sides = [((x,y,z),(x,y,Z),(x,Y,Z),(x,Y,z)),
                 ((X,y,z),(X,Y,z),(X,Y,Z),(X,y,Z)),
                 ((x,y,z),(X,y,z),(X,y,Z),(x,y,Z)),
                 ((x,Y,z),(x,Y,Z),(X,Y,Z),(X,Y,z)),
                 ((x,y,z),(x,Y,z),(X,Y,z),(X,y,z)),
                 ((x,y,Z),(X,y,Z),(X,Y,Z),(x,Y,Z))]
        for d, points in zip(directions, sides):
            if (i+d[0],j+d[1],k+d[2]) not in occupied:
                faces.append([vertex(p) for p in points])
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    scene.collection.objects.link(obj)
    return obj


def box(name, bounds):
    x,X,y,Y,z,Z = bounds
    return surface(name, [x,X], [y,Y], [z,Z], {(0,0,0)})


def finish(obj):
    mesh = obj.data
    for mat in materials:
        mesh.materials.append(mat)
    uv0 = mesh.uv_layers.new(name='UV0')
    uv1 = mesh.uv_layers.new(name='UV1')
    n = math.ceil(math.sqrt(len(mesh.polygons)))
    cell, pad = 1/n, .04/n
    for index, poly in enumerate(mesh.polygons):
        axis = max(range(3), key=lambda a: abs(poly.normal[a]))
        axes = [(1,2), (0,2), (0,1)][axis]
        coords = [(mesh.vertices[mesh.loops[li].vertex_index].co[axes[0]],
                   mesh.vertices[mesh.loops[li].vertex_index].co[axes[1]]) for li in poly.loop_indices]
        lo = [min(p[a] for p in coords) for a in range(2)]
        span = max(max(p[a] for p in coords)-lo[a] for a in range(2))
        for li, p in zip(poly.loop_indices, coords):
            uv0.data[li].uv = (p[0]/2, p[1]/2)
            uv1.data[li].uv = ((index % n)*cell+pad+(p[0]-lo[0])/span*(cell-2*pad),
                               (index // n)*cell+pad+(p[1]-lo[1])/span*(cell-2*pad))
        poly.material_index = 0 if axis == 1 else 1
        poly.use_smooth = False
    mesh.uv_layers.active_index = 0
    uv0.active_render = True
    bm = bmesh.new(); bm.from_mesh(mesh)
    assert all(len(f.verts) == 4 for f in bm.faces), obj.name
    assert all(e.is_manifold for e in bm.edges), obj.name
    assert bm.calc_volume(signed=True) > 0, obj.name
    bm.free()


assets = []
def register(obj, bounds, description, pivot):
    finish(obj)
    collisions = [box(f'UCX_{obj.name}_{i:02d}', b) for i,b in enumerate(bounds)]
    assets.append((obj, collisions, description, pivot))


for length in (1,2,4):
    b = (0,length,-.1,.1,0,3)
    register(box(f'SM_IntWall_{length*100}', b), [b], f'{length*100}×20×300', '墙段左端底部中心线 (0,0,0)；X=0，Y=±10cm')

for name,length,width,bottom,height in [('SM_IntWall_Door_200',2,1.1,0,2.2),
        ('SM_IntWall_DoubleDoor_400',4,1.8,0,2.2), ('SM_IntWall_Window_200',2,1,1,1)]:
    a,b = (length-width)/2,(length+width)/2
    xs, zs = [0,a,b,length], sorted(set([0,bottom,bottom+height,3]))
    cells = {(i,0,k) for i in range(3) for k in range(len(zs)-1)
             if not (i==1 and bottom <= zs[k] and zs[k+1] <= bottom+height)}
    obj = surface(name,xs,[-.1,.1],zs,cells)
    bounds = [(0,a,-.1,.1,0,3),(b,length,-.1,.1,0,3),(a,b,-.1,.1,bottom+height,3)]
    if bottom: bounds.append((a,b,-.1,.1,0,bottom))
    register(obj,bounds,f'{length*100:g}×20×300；居中洞口 {width*100:g}×{height*100:g}，洞底 {bottom*100:g}', '墙段左端底部中心线 (0,0,0)')

b = (0,.2,-.1,.1,0,3)
register(box('SM_IntWall_Corner',b),[b],'20×20×300','左端底部中心线 (0,0,0)；X=0，Y=±10cm')
for size in (2,4):
    b = (0,size,0,size,0,.1)
    register(box(f'SM_IntCeiling_{size*100}x{size*100}',b),[b],f'{size*100}×{size*100}×10','角落底面 (0,0,0)；沿 +X、+Y 延伸')
xs,zs = [0,.08,1.18,1.26],[0,2.2,2.28]
cells = {(i,0,k) for i in range(3) for k in range(2) if i != 1 or k != 0}
obj = surface('SM_IntDoorFrame_110',xs,[-.12,.12],zs,cells)
register(obj,[(0,.08,-.12,.12,0,2.28),(1.18,1.26,-.12,.12,0,2.28),(.08,1.18,-.12,.12,2.2,2.28)],
         '外径126×24×228；内径110×220；框宽8，无底槛','门框外侧左端底部中心线 (0,0,0)；装到200墙门洞时 X偏移37cm')
# The complete frame fits a 126x228 rough opening; a 110x220 opening is its clear passage.
report = []
for obj,collisions,description,pivot in assets:
    bpy.ops.object.select_all(action='DESELECT')
    for item in [obj]+collisions: item.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = OUT / (obj.name+'.fbx')
    # Meter scene => unit factor 100. NONE bakes that factor into FBX coordinates.
    # global_scale=1 avoids applying centimeters twice; 1m becomes 100 FBX/UE units.
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},
        global_scale=1.0,apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',
        axis_forward='-Y',axis_up='Z',use_mesh_modifiers=False,mesh_smooth_type='FACE',
        add_leaf_bones=False,bake_anim=False,use_custom_props=False)
    assert path.is_file() and path.stat().st_size > 1000
    report.append({'file':path.name,'vertices':len(obj.data.vertices),'quads':len(obj.data.polygons),
                   'collision_boxes':len(collisions),'dimensions_m':list(obj.dimensions),
                   'topology':'closed manifold quads, positive volume','uv1':'isolated padded face cells'})
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'interior_kit.blend'))
(ROOT/'validation_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
lines = ['# UE5 模块化室内套件','', '由 build_interior_kit.py 程序生成。尺寸单位均为厘米，表中顺序为 X长×Y厚/宽×Z高。', '',
         '| FBX 文件 | 尺寸（cm） | Pivot |','|---|---|---|']
for obj,_,description,pivot in assets:
    lines.append(f'| {obj.name}.fbx | {description} | {pivot} |')
lines += ['', '## 建模与导入', '',
    '- Blender 内按米建模，单位比例1；FBX apply_unit_scale=True、global_scale=1、FBX_SCALE_NONE，将米到厘米的100倍写入坐标。前向 -Y，上 Z。UE 导入缩放设1。',
    '- 墙段沿 +X，厚度关于 Y=0 对称；所有对象位置/旋转为0，缩放为1。天花板底面 Z=0，安装高度由关卡放置决定。',
    '- 渲染网格为焊接、封闭、全四边形表面；无内部重复面。FBX/UE 渲染时仍可能三角化。',
    '- UV0 为局部原点与世界坐标一致的轴向 box 投影，1 UV=2米；放置后纹理相位随模块起点重置。UV1 每个面独立打包到0–1，互不重叠，留白为单元宽度的4%；建议 Lightmap Resolution 256。',
    '- M_Wall_Plaster 用于 Y正负墙面；M_Wall_Trim 用于端面、洞口内侧、顶底边。材质仅为预览色，请在UE中替换为平铺PBR材质。',
    '- 每个 FBX 仅含一个渲染模型及其 UCX_<模型名>_00 等凸盒。关闭自动生成碰撞，启用导入自定义碰撞；每个UCX一个凸包。洞口没有碰撞遮挡。',
    '- UE 中保留导入 UV，关闭 Generate Lightmap UVs，Light Map Coordinate Index=1。建议关闭 Combine Meshes；导入材质槽但不自动创建材质也可。',
    '- 门框内径110×220，外径126×228，因此不能完整塞入110×220的墙洞；它是覆盖洞口边缘的门框套。与200墙对齐时平移X=37cm，Y=0，Z=0；两侧各覆盖墙面8cm，深24cm比墙体两侧各突出2cm。',
    '- interior_kit.blend 保存可编辑模型；validation_report.json 记录拓扑检查与碰撞数量。脚本可重复执行。',
    '- 已做 Blender 几何与FBX文件检查；尚未在UE编辑器中实机验证。资产为本地脚本原创程序几何，无外部素材。', '',
    '运行：`& "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Blender\\blender.exe" --background --python build_interior_kit.py`']
(ROOT/'README.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('INTERIOR_KIT_SUCCESS',len(assets),'FBX files')
