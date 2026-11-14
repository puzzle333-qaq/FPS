import bpy,bmesh,json
from pathlib import Path
A=Path(r'C:\Users\ASUS\Documents\Unreal Projects\FPS\ArtSource\FactoryDensity');out={}
for name in ['SM_Pipe02a_joint_90elbow','SM_Pipe02a_joint_90elbow_B','SM_Pipe02a_joint_tjunc','SM_L_Pipe_Bend']:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(A/(name+'.fbx')));o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX'));bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.transform(bm,matrix=o.matrix_world,verts=list(bm.verts));bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.00001)
 edges={e for e in bm.edges if e.is_boundary};loops=[]
 while edges:
  e=next(iter(edges));todo=[e];verts=set();used=set()
  while todo:
   e=todo.pop()
   if e in used:continue
   used.add(e);verts.update(e.verts)
   for v in e.verts:
    todo.extend(x for x in v.link_edges if x in edges and x not in used)
  edges-=used;pts=[[v.co.x*100,-v.co.y*100,v.co.z*100] for v in verts]
  if len(pts)>5:loops.append({'count':len(pts),'center':[sum(p[i] for p in pts)/len(pts) for i in range(3)],'bounds':[min(p[i] for p in pts) for i in range(3)]+[max(p[i] for p in pts) for i in range(3)]})
 out[name]=loops;bm.free()
(A/'PipeBoundaryLoops.json').write_text(json.dumps(out,indent=2))
