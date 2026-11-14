import bpy,json
from pathlib import Path
bpy.ops.wm.read_factory_settings(use_empty=True)
p=Path(r'C:\Users\ASUS\Documents\Unreal Projects\FPS\ArtSource\FactoryRepair')
bpy.ops.import_scene.fbx(filepath=str(p/'SM_Walkway_Source.fbx'))
rows=[]
for o in bpy.context.scene.objects:
 if o.type=='MESH':
  pts=[o.matrix_world@v.co for v in o.data.vertices]
  rows.append({'name':o.name,'verts':len(pts),'min':[min(p[i] for p in pts)*100 for i in range(3)],'max':[max(p[i] for p in pts)*100 for i in range(3)]})
(p/'source_bounds.json').write_text(json.dumps(rows,indent=2))
print(rows)
