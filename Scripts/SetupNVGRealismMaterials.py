"""Rebuild NVG shaders in place, preserving asset identity and the pre-bloom insertion point."""
import unreal,shutil,json
from pathlib import Path
root=Path(unreal.Paths.project_dir());E=unreal.MaterialEditingLibrary;assets=unreal.AssetToolsHelpers.get_asset_tools();backup=root/'Saved/NVGRealism/Before';backup.mkdir(parents=True,exist_ok=True)
def material(name):
 path='/Game/FPS/Materials/'+name;local=root/'Content/FPS/Materials'/(name+'.uasset')
 if local.exists()and not(backup/local.name).exists():shutil.copy2(local,backup/local.name)
 m=unreal.load_asset(path)if unreal.EditorAssetLibrary.does_asset_exist(path)else assets.create_asset(name,'/Game/FPS/Materials',unreal.Material,unreal.MaterialFactoryNew())
 E.delete_all_material_expressions(m);return m

def node(m,cls,**props):
 n=E.create_material_expression(m,cls)
 for k,v in props.items():n.set_editor_property(k,v)
 return n

def custom(m,code,names,output):
 c=node(m,unreal.MaterialExpressionCustom,code=code,output_type=output)
 pins=[]
 for name in names:
  p=unreal.CustomInput();p.set_editor_property('input_name',name);pins.append(p)
 c.set_editor_property('inputs',pins);return c

def scalar(m,name,value):return node(m,unreal.MaterialExpressionScalarParameter,parameter_name=name,default_value=value)

m=material('M_NVG_Phosphor');m.set_editor_property('material_domain',unreal.MaterialDomain.MD_POST_PROCESS);m.set_editor_property('blendable_location',unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_BLOOM)
c=custom(m,(root/'Scripts/Shaders/NVGPhosphor.hlsl').read_text(encoding='utf-8-sig'),['SceneColor','SceneDepth','InputInvSize','TubeTime','Battery','Quad','ViewportUV','PhosphorTint'],unreal.CustomMaterialOutputType.CMOT_FLOAT3)
scene=node(m,unreal.MaterialExpressionSceneTexture,scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
depth=node(m,unreal.MaterialExpressionSceneTexture,scene_texture_id=unreal.SceneTextureId.PPI_SCENE_DEPTH)
for src,pin,dest in [(scene,'Color','SceneColor'),(scene,'InvSize','InputInvSize'),(depth,'Color','SceneDepth'),(node(m,unreal.MaterialExpressionTime),'','TubeTime'),(scalar(m,'Battery',1.),'','Battery'),(scalar(m,'Quad',0.),'','Quad'),(node(m,unreal.MaterialExpressionScreenPosition),'ViewportUV','ViewportUV'),(node(m,unreal.MaterialExpressionVectorParameter,parameter_name='PhosphorTint',default_value=unreal.LinearColor(.38,1.,.44,1.)),'','PhosphorTint')]:assert E.connect_material_expressions(src,pin,c,dest),(pin,dest)
assert E.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
E.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)

m=material('M_NVG_Housing');m.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
c=custom(m,(root/'Scripts/Shaders/NVGHousing.hlsl').read_text(encoding='utf-8-sig'),['UV','Quad','Aspect','ScreenHeight'],unreal.CustomMaterialOutputType.CMOT_FLOAT1)
for src,dest in [(node(m,unreal.MaterialExpressionTextureCoordinate),'UV'),(scalar(m,'Quad',0.),'Quad'),(scalar(m,'Aspect',16./9.),'Aspect'),(scalar(m,'ScreenHeight',1080.),'ScreenHeight')]:assert E.connect_material_expressions(src,'',c,dest)
assert E.connect_material_property(c,'',unreal.MaterialProperty.MP_OPACITY)
black=node(m,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(0,0,0,1));assert E.connect_material_property(black,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
E.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)
print('Saved NVG tube and housing materials')
