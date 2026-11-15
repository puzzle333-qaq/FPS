import unreal,json
from pathlib import Path
E=unreal.EditorAssetLibrary;D='/Game/FPS/Animations/Compliance';skeleton=unreal.load_asset('/Game/Bandits/Mesh/SKM_Bandit_Skeleton');rows=[]
for name,source in [('A_DoorOpenLeft3P','AS_OpenDoorLnew'),('A_DoorOpenRight3P','AS_OpenDoorR'),('A_DoorKick3P','AS_Kick')]:
 path=D+'/'+name;a=unreal.load_asset(path)if E.does_asset_exist(path)else E.duplicate_asset('/Game/DoorInteractionKit/Animations/AnimationSequence/'+source,path);assert a
 if a.get_editor_property('skeleton')!=skeleton:assert unreal.CompliancePoseComponent.prepare_compatible_animation(a,skeleton)
 a.set_editor_property('force_root_lock',True);a.set_editor_property('root_motion_root_lock',unreal.RootMotionRootLock.REF_POSE);a.set_editor_property('enable_root_motion',False);assert E.save_loaded_asset(a,False)
 rows.append({'asset':path,'source':source,'length':a.get_play_length()})
Path(unreal.Paths.project_saved_dir(),'DangerPack/DoorAnimations.json').write_text(json.dumps(rows,indent=2));print(rows)
