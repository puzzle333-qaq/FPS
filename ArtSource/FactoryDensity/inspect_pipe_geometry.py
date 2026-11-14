import bpy,json,collections
from pathlib import Path
A=Path(r'C:\Users\ASUS\Documents\Unreal Projects\FPS\ArtSource\FactoryDensity');out={}
for file in A.glob('*.fbx'):
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(file));r=[]
 for o in bpy.context.scene.objects:
  if o.type!='MESH' or o.name.startswith('UCX'):continue
  pts=[(p.x*100,-p.y*100,p.z*100) for p in [o.matrix_world@v.co for v in o.data.vertices]]
  bins=[]
  for axis in range(3):
   c=collections.Counter(round(p[axis],1) for p in pts);bins.append(c.most_common(12))
  r.append({'name':o.name,'planes_by_vertex_count':bins})
 out[file.stem]=r
(A/'PipeGeometryInspection.json').write_text(json.dumps(out,indent=2))
