import unreal,json
from pathlib import Path
E=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=E.get_all_level_actors()
nav=next((a for a in actors if isinstance(a,unreal.FactoryNavigationBounds)),None)
if not nav:
 nav=E.spawn_actor_from_class(unreal.FactoryNavigationBounds,unreal.Vector(2700,-1600,200),unreal.Rotator());nav.set_actor_label('Factory_NavigationBounds');nav.set_folder_path('Mission/Navigation')
nav.rebuild_navigation();print('nav bounds',nav.get_actor_bounds(False))
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
