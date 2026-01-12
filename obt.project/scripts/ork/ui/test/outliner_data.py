from orkengine.core import VarMap 

def test_data():
    """Build hierarchical test data using VarMap."""
    data = VarMap()

    # Scene hierarchy
    scene = VarMap()

    # Cameras
    cameras = VarMap()
    cameras.MainCamera = "perspective"
    cameras.TopCamera = "orthographic"
    cameras.SideCamera = "orthographic"
    scene.Cameras = cameras

    # Lights
    lights = VarMap()
    lights.SunLight = "directional"
    lights.PointLight1 = "point"
    lights.SpotLight1 = "spot"
    scene.Lights = lights

    # Objects
    objects = VarMap()

    # Character group
    character = VarMap()
    character.Body = "mesh"
    character.Head = "mesh"
    character.LeftArm = "mesh"
    character.RightArm = "mesh"
    objects.Character = character

    # Environment group
    environment = VarMap()
    environment.Ground = "mesh"
    environment.Sky = "dome"
    environment.Tree1 = "mesh"
    environment.Tree2 = "mesh"
    environment.Rock1 = "mesh"
    objects.Environment = environment

    scene.Objects = objects
    data.Scene = scene

    # Materials
    materials = VarMap()
    materials.CharacterSkin = "pbr"
    materials.GroundGrass = "pbr"
    materials.TreeBark = "pbr"
    materials.SkyDome = "unlit"
    data.Materials = materials

    # Settings
    settings = VarMap()
    settings.RenderQuality = "high"
    settings.ShadowResolution = 2048
    settings.AntiAliasing = "MSAA4x"
    data.Settings = settings

    return data