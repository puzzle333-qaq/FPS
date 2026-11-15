"""Safe FK-only version of the real Hostage_Set restraint pair, with a fixed kneeling lower body."""
import unreal,json
from pathlib import Path
def bake():
 E=unreal.EditorAssetLibrary;D='/Game/FPS/Animations/Compliance';src=unreal.load_asset('/Game/Hostage_Set/Mannequin/Character/Mesh/SK_Mannequin');target=unreal.load_asset('/Game/FPS/Characters/Police/SKM_Police_Preset_07');rp=D+'/Retarget/RTG_Hostage_KneelFK'
 rtg=unreal.load_asset(rp)if E.does_asset_exist(rp)else E.duplicate_asset(D+'/Retarget/RTG_Hostage_UE5',rp);ctrl=unreal.IKRetargeterController.get_controller(rtg)
 for i in range(ctrl.get_num_retarget_ops()):ctrl.set_retarget_op_enabled(i,i<2)
 E.save_loaded_asset(rtg,False);base=unreal.load_asset(D+'/A_KneelBound');pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(base,0,unreal.AnimPoseEvaluationOptions());assets=[];rows=[]
 for role,name in [('Att','A_OperatorKneelBind3P'),('Vic','A_TargetKneelBind')]:
  dst=D+'/'+name
  if not E.does_asset_exist(dst):
   path='/Game/Hostage_Set/Animations/Paired_HandcuffHostage/Paired_HandcuffHostage_Start_'+role
   result=unreal.IKRetargetBatchOperation.duplicate_and_retarget([E.find_asset_data(path)],src,target,rtg,'','','','_KneelFK',D+'/Retargeted',False,False,False);assert len(result)==1;assert E.rename_asset(str(result[0].package_name),dst)
  a=unreal.load_asset(dst);n=a.data_model_interface.get_number_of_keys();ctl=a.controller;ctl.open_bracket('Fixed kneeling lower body; authored FK upper body',False)
  try:
   for bone in a.data_model_interface.get_bone_track_names():
    bn=str(bone)
    if bn=='root'or bn=='pelvis'or bn.startswith(('thigh','calf','foot','ball','ik_foot')):
     tr=unreal.Transform()if bn=='root'else unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.LOCAL)
     v=unreal.Vector(tr.translation.x,tr.translation.y,tr.translation.z);q=unreal.Quat(tr.rotation.x,tr.rotation.y,tr.rotation.z,tr.rotation.w);s=unreal.Vector(tr.scale3d.x,tr.scale3d.y,tr.scale3d.z)
     assert ctl.set_bone_track_keys(bone,[v]*n,[q]*n,[s]*n,False)
  finally:ctl.close_bracket(False)
  a.set_editor_property('force_root_lock',True);a.set_editor_property('root_motion_root_lock',unreal.RootMotionRootLock.REF_POSE);a.set_editor_property('enable_root_motion',False);assert E.save_loaded_asset(a,False);assets.append(a);rows.append({'path':dst,'source_role':role,'duration':a.get_play_length()})
 fp=D+'/A_OperatorKneelBind1P'
 if not E.does_asset_exist(fp):assert E.duplicate_asset(D+'/A_OperatorKneelBind3P',fp)
 assert E.save_asset(fp,False)
 centers=[]
 for a in assets:
  p=unreal.AnimPoseExtensions.get_anim_pose_at_time(a,3.,unreal.AnimPoseEvaluationOptions());l=unreal.AnimPoseExtensions.get_bone_pose(p,'hand_l',unreal.AnimPoseSpaces.WORLD);r=unreal.AnimPoseExtensions.get_bone_pose(p,'hand_r',unreal.AnimPoseSpaces.WORLD);centers.append((l.translation+r.translation)*.5)
 delta=centers[1]-centers[0];report={'assets':rows,'operator_alignment':[delta.y,-delta.x,0],'vertical_difference':delta.z,'mode':'FK-only; no synthetic spine/arm IK or bone stretching'}
 Path(unreal.Paths.project_saved_dir(),'DangerPack/KneelFKPair.json').write_text(json.dumps(report,indent=2));print(report)
bake()
