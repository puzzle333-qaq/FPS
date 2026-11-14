import bpy,json
from pathlib import Path
A=Path(r'C:\Users\ASUS\Documents\Unreal Projects\FPS\ArtSource\FactoryArt');results={}
for name in ['FactoryWall','OfficeWall','UndergroundWall']:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(A/(name+'.fbx')));objs=[o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX')];r=[]
 for o in objs:
  pts=[o.matrix_world@v.co for v in o.data.vertices];r.append({'name':o.name,'min':[min(p[i] for p in pts)*100 for i in range(3)],'max':[max(p[i] for p in pts)*100 for i in range(3)],'faces':len(o.data.polygons),'materials':[m.name if m else None for m in o.data.materials]})
 results[name]=r
(A/'SourceInspection.json').write_text(json.dumps(results,indent=2));print(results)
