import unreal,json
from pathlib import Path
E=unreal.EditorAssetLibrary;source=unreal.load_asset('/Game/FPS/Characters/Police/SKM_Police_Sleeves');assert source
D='/Game/FPS/Characters/Police/SKM_Police_BindSleeves';target=unreal.load_asset(D)if E.does_asset_exist(D)else E.duplicate_asset(source.get_path_name(),D);assert target
mesh=unreal.DynamicMesh();_,result=unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(source,mesh,unreal.GeometryScriptCopyMeshFromAssetOptions(),unreal.GeometryScriptMeshReadLOD());assert result==unreal.GeometryScriptOutcomePins.SUCCESS
_,bones=unreal.GeometryScript_BoneWeights.get_all_bones_info(mesh);names={b.index:str(b.name)for b in bones};remove=[];n=unreal.GeometryScript_MeshQueries.get_vertex_count(mesh)
for v in range(n):
 _,weight,valid=unreal.GeometryScript_BoneWeights.get_largest_vertex_bone_weight(mesh,v)
 if not valid or not names.get(weight.bone_index,'').startswith(('clavicle','upperarm','lowerarm','hand','thumb','index','middle','ring','pinky')):remove.append(v)
assert 0<len(remove)<n,(len(remove),n)
indices=unreal.GeometryScript_List.convert_array_to_index_list(remove,unreal.GeometryScriptIndexType.VERTEX)
_,deleted=unreal.GeometryScript_MeshEdits.delete_vertices_from_mesh(mesh,indices,False)
_,result=unreal.GeometryScript_AssetUtils.copy_mesh_to_skeletal_mesh(mesh,target,unreal.GeometryScriptCopyMeshToAssetOptions(),unreal.GeometryScriptMeshWriteLOD());assert result==unreal.GeometryScriptOutcomePins.SUCCESS
assert E.save_loaded_asset(target,False)
Path(unreal.Paths.project_saved_dir(),'DangerPack/BindSleeves.json').write_text(json.dumps({'source':source.get_path_name(),'result':D,'original_vertices':n,'removed_torso_hood_vertices':deleted,'remaining_vertices':unreal.GeometryScript_MeshQueries.get_vertex_count(mesh)},indent=2));print('sleeves',n,deleted)
