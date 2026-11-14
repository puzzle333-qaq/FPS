import bpy,bmesh
from pathlib import Path
R=Path(r'C:\Users\ASUS\Documents\Unreal Projects\FPS\ArtSource\FactoryRepair');bpy.ops.wm.read_factory_settings(use_empty=True);bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
bpy.ops.import_scene.fbx(filepath=str(R/'SM_StairRail_Source.fbx'))
objs=[o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX')];assert len(objs)==1;o=objs[0];o.data.transform(o.matrix_world);o.matrix_world.identity();o.name='SM_StairsHandrail_Opposite'
for v in o.data.vertices:v.co.x=3.29686279296875-v.co.x
bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.reverse_faces(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free()
bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
bpy.ops.export_scene.fbx(filepath=str(R/'FBX/SM_StairsHandrail_Opposite.fbx'),use_selection=True,object_types={'MESH'},global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False)
