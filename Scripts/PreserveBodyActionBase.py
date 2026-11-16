import unreal,json,hashlib
from pathlib import Path
E=unreal.EditorAssetLibrary;B=unreal.BlueprintEditorLibrary;a=unreal.load_asset('/Game/FPS/Character/ABP_ThirdPerson');g=B.find_graph(a,'AnimGraph');ed=unreal.BlueprintGraphEditor.get_graph_editor(g)
def connect(n,o,m,i):
 p=m.find_input_pin(i);p.break_pin_links();assert n.find_output_pin(o).try_create_connection(p)
nodes=list(ed.list_all_nodes());slot=next(n for n in nodes if isinstance(n,unreal.AnimGraphNode_Slot) and str(n.get_editor_property('node').slot_name)=='BodyUpperAction')
layer=next(n for n in nodes if isinstance(n,unreal.AnimGraphNode_LayeredBoneBlend) and any(p.get_owning_node()==slot for p in n.find_input_pin('BlendPoses_0').list_connected_pins()))
legacy=next(n for n in nodes if n.get_name()=='AnimGraphNode_LayeredBoneBlend_2')
# Gate the added overlay by its own montage weight. At zero it leaves the entire
# legacy graph untouched (including other weapons' BodySlot actions).
weight=ed.create_node_from_name('Animation|Montage|GetSlotLocalWeight',unreal.Vector2D(1700,-700),[],None)
assert weight
weight.find_input_pin('SlotNodeName').set_pin_value('BodyUpperAction')
connect(weight,'ReturnValue',layer,'BlendWeights_0')
key='BodyUpperAction slot weight gates added overlay'
B.compile_blueprint(a)
errors=[str(n.error_msg) for g in B.list_graphs(a) for n in unreal.BlueprintGraphEditor.get_graph_editor(g).list_nodes_with_errors()];assert not errors,errors
assert E.save_loaded_asset(a,False)
# Validate data isolation and actual montage -> retargeted sequence references.
baseline=json.loads(Path(unreal.Paths.project_saved_dir(),'BodyPitchAudit/audit.json').read_text(encoding='utf8'));data=unreal.load_asset('/Game/FPS/Data/DA_WeaponData');checked=[]
for table in ['FirstPersonAnims','FirstPersonMontages','ThirdPersonAnims','ThirdPersonMontages']:
 for k,row in data.get_editor_property(table).items():
  tag=k.export_text()
  if table.startswith('First') or 'Weapon.type.rar' not in tag:
   assert row.export_text()==baseline[table][tag],(table,tag);checked.append(table+':'+tag)
report={'legacy_pose_cache':key,'errors':errors,'unchanged_data_rows':checked}
Path(unreal.Paths.project_saved_dir(),'RARBodyActions/isolation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print('RAR_BODY_ISOLATION_DONE')

