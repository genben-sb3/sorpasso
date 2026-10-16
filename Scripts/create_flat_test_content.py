"""Creates the M1 content that lives as binary assets: the SportsCar data asset and the flat test map.

Run headless from the repo root after building (the GTO classes must exist):

    & "C:\\Program Files\\Epic Games\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "$PWD\\GTO.uproject" `
        -run=pythonscript -script="$PWD\\Scripts\\create_flat_test_content.py" -unattended -nullrhi -nosplash -nosound

The data asset is created only if missing, so tuned values survive. The map is rebuilt every time.
"""

import unreal

DATA_ASSET_DIR = "/Game/Vehicles/SportsCar"
DATA_ASSET_NAME = "DA_SportsCar"
MAP_PATH = "/Game/Maps/FlatTest"


def create_data_asset():
    if unreal.EditorAssetLibrary.does_asset_exist(f"{DATA_ASSET_DIR}/{DATA_ASSET_NAME}"):
        unreal.log(f"{DATA_ASSET_NAME} exists, kept")
        return
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.GTOVehicleDataAsset)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(DATA_ASSET_NAME, DATA_ASSET_DIR, unreal.GTOVehicleDataAsset, factory)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)


def spawn(actor_class, location=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0), label=None):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actors.spawn_actor_from_class(actor_class, unreal.Vector(*location), unreal.Rotator(roll=rotation[2], pitch=rotation[0], yaw=rotation[1]))
    if label:
        actor.set_actor_label(label)
    return actor


def create_map():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        unreal.EditorAssetLibrary.delete_asset(MAP_PATH)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    levels.new_level(MAP_PATH)

    # 400 m x 400 m slab, top face at z = 0, with the world grid so motion is visible.
    floor = spawn(unreal.StaticMeshActor, (0.0, 0.0, -50.0), label="Floor")
    floor.set_actor_scale3d(unreal.Vector(400.0, 400.0, 1.0))
    mesh = floor.static_mesh_component
    mesh.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    mesh.set_material(0, unreal.load_asset("/Engine/EngineMaterials/WorldGridMaterial"))

    sun = spawn(unreal.DirectionalLight, rotation=(-40.0, -30.0, 0.0), label="Sun")
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    sun.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    spawn(unreal.SkyAtmosphere, label="SkyAtmosphere")
    sky = spawn(unreal.SkyLight, label="SkyLight")
    sky.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property("real_time_capture", True)
    spawn(unreal.ExponentialHeightFog, label="Fog")

    spawn(unreal.PlayerStart, (0.0, 0.0, 100.0), label="Start")

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.GTOGameMode.static_class())

    levels.save_current_level()


create_data_asset()
create_map()
