#!/usr/bin/env ork.python

################################################################################
# Scene Editor Mockup Test
# Layout using lg.split() for draggable anchor guides:
#   - Main hsplit: left panel (25%) | viewport (75%)
#   - Left vsplit: outliner (40%) / property sheet (60%)
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import math, sys, signal, random
from orkengine.core import vec2, vec3, vec4, quat, VarMap, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

l2exdir = (lev2.lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir)
from lev2utils.cameras import setupUiCameraX
from lev2utils.shaders import createPipeline
from lev2utils.primitives import createGridData, createCubePrim

################################################################################

class SceneEditorTest:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, width=1280, height=720, fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    lg.clearColorStd = vec4(0.15, 0.15, 0.15, 1)
    lg.clearColorGuide = vec4(0.4, 0.4, 0.2, 1)  # Yellow-ish guides

    ############################################
    # Start with viewport filling whole area
    ############################################

    viewport_items = lg.makeGrid(
      width=1, height=1,
      margin=2,
      uiclass=lev2.ui.SceneGraphViewport,
      args=["viewport", vec4(0.1, 0.1, 0.12, 1)]
    )
    self.sgv = viewport_items[0].widget

    ############################################
    # Split LEFT from viewport to create outliner
    # (25% left for outliner, 75% right for viewport)
    ############################################

    outliner_item = lg.split(
      layout=viewport_items[0].layout,
      proportion=0.25,
      placement=tokens.LEFT,
      margin=2,
      uiclass=lev2.ui.Outliner,
      args=["outliner"]
    )
    self.outliner = outliner_item.widget

    ############################################
    # Split BOTTOM from outliner to create property sheet
    # (60% bottom for propsheet, 40% top for outliner)
    ############################################

    propsheet_item = lg.split(
      layout=outliner_item.layout,
      proportion=0.6,
      placement=tokens.BOTTOM,
      margin=2,
      uiclass=lev2.ui.PropertySheet,
      args=["propsheet"]
    )
    self.propsheet = propsheet_item.widget

    ############################################
    # Setup outliner data (mock scene hierarchy)
    ############################################

    self._setupOutliner()
    self._setupPropertySheet()

    ############################################

    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def _setupOutliner(self):
    """Setup mock scene hierarchy in outliner."""
    scene_data = VarMap()

    # Scene root
    root = VarMap()

    # Cameras
    cameras = VarMap()
    cameras.MainCamera = "PerspectiveCamera"
    cameras.TopCamera = "OrthographicCamera"
    root.Cameras = cameras

    # Lights
    lights = VarMap()
    lights.DirectionalLight = "Sun"
    lights.PointLight1 = "Fill"
    lights.PointLight2 = "Rim"
    root.Lights = lights

    # Objects
    objects = VarMap()

    player = VarMap()
    player.Mesh = "player_mesh"
    player.Material = "player_mat"
    player.Collider = "capsule"
    objects.Player = player

    ground = VarMap()
    ground.Mesh = "ground_mesh"
    ground.Material = "ground_mat"
    objects.Ground = ground

    props = VarMap()
    props.Tree1 = "tree_prefab"
    props.Tree2 = "tree_prefab"
    props.Rock1 = "rock_prefab"
    objects.Props = props

    root.Objects = objects

    scene_data.Scene = root

    self.outliner.data = scene_data
    self.outliner.model.allow_rename = True
    self.outliner.model.allow_delete = True
    self.outliner.model.allow_add = True
    self.outliner.model.allow_multiselect = True
    self.outliner.expandAll()

    # Selection callback - update property sheet when selection changes
    def on_select(key):
      print(f"Selected: {key}")
      self._updatePropertySheetForSelection(key)

    self.outliner.onSelect(on_select)

    # Style
    self.outliner.bgcolor = vec4(0.12, 0.12, 0.14, 1)
    self.outliner.text_color = vec4(0.9, 0.9, 0.9, 1)
    self.outliner.selected_color = vec4(0.2, 0.4, 0.6, 1)
    self.outliner.hover_color = vec4(0.2, 0.2, 0.25, 1)
    self.outliner.item_height = 22

  ##############################################

  def _setupPropertySheet(self):
    """Setup property sheet with default data."""
    data = VarMap()

    transform = VarMap()
    transform.position_x = 0.0
    transform.position_y = 0.0
    transform.position_z = 0.0
    transform.rotation_x = 0.0
    transform.rotation_y = 0.0
    transform.rotation_z = 0.0
    transform.scale_x = 1.0
    transform.scale_y = 1.0
    transform.scale_z = 1.0
    data.Transform = transform

    data.Name = "Selected Object"
    data.Visible = True
    data.Layer = "Default"

    self.propsheet.data = data
    self.propsheet.expandAll()

    def on_property_changed(key, value):
      print(f"Property changed: {key} = {value}")

    self.propsheet.onPropertyChanged(on_property_changed)

    # Style
    self.propsheet.bgcolor = vec4(0.12, 0.12, 0.14, 1)
    self.propsheet.label_color = vec4(0.85, 0.85, 0.85, 1)
    self.propsheet.group_color = vec4(0.16, 0.16, 0.2, 1)
    self.propsheet.row_height = 26
    self.propsheet.label_width = 100

  ##############################################

  def _updatePropertySheetForSelection(self, key):
    """Update property sheet based on outliner selection."""
    data = VarMap()

    transform = VarMap()
    transform.position_x = random.uniform(-10, 10)
    transform.position_y = random.uniform(0, 5)
    transform.position_z = random.uniform(-10, 10)
    transform.rotation_x = 0.0
    transform.rotation_y = random.uniform(0, 360)
    transform.rotation_z = 0.0
    transform.scale_x = 1.0
    transform.scale_y = 1.0
    transform.scale_z = 1.0
    data.Transform = transform

    # Extract name from key path
    name = key.split("/")[-1] if "/" in key else key
    data.Name = name
    data.Visible = True
    data.Layer = "Default"

    self.propsheet.data = data
    self.propsheet.expandAll()

  ##############################################

  def onGpuInit(self, ctx):
    # Setup theme engine
    self.uicontext = self.ezapp.uicontext
    self.base_db = lev2.ui.createDefaultStyleDatabase()
    self.custom_db = lev2.ui.StyleDatabase.createChild(self.base_db)
    custom_theme = lev2.ui.ThemeEngine(self.custom_db)
    self.uicontext.theme_engine = custom_theme

    ############################################
    # Setup scenegraph
    ############################################

    sg_params = VarMap()
    sg_params.SkyboxIntensity = 2.0
    sg_params.DiffuseIntensity = 1.0
    sg_params.SpecularIntensity = 1.0
    sg_params.AmbientLevel = vec3(0.15)
    sg_params.preset = "ForwardPBR"
    sg_params.ssaa = 4
    sg_params.enable_skybox = False
    sg_params.clearcolor = vec3(0.08, 0.08, 0.1)

    self.scenegraph = lev2.scenegraph.Scene(sg_params)
    self.layer = self.scenegraph.createLayer("std_forward")

    # Grid
    self.grid_data = createGridData()
    self.grid_node = self.layer.createDrawableNodeFromData("grid", self.grid_data)
    self.grid_node.sortkey = 1

    # Cube
    cube_prim = createCubePrim(ctx=ctx, size=1.0)
    pipeline_cube = createPipeline(app=self, ctx=ctx, rendermodel="FORWARD_PBR", techname="std_mono_fwd")
    self.cube_node = cube_prim.createNode("cube", self.layer, pipeline_cube)
    self.cube_node.worldTransform.translation = vec3(0, 0.5, 0)

    ############################################
    # Setup camera
    ############################################

    self.camname = "EditorCamera"
    self.cameralut = lev2.CameraDataLut()
    self.camera, self.uicam = setupUiCameraX(cameralut=self.cameralut, camname=self.camname)

    self.uicam.distance = 1
    self.uicam.lookAt(vec3(5, 4, 5), vec3(0, 0.5, 0), vec3(0, 1, 0))
    self.uicam.updateMatrices()
    self.camera.copyFrom(self.uicam.cameradata)

    ############################################
    # Assign scenegraph to viewport
    ############################################

    self.sgv.cameraName = self.camname
    self.sgv.scenegraph = self.scenegraph
    self.sgv.camera_evhandler = lambda ev: self._onCameraEvent(ev)
    self.sgv.forkDB()
    self.scenegraph.lightingmanager.gpuInit(ctx)

  ##############################################

  def _onCameraEvent(self, uievent):
    """Handle camera manipulation events (orbit, pan, zoom)."""
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.uicam.updateMatrices()
      self.camera.copyFrom(self.uicam.cameradata)
    return lev2.ui.HandlerResult()

  ##############################################

  def onUpdate(self, updinfo):
    abstime = updinfo.absolutetime

    # Update scenegraph
    self.scenegraph.updateScene(self.cameralut)
    self.sgv.setDirty()

  ##############################################

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

################################################################################

SceneEditorTest().ezapp.mainThreadLoop()
