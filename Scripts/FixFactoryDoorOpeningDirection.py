import unreal,json,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir());backup=root/'Saved/DangerPack/BeforeDoorDirectionFix';backup.mkdir(parents=True,exist_ok=True)
map_path=root/'Content/Maps/Lvl_Factory_Whitebox.umap'
if not (backup/'Lvl_Factory_Whitebox.umap').exists():shutil.copy2(map_path,backup/'Lvl_Factory_Whitebox.umap')
actors=[d for d in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()if isinstance(d,unreal.BreakableDoor)]
classes={d.get_class().get_path_name():d.get_class()for d in actors}
for path,cls in classes.items():
 asset=path.split('.')[0];local=root/'Content'/(asset.removeprefix('/Game/')+'.uasset')
 if local.exists()and not (backup/local.name).exists():shutil.copy2(local,backup/local.name)
 default=unreal.get_default_object(cls);default.modify();default.set_editor_property('bAutoOpenAwayFromPlayer',True)
 assert unreal.EditorAssetLibrary.save_asset(asset,False)
for d in actors:d.modify();d.set_editor_property('bAutoOpenAwayFromPlayer',True)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('Enabled two-way opening:',len(actors),'doors; classes:',list(classes))
