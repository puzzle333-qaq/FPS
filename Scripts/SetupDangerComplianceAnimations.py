import unreal,json
from pathlib import Path
clips={'A_Crawl':'ProneCrawl/am_ProneCrawl_Loco_Crawl_Fwd_NoRM','A_Calm':'Idle/am_Stand_Idle_01','A_Scared':'Ready/am_Ready_Idle_01','A_Flee':'Locomotion/am_Loco_Jog_Tired_NoRM','A_HandsUpEnter':'HandsUp/am_Stand_Trans_HandsUp_01','A_KneelEnter':'HandsUp/am_HandsUp_Trans_HandsUpKneel','A_KneelHandsUp':'HandsUp/am_HandsUpKneel_01_Idle','A_KneelBound':'HandsTiedBack/am_HandsTiedBackKneel_01_Idle','A_TableHideEnter':'TableHide/am_Ready_Trans_TableHideA','A_TableHide':'TableHide/am_TableHideA_01_Idle','A_ProneEnter':'HideProne/am_Ready_Trans_HideProne','A_ProneHide':'HideProne/am_HideProne_01_Idle'}
target=unreal.load_asset('/Game/Bandits/Mesh/SKM_Bandit_Skeleton');rows=[]
for name,src in clips.items():
 dst='/Game/FPS/Animations/Compliance/'+name
 a=unreal.load_asset(dst)if unreal.EditorAssetLibrary.does_asset_exist(dst)else unreal.EditorAssetLibrary.duplicate_asset('/Game/MC_Danger_Pack/Animations/'+src,dst)
 assert a,src
 if a.get_editor_property('skeleton')!=target:assert unreal.CompliancePoseComponent.prepare_compatible_animation(a,target)
 a.set_editor_property('force_root_lock',True)
 a.set_editor_property('root_motion_root_lock',unreal.RootMotionRootLock.REF_POSE)
 a.set_editor_property('enable_root_motion',False)
 assert unreal.EditorAssetLibrary.save_loaded_asset(a,False)
 rows.append({'asset':dst,'source':src,'length':a.get_play_length(),'skeleton':a.get_editor_property('skeleton').get_path_name()})
Path(unreal.Paths.project_saved_dir(),'DangerPack/SetupAnimations.json').write_text(json.dumps(rows,indent=2));print(rows)
