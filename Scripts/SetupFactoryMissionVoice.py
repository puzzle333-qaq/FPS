"""Import factory VO and add the task actors to the existing level; run in UE editor Python with PIE stopped."""
import unreal,json,shutil,hashlib,wave,math
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/FactoryVoice';out.mkdir(parents=True,exist_ok=True)
assets=unreal.AssetToolsHelpers.get_asset_tools()
source=root/'ArtSource/FactoryVoice';dest='/Game/FPS/Audio/Voice/Factory'
tasks=[]
for p in source.glob('*.wav'):
 t=unreal.AssetImportTask();t.filename=str(p);t.destination_path=dest;t.automated=True;t.replace_existing=True;t.save=True;t.factory=unreal.SoundFactory();tasks.append(t)
assets.import_asset_tasks(tasks)
classes={}
for name,volume in [('SC_MissionRadio',.95),('SC_OperatorVoice',.9)]:
 path=dest+'/'+name
 a=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(name,dest,unreal.SoundClass,unreal.SoundClassFactory())
 props=a.get_editor_property('properties');props.set_editor_property('volume',volume);a.set_editor_property('properties',props)
 unreal.EditorAssetLibrary.save_loaded_asset(a);classes[name]=a
manifest=[]
for task in tasks:
 for path in task.imported_object_paths:
  a=unreal.load_asset(path);a.set_editor_property('sound_class_object',classes['SC_OperatorVoice'] if '_Shout_' in a.get_name() else classes['SC_MissionRadio'])
  unreal.EditorAssetLibrary.save_loaded_asset(a)
  manifest.append({'asset':path,'duration':a.duration,'source':str(source/(a.get_name()+'.wav'))})
# Preserve the pre-task level, including external actors if this level uses them.
map_file=root/'Content/Maps/Lvl_Factory_Whitebox.umap';backup=out/'Before';backup.mkdir(exist_ok=True)
if not (backup/map_file.name).exists():shutil.copy2(map_file,backup/map_file.name)
external=root/'Content/__ExternalActors__/Maps/Lvl_Factory_Whitebox'
if external.exists() and not (backup/'ExternalActors').exists():shutil.copytree(external,backup/'ExternalActors')
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);level.load_level('/Game/Maps/Lvl_Factory_Whitebox')
eas=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
all_actors=eas.get_all_level_actors();by={a.get_actor_label():a for a in all_actors}
created=[]
def mesh(name):return unreal.load_asset(name)
def objective(label,kind,path,location,rotation=None):
 a=by.get(label)
 if not a:a=eas.spawn_actor_from_class(unreal.FactoryMissionObjective,unreal.Vector(*location))
 a.set_actor_label(label);a.set_editor_property('Task',kind)
 a.set_actor_scale3d(unreal.Vector(1,1,1))
 if rotation:a.set_actor_rotation(rotation,False)
 m=a.get_component_by_class(unreal.StaticMeshComponent);m.set_static_mesh(mesh(path) if path else None);m.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
 a.tags=sorted(set(str(t) for t in list(a.tags)+['FactoryMissionTask']))
 a.set_folder_path('Gameplay/FactoryMission')
 created.append(a);return a,m

def on_surface(label,kind,path,target,top):
 a,m=objective(label,kind,path,(*target,top+5))
 rotations=[unreal.Rotator(),unreal.Rotator(roll=90),unreal.Rotator(pitch=90)]
 best=None
 for rot in rotations:
  a.set_actor_rotation(rot,False);origin,extent,_=unreal.SystemLibrary.get_component_bounds(m)
  if best is None or extent.z<best[0]:best=(extent.z,rot)
 a.set_actor_rotation(best[1],False)
 origin,extent,_=unreal.SystemLibrary.get_component_bounds(m);p=a.get_actor_location()
 a.set_actor_location(unreal.Vector(p.x+target[0]-origin.x,p.y+target[1]-origin.y,p.z+top+1-(origin.z-extent.z)),False,True)
 return a
book='/Game/Modern_Abandoned_Office/Static_Meshes/SM_Book_03'
# All three remain authorable; the server keeps only one randomly chosen ledger each round.
for label,desk,offset in [('Task_Ledger_L1','FD_010',(0,-50)),('Task_Ledger_L2','FD_018',(0,0)),('Task_Ledger_L3','FD_045',(-45,0))]:
 a=by[desk];o,e=a.get_actor_bounds(False)
 on_surface(label,unreal.FactoryTask.LEDGER,book,(o.x+offset[0],o.y+offset[1]),o.z+e.z)
# Use the existing loading-area pallet as a surface for the shipping document.
pallets=[a for a in all_actors if isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh and 'palet' in a.static_mesh_component.static_mesh.get_name().lower() and 3800<a.get_actor_location().x<5100 and -1400<a.get_actor_location().y<0]
assert pallets,'Missing loading-area pallet'
pallet=by.get('FD_102_stack3') or min(pallets,key=lambda a:(a.get_actor_location()-unreal.Vector(4000,-1120,0)).length())
o,e=pallet.get_actor_bounds(False)
on_surface('Task_Hidden_Shipping',unreal.FactoryTask.SHIPPING,'/Game/Modern_Abandoned_Office/Static_Meshes/SM_clipboard',(o.x,o.y),o.z+e.z)
# A real ammunition crate on the boiler-room floor, beside the existing props.
on_surface('Task_Hidden_Crates',unreal.FactoryTask.CRATES,'/Game/FPS_Controller/Environment/Meshes/SM_Ammo_Box',(5480,-2790),.1)
objective('Task_Hidden_Breaker',unreal.FactoryTask.BREAKER,'/Game/UndergroundTunnel/Meshes/Props/SM_ElectricalBox05a',(5700,-4540,-270))
a,_=objective('Task_Extraction_X1',unreal.FactoryTask.EXTRACTION,None,(2000,800,80))
a.get_editor_property('InteractionBox').set_box_extent(unreal.Vector(95,95,70),True)
sign=by.get('Task_Extraction_Sign') or eas.spawn_actor_from_class(unreal.TextRenderActor,unreal.Vector(2000,800,170))
sign.set_actor_label('Task_Extraction_Sign');sign.set_folder_path('Gameplay/FactoryMission')
sign.set_actor_rotation(unreal.Rotator(yaw=-90),False)
text=sign.get_component_by_class(unreal.TextRenderComponent)
text.set_text('EXTRACT / X1');text.set_world_size(20)
# Cut normal interior fixtures; preserve basement/emergency and exterior lights.
cut=[]
for a in all_actors:
 if isinstance(a,unreal.PointLight):
  p=a.get_actor_location();name=a.get_actor_label()
  if p.y<0 and p.z>0 and not (name.startswith('FD_099_PointLight') and name!='FD_099_PointLight') and not name.startswith(('FD_106','FD_107','FD_108','FD_109','FD_185','FD_186','FD_187')):
   a.tags=sorted(set(str(t) for t in list(a.tags)+['Factory.SwitchablePower']));cut.append(a)
for a in all_actors:
 if isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh:
  n=a.static_mesh_component.static_mesh.get_name().lower();p=a.get_actor_location()
  if ('light' in n or 'lamp' in n) and any((p-c.get_actor_location()).length()<320 for c in cut):
   a.tags=sorted(set(str(t) for t in list(a.tags)+['Factory.SwitchablePower']))
# Finish compilation before saving generated audio and the modified level.
unreal.SystemLibrary.execute_console_command(None,'Editor.AsyncAssetCompilation 0')
for task in tasks:
 for path in task.imported_object_paths:unreal.EditorAssetLibrary.save_asset(path,False)
assert level.save_current_level()
report={'voice_assets':manifest,'objectives':[{'label':a.get_actor_label(),'kind':str(a.get_editor_property('Task')),'location':str(a.get_actor_location()),'scale':str(a.get_actor_scale3d())} for a in created],'cut_light_count':len(cut),'map_sha256':hashlib.sha256(map_file.read_bytes()).hexdigest()}
(out/'Setup.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf8')
