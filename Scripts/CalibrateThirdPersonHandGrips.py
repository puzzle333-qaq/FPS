"""Bake authored third-person left-hand grips and the existing X24 right-hand socket calibration.
Run in the Unreal editor outside PIE after compiling the new AWeapon fields.
Source meshes and animation sequences are read-only; only FPS weapon Blueprint defaults change.
"""
import unreal,json,shutil,datetime
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/ScreenRefresh/GripBackups'/datetime.datetime.now().strftime('%Y%m%d_%H%M%S');out.mkdir(parents=True,exist_ok=True)
report=[];pending=[]
for name in ('AK105','TR15','Mk14','MKR9','MX68','R08','SRM12','Herrington','Viper'):
 path='/Game/FPS/Weapon/Kinemation/BP_Weapon_'+name;bp=unreal.load_asset(path);cdo=unreal.get_default_object(unreal.load_class(None,path+'.BP_Weapon_'+name+'_C'));seq=cdo.get_editor_property('ArmsIdleSequence');assert seq,name
 pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(seq,0,unreal.AnimPoseEvaluationOptions());gun=unreal.AnimPoseExtensions.get_bone_pose(pose,'ik_hand_gun',unreal.AnimPoseSpaces.WORLD);left=unreal.AnimPoseExtensions.get_bone_pose(pose,'hand_l',unreal.AnimPoseSpaces.WORLD);grip=unreal.MathLibrary.make_relative_transform(left,gun);grip.scale3d=unreal.Vector(1,1,1)
 rebuilt=unreal.MathLibrary.compose_transforms(grip,gun);error=(rebuilt.translation-left.translation).length();assert error<.001,name
 pending.append((path,bp,cdo,grip,None));report.append({'weapon':name,'source':seq.get_path_name(),'left_grip_cm':[grip.translation.x,grip.translation.y,grip.translation.z],'reconstruction_error_cm':error})
# PistolSocketX24 on the existing FP arms is a hand_r-relative mount. The old
# third-person fallback to the independent ik_hand_gun bone did not preserve it.
mount=unreal.Transform();mount.translation=unreal.Vector(-6.936427,2.185551,1.308499);mount.rotation=unreal.Quat(.08107389435,-.17516861457,.73362624135,.65156390180);mount.scale3d=unreal.Vector(1,1,1)
path='/Game/FPS/Weapon/BP_Weapon_X24';bp=unreal.load_asset(path);cdo=unreal.get_default_object(unreal.load_class(None,path+'.BP_Weapon_X24_C'));seq=unreal.load_asset('/Game/FPS_Controller/Animations/FPP_Anims/Pistol/Walk/A_FPP_X24_Idle');pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(seq,0,unreal.AnimPoseEvaluationOptions());right=unreal.AnimPoseExtensions.get_bone_pose(pose,'hand_r',unreal.AnimPoseSpaces.WORLD);left=unreal.AnimPoseExtensions.get_bone_pose(pose,'hand_l',unreal.AnimPoseSpaces.WORLD);gun=unreal.MathLibrary.compose_transforms(mount,right);grip=unreal.MathLibrary.make_relative_transform(left,gun);grip.scale3d=unreal.Vector(1,1,1);pending.append((path,bp,cdo,grip,mount));report.append({'weapon':'X24','source':seq.get_path_name(),'mount_socket':'hand_r','source_mount':'existing FP PistolSocketX24, measured before changing weapon defaults','left_grip_cm':[grip.translation.x,grip.translation.y,grip.translation.z]})
for path,bp,cdo,grip,mount in pending:
 source=root/'Content'/(path.removeprefix('/Game/')+'.uasset');shutil.copy2(source,out/source.name)
 cdo.set_editor_property('bHasThirdPersonLeftHandGrip',True);cdo.set_editor_property('ThirdPersonLeftHandGrip',grip)
 if mount is not None:cdo.set_editor_property('Mesh3PAttachSocket','hand_r');cdo.set_editor_property('Mesh3PAttachTransform',mount)
 unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False),path
(out/'Calibration.json').write_text(json.dumps(report,indent=2));(root/'Saved/ScreenRefresh/GripCalibration.json').write_text(json.dumps(report,indent=2));print('Calibrated',len(report),'weapons; backup',out)
