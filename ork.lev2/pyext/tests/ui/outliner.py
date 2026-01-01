#!/usr/bin/env ork.python

################################################################################
# Outliner Widget Test
# Demonstrates tree view with hierarchical data
################################################################################

import signal
from orkengine.core import vec2, vec3, vec4, VarMap
from orkengine import lev2

################################################################################

class OutlinerTest:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorStd = vec4(0.1, 0.1, 0.1, 1)

    # Create outliner widget
    outliner_layout = lg_group.makeChild(uiclass=lev2.ui.Outliner, args=["outliner"])
    self.outliner = outliner_layout.widget

    # Set up layout - anchor to parent
    root_layout = lg_group.layout
    outliner_layout.layout.top.anchorTo(root_layout.top)
    outliner_layout.layout.left.anchorTo(root_layout.left)
    outliner_layout.layout.bottom.anchorTo(root_layout.bottom)
    outliner_layout.layout.right.anchorTo(root_layout.right)

    # Build test data
    self.outliner.data = self._buildTestData()
    self.outliner.expandAll()

    # Set selection callback
    def on_select(key):
      print(f"Selected: {key}")
      # Get value from data
      value = self._getValueByKey(key)
      print(f"  Value: {value}")

    self.outliner.onSelect(on_select)

    # Style
    self.outliner.bgcolor = vec4(0.15, 0.15, 0.15, 1)
    self.outliner.text_color = vec4(0.9, 0.9, 0.9, 1)
    self.outliner.selected_color = vec4(0.2, 0.4, 0.6, 1)
    self.outliner.hover_color = vec4(0.25, 0.25, 0.3, 1)
    self.outliner.item_height = 24

    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  def _buildTestData(self):
    """Build hierarchical test data using VarMap."""
    data = VarMap()

    # Scene hierarchy example
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

  def _getValueByKey(self, key):
    """Navigate to a value by slash-separated key path."""
    parts = key.split("/")
    current = self.outliner.data
    for part in parts:
      if part in current:
        val = current[part]
        # Check if it's a nested VarMap
        if isinstance(val, VarMap):
          current = val
        else:
          return val
      else:
        return None
    return current

  def onGpuInit(self, ctx):
    # Set up theme engine for SDF rendering
    self.uicontext = self.ezapp.uicontext
    self.base_db = lev2.ui.createDefaultStyleDatabase()
    self.custom_db = lev2.ui.StyleDatabase.createChild(self.base_db)
    custom_theme = lev2.ui.ThemeEngine(self.custom_db)
    self.uicontext.theme_engine = custom_theme

  def onUpdate(self, updinfo):
    pass

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

###############################################################################

OutlinerTest().ezapp.mainThreadLoop()
