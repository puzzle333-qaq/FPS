"""RAR/body pitch repair only. Run using UE 5.8 Python commandlet; no native/1P edits."""
import unreal,json,shutil,hashlib
from pathlib import Path
from editor_toolset.toolsets.blueprint import BlueprintTools
E=unreal.EditorAssetLibrary;B=unreal.BlueprintEditorLibrary
root=Path(unreal.Paths.project_dir());out=root/'Saved/BodyPitchAudit';out.mkdir(exist_ok=True)
path='/Game/FPS/Character/ABP_ThirdPerson';file=root/'Content/FPS/Character/ABP_ThirdPerson.uasset'
bk=out/'Before/ABP_ThirdPerson.uasset';bk.parent.mkdir(exist_ok=True)
if not bk.exists():shutil.copy2(file,bk)
protected=[root/'Source/FPS/Private/ShooterCharacter.cpp',root/'Source/FPS/Public/ShooterCharacter.h',root/'Source/FPS/Private/CombatComponent.cpp',root/'Content/FPS/Character/ABP_FirstPerson.uasset',root/'Content/FPS/Character/BP_ShooterCharacter.uasset',root/'Content/FPS/Data/DA_WeaponData.uasset']
hashes={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in protected if p.exists()}
(out/'protected_before.json').write_text(json.dumps(hashes,indent=2),encoding='utf8')
a=unreal.load_asset(path)
def graph(name):
 g=B.find_graph(a,name);assert g,name;return unreal.BlueprintGraphEditor.get_graph_editor(g)
def named(ed,name):return next(n for n in ed.list_all_nodes() if n.get_name()==name)
def pin(n,name,output=False):
 p=n.find_output_pin(name) if output else n.find_input_pin(name);assert p.is_valid(),(n.get_name(),name);return p
def connect(n,o,m,i):
 dest=pin(m,i);dest.break_pin_links();assert pin(n,o,True).try_create_connection(dest),(n.get_node_title(),o,m.get_node_title(),i)
def create(ed,key,x,y):
 n=ed.create_node_from_name(key,unreal.Vector2D(x,y),[],None);assert n,key;return n
# Reuse AimRotation as the existing two-axis container. AO_Yaw retains its distinct
# turn-in-place/OrientationWarping meaning, so locomotion is not changed by look math.
event=graph('EventGraph');last=named(event,'K2Node_VariableSet_7');assert not pin(last,'then',True).list_connected_pins()
owner=event.add_get_member_variable_node('Shooter Character','');owner.set_node_pos(unreal.IntPoint(900,900))
aim=create(event,'Pawn|GetBaseAimRotation',1150,800);actor=create(event,'Transformation|GetActorRotation',1150,1000)
connect(owner,'Shooter Character',aim,'self');connect(owner,'Shooter Character',actor,'self')
delta=create(event,'Math|Rotator|Delta(Rotator)',1400,900)
connect(aim,'ReturnValue',delta,'A');connect(actor,'ReturnValue',delta,'B')
setter=event.add_set_member_variable_node('AimRotation','');setter.set_node_pos(unreal.IntPoint(1650,800))
connect(delta,'ReturnValue',setter,'AimRotation');connect(last,'then',setter,'execute')
# Remove the old thread-safe writer (world aim) while preserving the following IsInAir update.
thread=graph('BlueprintThreadSafeUpdateAnimation');old=named(thread,'K2Node_VariableSet_1');seq=named(thread,'K2Node_ExecutionSequence_0');nextnode=named(thread,'K2Node_VariableSet_13')
connect(seq,'then_1',nextnode,'execute');thread.remove_nodes([old])
# Keep the existing AO and source sequence. Select existing Hip/Aim fields by IsAiming.
ed=graph('Aim Offset');ao=named(ed,'AnimGraphNode_RotationOffsetBlendSpace_1');br=named(ed,'K2Node_BreakStruct_0');rot=named(ed,'K2Node_VariableGet_2')
opts=list(ao.get_editor_property('show_pin_for_properties'))
for opt in opts:
 if str(opt.get_editor_property('property_name'))=='X':opt.set_editor_property('show_pin',True)
ao.set_editor_property('show_pin_for_properties',opts)
# Updating the node callback member invokes the engine's normal schema reconstruction,
# rebuilding optional coordinate pins without changing the callback itself.
ao.set_editor_property('update_function',ao.get_editor_property('update_function'),notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
assert ao.find_input_pin('X').is_valid(),'X pin failed to expose'
sel=create(ed,'Utilities|Select',-50,-200);aiming=ed.add_get_member_variable_node('IsAiming','');aiming.set_node_pos(unreal.IntPoint(-350,-350))
connect(aiming,'IsAiming',sel,'Index')
# Bool specialization renames Option 0/1 into False/True.
inputs=[str(p.get_pin_name()) for p in sel.list_input_pins()];print('SELECT_INPUTS',inputs)
false_name=next(k for k in inputs if k in ['False','Option 0','Option_0']);true_name=next(k for k in inputs if k in ['True','Option 1','Option_1'])
connect(br,'AimOffset_Hip',sel,false_name);connect(br,'AimOffset_Aim',sel,true_name);connect(sel,'ReturnValue',ao,'BlendSpace')
# The authored RAR TPP asset has pitch samples -90..90 and X range 0..100
# (all samples lie at X=50). Clamp axes without changing or inventing source poses.
for axis,source,lo,hi,y in [('Y','AimRotation_Pitch',-90,90,180),('X','AimRotation_Yaw',0,100,360)]:
 clamp=create(ed,'Math|Float|Clamp(Float)',200,y)
 pin(clamp,'Min').set_pin_value(str(lo));pin(clamp,'Max').set_pin_value(str(hi))
 connect(rot,source,clamp,'Value');connect(clamp,'ReturnValue',ao,axis)
# The existing spine contribution was discarded by the clavicle-only mask.
main=graph('AnimGraph');layer=named(main,'AnimGraphNode_LayeredBoneBlend_1');value=layer.get_editor_property('node')
before=value.export_text();layer_setup=list(value.get_editor_property('layer_setup'));assert len(layer_setup)==1
f=unreal.BranchFilter();f.set_editor_property('bone_name','spine_01');f.set_editor_property('blend_depth',0)
entry=layer_setup[0];entry.set_editor_property('branch_filters',[f]);value.set_editor_property('layer_setup',[entry]);layer.set_editor_property('node',value)
B.compile_blueprint(a)
errors=[];warnings=[]
for g in B.list_graphs(a):
 ge=unreal.BlueprintGraphEditor.get_graph_editor(g)
 errors += [g.get_name()+':'+n.get_node_title()+':'+str(n.error_msg) for n in ge.list_nodes_with_errors()]
 warnings += [g.get_name()+':'+n.get_node_title()+':'+str(n.error_msg) for n in ge.list_nodes_with_warnings()]
assert not errors,errors
assert E.save_loaded_asset(a,False),'Save failed'
assert all(hashlib.sha256((root/k).read_bytes()).hexdigest()==v for k,v in hashes.items()),'Protected file changed'
report={'asset':path,'before_mask':before,'after_mask':layer.get_editor_property('node').export_text(),'errors':errors,'warnings':warnings,'protected_unchanged':list(hashes),'saved':True}
(out/'fix_result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print('BODY_PITCH_REPAIR_COMPLETE')
