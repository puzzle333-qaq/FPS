"""Shared category holds, grip-specific left-hand targets and first-person FABRIK."""
import unreal,json,shutil
from pathlib import Path
from editor_toolset.toolsets.blueprint import BlueprintTools
E=unreal.EditorAssetLibrary; B=unreal.BlueprintEditorLibrary
root=Path(unreal.Paths.project_dir());report=[]
def save(a):assert E.save_loaded_asset(a,False)
def backup(a):
 p=root/'Content'/(a.get_path_name().split('.')[0].removeprefix('/Game/')+'.uasset');d=root/'Saved/BeforeHybridSetup'/p.relative_to(root)
 if p.exists() and not d.exists():d.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,d)
# All long guns resolve this row, retaining their own action/reload animations.
data=unreal.load_asset('/Game/FPS/Data/DA_WeaponData');backup(data)
rows=data.get_editor_property('FirstPersonAnims')
for tag,row in rows.items():
 if 'Weapon.type.rifle' in tag.export_text():
  base='/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Poses/'
  row.import_text('(IdleAnim="'+base+'A_RAR_FP_PCH_AssaultRifle_Idle_Pose.A_RAR_FP_PCH_AssaultRifle_Idle_Pose",AimIdleAnim="'+base+'A_RAR_FP_PCH_AssaultRifle_Aim_Pose.A_RAR_FP_PCH_AssaultRifle_Aim_Pose")');rows[tag]=row
 data.set_editor_property('FirstPersonAnims',rows)
save(data)
# Author separately named targets on each grip model, based on its existing hand socket.
allowed=set()
for path in ['/Game/FPS/Weapon/RAR/BP_FPS_RAR','/Game/FPS/Weapon/Kinemation/BP_Weapon_TR15']:
 cdo=unreal.get_default_object(E.load_blueprint_class(path))
 for slot in cdo.get_editor_property('AttachmentSlots'):
  if 'grip' in slot.get_editor_property('SlotTag').export_text().lower():
   for a in slot.get_editor_property('AllowedAttachments'):allowed.add(a.get_path_name())
   default=slot.get_editor_property('DefaultAttachment')
   if default:allowed.add(default.get_path_name())
registry=unreal.AssetRegistryHelpers.get_asset_registry()
for asset in registry.get_assets_by_class(unreal.TopLevelAssetPath('/Script/FPS','AttachmentData'),True):
 a=asset.get_asset()
 if a.get_path_name() not in allowed:continue
 if not str(a.get_editor_property('SlotTag').export_text()).lower().find('grip')>=0:continue
 mesh=a.get_editor_property('Mesh')
 if not mesh:continue
 name=a.get_name().lower()
 style='Vertical' if a.get_editor_property('GripHoldStyle')==unreal.GripHoldStyle.GRAB else ('Triangle' if '/RAR/' in a.get_path_name() or any(k in name for k in ['tri','afg']) else 'Angled')
 target='LH_'+style
 source=next((mesh.find_socket(key) for key in [str(a.get_editor_property('FabrikSocketName')),'GripSocket','FABRIK','ik_hand_l'] if mesh.find_socket(key)),None)
 if not source:
  # Bake a hand anchor from the authored InfimaGames grip pose into the actual grip component.
  path='/Game/FPS/Weapon/RAR/BP_FPS_RAR' if '/RAR/' in a.get_path_name() else '/Game/FPS/Weapon/Kinemation/BP_Weapon_TR15'
  sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
  gun=sub.spawn_actor_from_class(E.load_blueprint_class(path),unreal.Vector(0,0,0),unreal.Rotator(),True)
  for slot in gun.get_editor_property('AttachmentSlots'):
   default=slot.get_editor_property('DefaultAttachment')
   if default:gun.equip_attachment(default)
  assert gun.equip_attachment(a),a.get_path_name()
  gunmesh=gun.get_editor_property('Mesh1P')
  grip=None
  for comp in gun.get_components_by_class(unreal.StaticMeshComponent):
   if comp.get_editor_property('static_mesh')!=mesh:continue
   parent=comp.get_attach_parent()
   while parent:
    if parent==gunmesh:grip=comp;break
    parent=parent.get_attach_parent()
   if grip:break
  assert grip,a.get_path_name()
  suffix='Vertical' if style=='Vertical' else 'Angled'
  pose_asset=unreal.load_asset('/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Poses/A_RAR_FP_PCH_AssaultRifle_Idle_Pose_Grip_'+suffix)
  pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(pose_asset,0,unreal.AnimPoseEvaluationOptions())
  hand=unreal.AnimPoseExtensions.get_bone_pose(pose,'hand_l',unreal.AnimPoseSpaces.WORLD)
  pivot=unreal.AnimPoseExtensions.get_bone_pose(pose,'ik_hand_gun',unreal.AnimPoseSpaces.WORLD)
  hand_gun=unreal.MathLibrary.make_relative_transform(hand,pivot)
  world=unreal.MathLibrary.compose_transforms(hand_gun,gunmesh.get_world_transform())
  local=unreal.MathLibrary.make_relative_transform(world,grip.get_world_transform())
  source=unreal.new_object(unreal.StaticMeshSocket,outer=mesh)
  source.set_editor_property('relative_location',local.translation)
  source.set_editor_property('relative_rotation',local.rotation.rotator())
  source.set_editor_property('relative_scale',unreal.Vector(1,1,1))
  sub.destroy_actor(gun)
 backup(a);backup(mesh)
 new=mesh.find_socket(target)
 if not new:
  new=unreal.new_object(unreal.StaticMeshSocket,outer=mesh);new.set_editor_property('socket_name',target);mesh.add_socket(new)
 for field in ['relative_location','relative_rotation','relative_scale']:new.set_editor_property(field,source.get_editor_property(field))
 save(mesh)
 a.set_editor_property('FirstPersonFabrikSocketName',target);save(a);report.append({'grip':a.get_path_name(),'target':target})
# RAR and TR15 share the same first-person mount/hold/draw/fire profile.
rar_bp=unreal.load_asset('/Game/FPS/Weapon/RAR/BP_FPS_RAR');tr_bp=unreal.load_asset('/Game/FPS/Weapon/Kinemation/BP_Weapon_TR15')
rar=unreal.get_default_object(B.generated_class(rar_bp));tr=unreal.get_default_object(B.generated_class(tr_bp))
backup(rar_bp);backup(tr_bp)
for field in ['Mesh1PAttachSocket','Mesh1PAttachRotation','Mesh1PAttachLocation','bMesh1PAttachUsePivotOffset',
              'bUseMeshAttachRelativeRotation','Mesh1PAttachRelativeLocation','Mesh1PAttachRelativeRotation',
              'HipMesh1POffset','HipMesh1PRotationOffset']:
 tr.set_editor_property(field,rar.get_editor_property(field))
base='/Game/InfimaGames/RealisticAssaultRifle/Art/Character/Animations/Actions/'
for obj in [rar,tr]:
 for field,name in [('ArmsEquipSequence','A_RAR_FP_PCH_AssaultRifle_Equip'),('ArmsFireSequence','A_RAR_FP_PCH_AssaultRifle_Fire'),('ArmsFireAimSequence','A_RAR_FP_PCH_AssaultRifle_Fire_Aimed')]:
  obj.set_editor_property(field,unreal.load_asset(base+name))
# Reload sequence fields and TR15's Mag_Check resolver remain weapon-specific.
save(rar_bp);save(tr_bp)
# Finish the existing first-person graph with IK; the native AnimInstance supplies target and alpha.
bp=unreal.load_asset('/Game/FPS/Character/ABP_FirstPerson');backup(bp)
g=B.find_graph(bp,'AnimGraph');ed=unreal.BlueprintGraphEditor.get_graph_editor(g);nodes=list(ed.list_all_nodes())
if not any(isinstance(n,unreal.AnimGraphNode_Fabrik) for n in nodes):
 rootnode=next(n for n in nodes if isinstance(n,unreal.AnimGraphNode_Root))
 info=BlueprintTools.get_node_infos([rootnode])[0];source=info.input_pins[0].connected_pins[0].node
 available=list(ed.list_available_nodes([]))
 def create(part):
  key=next(n for n in available if part.lower() in n.lower());return ed.create_node_from_name(key,unreal.Vector2D(1500,0),[],None)
 def connect(a,out,b,inp):assert a.find_output_pin(out).try_create_connection(b.find_input_pin(inp))
 local=create('Animation|ConvertSpaces|LocalToComponent');ik=create('SkeletalControls|FABRIK');back=create('Animation|ConvertSpaces|ComponentToLocal')
 value=ik.get_editor_property('node');value.set_editor_property('effector_transform_space',unreal.BoneControlSpace.BCS_COMPONENT_SPACE)
 for field,bone_name in [('tip_bone','hand_l'),('root_bone','upperarm_l')]:
  bone=unreal.BoneReference();bone.set_editor_property('bone_name',bone_name);value.set_editor_property(field,bone)
 value.set_editor_property('effector_rotation_source',unreal.BoneRotationSource.BRS_COPY_FROM_TARGET);ik.set_editor_property('node',value)
 connect(source,'Pose',local,'LocalPose');connect(local,'ComponentPose',ik,'ComponentPose');connect(ik,'Pose',back,'ComponentPose');connect(back,'Pose',rootnode,'Result')
 for var,pin in [('FABRIK_Socket1P','EffectorTransform'),('FABRIK_Alpha1P','Alpha')]:
  getter=ed.add_get_member_variable_node(var,'');connect(getter,var,ik,pin)
 B.compile_blueprint(bp);assert not list(ed.list_nodes_with_errors());save(bp)
Path(unreal.Paths.project_saved_dir(),'HybridSetup.json').write_text(json.dumps(report,indent=2),encoding='utf8')
print('HYBRID_SETUP_COMPLETE',len(report))
