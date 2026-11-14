import bpy,bmesh,json
from mathutils import Vector
from pathlib import Path
A=Path(r'C:\Users\ASUS\Documents\Unreal Projects\FPS\ArtSource\FactoryArt');OUT=A/'ArchitectureFBX';OUT.mkdir(exist_ok=True);bpy.ops.wm.read_factory_settings(use_empty=True);bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
plan=json.loads((A/'ArchitecturePlan.json').read_text());sources={}
for family in ['RoofTile','CeilingTile','WindowTile','FloorTile']:
 before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(A/(family+'.fbx')));o=next(o for o in bpy.data.objects if o not in before and o.type=='MESH' and not o.name.startswith('UCX'));o.data.transform(o.matrix_world);o.matrix_world.identity()
 mins=[min(v.co[i] for v in o.data.vertices) for i in range(3)]
 for v in o.data.vertices:
  x,y,z=v.co
  if family=='RoofTile':v.co=Vector((x,-y,z+.05))
  elif family=='CeilingTile':v.co=Vector((x,y,z))
  elif family=='FloorTile':v.co=Vector((x-mins[0],y-mins[1],z-mins[2]))
  else:v.co=Vector((-y,x+.4,z))
 sources[family]=o.data.copy()
 for ob in list(bpy.data.objects):
  if ob not in before:bpy.data.objects.remove(ob,do_unlink=True)
for name,d in plan['variants'].items():
 o=bpy.data.objects.new(name,sources[d['family']].copy());bpy.context.scene.collection.objects.link(o);bm=bmesh.new();bm.from_mesh(o.data)
 limits=[(0,0,False),(0,d['width']/100,True)]
 if d['family']=='WindowTile':limits += [(2,0,False),(2,3,True)]
 else:limits += [(1,0,False),(1,d['depth']/100,True)]
 for axis,l,outer in limits:
  n=Vector([1 if i==axis else 0 for i in range(3)]);rr=bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=.000001,plane_co=n*l,plane_no=n,clear_outer=outer,clear_inner=not outer);es=[e for e in rr['geom_cut'] if isinstance(e,bmesh.types.BMEdge) and e.is_boundary]
  if es:
   try:bmesh.ops.holes_fill(bm,edges=es,sides=0)
   except:pass
 bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free()
 for v in o.data.vertices:v.co.y=-v.co.y
 bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.reverse_faces(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free()
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,object_types={'MESH'},global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False);bpy.data.objects.remove(o,do_unlink=True)
print('ARCHITECTURE_ADAPTATIONS_SUCCESS',len(plan['variants']))
