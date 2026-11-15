"""Keep the victim's authored wrist motion behind the body when adapting standing UE4 cuffing to kneeling UE5."""
import unreal,json
from pathlib import Path

def bake():
 E=unreal.EditorAssetLibrary;D='/Game/FPS/Animations/Compliance';sp='/Game/Hostage_Set/Animations/Paired_HandcuffHostage/Paired_HandcuffHostage_Start_Vic';original=unreal.load_asset(sp);source=unreal.load_asset('/Game/Hostage_Set/Mannequin/Character/Mesh/SK_Mannequin');target=unreal.load_asset('/Game/FPS/Characters/Police/SKM_Police_Preset_07');rtg=unreal.load_asset(D+'/Retarget/RTG_Hostage_UE5');rp=D+'/Retargeted/Paired_HandcuffHostage_Start_Vic_AnatomySource'
 if not E.does_asset_exist(rp):assert unreal.IKRetargetBatchOperation.duplicate_and_retarget([E.find_asset_data(sp)],source,target,rtg,'','','','_AnatomySource',D+'/Retargeted',False,False,False)
 raw=unreal.load_asset(rp);out=unreal.load_asset(D+'/A_TargetBind');base=unreal.load_asset(D+'/A_KneelBound');bp=unreal.AnimPoseExtensions.get_anim_pose_at_time(base,0,unreal.AnimPoseEvaluationOptions());mod=unreal.SkeletonModifier();mod.set_skeletal_mesh(target);parents={str(b):str(mod.get_parent_name(b))for b in mod.get_all_bone_names()};names=list(raw.data_model_interface.get_bone_track_names());n=raw.data_model_interface.get_number_of_keys();fr=raw.data_model_interface.get_frame_rate();fps=fr.numerator/fr.denominator;keys={str(b):[[],[],[]]for b in names};reports=[];two=getattr(unreal.AnimGraphLibrary,'two_bone_ik',getattr(unreal.AnimGraphLibrary,'k2_two_bone_ik',None))
 for i in range(n):
  t=i/fps;p=unreal.AnimPoseExtensions.get_anim_pose_at_time(raw,t,unreal.AnimPoseEvaluationOptions());op=unreal.AnimPoseExtensions.get_anim_pose_at_time(original,t,unreal.AnimPoseEvaluationOptions());origpelvis=unreal.AnimPoseExtensions.get_bone_pose(op,'pelvis',unreal.AnimPoseSpaces.WORLD).translation;local={str(b):unreal.AnimPoseExtensions.get_bone_pose(p,b,unreal.AnimPoseSpaces.LOCAL)for b in names}
  for b in local:
   if b=='root':local[b]=unreal.Transform()
   elif b=='pelvis'or b.startswith(('thigh','calf','foot','ball','ik_foot')):local[b]=unreal.AnimPoseExtensions.get_bone_pose(bp,b,unreal.AnimPoseSpaces.LOCAL)
  cache={}
  def world(b):
   if b in cache:return cache[b]
   parent=parents.get(b,'None');lt=local[b] if b in local else mod.get_bone_transform(b,False);v=lt if parent in ['None','']else unreal.MathLibrary.compose_transforms(lt,world(parent));cache[b]=v;return v
  for side in ['l','r']:
   u_name='upperarm_'+side;l_name='lowerarm_'+side;h_name='hand_'+side;u=world(u_name);l=world(l_name);w=world(h_name)
   goal=unreal.AnimPoseExtensions.get_bone_pose(op,h_name,unreal.AnimPoseSpaces.WORLD).translation-origpelvis+world('pelvis').translation
   j,e=two(u.translation,l.translation,w.translation,l.translation,goal,True,1.05,1.15)
   ur=unreal.MathLibrary.quat_find_between_vectors(l.translation-u.translation,j-u.translation)*u.rotation;lr=unreal.MathLibrary.quat_find_between_vectors(w.translation-l.translation,e-j)*l.rotation
   a=unreal.Transform();a.translation=u.translation;a.rotation=ur;a.scale3d=u.scale3d;b=unreal.Transform();b.translation=j;b.rotation=lr;b.scale3d=l.scale3d;hand=unreal.Transform();hand.translation=e;hand.rotation=unreal.AnimPoseExtensions.get_bone_pose(p,h_name,unreal.AnimPoseSpaces.WORLD).rotation;hand.scale3d=w.scale3d
   local[u_name]=unreal.MathLibrary.make_relative_transform(a,world(parents[u_name]));local[l_name]=unreal.MathLibrary.make_relative_transform(b,a);local[h_name]=unreal.MathLibrary.make_relative_transform(hand,b);cache.clear()
   if i in [60,90,120]:reports.append({'time':t,'side':side,'hand_y':e.y,'wrist_error_cm':(e-goal).length()})
  for b in keys:
   tr=local[b];keys[b][0].append(unreal.Vector(tr.translation.x,tr.translation.y,tr.translation.z));keys[b][1].append(unreal.Quat(tr.rotation.x,tr.rotation.y,tr.rotation.z,tr.rotation.w));keys[b][2].append(unreal.Vector(tr.scale3d.x,tr.scale3d.y,tr.scale3d.z))
 ctl=out.controller;ctl.open_bracket('Kneeling victim, wrists behind back',False)
 try:
  for b,k in keys.items():assert ctl.set_bone_track_keys(b,*k,False)
 finally:ctl.close_bracket(False)
 out.set_editor_property('force_root_lock',True);out.set_editor_property('root_motion_root_lock',unreal.RootMotionRootLock.REF_POSE);assert E.save_loaded_asset(out,False)
 Path(unreal.Paths.project_saved_dir(),'DangerPack/VictimWristPlacement.json').write_text(json.dumps(reports,indent=2));print(reports)

bake()
