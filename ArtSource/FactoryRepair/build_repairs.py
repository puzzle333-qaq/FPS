import bpy,bmesh,math,json
from mathutils import Vector
from pathlib import Path
ROOT=Path(r'C:\Users\ASUS\Documents\Unreal Projects\FPS\ArtSource\FactoryRepair');OUT=ROOT/'FBX';OUT.mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
bpy.ops.import_scene.fbx(filepath=str(ROOT/'SM_Walkway_Source.fbx'))
source=next(o for o in scene.objects if o.type=='MESH' and not o.name.startswith('UCX'))
source.data.transform(source.matrix_world);source.matrix_world.identity()
mins=[min(v.co[i] for v in source.data.vertices) for i in range(3)]
for v in source.data.vertices:v.co-=Vector(mins)
source.hide_set(True);source.hide_render=True
report=[]
def box(name,lo,hi):
 v=[(x/100,y/100,z/100) for x in [lo[0],hi[0]] for y in [lo[1],hi[1]] for z in [lo[2],hi[2]]]
 f=[(0,4,6,2),(1,3,7,5),(0,1,5,4),(2,6,7,3),(0,2,3,1),(4,5,7,6)]
 # These vertex orders originally faced inward, exposing slab undersides from above.
 f=[tuple(reversed(face)) for face in f]
 m=bpy.data.meshes.new(name);m.from_pydata(v,[],f);m.update();o=bpy.data.objects.new(name,m);scene.collection.objects.link(o);return o
def join(name,objs):
 bpy.ops.object.select_all(action='DESELECT')
 for o in objs:o.hide_set(False);o.select_set(True)
 bpy.context.view_layer.objects.active=objs[0];bpy.ops.object.join();o=bpy.context.object;o.name=name;return o
def export(o,boxes):
 cols=[box('UCX_'+o.name+'_%02d'%i,lo,hi) for i,(lo,hi) in enumerate(boxes)]
 # FBX coordinate handedness: author coordinates are UE centimetres.
 for item in [o]+cols:
  for vertex in item.data.vertices:vertex.co.y=-vertex.co.y
  bm=bmesh.new();bm.from_mesh(item.data);bmesh.ops.reverse_faces(bm,faces=list(bm.faces));bm.to_mesh(item.data);bm.free()
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True)
 for c in cols:c.select_set(True)
 bpy.context.view_layer.objects.active=o
 bpy.ops.export_scene.fbx(filepath=str(OUT/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',use_mesh_modifiers=False,mesh_smooth_type='FACE',add_leaf_bones=False,bake_anim=False)
 pts=[v.co for v in o.data.vertices];report.append({'name':o.name,'min':[min(v[i] for v in pts)*100 for i in range(3)],'max':[max(v[i] for v in pts)*100 for i in range(3)],'collision_boxes':len(cols)})
 o.hide_set(True)
 for c in cols:c.hide_set(True)
for name,w,h in [('SM_Walkway_400x250_Trim',400,250),('SM_Walkway_300x325_Trim',300,325)]:
 o=bpy.data.objects.new(name,source.data.copy());scene.collection.objects.link(o)
 bm=bmesh.new();bm.from_mesh(o.data)
 for axis,limit in [(0,w/100),(1,h/100)]:
  no=Vector((1,0,0) if axis==0 else (0,1,0));co=no*limit
  r=bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=.00001,plane_co=co,plane_no=no,clear_outer=True,clear_inner=False)
  es=[e for e in r['geom_cut'] if isinstance(e,bmesh.types.BMEdge) and e.is_boundary]
  if es:
   try:bmesh.ops.holes_fill(bm,edges=es,sides=0)
   except:pass
 for v in bm.verts:v.co.z+=(20-8.7550275)/100
 bm.to_mesh(o.data);bm.free();o.data.update()
 # Thin support frame supplies the specified 20cm total depth under the original grating.
 frame=[box(name+'_supportA',(0,0,0),(w,5,11.2449725)),box(name+'_supportB',(0,h-5,0),(w,h,11.2449725)),box(name+'_supportC',(0,5,0),(5,h-5,11.2449725)),box(name+'_supportD',(w-5,5,0),(w,h-5,11.2449725))]
 o=join(name,[o]+frame);export(o,[((0,0,0),(w,h,20))])
# Ground floor and basement ceiling, with an actual open stairwell.
slabs=[((0,-4000,-20),(4940,0,0)),((4940,-4000,-20),(5600,-3200,0)),((4940,-3020,-20),(5600,0,0)),((5600,-4600,-20),(6200,-2600,0)),((4200,-4600,-20),(5600,-4000,0))]
o=join('SM_FactoryFloor_Stairwell',[box('floor_%d'%i,*b) for i,b in enumerate(slabs)]);export(o,slabs)
# Native-size railing assemblies, per perimeter, leaving two 160cm stair entries.
def railing(name,segments,z):
 bs=[]
 for x1,y1,x2,y2 in segments:
  L=math.hypot(x2-x1,y2-y1);n=max(1,math.ceil(L/150))
  if x1==x2:
   lo,hi=sorted((y1,y2))
   for h in [55,108]:bs.append(((x1-2,lo,z+h-2),(x1+2,hi,z+h+2)))
   for i in range(n+1):
    y=lo+(hi-lo)*i/n;bs.append(((x1-2,y-2,z),(x1+2,y+2,z+110)))
  else:
   lo,hi=sorted((x1,x2))
   for h in [55,108]:bs.append(((lo,y1-2,z+h-2),(hi,y1+2,z+h+2)))
   for i in range(n+1):
    x=lo+(hi-lo)*i/n;bs.append(((x-2,y1-2,z),(x+2,y1+2,z+110)))
 o=join(name,[box('rail_%d'%i,*b) for i,b in enumerate(bs)]);export(o,bs)
railing('SM_FactoryCatwalk_Guards',[(1400,-2600,3800,-2600),(1400,-2600,1400,-800),(3800,-2600,3800,-2100),(1700,-2100,1740,-2100),(1900,-2100,3630,-2100),(3790,-2100,3800,-2100),(1700,-2100,1700,-800),(1400,-800,1700,-800)],400)
railing('SM_FactoryStairwell_Guards',[(4940,-3200,5600,-3200),(4940,-3020,5600,-3020),(5600,-3200,5600,-3020)],0)

DATA=json.loads((ROOT/'layout.json').read_text())
headers=[]
for key in ['walls','lintels']:
 for o,a,b,c,lv in DATA[key]:
  is_shop=(lv==0 and ((o=='v' and c in [14,38] and a<26 and b>0) or (o=='h' and c in [0,26] and a<38 and b>14)))
  if is_shop:a,b=(max(a,0),min(b,26)) if o=='v' else (max(a,14),min(b,38))
  if not lv and not is_shop:continue
  low,high=(-100,-20) if lv else (300,650)
  if lv:a,b,c=(a-20,b-20,c+18) if o=='h' else (a+18,b+18,c-20)
  lo=(a*100,-c*100-10,low) if o=='h' else (c*100-10,-b*100,low)
  hi=(b*100,-c*100+10,high) if o=='h' else (c*100+10,-a*100,high)
  headers.append((lo,hi))
o=join('SM_FactoryWall_UpperClosures',[box('header_%d'%i,*b) for i,b in enumerate(headers)]);export(o,headers)
rects=[(0,0,14,4),(0,4,5,11),(0,11,5,19),(5,4,7,19),(7,4,14,10),(7,10,14,15),(7,15,14,19),(0,19,14,26),(14,0,38,26),(38,0,50,14),(50,0,56,14),(38,14,56,26),(14,26,23,33),(23,26,32,33),(32,26,42,40),(14,33,32,40),(42,26,56,40)]
ceilings=[]
for i,(x,y,X,Y) in enumerate(rects):
 z=650 if i==8 else 300
 ceilings.append(((x*100,-Y*100,z),(X*100,-y*100,z+20)))
o=join('SM_FactoryRoom_Ceilings',[box('ceiling_%d'%i,*b) for i,b in enumerate(ceilings)]);export(o,ceilings)
# A/B deployment points and E1/E2 approaches need a continuous floor outside the footprint.
approach=[((-800,0,-20),(6200,1400,0)),((-800,-4000,-20),(0,0,0))]
o=join('SM_FactoryDeployment_Apron',[box('apron_%d'%i,*b) for i,b in enumerate(approach)]);export(o,approach)

QA=json.loads((ROOT.parent.parent/'Saved'/'FactoryRepair_QA.json').read_text())
side=[]
for key,pts in QA['stairs'].items():
 if key=='FD_175':
  for one,two in zip(pts,pts[1:]):
   top=max(one['hit']['z'],two['hit']['z'])+110
   for y in [-3190,-3040]:side.append(((one['x'],y-3,-465),(two['x'],y+3,top)))
 else:
  mid=pts[0]['x']
  for one,two in zip(pts,pts[1:]):
   top=max(one['hit']['z'],two['hit']['z'])+110
   for x in [mid-75,mid+75]:side.append(((x-3,one['y'],-65),(x+3,two['y'],top)))
o=join('SM_FactoryStair_SideCollision',[box('side_%d'%i,*b) for i,b in enumerate(side)]);export(o,side)
(ROOT/'validation.json').write_text(json.dumps(report,indent=2));print('FACTORY_REPAIR_ASSETS_SUCCESS',report)
