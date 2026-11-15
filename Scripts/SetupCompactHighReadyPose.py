"""Persist the user-reviewed high-ready pose; only the final horizontal nudge changes placement."""
import unreal,shutil
from pathlib import Path
asset='/Game/FPS/Character/BP_ShooterCharacter'
root=Path(unreal.Paths.project_dir());backup=root/'Saved/DangerPack/BeforeAcceptedHighReady';backup.mkdir(parents=True,exist_ok=True)
source=root/'Content/FPS/Character/BP_ShooterCharacter.uasset'
if not (backup/source.name).exists():shutil.copy2(source,backup/source.name)
default=unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(asset));default.modify()
settings={'WeaponBlockRaisePitch':85.,'WeaponBlockRetract':-36.,'WeaponBlockSideOffset':32.,'WeaponBlockLowerOffset':34.,'WeaponBlockRoll':0.}
for name,value in settings.items():default.set_editor_property(name,value)
assert unreal.EditorAssetLibrary.save_asset(asset,False)
print('Saved accepted high-ready defaults:',settings)
