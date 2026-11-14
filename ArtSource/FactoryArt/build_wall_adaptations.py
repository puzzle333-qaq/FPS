import bpy,bmesh,json,math
from mathutils import Vector
from pathlib import Path
A=Path(r'C:\Users\ASUS\Documents\Unreal Projects\FPS\ArtSource\FactoryArt');OUT=A/'WallFBX';OUT.mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
plan=json.loads((A/'WallPlan.json').read_text());sources={}
for family,file in [('Factory','FactoryWall'),('Office','OfficeWall'),('Underground','UndergroundWall')]:
 before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(A/(file+'.fbx')));objects=[o for o in bpy.data.objects if o not in before and o.type=='MESH' and not o.name.startswith('UCX')];assert len(objects)==1
 o=objects[0];o.data.transform(o.matrix_world);o.matrix_world.identity()
 for v in o.data.vertices:
  x,y,z=v.co
  if family=='Factory':v.co=Vector((-y,x+.1,z))
  elif family=='Office':v.co=Vector((x,-y,z))
  else:v.co=Vector((y-.1,-x-.9,z))
 sources[family]=o.data.copy()
 for obj in [q for q in bpy.data.objects if q not in before]:bpy.data.objects.remove(obj,do_unlink=True)
report=[]
for name,d in plan['variants'].items():
 mesh=sources[d['family']].copy();o=bpy.data.objects.new(name,mesh);scene.collection.objects.link(o);bm=bmesh.new();bm.from_mesh(mesh)
 for axis,limit,positive in [(0,0,False),(0,d['width']/100,True),(1,-.1,False),(1,.1,True),(2,0,False),(2,d['height']/100,True)]:
  no=Vector([1 if i==axis else 0 for i in range(3)]);co=no*limit
  rr=bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=.000001,plane_co=co,plane_no=no,clear_outer=positive,clear_inner=not positive)
  es=[e for e in rr['geom_cut'] if isinstance(e,bmesh.types.BMEdge) and e.is_boundary]
  if es:
   try:bmesh.ops.holes_fill(bm,edges=es,sides=0)
   except:pass
 bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free();mesh.update()
 assert len(mesh.polygons)>0,(name,'empty trim')
 mins=[min(v.co[i] for v in mesh.vertices)*100 for i in range(3)];maxs=[max(v.co[i] for v in mesh.vertices)*100 for i in range(3)]
 # Export in UE cm with the established FBX handedness conversion.
 for v in mesh.vertices:v.co.y=-v.co.y
 bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.reverse_faces(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free()
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,object_types={'MESH'},global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
 report.append({'name':name,**d,'min':mins,'max':maxs,'polygons':len(mesh.polygons),'collision':'visual only; keep verified original collision'})
 bpy.data.objects.remove(o,do_unlink=True)
(A/'WallAdaptationReport.json').write_text(json.dumps(report,indent=2));print('SUCCESS',len(report))
