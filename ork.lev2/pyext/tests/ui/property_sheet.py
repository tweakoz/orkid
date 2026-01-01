#!/usr/bin/env ork.python

################################################################################
# PropertySheet Widget Test
# Demonstrates hierarchical property editor with VarMap-based data
# including color properties with ColorSwatch inline and ColorPicker detail
################################################################################

import signal
from orkengine.core import vec2, vec3, vec4, VarMap, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

def create_color_inline_factory(propsheet):
  """Factory function that creates ColorSwatch widgets for color properties."""
  def factory(sheet, key, value, annotations):
    # Create a ColorSwatch widget
    swatch = lev2.ui.ColorSwatch.wfactory(["swatch_" + key])
    if value is not None:
      swatch.color = value
    swatch.show_hex = True

    # When swatch is clicked, request detail editor
    def on_click():
      propsheet.requestDetailEditor(key)
    swatch.onClick(on_click)

    return swatch
  return factory


def create_color_detail_factory(propsheet):
  """Factory function that creates ColorPicker widgets for color detail editing."""
  def factory(sheet, key, value, annotations, binding):
    # Create a ColorPicker (Python composite widget)
    picker = lev2.ui.ColorPicker.wfactory(["picker_" + key])
    if value is not None:
      picker.color = value

    # Connect picker to binding callbacks
    def on_color_changed(color):
      binding["onValueChanged"](color)
      # Also update the inline swatch
    picker.onColorChanged(on_color_changed)

    return picker
  return factory


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

    # Register color editor factory using token (preferred over enum for CrcEnum)
    self.propsheet.registerEditorFactory(
      tokens.Color,
      create_color_inline_factory(self.propsheet),
      create_color_detail_factory(self.propsheet)
    )

    # Use VarMap data
    self.propsheet.data = self._buildTestData()

    # Set annotations for color properties (use tokens for CrcEnum types)
    model = self.propsheet.model
    diffuse_annot = VarMap()
    diffuse_annot.type = tokens.Color
    model.setAnnotations("Material.diffuse_color", diffuse_annot)

    emissive_annot = VarMap()
    emissive_annot.type = tokens.Color
    model.setAnnotations("Material.emissive_color", emissive_annot)

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

    # Material group - using color properties
    material = VarMap()
    material.diffuse_color = vec4(0.8, 0.2, 0.2, 1.0)  # Color property
    material.emissive_color = vec4(0.0, 0.0, 0.0, 1.0)  # Color property
    material.metallic = 0.0
    material.roughness = 0.5
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

    # Settings group (integers)
    settings = VarMap()
    settings.lod_level = 2
    settings.max_instances = 100
    settings.update_rate = 60
    settings.priority = 5
    data.Settings = settings

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
