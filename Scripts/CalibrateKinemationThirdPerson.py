"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.

Bake each weapon's authored idle grip into its independent third-person mount.
Only the nine FPS weapon Blueprints are saved; source meshes/animations are read-only.
Backups and numerical verification results are written under Saved/Kin3PAttach.
"""
import datetime
import json
import math
import shutil
from pathlib import Path

import unreal


NAMES = ("AK105", "TR15", "Mk14", "MKR9", "MX68", "R08", "SRM12", "Herrington", "Viper")
ROOT = Path(unreal.Paths.project_dir())
OUTPUT = ROOT / "Saved" / "Kin3PAttach" / datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
OUTPUT.mkdir(parents=True, exist_ok=True)


def vector(value):
    return [value.x, value.y, value.z]


def angle_error(a, b):
    dot = abs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w)
    return math.degrees(2 * math.acos(max(-1, min(1, dot))))


report = []
pending = []
for name in NAMES:
    path = "/Game/FPS/Weapon/Kinemation/BP_Weapon_" + name
    blueprint = unreal.load_asset(path)
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    cdo = unreal.get_default_object(cls)
    sequence = cdo.get_editor_property("arms_idle_sequence")
    if not sequence:
        raise RuntimeError(name + ": missing authored idle sequence")
    options = unreal.AnimPoseEvaluationOptions()
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, 0.0, options)
    bones = {str(bone) for bone in unreal.AnimPoseExtensions.get_bone_names(pose)}
    if not {"hand_r", "ik_hand_gun"}.issubset(bones):
        raise RuntimeError(name + ": source pose lacks the grip bones")
    hand = unreal.AnimPoseExtensions.get_bone_pose(pose, "hand_r", unreal.AnimPoseSpaces.WORLD)
    gun = unreal.AnimPoseExtensions.get_bone_pose(pose, "ik_hand_gun", unreal.AnimPoseSpaces.WORLD)
    # Both transforms are evaluated at the same time in the same component space.
    # Reference-pose hand_r == ik_hand_gun does NOT describe the animated grip.
    mount = unreal.MathLibrary.make_relative_transform(gun, hand)
    rebuilt = unreal.MathLibrary.compose_transforms(mount, hand)
    distance = (rebuilt.translation - gun.translation).length()
    angle = angle_error(rebuilt.rotation, gun.rotation)
    if distance > 0.001 or angle > 0.01:
        raise RuntimeError(name + ": grip reconstruction failed")
    # Avoid importing tiny animation compression scale errors into the weapon mesh.
    mount.scale3d = cdo.get_editor_property("mesh3p").get_editor_property("relative_scale3d")
    rotation = mount.rotation.rotator()
    report.append({
        "weapon": name, "sequence": sequence.get_path_name(), "socket": "hand_r",
        "location_cm": vector(mount.translation),
        "rotation_pitch_yaw_roll": [rotation.pitch, rotation.yaw, rotation.roll],
        "reconstruction_error_cm": distance, "reconstruction_error_degrees": angle,
    })
    pending.append((name, path, blueprint, cdo, mount))

# Validate every source before changing any Blueprint, then back up every target.
for name, path, blueprint, cdo, mount in pending:
    shutil.copy2(ROOT / "Content" / "FPS" / "Weapon" / "Kinemation" / ("BP_Weapon_" + name + ".uasset"), OUTPUT)
for name, path, blueprint, cdo, mount in pending:
    cdo.set_editor_property("mesh3p_attach_socket", "hand_r")
    cdo.set_editor_property("mesh3p_attach_transform", mount)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        raise RuntimeError(name + ": save failed")
    saved = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
    if str(saved.get_editor_property("mesh3p_attach_socket")) != "hand_r":
        raise RuntimeError(name + ": socket did not survive compilation")
    result = saved.get_editor_property("mesh3p_attach_transform")
    if (result.translation - mount.translation).length() > 0.001 or angle_error(result.rotation, mount.rotation) > 0.01:
        raise RuntimeError(name + ": mount did not survive compilation")

(OUTPUT / "calibration.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("[Kin3PAttach] Calibrated and verified all nine weapons. Backup/report: " + str(OUTPUT))
