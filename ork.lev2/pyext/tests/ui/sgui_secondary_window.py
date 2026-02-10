#!/usr/bin/env ork.python
################################################################################
# Multi-window scenegraph test: Two windows with themed viewports
# Primary: Single SceneGraphViewport with skybox
# Secondary: SceneGraphViewport + PropertySheet for scene editing
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import signal
import sys
import math

from orkengine.core import vec3, vec4, quat, mtx4, VarMap, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

# Auto-close after 5 seconds for automated testing
AUTO_CLOSE = "--auto-close" in sys.argv

################################################################################

l2exdir = (lev2.lev2exdir() / "python").normalized.as_string
if l2exdir not in sys.path:
  sys.path.append(l2exdir)
from lev2utils.cameras import setupUiCameraX
from lev2utils.primitives import createGridData, createCubePrim
from lev2utils.shaders import createPbrMaterialWithColor

################################################################################
# Panel class - matches MultiScene1Component pattern
################################################################################

class Panel:
  """
  Encapsulates a scenegraph viewport with scene, camera, and geometry.
  Matches the pattern used in MultiScene1Component/themes.py.
  """

  def __init__(self, parent, index, sgviewport, ctx, skybox_name, grid_data, cube_prim, cube_pipeline):
    """
    Args:
      parent: Parent application
      index: Panel index (for unique naming)
      sgviewport: SceneGraphViewport widget (not a layout item)
      ctx: GPU context
      skybox_name: Name of skybox texture
      grid_data: Shared grid drawable data
      cube_prim: Shared cube primitive
      cube_pipeline: Shared cube pipeline
    """
    self.parent = parent
    self.index = index
    self.camname = f"Camera{index}"
    self.sgviewport = sgviewport
    self.autocam = False

    # Scene parameters matching themes.py pattern
    sg_params = VarMap()
    sg_params.SkyboxIntensity = 1.0
    sg_params.DiffuseIntensity = 1.0
    sg_params.SpecularIntensity = 1.0
    sg_params.AmbientLevel = vec3(0.0)
    sg_params.preset = "ForwardPBR"
    sg_params.ssaa = 4  # 4x4 SuperSample AntiAliasing
    sg_params.SkyboxTexPathStr = skybox_name

    # Create scenegraph and layer
    self.scenegraph = lev2.scenegraph.Scene(sg_params)
    self.layer = self.scenegraph.createLayer("std_forward")

    # Add shared geometry
    self.grid_node = self.layer.createDrawableNodeFromData("grid", grid_data)
    self.grid_node.sortkey = 0

    self.cube_node = cube_prim.createNode(f"cube{index}", self.layer, cube_pipeline)
    self.cube_node.sortkey = 1

    # Camera setup
    self.cameralut = lev2.CameraDataLut()
    self.camera, self.uicam = setupUiCameraX(
      cameralut=self.cameralut,
      camname=self.camname
    )

    # Autocam state
    self.cur_eye = vec3(5, 3, 5)
    self.cur_tgt = vec3(0, 0, 0)

    # Set initial camera position
    self.uicam.lookAt(self.cur_eye, self.cur_tgt, vec3(0, 1, 0))

    # Attach to viewport widget
    self.sgviewport.cameraName = self.camname
    self.sgviewport.scenegraph = self.scenegraph
    self.sgviewport.forkDB()

    # Initialize lighting
    self.scenegraph.lightingmanager.gpuInit(ctx)

  def setEventHandler(self, handler):
    """Set UI event handler for this panel's widget."""
    self.sgviewport.evhandler = handler
    self.sgviewport.ignoreEvents = False

  def update(self, abstime, deltatime):
    """Update scene for this frame."""
    # Animate cube
    y = math.sin(abstime * (self.index + 1) * 0.5) * 0.5
    q = quat(vec3(0, 1, 0), abstime * (self.index + 1) * 0.3)
    self.cube_node.worldTransform.translation = vec3(0, y + 0.5, 0)
    self.cube_node.worldTransform.orientation = q

    # Update camera and scene
    self.camera.copyFrom(self.uicam.cameradata)
    self.scenegraph.updateScene(self.cameralut)

  def setDirty(self):
    """Mark viewport for redraw."""
    self.sgviewport.setDirty()

################################################################################
# PropertySheet factory functions
################################################################################

def create_color_inline_factory(propsheet):
  """Factory for ColorSwatch inline widgets."""
  def factory(sheet, key, value, annotations):
    swatch = lev2.ui.ColorSwatch.wfactory(["swatch_" + key])
    if value is not None:
      swatch.color = value
    swatch.show_hex = True

    def on_click(sw):
      propsheet.requestDetailEditor(key)
    swatch.onClick = on_click

    return swatch
  return factory


def create_color_detail_factory(propsheet):
  """Factory for ColorPicker detail widgets."""
  from ork.ui.color_picker import ColorPicker

  def factory(sheet, key, value, annotations, binding):
    bg_color = vec3(0.15, 0.15, 0.18)
    initial_color = value if value is not None else vec4(0.5, 0.5, 0.5, 1.0)
    picker_widget = ColorPicker.wfactory(["picker_" + key, bg_color, initial_color])

    picker = picker_widget.uservars.color_picker

    def on_commit(color):
      binding["onValueCommit"](color)
    picker.onCommit = on_commit

    def on_cancel():
      binding["onCancel"]()
    picker.onCancel = on_cancel

    return picker_widget
  return factory

################################################################################
# Main Application
################################################################################

class DualSceneGraphWindow:

  def __init__(self):
    super().__init__()

    self.win_width = 640
    self.win_height = 600

    self.ezapp = lev2.OrkEzApp.create(
      self,
      name="DualSceneGraph::Primary",
      width=self.win_width,
      height=self.win_height,
      left=100,
      top=100,
      fullscreen=True,
      #fullscreen_monitor="W2361",
      use_subsystems=['opq', 'core', 'gpu', 'lev2']
    )
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    # Primary window: SceneGraphViewport in layout
    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    self.primary_griditem = lg.makeChild(
      uiclass=lev2.ui.SceneGraphViewport,
      args=["PrimarySG"],
      fill=True
    )

    self.secondary_win = None
    self.secondary_sgviewport = None
    self.propsheet = None
    self.frame_count = 0

    # Panels (set up in onGpuInit)
    self.panel1 = None
    self.panel2 = None

    # Shared geometry (set up in onGpuInit)
    self.grid_data = None
    self.cube_prim = None
    self.cube_pipeline = None

    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def _create_shared_geometry(self, ctx):
    """Create geometry shared between all panels."""
    self.grid_data = createGridData()

    self.cube_prim = createCubePrim(ctx=ctx, size=1.5)

    self.cube_mtl = createPbrMaterialWithColor(
      ctx=ctx,
      color=vec4(0.8, 0.6, 0.4, 1),
      roughness=0.5,
      metallic=0.2
    )

    permu = lev2.FxPipelinePermutation(rendermodel="FORWARD_PBR")
    self.cube_pipeline = self.cube_mtl.fxcache.findPipeline(permu)

  ##############################################

  def _setup_theme_engine(self, uicontext):
    """Set up theme engine for UI rendering."""
    self.base_db = lev2.ui.createDefaultStyleDatabase()
    self.custom_db = lev2.ui.StyleDatabase.createChild(self.base_db)
    custom_theme = lev2.ui.ThemeEngine(self.custom_db)
    uicontext.theme_engine = custom_theme

  ##############################################

  def _setup_primary_window(self, ctx):
    """Set up the primary window with a Panel."""
    # primary_griditem is a layout item from makeChild, access .widget
    self.panel1 = Panel(
      parent=self,
      index=0,
      sgviewport=self.primary_griditem.widget,
      ctx=ctx,
      skybox_name="arena",
      grid_data=self.grid_data,
      cube_prim=self.cube_prim,
      cube_pipeline=self.cube_pipeline
    )
    self.panel1.setEventHandler(lambda e: self._onPanelUiEvent(self.panel1, e))

    print("Primary window with Panel created")

  ##############################################

  def _setup_secondary_window(self, ctx):
    """Set up the secondary window with Panel + PropertySheet."""
    # Create secondary window positioned beside primary
    sec_x = 100 + self.win_width + 20
    self.secondary_win = self.ezapp.createSecondaryWindow(
      width=self.win_width,
      height=self.win_height,
      x=sec_x,
      y=100,
      title="DualSceneGraph::Secondary",
      decorated=True,
      resizable=True,
      floating=True
    )

    # Set up layout for secondary window
    uic = self.secondary_win.ui_context
    win_w = self.secondary_win.width
    win_h = self.secondary_win.height

    # Create root layout group
    root = lev2.ui.LayoutGroup.create("sec_lg")
    root.setRect(0, 0, win_w, win_h)
    uic.top = root
    root.margin = 4

    # Create vertical pack for SceneGraphViewport + PropertySheet
    vpack_lg = root.makeChild(
      uiclass=lev2.ui.VerticalPack,
      args=["sec_vpack"],
      fill=True
    )
    vpack = vpack_lg.widget
    vpack.margin = 4
    vpack.fill = True

    # Add SceneGraphViewport (fixed height)
    # vpack.makeChild returns the widget directly (not a layout item)
    self.secondary_sgviewport = vpack.makeChild(
      uiclass=lev2.ui.SceneGraphViewport,
      args=["SecondarySG", vec4(1, 0, 1, 1)]
    )
    self.secondary_sgviewport.fixed_height = 320

    # Create Panel for secondary viewport
    self.panel2 = Panel(
      parent=self,
      index=1,
      sgviewport=self.secondary_sgviewport,
      ctx=ctx,
      skybox_name="futcity",
      grid_data=self.grid_data,
      cube_prim=self.cube_prim,
      cube_pipeline=self.cube_pipeline
    )
    self.panel2.setEventHandler(lambda e: self._onPanelUiEvent(self.panel2, e))

    # Add PropertySheet (fixed height at bottom)
    self.propsheet = vpack.makeChild(uiclass=lev2.ui.PropertySheet, args=["propsheet"])
    self.propsheet.fixed_height = 250

    # Register color editor factory
    self.propsheet.registerEditorFactory(
      tokens.Color,
      create_color_inline_factory(self.propsheet),
      create_color_detail_factory(self.propsheet)
    )

    # Set property sheet data
    self.propsheet.data = self._build_property_data()

    # Set annotations for color properties
    model = self.propsheet.model
    diffuse_annot = VarMap()
    diffuse_annot.type = tokens.Color
    model.setAnnotations("Material/diffuse_color", diffuse_annot)

    emissive_annot = VarMap()
    emissive_annot.type = tokens.Color
    model.setAnnotations("Material/emissive_color", emissive_annot)

    self.propsheet.expandAll()

    # Property change callback
    def on_property_changed(key, value):
      print(f"Property changed: {key} = {value}")
      self._apply_property_change(key, value)

    self.propsheet.onPropertyChanged(on_property_changed)

    # Style property sheet
    self.propsheet.bgcolor = vec4(0.12, 0.12, 0.14, 1)
    self.propsheet.label_color = vec4(0.9, 0.9, 0.9, 1)
    self.propsheet.group_color = vec4(0.18, 0.18, 0.22, 1)
    self.propsheet.row_height = 26
    self.propsheet.label_width = 120

    print("Secondary window with Panel + PropertySheet created")

  ##############################################

  def _build_property_data(self):
    """Build property data for the PropertySheet."""
    data = VarMap()

    # Transform group
    transform = VarMap()
    transform.position_x = 0.0
    transform.position_y = 0.5
    transform.position_z = 0.0
    transform.rotation_y = 0.0
    transform.scale = 1.0
    data.Transform = transform

    # Material group
    material = VarMap()
    material.diffuse_color = vec4(0.8, 0.6, 0.4, 1.0)
    material.emissive_color = vec4(0.0, 0.0, 0.0, 1.0)
    material.metallic = 0.2
    material.roughness = 0.5
    data.Material = material

    # Scene group
    scene = VarMap()
    scene.cube_speed = 1.0
    scene.grid_visible = True
    data.Scene = scene

    return data

  ##############################################

  def _apply_property_change(self, key, value):
    """Apply property changes to the scene."""
    # Could wire these to actually modify panel2's scene
    # For now just print - extend as needed
    pass

  ##############################################

  def _onPanelUiEvent(self, panel, uievent):
    """Handle UI events for a panel (camera control)."""
    if panel.uicam is None:
      return lev2.ui.HandlerResult()
    handled = panel.uicam.uiEventHandler(uievent)
    if handled:
      panel.uicam.updateMatrices()
      panel.camera.copyFrom(panel.uicam.cameradata)
    return lev2.ui.HandlerResult()

  ##############################################

  def onGpuInit(self, ctx):
    print("GPU init")

    # Set up theme engine for primary window
    self._setup_theme_engine(self.ezapp.uicontext)

    # Create shared geometry
    self._create_shared_geometry(ctx)

    # Set up windows
    self._setup_primary_window(ctx)
    self._setup_secondary_window(ctx)

  ##############################################

  def onUpdate(self, updinfo):
    self.frame_count += 1
    abstime = updinfo.absolutetime
    deltatime = updinfo.deltatime

    # Update panels
    if self.panel1:
      self.panel1.update(abstime, deltatime)
      self.panel1.setDirty()

    if self.panel2:
      self.panel2.update(abstime, deltatime)
      self.panel2.setDirty()

    # Auto-close after 5 seconds for automated testing
    if AUTO_CLOSE and self.frame_count > 300:
      print("Test complete - closing")
      self.ezapp.signalExit()

  ##############################################

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

################################################################################

app = DualSceneGraphWindow()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
print("Dual scenegraph window test passed!")
