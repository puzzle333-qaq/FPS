"""Connect RAR upper-body actions, ADS right-arm IK and post-ADS support-hand IK."""
import unreal,json,shutil
from pathlib import Path
E=unreal.EditorAssetLibrary;B=unreal.BlueprintEditorLibrary;out=Path(unreal.Paths.project_saved_dir())/'RARBodyActions';a=unreal.load_asset('/Game/FPS/Character/ABP_ThirdPerson')
file=Path(unreal.Paths.project_content_dir())/'FPS/Character/ABP_ThirdPerson.uasset';backup=out/'Before/ABP_ThirdPerson.uasset';backup.parent.mkdir(exist_ok=True)
if not backup.exists():shutil.copy2(file,backup)
def ed(name):return unreal.BlueprintGraphEditor.get_graph_editor(B.find_graph(a,name))
def node(e,n):return next(v for v in e.list_all_nodes() if v.get_name()==n)
def connect(n,o,m,i):
 p=m.find_input_pin(i);assert p.is_valid(),(m.get_name(),i);p.break_pin_links();q=n.find_output_pin(o);assert q.is_valid(),(n.get_name(),o);assert q.try_create_connection(p),(n.get_name(),o,m.get_name(),i)
def create(e,k,x,y):
 n=e.create_node_from_name(k,unreal.Vector2D(x,y),[],None);assert n,k;return n
def source(e,native,pinname,receiver):
 c=e.add_get_member_variable_node('Shooter Character','');v=e.add_get_member_variable_node(native,'/Script/FPS.ShooterCharacter');connect(c,'Shooter Character',v,'self');connect(v,native,receiver,pinname)
# Body's current Idle/Aim sequence now uses the actual IsAiming state.
e=ed('Aim Offset');seq=node(e,'AnimGraphNode_SequencePlayer_1');br=node(e,'K2Node_BreakStruct_0')
sel=create(e,'Utilities|Select',-500,450);aim=e.add_get_member_variable_node('IsAiming','');connect(aim,'IsAiming',sel,'Index')
connect(br,'IdleAnim',sel,'Option 0');connect(br,'AimIdleAnim',sel,'Option 1');connect(sel,'ReturnValue',seq,'Sequence')
# A dedicated slot, layered from spine_01, consumes body action montages without overriding legs.
e=ed('AnimGraph');slot=create(e,"Animation|Montage|Slot'DefaultSlot'",1700,-400);v=slot.get_editor_property('node');v.set_editor_property('slot_name','BodyUpperAction');slot.set_editor_property('node',v)
layer=create(e,'Animation|Blends|Layeredblendperbone',1950,-200);v=layer.get_editor_property('node')
f=unreal.BranchFilter();f.set_editor_property('bone_name','spine_01');f.set_editor_property('blend_depth',0);entry=unreal.InputBlendPose();entry.set_editor_property('branch_filters',[f]);v.set_editor_property('layer_setup',[entry]);v.set_editor_property('mesh_space_rotation_blend',True);layer.set_editor_property('node',v)
connect(node(e,'AnimGraphNode_UseCachedPose_5'),'Pose',slot,'Source');connect(node(e,'AnimGraphNode_LayeredBoneBlend_2'),'Pose',layer,'BasePose');connect(slot,'Pose',layer,'BlendPoses_0');connect(layer,'Pose',node(e,'AnimGraphNode_LocalToComponentSpace_0'),'LocalPose')
weight=create(e,'Animation|Montage|GetSlotLocalWeight',1700,-700)
weight.find_input_pin('SlotNodeName').set_pin_value('BodyUpperAction')
connect(weight,'ReturnValue',layer,'BlendWeights_0')
# Keep body pitch/lean, then align hand_r (the gun remains attached to the hand).
right=create(e,'Animation|SkeletalControls|FABRIK',2500,50);v=right.get_editor_property('node');v.set_editor_property('effector_transform_space',unreal.BoneControlSpace.BCS_COMPONENT_SPACE)
for prop,bone in [('tip_bone','hand_r'),('root_bone','upperarm_r')]:
 ref=unreal.BoneReference();ref.set_editor_property('bone_name',bone);v.set_editor_property(prop,ref)
v.set_editor_property('effector_rotation_source',unreal.BoneRotationSource.BRS_COPY_FROM_TARGET);v.set_editor_property('precision',.01);v.set_editor_property('max_iterations',32);right.set_editor_property('node',v)
connect(node(e,'AnimGraphNode_ModifyBone_0'),'Pose',right,'ComponentPose');source(e,'BodyAdsRightHand','EffectorTransform',right);source(e,'BodyAdsAlpha','Alpha',right)
left=create(e,'Animation|SkeletalControls|FABRIK',2800,50);v=left.get_editor_property('node');v.set_editor_property('effector_transform_space',unreal.BoneControlSpace.BCS_BONE_SPACE)
for prop,bone in [('tip_bone','hand_l'),('root_bone','upperarm_l')]:
 ref=unreal.BoneReference();ref.set_editor_property('bone_name',bone);v.set_editor_property(prop,ref)
target=unreal.BoneSocketTarget();ref=unreal.BoneReference();ref.set_editor_property('bone_name','hand_r');target.set_editor_property('bone_reference',ref);v.set_editor_property('effector_target',target)
v.set_editor_property('effector_rotation_source',unreal.BoneRotationSource.BRS_COPY_FROM_TARGET);v.set_editor_property('precision',.01);v.set_editor_property('max_iterations',32);left.set_editor_property('node',v)
connect(right,'Pose',left,'ComponentPose');source(e,'FABRIK_SocketTransform','EffectorTransform',left);source(e,'BodySupportHandAlpha','Alpha',left);connect(left,'Pose',node(e,'AnimGraphNode_ComponentToLocalSpace_0'),'ComponentPose')
B.compile_blueprint(a)
errors=[]
for g in B.list_graphs(a):errors += [g.get_name()+':'+n.get_node_title()+':'+str(n.error_msg) for n in unreal.BlueprintGraphEditor.get_graph_editor(g).list_nodes_with_errors()]
assert not errors,errors
assert E.save_loaded_asset(a,False)
(out/'graph_result.json').write_text(json.dumps({'errors':errors,'action_slot':'BodyUpperAction','mask':'spine_01','ads_target':'hand_r','support_target':'hand_l'},indent=2),encoding='utf8');print('RAR_BODY_GRAPH_DONE')
