"""Repair the moved Police pack and build project-owned Preset 07 modules on the live skeleton.
Run with PIE stopped after compiling the FPS module (registers the /Game/MMC package redirect).
"""
import unreal,json,re,shutil,math
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/PoliceCustomization';out.mkdir(parents=True,exist_ok=True)
assets=unreal.AssetToolsHelpers.get_asset_tools();dest='/Game/FPS/Characters/Police';prefix='/Game/Police/MMC/Materials/'
slot_paths={
'M_Bag_Hip':'Hips_Modul/Bag_Hip/M_Bag_Hip_Black','M_Balaclava':'Face_Modul/Balaclava/M_Balaclava_Black',
'M_Googles':'Face_Modul/Goggles/M_Gogles_Black','M_Googles_Glass':'Face_Modul/Goggles/M_Gogles_Glass',
'M_Holster_Soft':'Hips_Modul/Holsher_Soft/M_Holster_Soft_Black','M_Pants_Knees_Tactical':'Pants/Pants_Knees/M_Pants_Knees_Black',
'M_Bulletproof_Light':'Chest/Bulletproof/M_Bulletproof_Black','M_Eyelashesh':'Body/M_Eyelashesh','M_Head':'Body/M_Head',
'M_Arms_Gloves_Tactical':'Arms/Gloves_Tactical/M_Gloves_Tactical_Black','T_Heavy_Belt':'Chest/Belt_Heavy/M_Heavy_Belt_Black',
'M_Drops_Tactical':'Chest/Drops/M_Drops_Tactical_Black','M_Knife':'Weapon/Knife/M_Knife','M_Bags_Tactical':'Chest/Bags_Tactical/M_Tactical_Bags_Black',
'M_Headphones_Helmet':'Helmet_Modul/Helmet_HedPh_Basic/M_Helmet_HedPh_Basic_Black','M_Helmet_Tactical':'Helmet_Modul/Helmet_Tactical/M_Helmet_Tactical_Black',
'M_Heavy_Armor':'Chest/Armor/M_Heavy_Armor_Black','M_Tactical_Jacket':'Cloth/Jacket_Hood/M_Tactical_Jacket_Black',
'M_Eyeball':'Body/M_Eyeball','M_Sclera':'Body/Eye/MI_EyeRefractive_Inst','M_Teeth':'Body/M_Teeth',
'M_Pistol':'Weapon/Pistol/M_Pistol','M_Bacpack_Molly':'Backpacks/Bacpack_Molly/M_Bacpack_Molly_Black','M_Helmet_Visor':'Helmet_Modul/Helmet_Visor/M_Hemet_Visor_Black'}
materials={};mesh_rows=[]
def duplicate(source,path):
 if unreal.EditorAssetLibrary.does_asset_exist(path):return unreal.load_asset(path)
 folder,name=path.rsplit('/',1);a=assets.duplicate_asset(name,folder,source);assert a,path;return a

def material_copy(source):
 if not source:return None
 path=source.get_path_name().split('.')[0]
 if not path.startswith(prefix):return source
 if path in materials:return materials[path]
 target=dest+'/Materials/'+path.removeprefix(prefix);copy=duplicate(source,target);materials[path]=copy
 if isinstance(copy,unreal.MaterialInstanceConstant):
  parent=source.get_editor_property('parent')
  assert parent,'Missing parent '+path
  unreal.MaterialEditingLibrary.set_material_instance_parent(copy,material_copy(parent))
 elif isinstance(copy,unreal.Material):
  tex_nodes=[e for e in unreal.ObjectIterator(unreal.MaterialExpressionTextureSample) if e.get_path_name().startswith(copy.get_path_name()+':')]
  assert all(e.get_editor_property('texture') for e in tex_nodes),'Unresolved texture '+path
  unreal.MaterialEditingLibrary.recompile_material(copy)
 unreal.EditorAssetLibrary.save_loaded_asset(copy)
 return copy

target_body=unreal.load_asset('/Game/Bandits/Mesh/Preset/SKM_Bandit_Preset_13');target_skeleton=target_body.get_editor_property('skeleton')
assert target_skeleton
specs={
'SKM_Police_Preset_07':'/Game/Police/MMC/Mesh/Presets/SKM_Preset_07',
'SKM_Police_Goggles':'/Game/Police/MMC/Mesh/Face_Modul/Goggles/SKM_Goggles_Black',
'SKM_Police_BallisticMask':'/Game/Police/MMC/Mesh/Helmet_Modul/Helmet_Mask/SKM_Helmet_Mask_Black',
'SKM_Police_GasMask':'/Game/Police/MMC/Mesh/Face_Modul/Gasmask/SKM_Gasmask_Black',
'SKM_Police_LightVest':'/Game/Police/MMC/Mesh/Chest/Bulletproof/SKM_Bulletproof_Black',
'SKM_Police_HeavyVest':'/Game/Police/MMC/Mesh/Chest/Bulletproof_Heavy/SKM_Bulletproof_Heavy_Black',
'SKM_Police_LightPlates':'/Game/Police/MMC/Mesh/Chest/Armor/SKM_Armor_Light_Black',
'SKM_Police_HeavyPlates':'/Game/Police/MMC/Mesh/Chest/Armor/SKM_Armor_Heavy_Black',
'SKM_Police_NeckArmor':'/Game/Police/MMC/Mesh/Chest/Armor/SKM_Armor_Neck_Black',
'SKM_Police_ShoulderArmor':'/Game/Police/MMC/Mesh/Chest/Armor/SKM_Armor_Shoulder_Black',
'SKM_Police_Sleeves':'/Game/Police/MMC/Mesh/Cloth/Jacket_Hood/SKM_Jacket_Tactical_Black',
'SKM_Police_Arms_Gloves':'/Game/Bandits/Mesh/Arms/Arms_Gloves/SKM_Arms_Gloves_Black'}
meshes={}
for name,path in specs.items():
 source=unreal.load_asset(path);assert isinstance(source,unreal.SkeletalMesh),path
 copy=duplicate(source,dest+'/'+name);copy.call_method('SetSkeleton',(target_skeleton,))
 slots=list(copy.materials)
 for i,slot in enumerate(slots):
  mat=source.materials[i].material_interface
  if not mat and str(slot.material_slot_name) in slot_paths:mat=unreal.load_asset(prefix+slot_paths[str(slot.material_slot_name)])
  assert mat,(path,str(slot.material_slot_name))
  slot.material_interface=material_copy(mat)
 copy.set_editor_property('materials',slots)
 copy.set_editor_property('physics_asset',target_body.get_editor_property('physics_asset') if name=='SKM_Police_Preset_07' else None)
 unreal.EditorAssetLibrary.save_loaded_asset(copy);meshes[name]=copy
 mod=unreal.SkeletonModifier();assert mod.set_skeletal_mesh(copy)
 mesh_rows.append({'asset':copy.get_path_name(),'source':path,'skeleton':target_skeleton.get_path_name(),'bones':len(mod.get_all_bone_names()),'materials':[{'slot':str(m.material_slot_name),'material':m.material_interface.get_path_name()} for m in copy.materials]})
factory=unreal.DataAssetFactory();factory.set_editor_property('DataAssetClass',unreal.PoliceAppearanceData)
data=unreal.load_asset(dest+'/DA_PoliceAppearance') if unreal.EditorAssetLibrary.does_asset_exist(dest+'/DA_PoliceAppearance') else assets.create_asset('DA_PoliceAppearance',dest,unreal.PoliceAppearanceData,factory)
for k,v in {'BaseMesh':'SKM_Police_Preset_07','Goggles':'SKM_Police_Goggles','BallisticMask':'SKM_Police_BallisticMask','GasMask':'SKM_Police_GasMask'}.items():data.set_editor_property(k,meshes[v])
data.set_editor_property('LightArmor',[meshes[n]for n in ['SKM_Police_LightVest','SKM_Police_LightPlates']])
data.set_editor_property('HeavyArmor',[meshes[n]for n in ['SKM_Police_HeavyVest','SKM_Police_HeavyPlates','SKM_Police_NeckArmor','SKM_Police_ShoulderArmor']])
data.set_editor_property('ReplacedMaterialSlots',['M_Googles','M_Googles_Glass','M_Bulletproof_Light','M_Heavy_Armor','M_Pistol'])
data.set_editor_property('UnarmoredHiddenSlots',['M_Drops_Tactical','M_Bags_Tactical'])
unreal.EditorAssetLibrary.save_loaded_asset(data)
# Compare actual local bind transforms, not just matching bone names.
a=unreal.SkeletonModifier();a.set_skeletal_mesh(meshes['SKM_Police_Preset_07']);b=unreal.SkeletonModifier();b.set_skeletal_mesh(target_body)
names=list(a.get_all_bone_names());assert names==list(b.get_all_bone_names())
max_translation=0;max_angle=0
for n in names:
 assert a.get_parent_name(n)==b.get_parent_name(n)
 x=a.get_bone_transform(n,False);y=b.get_bone_transform(n,False)
 max_translation=max(max_translation,(x.translation-y.translation).length())
 dot=abs(x.rotation.x*y.rotation.x+x.rotation.y*y.rotation.y+x.rotation.z*y.rotation.z+x.rotation.w*y.rotation.w)
 max_angle=max(max_angle,math.degrees(2*math.acos(min(1,dot))))
report={'meshes':mesh_rows,'materials':[m.get_path_name()for m in materials.values()],'bone_count':len(names),'max_bind_translation_difference_cm':max_translation,'max_bind_rotation_difference_degrees':max_angle,'appearance_data':data.get_path_name()}
(out/'Setup.json').write_text(json.dumps(report,indent=2),encoding='utf8');print('Police appearance assets saved',len(mesh_rows),len(materials),max_translation,max_angle)
