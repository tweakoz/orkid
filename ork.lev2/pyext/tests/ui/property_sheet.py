#!/usr/bin/env ork.python

################################################################################
# PropertySheet Widget Test
# Demonstrates hierarchical property editor with VarMap-based data
################################################################################

import signal
from orkengine.core import vec2, vec3, vec4, VarMap
from orkengine import lev2

################################################################################

class PropertySheetTest:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, width=480, fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorStd = vec4(0.1, 0.1, 0.1, 1)

    # Create property sheet widget
    sheet_layout = lg_group.makeChild(uiclass=lev2.ui.PropertySheet, args=["propsheet"])
    self.propsheet = sheet_layout.widget

    # Set up layout - anchor to parent
    root_layout = lg_group.layout
    sheet_layout.layout.top.anchorTo(root_layout.top)
    sheet_layout.layout.left.anchorTo(root_layout.left)
    sheet_layout.layout.bottom.anchorTo(root_layout.bottom)
    sheet_layout.layout.right.anchorTo(root_layout.right)

    # Use VarMap data
    self.propsheet.data = self._buildTestData()
    self.propsheet.expandAll()

    # Set property change callback
    def on_property_changed(key, value):
      print(f"Property changed: {key} = {value}")

    self.propsheet.onPropertyChanged(on_property_changed)

    # Style
    self.propsheet.bgcolor = vec4(0.12, 0.12, 0.12, 1)
    self.propsheet.label_color = vec4(0.9, 0.9, 0.9, 1)
    self.propsheet.group_color = vec4(0.18, 0.18, 0.22, 1)
    self.propsheet.row_height = 28
    self.propsheet.label_width = 140

    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  def _buildTestData(self):
    """Build hierarchical test data using VarMap."""
    data = VarMap()

    # Transform group
    transform = VarMap()
    transform.position_x = 0.0
    transform.position_y = 0.0
    transform.position_z = 0.0
    transform.rotation_x = 0.0
    transform.rotation_y = 0.0
    transform.rotation_z = 0.0
    transform.scale = 1.0
    data.Transform = transform

    # Material group
    material = VarMap()
    material.diffuse_r = 0.8
    material.diffuse_g = 0.2
    material.diffuse_b = 0.2
    material.metallic = 0.0
    material.roughness = 0.5
    material.emissive = 0.0
    data.Material = material

    # Rendering group
    rendering = VarMap()
    rendering.cast_shadows = True
    rendering.receive_shadows = True
    rendering.visible = True
    data.Rendering = rendering

    # Physics group
    physics = VarMap()
    physics.mass = 1.0
    physics.friction = 0.5
    physics.restitution = 0.3
    physics.is_static = False
    data.Physics = physics

    return data

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

PropertySheetTest().ezapp.mainThreadLoop()
