#!/usr/bin/env ork.python

################################################################################
# SDF Primitives Test - Comprehensive demonstration of all SDF shapes
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
#
# Demonstrates:
#   - All 7 SDF primitive types (box, tab, circle, triangle, ring, box_per_corner, pause)
#   - Corner radius variations
#   - Border width variations
#   - Blend mode variations
#   - Rotation/orientation (triangles)
#   - Shape parameters (bar widths, radii)
#   - Color theming
#   - TabWidget + DynaGrid organization
################################################################################

import signal
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

class SDFPrimsTest(object):

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(
        self,
        width=1400,
        height=1000,
        left=100,
        top=100)

    self.ezapp.topWidget.enableUiDraw()

    # Store triangle and star shapes for animation
    self.triangle_shapes = []
    self.star_shapes = []

  ############################################################################

  def onGpuInit(self, ctx):

    self.uicontext = self.ezapp.uicontext

    ########################################
    # Setup theme database
    ########################################

    self.base_db = lev2.ui.createDefaultStyleDatabase()
    self.custom_db = lev2.ui.StyleDatabase.createChild(self.base_db)

    # Create custom theme
    custom_theme = lev2.ui.ThemeEngine(self.custom_db)
    self.uicontext.theme_engine = custom_theme

    ########################################
    # Create main layout
    ########################################

    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 4
    lg_group.clearColorGuide = vec4(0.1, 0.1, 0.15, 1)

    ############################################
    # Create main layout with single cell
    ############################################

    self.griditems = lg_group.makeGrid(
      width=1,
      height=1,
      margin = 4,
      uiclass = lev2.ui.Box,
      args = ["label", vec4(0.1, 0.1, 0.3, 1)],
    )

    ############################################
    # Create tab widget
    ############################################

    tabs = lg_group.makeChild(
        uiclass=lev2.ui.TabsWidget,
        args=["SDF Primitives", vec3(0.5, 0.5, 1.0)]
    )
    self.tabsw = tabs.widget
    lg_group.replaceChild(self.griditems[0].layout, tabs)
    self.tabsw.draw_tabs = True

    ########################################
    # Create tabs for each primitive category
    ########################################

    self._createBoxesTab()
    self._createTabsTab()
    self._createCirclesTab()
    self._createTrianglesTab()
    self._createRingsTab()
    self._createPerCornerTab()
    self._createSpecialTab()

  ############################################################################
  # TAB 1: Boxes
  ############################################################################

  def _createBoxesTab(self):
    """Box primitives with various corner radii and borders"""

    # Create DynaGrid for this tab
    grid = self.tabsw.makeChild(uiclass=lev2.ui.DynaGrid, args=["Boxes"])
    grid.margin = 4

    variations = [
      # Row 1: Corner radius variations
      ("Sharp\nCorners\nr=0", "box", 0, 2, vec4(0.3, 0.4, 0.5, 0.9), "ALPHA", {}),
      ("Small\nRadius\nr=4", "box", 4, 2, vec4(0.3, 0.4, 0.5, 0.9), "ALPHA", {}),
      ("Medium\nRadius\nr=16", "box", 16, 2, vec4(0.3, 0.4, 0.5, 0.9), "ALPHA", {}),
      ("Large\nRadius\nr=32", "box", 32, 2, vec4(0.3, 0.4, 0.5, 0.9), "ALPHA", {}),

      # Row 2: Border width variations
      ("Thin\nBorder\nw=1", "box", 8, 1, vec4(0.4, 0.3, 0.5, 0.9), "ALPHA", {}),
      ("Medium\nBorder\nw=4", "box", 8, 4, vec4(0.4, 0.3, 0.5, 0.9), "ALPHA", {}),
      ("Thick\nBorder\nw=8", "box", 8, 8, vec4(0.4, 0.3, 0.5, 0.9), "ALPHA", {}),
      ("No\nBorder\nw=0", "box", 8, 0, vec4(0.4, 0.3, 0.5, 0.9), "ALPHA", {}),

      # Row 3: Blend modes
      ("Alpha\nBlend\n0.85", "box", 12, 2, vec4(0.5, 0.3, 0.4, 0.85), "ALPHA", {}),
      ("Additive\n(glow)\n0.7", "box", 12, 2, vec4(0.6, 0.4, 0.2, 0.7), "ADDITIVE", {}),
      ("Subtractive\n(invert)\n0.6", "box", 12, 2, vec4(0.3, 0.5, 0.6, 0.6), "SUBTRACTIVE", {}),
      ("Modulate\n(tint)\n0.75", "box", 12, 2, vec4(0.7, 0.4, 0.5, 0.75), "ALPHA_MODULATE", {}),
    ]

    self._populateGrid(grid, variations, "boxes")

  ############################################################################
  # TAB 2: Tabs
  ############################################################################

  def _createTabsTab(self):
    """Tab shapes (round top, sharp bottom)"""

    grid = self.tabsw.makeChild(uiclass=lev2.ui.DynaGrid, args=["Tabs"])
    grid.margin = 4

    variations = [
      # Different radius variations for tabs
      ("Subtle\nRadius\nr=4", "tab", 4, 2, vec4(0.35, 0.35, 0.4, 0.95), "ALPHA", {}),
      ("Default\nRadius\nr=12", "tab", 12, 2, vec4(0.35, 0.35, 0.4, 0.95), "ALPHA", {}),
      ("Large\nRadius\nr=20", "tab", 20, 2, vec4(0.35, 0.35, 0.4, 0.95), "ALPHA", {}),
      ("XL\nRadius\nr=32", "tab", 32, 2, vec4(0.35, 0.35, 0.4, 0.95), "ALPHA", {}),

      # Border variations
      ("Thin\nBorder\nw=1", "tab", 12, 1, vec4(0.3, 0.4, 0.5, 0.95), "ALPHA", {}),
      ("Thick\nBorder\nw=4", "tab", 12, 4, vec4(0.3, 0.4, 0.5, 0.95), "ALPHA", {}),

      # Color variations (simulating active/inactive tabs)
      ("Inactive\nTab\nDim", "tab", 12, 1, vec4(0.25, 0.25, 0.3, 0.9), "ALPHA", {}),
      ("Active\nTab\nBright", "tab", 12, 2, vec4(0.4, 0.4, 0.45, 0.95), "ALPHA", {}),
    ]

    self._populateGrid(grid, variations, "tabs")

  ############################################################################
  # TAB 3: Circles
  ############################################################################

  def _createCirclesTab(self):
    """Perfect circles"""

    grid = self.tabsw.makeChild(uiclass=lev2.ui.DynaGrid, args=["Circles"])
    grid.margin = 4

    variations = [
      # Circle sizes (shape_param = radius, 0 = auto)
      ("Auto\nRadius\n(fit)", "circle", 8, 2, vec4(0.4, 0.3, 0.5, 0.9), "ALPHA", {"shape_param": 0.0}),
      ("Small\nRadius\n20px", "circle", 8, 2, vec4(0.4, 0.3, 0.5, 0.9), "ALPHA", {"shape_param": 20.0}),
      ("Medium\nRadius\n40px", "circle", 8, 2, vec4(0.4, 0.3, 0.5, 0.9), "ALPHA", {"shape_param": 40.0}),
      ("Large\nRadius\n60px", "circle", 8, 2, vec4(0.4, 0.3, 0.5, 0.9), "ALPHA", {"shape_param": 60.0}),

      # Border variations
      ("Thin\nOutline\nw=1", "circle", 8, 1, vec4(0.3, 0.5, 0.4, 0.9), "ALPHA", {"shape_param": 0.0}),
      ("Thick\nOutline\nw=6", "circle", 8, 6, vec4(0.3, 0.5, 0.4, 0.9), "ALPHA", {"shape_param": 0.0}),

      # Blend modes (good for indicators/dots)
      ("Glow\nCircle\nAdditive", "circle", 8, 2, vec4(0.8, 0.6, 0.2, 0.8), "ADDITIVE", {"shape_param": 0.0}),
      ("Soft\nCircle\nPremul", "circle", 8, 2, vec4(0.5, 0.3, 0.6, 0.7), "PREMA", {"shape_param": 0.0}),
    ]

    self._populateGrid(grid, variations, "circles")

  ############################################################################
  # TAB 4: Triangles
  ############################################################################

  def _createTrianglesTab(self):
    """Equilateral triangles (for arrows, indicators)"""

    import math
    grid = self.tabsw.makeChild(uiclass=lev2.ui.DynaGrid, args=["Triangles"])
    grid.margin = 4

    variations = [
      # Rotation variations (shape_param = rotation angle in radians)
      ("Point\nUp\n0°", "triangle", 8, 2, vec4(0.5, 0.4, 0.3, 0.9), "ALPHA", {"shape_param": 0.0}),
      ("Rotate\n60°\nCW", "triangle", 8, 2, vec4(0.5, 0.4, 0.3, 0.9), "ALPHA", {"shape_param": math.pi/3}),
      ("Rotate\n120°\nCW", "triangle", 8, 2, vec4(0.5, 0.4, 0.3, 0.9), "ALPHA", {"shape_param": 2*math.pi/3}),
      ("Point\nDown\n180°", "triangle", 8, 2, vec4(0.5, 0.4, 0.3, 0.9), "ALPHA", {"shape_param": math.pi}),

      # Corner radius variations
      ("Sharp\nCorners\nr=0", "triangle", 0, 2, vec4(0.4, 0.5, 0.3, 0.9), "ALPHA", {"shape_param": 0.0}),
      ("Round\nCorners\nr=12", "triangle", 12, 2, vec4(0.4, 0.5, 0.3, 0.9), "ALPHA", {"shape_param": 0.0}),

      # Border variations
      ("Thick\nBorder\nw=6", "triangle", 8, 6, vec4(0.3, 0.4, 0.5, 0.9), "ALPHA", {"shape_param": 0.0}),
      ("Glow\nTriangle\nAdditive", "triangle", 8, 2, vec4(0.8, 0.5, 0.2, 0.8), "ADDITIVE", {"shape_param": 0.0}),
    ]

    self._populateGrid(grid, variations, "triangles")

  ############################################################################
  # TAB 5: Rings
  ############################################################################

  def _createRingsTab(self):
    """Donut/ring shapes (for progress indicators, badges)"""

    grid = self.tabsw.makeChild(uiclass=lev2.ui.DynaGrid, args=["Rings"])
    grid.margin = 4

    variations = [
      # Ring thickness variations (shape_param = inner_radius)
      ("Auto\nThickness\n(60%)", "ring", 8, 2, vec4(0.4, 0.4, 0.6, 0.9), "ALPHA", {"shape_param": 0.0}),
      ("Thin\nRing\n70%", "ring", 8, 2, vec4(0.4, 0.4, 0.6, 0.9), "ALPHA", {"shape_param": 35.0}),
      ("Medium\nRing\n50%", "ring", 8, 2, vec4(0.4, 0.4, 0.6, 0.9), "ALPHA", {"shape_param": 25.0}),
      ("Thick\nRing\n30%", "ring", 8, 2, vec4(0.4, 0.4, 0.6, 0.9), "ALPHA", {"shape_param": 15.0}),

      # Border variations (creates double-ring effect)
      ("Outlined\nRing\nw=1", "ring", 8, 1, vec4(0.5, 0.4, 0.6, 0.9), "ALPHA", {"shape_param": 25.0}),
      ("Bold\nOutline\nw=4", "ring", 8, 4, vec4(0.5, 0.4, 0.6, 0.9), "ALPHA", {"shape_param": 25.0}),

      # Blend modes (good for progress indicators)
      ("Glow\nRing\nAdditive", "ring", 8, 2, vec4(0.7, 0.5, 0.3, 0.8), "ADDITIVE", {"shape_param": 25.0}),
      ("Soft\nRing\nAlpha", "ring", 8, 2, vec4(0.3, 0.6, 0.5, 0.7), "ALPHA", {"shape_param": 25.0}),
    ]

    self._populateGrid(grid, variations, "rings")

  ############################################################################
  # TAB 6: Per-Corner Boxes
  ############################################################################

  def _createPerCornerTab(self):
    """Boxes with individual corner radii"""

    grid = self.tabsw.makeChild(uiclass=lev2.ui.DynaGrid, args=["Per-Corner"])
    grid.margin = 4

    variations = [
      # Different corner combinations (corner_radii = TL, TR, BR, BL)
      ("Top Left\nOnly\n(16,0,0,0)", "box_per_corner", 0, 2, vec4(0.4, 0.3, 0.5, 0.9), "ALPHA",
       {"corner_radii": vec4(16, 0, 0, 0)}),
      ("Top Right\nOnly\n(0,16,0,0)", "box_per_corner", 0, 2, vec4(0.4, 0.3, 0.5, 0.9), "ALPHA",
       {"corner_radii": vec4(0, 16, 0, 0)}),
      ("Bottom Left\nOnly\n(0,0,0,16)", "box_per_corner", 0, 2, vec4(0.4, 0.3, 0.5, 0.9), "ALPHA",
       {"corner_radii": vec4(0, 0, 0, 16)}),
      ("Bottom Right\nOnly\n(0,0,16,0)", "box_per_corner", 0, 2, vec4(0.4, 0.3, 0.5, 0.9), "ALPHA",
       {"corner_radii": vec4(0, 0, 16, 0)}),

      # Useful combinations
      ("Top Both\n(tab-like)\n(12,12,0,0)", "box_per_corner", 0, 2, vec4(0.35, 0.35, 0.4, 0.95), "ALPHA",
       {"corner_radii": vec4(12, 12, 0, 0)}),
      ("Bottom Both\n(inverted)\n(0,0,12,12)", "box_per_corner", 0, 2, vec4(0.35, 0.35, 0.4, 0.95), "ALPHA",
       {"corner_radii": vec4(0, 0, 12, 12)}),
      ("Left Both\n(16,0,0,16)", "box_per_corner", 0, 2, vec4(0.3, 0.5, 0.4, 0.9), "ALPHA",
       {"corner_radii": vec4(16, 0, 0, 16)}),
      ("Right Both\n(0,16,16,0)", "box_per_corner", 0, 2, vec4(0.3, 0.5, 0.4, 0.9), "ALPHA",
       {"corner_radii": vec4(0, 16, 16, 0)}),

      # Asymmetric designs
      ("Diagonal 1\n(16,0,16,0)", "box_per_corner", 0, 2, vec4(0.5, 0.4, 0.3, 0.9), "ALPHA",
       {"corner_radii": vec4(16, 0, 16, 0)}),
      ("Diagonal 2\n(0,16,0,16)", "box_per_corner", 0, 2, vec4(0.5, 0.4, 0.3, 0.9), "ALPHA",
       {"corner_radii": vec4(0, 16, 0, 16)}),
      ("Varied\nRadii\n(24,12,6,3)", "box_per_corner", 0, 2, vec4(0.4, 0.5, 0.5, 0.9), "ALPHA",
       {"corner_radii": vec4(24, 12, 6, 3)}),
      ("All\nDifferent\n(4,12,20,8)", "box_per_corner", 0, 2, vec4(0.5, 0.3, 0.5, 0.9), "ALPHA",
       {"corner_radii": vec4(4, 12, 20, 8)}),
    ]

    self._populateGrid(grid, variations, "percorner")

  ############################################################################
  # TAB 7: Special Icons
  ############################################################################

  def _createSpecialTab(self):
    """Special icons (pause, play, etc.)"""

    grid = self.tabsw.makeChild(uiclass=lev2.ui.DynaGrid, args=["Special"])
    grid.margin = 4

    import math
    variations = [
      # Pause icon variations (shape_param = spacing between bars)
      ("Narrow\nGap\ns=0.6", "pause", 4, 2, vec4(0.4, 0.5, 0.6, 0.9), "ALPHA", {"shape_param": 0.6}),
      ("Medium\nGap\ns=0.8", "pause", 4, 2, vec4(0.5, 0.4, 0.6, 0.9), "ALPHA", {"shape_param": 0.8}),
      ("Wide\nGap\ns=1.0", "pause", 8, 3, vec4(0.6, 0.4, 0.5, 0.9), "ALPHA", {"shape_param": 1.0}),
      ("Rounded\nPause\nr=8", "pause", 8, 2, vec4(0.4, 0.6, 0.5, 0.9), "ALPHA", {"shape_param": 0.8}),

      # Star variations (shape_param = rotation angle in radians)
      ("Star\nUp\n0°", "star", 4, 2, vec4(0.9, 0.8, 0.2, 0.9), "ALPHA", {"shape_param": 0.0}),
      ("Star\nRotate\n36°", "star", 4, 2, vec4(0.9, 0.8, 0.2, 0.9), "ALPHA", {"shape_param": math.pi/5}),
      ("Star\nBorder\nw=4", "star", 4, 4, vec4(0.8, 0.6, 0.2, 0.9), "ALPHA", {"shape_param": 0.0}),
      ("Glow\nStar\nAdditive", "star", 4, 2, vec4(1.0, 0.8, 0.3, 0.9), "ADDITIVE", {"shape_param": 0.0}),
    ]

    self._populateGrid(grid, variations, "special")

  ############################################################################
  # Helper: Populate grid with shape variations
  ############################################################################

  def _populateGrid(self, grid, variations, tab_name=""):
    """
    variations = list of tuples:
      (label, shape_type, radius, border_width, color, blend_mode, extra_params)
    """

    for idx, (label, shape_type, radius, border, color, blend, extra) in enumerate(variations):
      # AlignmentGroup at grid level to center the entire cell
      alignment = grid.makeChild(uiclass=lev2.ui.AlignmentGroup, args=[f"align_{idx}"])
      alignment.alignment = tokens.CENTER
      alignment.width_proportional = 0.9
      alignment.height_proportional = 0.75
      alignment.draw_background = False
      alignment.margin = 4

      # VerticalPack inside AlignmentGroup for label + shape
      vpack = alignment.makeChild(uiclass=lev2.ui.VerticalPack, args=[f"cell_{idx}"])
      vpack.margin = 2
      vpack.draw_background = False
      vpack.fill = True

      # Label at top
      label_widget = vpack.makeChild(uiclass=lev2.ui.TextBox, args=[f"label_{idx}", vec4(0.1, 0.1, 0.1, 1), label])
      label_widget.fixed_height = 64
      label_widget.draw_background = False

      # SdfShape below label
      alignment2 = vpack.makeChild(uiclass=lev2.ui.AlignmentGroup, args=[f"align2_{idx}"])
      alignment2.alignment = tokens.CENTER
      alignment2.width_proportional = 1.0
      alignment2.height_proportional = 1.0
      alignment2.draw_background = True
      alignment2.margin = 0
      alignment2.maintain_aspect_ratio = 1.0

      shape_widget = alignment2.makeChild(uiclass=lev2.ui.SdfShape, args=[f"shape_{idx}"])

      # Configure shape
      shape_widget.shape_type = getattr(tokens, shape_type)

      # Apply extra parameters
      if "corner_radii" in extra:
        shape_widget.corner_radii = extra["corner_radii"]
      if "shape_param" in extra:
        shape_widget.shape_param = extra["shape_param"]
      if "horizontal" in extra:
        shape_widget.horizontal = extra["horizontal"]

      # Create and register style
      style = lev2.ui.Style()
      style.corner_radius = radius
      style.border_width = border
      style.bg_color = color
      style.border_color = vec4(color.x * 1.5, color.y * 1.5, color.z * 1.5, 1.0)
      style.blend_mode = getattr(tokens, blend)

      style_tag = f"{tab_name}_shape_{idx}_style"
      self.custom_db.registerStyle(getattr(tokens, style_tag), style)

      # Apply theme to shape
      shape_widget.theme = getattr(tokens, style_tag)

      # Store triangle and star shapes for animation
      if shape_type == "triangle":
        self.triangle_shapes.append(shape_widget)
      elif shape_type == "star":
        self.star_shapes.append(shape_widget)

  ############################################################################

  def onUpdate(self, updinfo):
    # Animate all triangles and stars with rotation
    import math
    time = updinfo.absolutetime

    # Animate triangles
    for idx, shape in enumerate(self.triangle_shapes):
      # Each triangle rotates at a different speed
      rotation_speed = 0.5 + (idx * 0.2)  # Different speeds for variety
      shape.shape_param = time * rotation_speed

    # Animate stars (slower rotation for better visibility)
    for idx, shape in enumerate(self.star_shapes):
      # Stars rotate slower and in alternating directions
      rotation_speed = 0.3 + (idx * 0.15)
      direction = 1.0 if (idx % 2 == 0) else -1.0  # Alternate directions
      shape.shape_param = time * rotation_speed * direction

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

################################################################################

def sigint_handler(signum, frame):
  print("Ctrl+C pressed, exiting...")
  exit(0)

signal.signal(signal.SIGINT, sigint_handler)

################################################################################

SDFPrimsTest().ezapp.mainThreadLoop()
