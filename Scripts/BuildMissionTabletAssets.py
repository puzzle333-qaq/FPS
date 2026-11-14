"""Run in UE Python. Creates the tablet-only HDR pass-through, preserving SlateUI parameters.

capture_factory_recon() captures an actual frame from the existing drone route in the open
Factory level. It creates only UI assets and removes its temporary scene capture actor.
"""
import unreal
import time
import json
import traceback
from pathlib import Path


def ensure_display_material():
    path = '/Game/FPS/UI/Tablet/M_TabletDisplay'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    material = unreal.EditorAssetLibrary.duplicate_asset(
        '/Engine/EngineMaterials/Widget3DPassThrough', path)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    lib = unreal.MaterialEditingLibrary
    source = lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    output = lib.get_material_property_input_node_output_name(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    gain = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, 640, -100)
    gain.set_editor_property('const_b', 1.6)
    lib.connect_material_expressions(source, output, gain, 'A')
    lib.connect_material_property(gain, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # Keep the white screen and dark type legible as the player's exposure changes.
    inverse = lib.create_material_expression(material, unreal.MaterialExpressionEyeAdaptationInverse, 850, -100)
    inputs = lib.get_material_expression_input_names(inverse)
    lib.connect_material_expressions(gain, '', inverse, inputs[0])
    one = lib.create_material_expression(material, unreal.MaterialExpressionConstant, 640, 100)
    one.set_editor_property('r', 1.0)
    lib.connect_material_expressions(one, '', inverse, inputs[1])
    lib.connect_material_property(inverse, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def capture_factory_recon():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    assert 'Factory' in world.get_path_name(), 'Open the Factory level before capturing its reconnaissance frame.'
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    capture = actors.spawn_actor_from_class(
        unreal.SceneCapture2D, unreal.Vector(-1000, -4800, 1800), unreal.Rotator(pitch=-27.4183, yaw=28.6105, roll=0))
    try:
        component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
        target = unreal.RenderingLibrary.create_render_target2d(
            world, 2000, 780, unreal.TextureRenderTargetFormat.RTF_RGBA8)
        component.set_editor_property('texture_target', target)
        component.set_editor_property('fov_angle', 70)
        component.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        component.set_editor_property('capture_every_frame', False)
        component.set_editor_property('capture_on_movement', False)
        component.set_editor_property('always_persist_rendering_state', True)
        # Same profile and phosphor material as the existing deployment drone.
        settings = component.get_editor_property('post_process_settings')
        for name, value in {
            'auto_exposure_method': unreal.AutoExposureMethod.AEM_HISTOGRAM,
            'auto_exposure_min_brightness': -2.0, 'auto_exposure_max_brightness': 3.0,
            'auto_exposure_bias': .4, 'bloom_intensity': 1.8, 'bloom_threshold': 1.05,
            'vignette_intensity': 0.0, 'film_grain_intensity': .22,
            'dynamic_global_illumination_method': unreal.DynamicGlobalIlluminationMethod.LUMEN,
            'reflection_method': unreal.ReflectionMethod.LUMEN,
        }.items():
            settings.set_editor_property('override_' + name, True)
            settings.set_editor_property(name, value)
        component.set_editor_property('post_process_settings', settings)
        component.set_editor_property('post_process_blend_weight', 1.0)
        component.add_or_update_blendable(unreal.load_asset('/Game/FPS/Materials/M_NVG_Phosphor'), 1.0)
        state = {'start': time.monotonic(), 'capture': capture, 'target': target}
        def finish_capture(_):
            component.capture_scene()
            if time.monotonic() - state['start'] < 4:
                return
            unreal.unregister_slate_post_tick_callback(state['handle'])
            try:
                texture = save_recon_capture(world, target)
                result = {'texture': texture.get_path_name(), 'drone_waypoint': [-1000, -4800, 1800], 'fov': 70}
            except Exception:
                result = {'error': traceback.format_exc()}
            finally:
                actors.destroy_actor(capture)
            (Path(unreal.Paths.project_saved_dir()) / 'TabletImplementation/ReconResult.json').write_text(json.dumps(result, indent=2))
        state['handle'] = unreal.register_slate_post_tick_callback(finish_capture)
        return capture
    except Exception:
        actors.destroy_actor(capture)
        raise


def save_recon_capture(world, target):
    folder = Path(unreal.Paths.project_content_dir()) / 'FPS/UI/Tablet'
    folder.mkdir(parents=True, exist_ok=True)
    unreal.RenderingLibrary.export_render_target(world, target, str(folder), 'T_Factory_Recon.png')
    task = unreal.AssetImportTask()
    task.filename = str(folder / 'T_Factory_Recon.png')
    task.destination_path = '/Game/FPS/UI/Tablet'
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset('/Game/FPS/UI/Tablet/T_Factory_Recon')
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture

ensure_display_material()
