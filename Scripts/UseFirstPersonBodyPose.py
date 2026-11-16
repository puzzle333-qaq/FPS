# Reuse the actual evaluated Mesh1P upper body; keep the legacy body graph as fallback.
import unreal,json,shutil
from pathlib import Path
E=unreal.EditorAssetLibrary;B=unreal.BlueprintEditorLibrary
bp=unreal.load_asset('/Game/FPS/Character/ABP_ThirdPerson');g=B.find_graph(bp,'AnimGraph');e=unreal.BlueprintGraphEditor.get_graph_editor(g)
Path(unreal.Paths.project_saved_dir(),'RARVisual/Before').mkdir(exist_ok=True)
backup=Path(unreal.Paths.project_saved_dir(),'RARVisual/Before/ABP_ThirdPerson.uasset')
if not backup.exists():shutil.copy2(Path(unreal.Paths.project_content_dir(),'FPS/Character/ABP_ThirdPerson.uasset'),backup)
def create(key,x,y):
 n=e.create_node_from_name(key,unreal.Vector2D(x,y),[],None);assert n,key;return n
def conn(n,o,m,i):
 p=m.find_input_pin(i);assert p.is_valid(),(m.get_name(),i);p.break_pin_links();assert n.find_output_pin(o).try_create_connection(p)
convert=next(n for n in e.list_all_nodes() if n.get_name()=='AnimGraphNode_LocalToComponentSpace_0')
# Remove an earlier prototype overlay without stacking duplicate pose layers.
remove=[]
for n in e.list_all_nodes():
 if not isinstance(n,unreal.AnimGraphNode_CopyPoseFromMesh):continue
 remove.append(n)
 links=n.find_output_pin('Pose').list_connected_pins()
 if links:
  ao_old=links[0].get_owning_node();remove.append(ao_old);links=ao_old.find_output_pin('Pose').list_connected_pins()
  if links:
   layer_old=links[0].get_owning_node()
   if isinstance(layer_old,unreal.AnimGraphNode_LayeredBoneBlend):
    base=layer_old.find_input_pin('BasePose').list_connected_pins()[0].get_owning_node()
    conn(base,'Pose',convert,'LocalPose');remove.append(layer_old)
e.remove_nodes(remove)
old=convert.find_input_pin('LocalPose').list_connected_pins()[0].get_owning_node()
copy=create('Animation|Misc.|CopyPoseFromMesh',1900,900);owner=e.add_get_member_variable_node('Shooter Character','');source=e.add_get_member_variable_node('Mesh1P','/Script/FPS.ShooterCharacter');conn(owner,'Shooter Character',source,'self');conn(source,'Mesh1P',copy,'SourceMeshComponent')
ao=create("Animation|BlendSpaces|AimOffsetPlayer'AO_TPP_Ak'",2250,850)
for opt in list(ao.get_editor_property('show_pin_for_properties')):
 if str(opt.get_editor_property('property_name')) in ['X','Y','BlendSpace']:opt.set_editor_property('show_pin',True)
# Reassign optional pin descriptors and ask the schema to rebuild.
opts=list(ao.get_editor_property('show_pin_for_properties'))
for opt in opts:
 if str(opt.get_editor_property('property_name')) in ['X','Y','BlendSpace']:opt.set_editor_property('show_pin',True)
ao.set_editor_property('show_pin_for_properties',opts);ao.set_editor_property('update_function',ao.get_editor_property('update_function'),notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
conn(copy,'Pose',ao,'BasePose')
rot=e.add_get_member_variable_node('AimRotation','')
# Split struct getter already exposes Pitch/Yaw as on the original AO graph.
print('ROT_PINS',[(str(p.get_pin_name()),str(p.get_pin_type())) for p in rot.list_output_pins()])
# Reuse Blueprint's Break Rotator node for the un-split getter.
breakrot=create('Math|Rotator|BreakRotator',2050,1100);conn(rot,'AimRotation',breakrot,'InRot');conn(breakrot,'Pitch',ao,'Y');ao.find_input_pin('X').set_pin_value('50')
current=e.add_get_member_variable_node('CurrentAnim','');br=create('Utilities|Struct|BreakPlayerAnim',1700,1200);conn(current,'CurrentAnim',br,'PlayerAnim');aiming=e.add_get_member_variable_node('IsAiming','');select=create('Utilities|Select',2000,1500);conn(aiming,'IsAiming',select,'Index');conn(br,'AimOffset_Hip',select,'Option 0');conn(br,'AimOffset_Aim',select,'Option 1');conn(select,'ReturnValue',ao,'BlendSpace')
layer=create('Animation|Blends|Layeredblendperbone',2600,900);v=layer.get_editor_property('node');f=unreal.BranchFilter();f.set_editor_property('bone_name','spine_01');f.set_editor_property('blend_depth',0);entry=unreal.InputBlendPose();f2=unreal.BranchFilter();f.set_editor_property('bone_name','upperarm_l');f2.set_editor_property('bone_name','upperarm_r');entry.set_editor_property('branch_filters',[f,f2]);v.set_editor_property('layer_setup',[entry]);v.set_editor_property('mesh_space_rotation_blend',False);layer.set_editor_property('node',v)
alpha=e.add_get_member_variable_node('BodyFirstPersonPoseAlpha','/Script/FPS.ShooterCharacter');conn(owner,'Shooter Character',alpha,'self');conn(alpha,'BodyFirstPersonPoseAlpha',layer,'BlendWeights_0')
conn(old,'Pose',layer,'BasePose');conn(ao,'Pose',layer,'BlendPoses_0');conn(layer,'Pose',convert,'LocalPose')
B.compile_blueprint(bp)
errors=[str(n.error_msg) for gr in B.list_graphs(bp) for n in unreal.BlueprintGraphEditor.get_graph_editor(gr).list_nodes_with_errors()];assert not errors,errors
assert E.save_loaded_asset(bp,False)
Path(unreal.Paths.project_saved_dir(),'RARVisual/copy_graph.json').write_text(json.dumps({'copy':copy.get_name(),'ao':ao.get_name(),'layer':layer.get_name(),'errors':errors}),encoding='utf8');print('BODY_COPY_GRAPH_READY')


