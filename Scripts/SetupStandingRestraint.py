"""Adapt the genuine cuffing operator clip to standing waist-bend plus wrist IK; victim remains kneeling."""
import unreal,json,math
from pathlib import Path

def bake():
 E=unreal.EditorAssetLibrary;D='/Game/FPS/Animations/Compliance';original='/Game/Hostage_Set/Animations/Paired_HandcuffHostage/Paired_HandcuffHostage_Start_Att'
 source=unreal.load_asset('/Game/Hostage_Set/Mannequin/Character/Mesh/SK_Mannequin');target=unreal.load_asset('/Game/FPS/Characters/Police/SKM_Police_Preset_07');rtg=unreal.load_asset(D+'/Retarget/RTG_Hostage_UE5')
 rawpath=D+'/Retargeted/Paired_HandcuffHostage_Start_Att_StandingSource'
 if not E.does_asset_exist(rawpath):
  result=unreal.IKRetargetBatchOperation.duplicate_and_retarget([E.find_asset_data(original)],source,target,rtg,'','','','_StandingSource',D+'/Retargeted',False,False,False);assert result
 raw=unreal.load_asset(rawpath);assert raw
 out=unreal.load_asset(D+'/A_OperatorBind3P');fp=unreal.load_asset(D+'/A_OperatorBind1P');victim=unreal.load_asset(D+'/A_TargetBind');standing=unreal.load_asset(D+'/A_Calm');assert out and fp and victim
 mod=unreal.SkeletonModifier();mod.set_skeletal_mesh(target);parents={str(b):str(mod.get_parent_name(b))for b in mod.get_all_bone_names()}
 n=raw.data_model_interface.get_number_of_keys();rate=raw.data_model_interface.get_frame_rate();fps=rate.numerator/rate.denominator;names=list(raw.data_model_interface.get_bone_track_names());keys={str(b):[[],[],[]]for b in names}
 base=unreal.AnimPoseExtensions.get_anim_pose_at_time(standing,0,unreal.AnimPoseEvaluationOptions());two_ik=getattr(unreal.AnimGraphLibrary,'two_bone_ik',getattr(unreal.AnimGraphLibrary,'k2_two_bone_ik',None));assert two_ik
 reports=[]
 for i in range(n):
  t=i/fps;pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(raw,t,unreal.AnimPoseEvaluationOptions());vp=unreal.AnimPoseExtensions.get_anim_pose_at_time(victim,t,unreal.AnimPoseEvaluationOptions())
  local={str(b):unreal.AnimPoseExtensions.get_bone_pose(pose,b,unreal.AnimPoseSpaces.LOCAL)for b in names}
  for b in list(local):
   if b=='root':local[b]=unreal.Transform()
   elif b=='pelvis'or b.startswith(('thigh','calf','foot','ball','ik_foot')):local[b]=unreal.AnimPoseExtensions.get_bone_pose(base,b,unreal.AnimPoseSpaces.LOCAL)
  cache={}
  def world(b):
   if b in cache:return cache[b]
   parent=parents.get(b,'None');lt=local[b] if b in local else mod.get_bone_transform(b,False)
   result=lt if parent in ['None','']else unreal.MathLibrary.compose_transforms(lt,world(parent));cache[b]=result;return result
  ramp=max(0,min(1,t/.65,(raw.get_play_length()-t)/.65));ramp=ramp*ramp*(3-2*ramp)
  bend=unreal.Rotator(roll=80*ramp,pitch=0,yaw=0).quaternion();pr=world('pelvis').rotation;lt=local['spine_01'];lt.rotation=unreal.MathLibrary.quat_inversed(pr)*bend*pr*lt.rotation;local['spine_01']=lt;cache.clear()
  for side in ['l','r']:
   upper='upperarm_'+side;lower='lowerarm_'+side;hand='hand_'+side
   goal=unreal.AnimPoseExtensions.get_bone_pose(vp,hand,unreal.AnimPoseSpaces.WORLD).translation+unreal.Vector(0,75,0)
   clav='clavicle_'+side;cl=world(clav);oldv=world(upper).translation-cl.translation;direction=goal-cl.translation;direction=direction/max(direction.length(),.001)
   desired=oldv*.35+direction*oldv.length()*.65;desired=desired/max(desired.length(),.001)*oldv.length()
   cr=unreal.MathLibrary.quat_find_between_vectors(oldv,desired)*cl.rotation;ct=unreal.Transform();ct.translation=cl.translation;ct.rotation=cr;ct.scale3d=cl.scale3d
   local[clav]=unreal.MathLibrary.make_relative_transform(ct,world(parents[clav]));cache.clear()
   u=world(upper);l=world(lower);w=world(hand)
   joint,end=two_ik(u.translation,l.translation,w.translation,l.translation,goal,True,1.05,1.15)
   ur=unreal.MathLibrary.quat_find_between_vectors(l.translation-u.translation,joint-u.translation)*u.rotation
   lr=unreal.MathLibrary.quat_find_between_vectors(w.translation-l.translation,end-joint)*l.rotation
   authored_transform=unreal.AnimPoseExtensions.get_bone_pose(pose,hand,unreal.AnimPoseSpaces.WORLD);authored=authored_transform.rotation
   new_u=unreal.Transform();new_u.translation=u.translation;new_u.rotation=ur;new_u.scale3d=u.scale3d
   new_l=unreal.Transform();new_l.translation=joint;new_l.rotation=lr;new_l.scale3d=l.scale3d
   new_h=unreal.Transform();new_h.translation=end;new_h.rotation=authored;new_h.scale3d=w.scale3d
   local[upper]=unreal.MathLibrary.make_relative_transform(new_u,world(parents[upper]));local[lower]=unreal.MathLibrary.make_relative_transform(new_l,new_u);local[hand]=unreal.MathLibrary.make_relative_transform(new_h,new_l);cache.clear()
   if i in [60,90,120]:reports.append({'time':t,'side':side,'wrist_error_cm':(end-goal).length()})
  for b in keys:
   tr=local[b];keys[b][0].append(unreal.Vector(tr.translation.x,tr.translation.y,tr.translation.z));keys[b][1].append(unreal.Quat(tr.rotation.x,tr.rotation.y,tr.rotation.z,tr.rotation.w));keys[b][2].append(unreal.Vector(tr.scale3d.x,tr.scale3d.y,tr.scale3d.z))
 for asset in [out,fp]:
  ctl=asset.controller;ctl.open_bracket('Standing waist-bend restraint with wrist IK',False)
  try:
   for b,k in keys.items():assert ctl.set_bone_track_keys(b,*k,False),b
  finally:ctl.close_bracket(False)
  asset.set_editor_property('force_root_lock',True);asset.set_editor_property('root_motion_root_lock',unreal.RootMotionRootLock.REF_POSE);asset.set_editor_property('enable_root_motion',False);assert E.save_loaded_asset(asset,False)
 Path(unreal.Paths.project_saved_dir(),'DangerPack/StandingRestraint.json').write_text(json.dumps({'source':original,'operator':'standing, waist bend; knees retain standing pose','actor_alignment':[-75,0,0],'wrist_checks':reports},indent=2));print(reports)




bake()
