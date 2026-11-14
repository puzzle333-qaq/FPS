import unreal,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir());src=root/'Content/Bandits/Mesh/SKM_Bandit_Skeleton.uasset';backup=root/'Saved/ScreenRefresh/Before/SKM_Bandit_Skeleton.uasset';backup.parent.mkdir(parents=True,exist_ok=True)
if not backup.exists():shutil.copy2(src,backup)
pose=unreal.load_asset('/Game/Police/MMC/Demo/Animations/A_Pose_1');existing=unreal.load_asset('/Game/Bandits/Mesh/SKM_Bandit_Skeleton');existing.add_compatible_skeleton(pose.get_editor_property('skeleton'));assert unreal.EditorAssetLibrary.save_loaded_asset(existing), 'Save skeleton outside PIE'
if not unreal.EditorAssetLibrary.does_asset_exist('/Game/FPS/Characters/Police/A_Police_LoadoutPose'):assert unreal.EditorAssetLibrary.duplicate_asset('/Game/Police/MMC/Demo/Animations/A_Pose_1','/Game/FPS/Characters/Police/A_Police_LoadoutPose'), 'Duplicate pose outside PIE'
assert unreal.EditorAssetLibrary.save_asset('/Game/FPS/Characters/Police/A_Police_LoadoutPose'), 'Save pose outside PIE'
print('Pose copied; existing skeleton accepts Police animation skeleton')
