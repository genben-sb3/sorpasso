"""Creates the M1 content that lives as binary assets: the SportsCar data asset, the physical materials, the flat test
map and the test track with its materials and sky.

Run headless from the repo root after building (the GTO classes must exist):

    & "C:\\Program Files\\Epic Games\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "$PWD\\GTO.uproject" `
        -run=pythonscript -script="$PWD\\Scripts\\create_content.py" -unattended -nullrhi -nosplash -nosound

The data asset and the physical materials are created only if missing, so tuned values survive. Materials and maps are
rebuilt every time. Textures from Poly Haven (CC0, see CREDITS.md) are downloaded to Saved/PolyHaven once and imported
when missing.
"""

import os
import urllib.request

import unreal

DATA_ASSET_DIR = "/Game/Vehicles/SportsCar"
DATA_ASSET_NAME = "DA_SportsCar"
FLAT_MAP_PATH = "/Game/Maps/FlatTest"
TRACK_MAP_PATH = "/Game/Maps/TestTrack"
PHYSICS_DIR = "/Game/Physics"
TRACK_DIR = "/Game/Track"
SKY_DIR = "/Game/Sky"

# Grip of each surface relative to dry asphalt, as the friction of its physical material.
SURFACE_FRICTION = {
    "PM_Asphalt": 1.0,  # the tyres' reference grip (Unreal's default material has 0.7)
    "PM_Curb": 0.85,    # painted concrete
    "PM_Grass": 0.55,
}

POLY_HAVEN = "https://dl.polyhaven.org/file/ph-assets"
# (download URL, asset name in the project, kind of texture)
ASPHALT_TEXTURES = [
    (f"{POLY_HAVEN}/Textures/jpg/2k/asphalt_track/asphalt_track_diff_2k.jpg", "T_AsphaltTrack_D", "color"),
    (f"{POLY_HAVEN}/Textures/jpg/2k/asphalt_track/asphalt_track_nor_dx_2k.jpg", "T_AsphaltTrack_N", "normal"),
    (f"{POLY_HAVEN}/Textures/jpg/2k/asphalt_track/asphalt_track_arm_2k.jpg", "T_AsphaltTrack_ARM", "masks"),
]
ASPHALT_TILE = 2.0  # m, the texture's real size
SKY_HDRI = (f"{POLY_HAVEN}/HDRIs/hdr/4k/kloofendal_48d_partly_cloudy_puresky_4k.hdr", "HDR_KloofendalPartlyCloudy")
# Sun in that HDRI, found as its brightest pixel: 48 deg up, at 0.595 of the image width. Unreal samples lat-long
# images at u = (1 + atan2(x, -y) / pi) / 2 for a world direction (x, y, z), so the sun lies at yaw -55.7 deg and the
# light shines the opposite way.
SUN_ROTATION = (-48.0, 124.3, 0.0)  # pitch, yaw, roll

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
materials = unreal.MaterialEditingLibrary


def create_data_asset():
    if unreal.EditorAssetLibrary.does_asset_exist(f"{DATA_ASSET_DIR}/{DATA_ASSET_NAME}"):
        unreal.log(f"{DATA_ASSET_NAME} exists, kept")
        return
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.GTOVehicleDataAsset)
    asset = asset_tools.create_asset(DATA_ASSET_NAME, DATA_ASSET_DIR, unreal.GTOVehicleDataAsset, factory)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)


def create_physical_materials():
    result = {}
    for name, friction in SURFACE_FRICTION.items():
        path = f"{PHYSICS_DIR}/{name}"
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.log(f"{name} exists, kept")
            result[name] = unreal.load_asset(path)
            continue
        asset = asset_tools.create_asset(name, PHYSICS_DIR, unreal.PhysicalMaterial, unreal.PhysicalMaterialFactoryNew())
        asset.set_editor_property("friction", friction)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        result[name] = asset
    return result


def download(url):
    """Path of the file from url below Saved/PolyHaven, fetched once."""
    folder = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "PolyHaven")
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, url.rsplit("/", 1)[1])
    if not os.path.exists(path):
        unreal.log(f"downloading {url}")
        urllib.request.urlretrieve(url, path)
    return path


def import_file(url, directory, name):
    """The asset imported from url, imported only if missing."""
    path = f"{directory}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", download(url))
    task.set_editor_property("destination_path", directory)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    asset_tools.import_asset_tasks([task])
    return unreal.load_asset(path)


def import_asphalt_textures():
    textures = {}
    for url, name, kind in ASPHALT_TEXTURES:
        texture = import_file(url, f"{TRACK_DIR}/Textures", name)
        if kind == "normal":
            texture.set_editor_property("srgb", False)
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        elif kind == "masks":
            texture.set_editor_property("srgb", False)
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        unreal.EditorAssetLibrary.save_loaded_asset(texture)
        textures[kind] = texture
    return textures


def new_material(name, physical_material):
    """An empty material at TRACK_DIR/Materials/name, cleared if it exists, with the given physical material."""
    directory = f"{TRACK_DIR}/Materials"
    path = f"{directory}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.load_asset(path)
        materials.delete_all_material_expressions(material)
    else:
        material = asset_tools.create_asset(name, directory, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("phys_material", physical_material)
    return material


def expression(material, expression_class, x, y, **properties):
    node = materials.create_material_expression(material, expression_class, x, y)
    for key, value in properties.items():
        node.set_editor_property(key, value)
    return node


def constant(material, value, x, y):
    return expression(material, unreal.MaterialExpressionConstant, x, y, r=value)


def color(material, rgb, x, y):
    return expression(material, unreal.MaterialExpressionConstant3Vector, x, y, constant=unreal.LinearColor(*rgb, 1.0))


def finish(material):
    materials.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def create_asphalt_material(textures, physical_material):
    """Poly Haven asphalt_track at its real size. The track's UVs are metres along and across the road."""
    material = new_material("M_TrackAsphalt", physical_material)
    uv = expression(material, unreal.MaterialExpressionTextureCoordinate, -900, 0, u_tiling=1.0 / ASPHALT_TILE, v_tiling=1.0 / ASPHALT_TILE)
    samples = {
        "color": expression(material, unreal.MaterialExpressionTextureSample, -500, -300, texture=textures["color"]),
        "masks": expression(material, unreal.MaterialExpressionTextureSample, -500, 0, texture=textures["masks"], sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS),
        "normal": expression(material, unreal.MaterialExpressionTextureSample, -500, 300, texture=textures["normal"], sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
    }
    for sample in samples.values():
        materials.connect_material_expressions(uv, "", sample, "UVs")
    materials.connect_material_property(samples["color"], "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    materials.connect_material_property(samples["masks"], "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    materials.connect_material_property(samples["masks"], "G", unreal.MaterialProperty.MP_ROUGHNESS)
    materials.connect_material_property(samples["masks"], "B", unreal.MaterialProperty.MP_METALLIC)
    materials.connect_material_property(samples["normal"], "RGB", unreal.MaterialProperty.MP_NORMAL)
    return finish(material)


def create_curb_material(physical_material):
    """Red and white blocks of 1 m each along the track."""
    material = new_material("M_TrackCurb", physical_material)
    uv = expression(material, unreal.MaterialExpressionTextureCoordinate, -1000, 0, u_tiling=0.5, v_tiling=0.5)
    along = expression(material, unreal.MaterialExpressionComponentMask, -800, 0, r=True, g=False, b=False, a=False)
    stripe = expression(material, unreal.MaterialExpressionFrac, -650, 0)
    block = expression(material, unreal.MaterialExpressionRound, -500, 0)
    paint = expression(material, unreal.MaterialExpressionLinearInterpolate, -300, 0)
    materials.connect_material_expressions(uv, "", along, "")
    materials.connect_material_expressions(along, "", stripe, "")
    materials.connect_material_expressions(stripe, "", block, "")
    materials.connect_material_expressions(color(material, (0.6, 0.02, 0.02), -500, -200), "", paint, "A")
    materials.connect_material_expressions(color(material, (0.75, 0.75, 0.75), -500, 200), "", paint, "B")
    materials.connect_material_expressions(block, "", paint, "Alpha")
    materials.connect_material_property(paint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    materials.connect_material_property(constant(material, 0.55, -300, 300), "", unreal.MaterialProperty.MP_ROUGHNESS)
    return finish(material)


def create_grass_material(physical_material):
    """Plain green, mottled by world-space noise so the large flat areas do not look painted."""
    material = new_material("M_TrackGrass", physical_material)
    noise = expression(material, unreal.MaterialExpressionNoise, -600, 200, scale=0.002, levels=4, output_min=0.75, output_max=1.15)
    tint = expression(material, unreal.MaterialExpressionMultiply, -300, 0)
    materials.connect_material_expressions(color(material, (0.035, 0.065, 0.018), -600, 0), "", tint, "A")
    materials.connect_material_expressions(noise, "", tint, "B")
    materials.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    materials.connect_material_property(constant(material, 0.95, -300, 300), "", unreal.MaterialProperty.MP_ROUGHNESS)
    return finish(material)


def create_line_material(physical_material):
    material = new_material("M_TrackLine", physical_material)
    materials.connect_material_property(color(material, (0.8, 0.8, 0.8), -300, 0), "", unreal.MaterialProperty.MP_BASE_COLOR)
    materials.connect_material_property(constant(material, 0.6, -300, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    return finish(material)


def spawn(actor_class, location=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0), label=None):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actors.spawn_actor_from_class(actor_class, unreal.Vector(*location), unreal.Rotator(roll=rotation[2], pitch=rotation[0], yaw=rotation[1]))
    if label:
        actor.set_actor_label(label)
    return actor


def spawn_at(actor_class, transform, label, lift=0.0):
    location = transform.translation
    yaw = transform.rotation.rotator().yaw
    return spawn(actor_class, (location.x, location.y, location.z + lift), (0.0, yaw, 0.0), label)


def open_empty_map(path):
    """Opens the map with no actors in it. An existing map is emptied, since deleting it fails while it is in use."""
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        levels.new_level(path)
        return levels
    levels.load_level(path)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in actors.get_all_level_actors():
        if not isinstance(actor, unreal.WorldSettings):
            actors.destroy_actor(actor)
    return levels


def save_map(levels):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.GTOGameMode.static_class())
    levels.save_current_level()


def create_flat_map(asphalt):
    levels = open_empty_map(FLAT_MAP_PATH)

    # 400 m x 400 m slab, top face at z = 0, with the world grid so motion is visible.
    floor = spawn(unreal.StaticMeshActor, (0.0, 0.0, -50.0), label="Floor")
    floor.set_actor_scale3d(unreal.Vector(400.0, 400.0, 1.0))
    mesh = floor.static_mesh_component
    mesh.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    mesh.set_material(0, unreal.load_asset("/Engine/EngineMaterials/WorldGridMaterial"))
    mesh.set_phys_material_override(asphalt)

    sun = spawn(unreal.DirectionalLight, rotation=(-40.0, -30.0, 0.0), label="Sun")
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    sun.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    spawn(unreal.SkyAtmosphere, label="SkyAtmosphere")
    sky = spawn(unreal.SkyLight, label="SkyLight")
    sky.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property("real_time_capture", True)
    spawn(unreal.ExponentialHeightFog, label="Fog")

    spawn(unreal.PlayerStart, (0.0, 0.0, 100.0), label="Start")
    save_map(levels)


def create_track_map(physical, track_materials):
    levels = open_empty_map(TRACK_MAP_PATH)

    track = spawn(unreal.GTOTestTrack, label="TestTrack")
    track.set_editor_property("road_material", track_materials["road"])
    track.set_editor_property("curb_material", track_materials["curb"])
    track.set_editor_property("grass_material", track_materials["grass"])

    line = spawn_at(unreal.GTOStartFinishLine, track.get_start_line_transform(), "StartFinishLine")
    line.set_editor_property("width", track.get_road_width())
    line.set_editor_property("stripe_material", track_materials["line"])

    spawn_at(unreal.PlayerStart, track.get_grid_transform(), "Grid", lift=100.0)

    # Sky and its light from the HDRI, sun lined up with the sun in it.
    hdri = import_file(SKY_HDRI[0], SKY_DIR, SKY_HDRI[1])
    # The backdrop has a floor of its own; well below the grass it stays out of sight.
    backdrop = spawn(unreal.load_class(None, "/HDRIBackdrop/Blueprints/HDRIBackdrop.HDRIBackdrop_C"), (0.0, 0.0, -5000.0), label="Sky")
    backdrop.set_editor_property("Cubemap", hdri)
    backdrop.set_editor_property("Size", 5000.0)  # m, around the whole track
    backdrop.set_editor_property("UseCameraProjection", True)  # sky at infinity: no ground projected from a pure sky
    sun = spawn(unreal.DirectionalLight, rotation=SUN_ROTATION, label="Sun")
    sun.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_editor_property("intensity", 8.0)
    fog = spawn(unreal.ExponentialHeightFog, label="Fog")
    fog.component.set_editor_property("fog_density", 0.002)
    fog.component.set_editor_property("inscattering_color_cubemap", hdri)

    save_map(levels)


def main():
    create_data_asset()
    physical = create_physical_materials()
    create_flat_map(physical["PM_Asphalt"])

    track_materials = {
        "road": create_asphalt_material(import_asphalt_textures(), physical["PM_Asphalt"]),
        "curb": create_curb_material(physical["PM_Curb"]),
        "grass": create_grass_material(physical["PM_Grass"]),
        "line": create_line_material(physical["PM_Asphalt"]),
    }
    create_track_map(physical, track_materials)


main()
