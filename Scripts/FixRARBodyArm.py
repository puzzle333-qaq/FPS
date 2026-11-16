import unreal,json,shutil
from pathlib import Path
E=unreal.EditorAssetLibrary;B=unreal.BlueprintEditorLibrary
bp=unreal.load_asset('/Game/FPS/Character/ABP_ThirdPerson');g=B.find_graph(bp,'AnimGraph');ed=unreal.BlueprintGraphEditor.get_graph_editor(g);nodes=list(ed.list_all_nodes())
def make(key):
 n=ed.create_node_from_name(key,unreal.Vector2D(3400,600),[],None);assert n,key;return n
def con(n,o,m,i):
 p=m.find_input_pin(i);assert p.is_valid(),(m.get_name(),i);p.break_pin_links();assert n.find_output_pin(o).try_create_connection(p),(o,i)
def native(name):
 owner=ed.add_get_member_variable_node('Shooter Character','');v=ed.add_get_member_variable_node(name,'/Script/FPS.ShooterCharacter');con(owner,'Shooter Character',v,'self');return v
# Keep anatomical upper/lower arm and twist-bone translations/scales from the body.
# Only the wrist/fingers consume the authored 1P local pose.
copy=next(n for n in nodes if isinstance(n,unreal.AnimGraphNode_CopyPoseFromMesh));ao=copy.find_output_pin('Pose').list_connected_pins()[0].get_owning_node();layer=ao.find_output_pin('Pose').list_connected_pins()[0].get_owning_node();v=layer.get_editor_property('node');entry=unreal.InputBlendPose();filters=[]
for bone in ['hand_l','hand_r']:
 f=unreal.BranchFilter();f.set_editor_property('bone_name',bone);filters.append(f)
entry.set_editor_property('branch_filters',filters);v.set_editor_property('layer_setup',[entry]);layer.set_editor_property('node',v)
end=next(n for n in nodes if n.get_name()=='AnimGraphNode_ComponentToLocalSpace_0');old=end.find_input_pin('ComponentPose').list_connected_pins()[0].get_owning_node()
ik=make('Animation|SkeletalControls|TwoBoneIK');v=ik.get_editor_property('node');ref=unreal.BoneReference();ref.set_editor_property('bone_name','hand_l');v.set_editor_property('ik_bone',ref);v.set_editor_property('effector_location_space',unreal.BoneControlSpace.BCS_COMPONENT_SPACE);v.set_editor_property('joint_target_location_space',unreal.BoneControlSpace.BCS_COMPONENT_SPACE);v.set_editor_property('allow_stretching',False);v.set_editor_property('allow_twist',False);v.set_editor_property('maintain_effector_rel_rot',True);ik.set_editor_property('node',v)
target=native('BodyLeftHandTarget');br=make('Math|Transform|BreakTransform');con(target,'BodyLeftHandTarget',br,'InTransform');con(br,'Location',ik,'EffectorLocation');pole=native('BodyLeftElbowTarget');con(pole,'BodyLeftElbowTarget',ik,'JointTargetLocation');alpha=native('BodyFirstPersonPoseAlpha');con(alpha,'BodyFirstPersonPoseAlpha',ik,'Alpha');con(old,'Pose',ik,'ComponentPose')
rot=make('Animation|SkeletalControls|Transform(Modify)Bone');v=rot.get_editor_property('node');v.set_editor_property('bone_to_modify',ref);v.set_editor_property('rotation_mode',unreal.BoneModificationMode.BMM_REPLACE);v.set_editor_property('rotation_space',unreal.BoneControlSpace.BCS_COMPONENT_SPACE);rot.set_editor_property('node',v);con(br,'Rotation',rot,'Rotation');con(alpha,'BodyFirstPersonPoseAlpha',rot,'Alpha');con(ik,'Pose',rot,'ComponentPose');con(rot,'Pose',end,'ComponentPose')
B.compile_blueprint(bp);errors=[str(n.error_msg) for gr in B.list_graphs(bp) for n in unreal.BlueprintGraphEditor.get_graph_editor(gr).list_nodes_with_errors()];assert not errors,errors;assert E.save_loaded_asset(bp,False)
Path(unreal.Paths.project_saved_dir(),'RARVisual/arm_graph.json').write_text(json.dumps({'ik':ik.get_name(),'rotation':rot.get_name(),'errors':errors}),encoding='utf8');print('RAR_ARM_POLE_READY')
