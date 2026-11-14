import unreal,json,shutil
from pathlib import Path
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
backup=root/'Saved/FactoryGMFix/GroundBefore';backup.mkdir(exist_ok=True)
r=[]
for name in ['SM_FactoryFloor_Stairwell','SM_FactoryDeployment_Apron']:
 path='/Game/LevelRepairs/Factory/'+name;s=unreal.load_asset(path)
 if unreal.EditorAssetLibrary.get_metadata_tag(s,'FactoryGroundNormalsFixed')=='1':continue
 file=root/'Content/LevelRepairs/Factory'/(name+'.uasset')
 if not (backup/file.name).exists():shutil.copy2(file,backup/file.name)
 d=unreal.DynamicMesh()
 _,outcome=unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(s,d,unreal.GeometryScriptCopyMeshFromAssetOptions(),unreal.GeometryScriptMeshReadLOD())
 assert outcome==unreal.GeometryScriptOutcomePins.SUCCESS,outcome
 unreal.GeometryScript_Normals.flip_normals(d)
 options=unreal.GeometryScriptCopyMeshToAssetOptions()
 print('copy options',options)
 _,outcome=unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(d,s,options,unreal.GeometryScriptMeshWriteLOD())
 assert outcome==unreal.GeometryScriptOutcomePins.SUCCESS,outcome
 unreal.EditorAssetLibrary.set_metadata_tag(s,'FactoryGroundNormalsFixed','1')
 assert unreal.EditorAssetLibrary.save_loaded_asset(s,False)
 r.append(path)
print('fixed',r)
