"""Place passive night illumination independently of electrical mission light groups."""
import unreal,shutil,json
from pathlib import Path
root=Path(unreal.Paths.project_dir());backup=root/'Saved/NVGRealism/Before';backup.mkdir(parents=True,exist_ok=True)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);rows=[]
for path in ['/Game/Maps/Lvl_Factory_Whitebox','/Game/FPS/Level/Lvl_Range']:
 local=root/'Content'/(path.removeprefix('/Game/')+'.umap')
 if not (backup/local.name).exists():shutil.copy2(local,backup/local.name)
 assert level.load_level(path)
 actors=sub.get_all_level_actors();rig=next((a for a in actors if isinstance(a,unreal.NightVisionLightFloor)),None)
 if not rig:rig=sub.spawn_actor_from_class(unreal.NightVisionLightFloor,unreal.Vector(0,0,500))
 rig.set_actor_label('NVG_Persistent_MoonAndSky');rig.tags=['NVGPersistentLight']
 external=[l for a in actors if a!=rig for l in a.get_components_by_class(unreal.DirectionalLightComponent)if l.is_visible()and l.get_editor_property('intensity')>.001]
 # Keep the range's authored daylight. The interior hemispheres are positioned
 # for the factory, so other maps retain only their general sky floor.
 for light in rig.interior_signal:light.set_visibility('Factory'in path)
 if external:
  rig.moonlight.set_visibility(False)
  if 'Factory'in path:
   external[0].set_intensity(.25);external[0].set_light_color(unreal.LinearColor(.62,.74,1.,1.))
 else:rig.moonlight.set_visibility(True)
 count=0
 for a in sub.get_all_level_actors():
  for mesh in a.get_components_by_class(unreal.MeshComponent):
   channels=mesh.get_editor_property('lighting_channels');channels.set_editor_property('bChannel1',True);mesh.set_editor_property('lighting_channels',channels);count+=1
 assert level.save_current_level()
 rows.append({'map':path,'world_meshes':count,'external_moonlights':len(external),'floor':.04,'electrical_group':False})
assert level.load_level('/Game/Maps/Lvl_Factory_Whitebox')
# Update inherited component defaults as well as native constructor values.
bp='/Game/FPS/Character/BP_ShooterCharacter';local=root/'Content/FPS/Character/BP_ShooterCharacter.uasset'
if not(backup/local.name).exists():shutil.copy2(local,backup/local.name)
default=unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(bp));light=default.get_component_by_class(unreal.SpotLightComponent);assert light.get_name()=='NVGIlluminator'
light.modify();light.set_relative_location(unreal.Vector(70,0,-5),False,False);light.set_intensity(8);light.set_attenuation_radius(3000);light.set_cast_shadows(False);light.set_lighting_channels(False,True,False);light.set_visibility(False)
assert unreal.EditorAssetLibrary.save_asset(bp,False)
(root/'Saved/NVGRealism/LightingSetup.json').write_text(json.dumps(rows,indent=2));print(rows)
