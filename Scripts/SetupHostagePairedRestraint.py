"""Retarget the real Hostage_Set cuffing pair, retaining kneeling lower-body poses."""
import unreal,json
from pathlib import Path
D='/Game/FPS/Animations/Compliance';E=unreal.EditorAssetLibrary
src=unreal.load_asset('/Game/Hostage_Set/Mannequin/Character/Mesh/SK_Mannequin');target=unreal.load_asset('/Game/FPS/Characters/Police/SKM_Police_Preset_07')
rp=D+'/Retarget/RTG_Hostage_UE5';rtg=unreal.load_asset(rp)if E.does_asset_exist(rp)else E.duplicate_asset('/Game/CGrPCC/DEMO/Characters/Mannequin_UE4/Rigs/RTG_UE4Manny_UE5Manny',rp)
assert rtg and src and target
controller=unreal.IKRetargeterController.get_controller(rtg);controller.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE,src);controller.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET,target);E.save_loaded_asset(rtg,False)
rows=[]
for role,name in [('Att','A_OperatorBind3P'),('Vic','A_TargetBind')]:
 dst=D+'/'+name
 if not E.does_asset_exist(dst):
  original='/Game/Hostage_Set/Animations/Paired_HandcuffHostage/Paired_HandcuffHostage_Start_'+role
  result=unreal.IKRetargetBatchOperation.duplicate_and_retarget([E.find_asset_data(original)],src,target,rtg,'','','','_UE5',D+'/Retargeted',False,False,True)
  assert len(result)==1,result
  assert E.rename_asset(str(result[0].package_name),dst)
 a=unreal.load_asset(dst);assert a
 # Use the authored paired upper-body motion on the already-kneeling victim and operator.
 base=unreal.load_asset(D+'/A_KneelHandsUp');pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(base,0,unreal.AnimPoseEvaluationOptions())
 model=a.data_model_interface;n=model.get_number_of_keys();ctl=a.controller;ctl.open_bracket('Kneeling restraint pair',False)
 try:
  for bone in model.get_bone_track_names():
   bn=str(bone)
   if bn in ['root','pelvis']or bn.startswith(('thigh','calf','foot','ball','ik_foot')):
    t=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.LOCAL)
    if bn=='root':t=unreal.Transform()
    assert ctl.set_bone_track_keys(bone,[t.translation]*n,[t.rotation]*n,[t.scale3d]*n,False),bone
 finally:ctl.close_bracket(False)
 a.set_editor_property('force_root_lock',True);a.set_editor_property('root_motion_root_lock',unreal.RootMotionRootLock.REF_POSE);a.set_editor_property('enable_root_motion',False);assert E.save_loaded_asset(a,False)
 rows.append({'asset':dst,'source':'Paired_HandcuffHostage_Start_'+role,'duration':a.get_play_length(),'keys':n})
fp=D+'/A_OperatorBind1P'
if not E.does_asset_exist(fp):assert E.duplicate_asset(D+'/A_OperatorBind3P',fp)
assert E.save_asset(fp,False)
# Record the authored pair's horizontal alignment in actor coordinates (meshes face -90 degrees).
centers=[]
for name in ['A_OperatorBind3P','A_TargetBind']:
 a=unreal.load_asset(D+'/'+name);poses=[unreal.AnimPoseExtensions.get_anim_pose_at_time(a,t,unreal.AnimPoseEvaluationOptions())for t in [2.,3.,4.]]
 hands=[(unreal.AnimPoseExtensions.get_bone_pose(p,'hand_l',unreal.AnimPoseSpaces.WORLD).translation+unreal.AnimPoseExtensions.get_bone_pose(p,'hand_r',unreal.AnimPoseSpaces.WORLD).translation)* .5 for p in poses];centers.append(sum(hands,unreal.Vector())/len(hands))
delta=centers[1]-centers[0]
report={'clips':rows,'alignment_actor_cm':[delta.y,-delta.x,0],'alignment_vertical_difference':delta.z,'source':'Hostage_Set paired handcuff animation; kneeling lower-body adaptation'}
Path(unreal.Paths.project_saved_dir(),'DangerPack/PairedRetarget.json').write_text(json.dumps(report,indent=2));print(report)
