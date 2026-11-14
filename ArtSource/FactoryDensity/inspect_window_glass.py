import bpy,json
from pathlib import Path
A=Path(r'C:\Users\ASUS\Documents\Unreal Projects\FPS\ArtSource\FactoryArt\ArchitectureFBX');r={}
for name in ['SM_FA_WindowTile_400x300','SM_FA_WindowTile_355x300']:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(A/(name+'.fbx')))
 o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX'))
 groups={}
 for face in o.data.polygons:
  m=o.data.materials[face.material_index];groups.setdefault(m.name,[])
  for i in face.vertices:
   p=o.matrix_world@o.data.vertices[i].co;groups[m.name].append([p.x*100,-p.y*100,p.z*100])
 r[name]={n:[min(p[i] for p in ps) for i in range(3)]+[max(p[i] for p in ps) for i in range(3)] for n,ps in groups.items()}
Path(r'C:\Users\ASUS\Documents\Unreal Projects\FPS\Saved\FactoryDensity_WindowGlass.json').write_text(json.dumps(r,indent=2))
