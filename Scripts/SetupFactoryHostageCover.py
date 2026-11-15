"""Run outside PIE after building FPS. Add two civilians and validated desk hiding markers."""
import unreal,json,shutil
from pathlib import Path
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()));out=root/'Saved/DangerPack';out.mkdir(exist_ok=True)
les=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);eas=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world and 'Lvl_Factory_Whitebox' in world.get_name()
backup=out/'Lvl_Factory_Whitebox.before-hostages.umap'
if not backup.exists():shutil.copy2(root/'Content/Maps/Lvl_Factory_Whitebox.umap',backup)
by={a.get_actor_label():a for a in eas.get_all_level_actors()};rows=[]
for tag,label,pawnpos,yaw,body in [('FD_010','Office',(280,-870,108),90,'05'),('FD_068','Workshop',(2280,-1530,108),0,'13')]:
 table=by[tag];source=table.static_mesh_component.static_mesh;path='/Game/FPS/Props/Hostage/SM_Cover'+label
 mesh=unreal.load_asset(path)if unreal.EditorAssetLibrary.does_asset_exist(path)else unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(),path)
 assert mesh
 mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
 table.static_mesh_component.set_static_mesh(mesh);table.static_mesh_component.set_collision_profile_name('BlockAll')
 center,extent=table.get_actor_bounds(False)
 name='HostageCover_'+label
 point=by.get(name)or eas.spawn_actor_from_class(unreal.HostageHidePoint,unreal.Vector(center.x,center.y,20),unreal.Rotator(pitch=0,yaw=yaw,roll=0))
 point.set_actor_label(name);point.set_folder_path('Mission/HostageCover');point.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False);point.set_editor_property('bUnderTable',True)
 name='Civilian_'+label
 hostage=by.get(name)or eas.spawn_actor_from_class(unreal.HostageActor,unreal.Vector(*pawnpos),unreal.Rotator())
 hostage.set_actor_label(name);hostage.set_folder_path('Mission/Civilians');hostage.set_editor_property('PreferredHidePoint',point)
 model=unreal.load_asset('/Game/Bandits/Mesh/Preset/SKM_Bandit_Preset_'+body);assert model
 hostage.civilian_mesh.set_skeletal_mesh_asset(model)
 rows.append({'civilian':hostage.get_path_name(),'position':str(hostage.get_actor_location()),'cover':point.get_path_name(),'table':table.get_actor_label(),'table_bounds':str((center,extent)),'approach':str(point.approach.get_world_location())})
assert les.save_current_level()
Path(out/'FactoryHostageSetup.json').write_text(json.dumps(rows,indent=2));print(rows)
