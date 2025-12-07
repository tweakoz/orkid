#!/usr/bin/env ork.python

################################################################################
# Compute shader visual test
# Renders a grid of points whose colors are animated by a compute shader
# All computation stays on the GPU (no GPU->CPU->GPU transfers)
# Copyright 1996-2025, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, signal
from orkengine.core import vec3, vec4, quat, mtx4, Transform, VarMap
from orkengine.core import CrcStringProxy, lev2_pyexdir
from orkengine import lev2
from orkengine.lev2 import scenegraph

tokens = CrcStringProxy()

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera, setupUiCameraX
from lev2utils.shaders import createPipeline
from lev2utils.primitives import createPointsPrimSSBO

################################################################################

# Grid dimensions for point cloud
DIM = 128
NUMPOINTS = DIM * DIM
SIZEOF_FLOAT = 4
SIZEOF_VEC4F = 4 * SIZEOF_FLOAT

# SSBO layout: positions (vec4 * NUMPOINTS) + colors (vec4 * NUMPOINTS) + time (float)
POS_OFFSET = 0
COL_OFFSET = POS_OFFSET + NUMPOINTS * SIZEOF_VEC4F
TIM_OFFSET = COL_OFFSET + NUMPOINTS * SIZEOF_VEC4F
SSBO_SIZE = TIM_OFFSET + SIZEOF_FLOAT

################################################################################
# Shader source with compute shader
################################################################################

SHADERTEXT = f"""
////////////////////////////////////////
fxconfig fxcfg_default {{}}
////////////////////////////////////////
uniform_set ublock_vtx {{
  mat4 mvp;
  float pointsize;
}}
////////////////////////////////////////
uniform_set ublock_frg {{
  vec4 modcolor;
}}
////////////////////////////////////////
// Storage interface for compute shader data
storage_interface sif_points (descriptor_set 0) {{
  buffer layout(std430) point_data {{
    vec4 positions[{NUMPOINTS}];
    vec4 colors[{NUMPOINTS}];
    float time;
  }};
}}
////////////////////////////////////////
vertex_interface iface_vtx_points //
  : ublock_vtx    //
  : sif_points {{ //
  outputs {{
    vec3 frg_col;
  }}
}}
////////////////////////////////////////
fragment_interface iface_frg_points : ublock_frg {{
  inputs {{
    vec3 frg_col;
  }}
  outputs {{ layout(location = 0) vec4 out_clr; }}
}}
////////////////////////////////////////
vertex_shader vs_points : iface_vtx_points {{
  vec3 posv3   = positions[gl_VertexID].xyz;
  vec4 posv4   = vec4(posv3.xyz, 1);
  frg_col      = colors[gl_VertexID].xyz;
  gl_PointSize = pointsize;
  gl_Position  = mvp * posv4;
}}
////////////////////////////////////////
fragment_shader ps_points : iface_frg_points {{
  out_clr = vec4(frg_col, 1);
}}
////////////////////////////////////////
technique tek_points_fwd {{
  fxconfig = fxcfg_default;
  pass p0 {{
    vertex_shader   = vs_points;
    fragment_shader = ps_points;
    state_block     = default;
  }}
}}
////////////////////////////////////////
// Compute interface referencing the storage
compute_interface iface_compute {{
  storage {{ sif_points }}
  inputs {{
    layout(local_size_x = 64, local_size_y = 1, local_size_z = 1);
  }}
}}
////////////////////////////////////////
// Compute shader that initializes and animates points
compute_shader cs_init_points : iface_compute {{
  int index = int(gl_GlobalInvocationID.x);
  if (index >= {NUMPOINTS}) return;

  // Convert linear index to 2D grid coordinates
  int x = index % {DIM};
  int y = index / {DIM};

  // Normalize to [-1, 1] range
  float fx = (float(x) / float({DIM}) - 0.5) * 2.0;
  float fy = (float(y) / float({DIM}) - 0.5) * 2.0;

  // Set position in a flat grid pattern, scaled up
  positions[index] = vec4(fx * 5.0, 0.0, fy * 5.0, 1.0);

  // Set initial color based on position
  colors[index] = vec4(fx * 0.5 + 0.5, 0.0, fy * 0.5 + 0.5, 1.0);
}}
////////////////////////////////////////
// Compute shader that animates points over time
compute_shader cs_animate_points : iface_compute {{
  int index = int(gl_GlobalInvocationID.x);
  if (index >= {NUMPOINTS}) return;

  // Get current position
  vec4 pos = positions[index];

  // Convert linear index to 2D grid coordinates
  int x = index % {DIM};
  int y = index / {DIM};
  float fx = float(x) / float({DIM});
  float fy = float(y) / float({DIM});

  // Animate Y position with a wave pattern
  float wave1 = sin(fx * PI2 * 2.0 + time * 2.0) * 0.5;
  float wave2 = sin(fy * PI2 * 3.0 + time * 1.5) * 0.3;
  pos.y = wave1 + wave2;
  positions[index] = pos;

  // Animate colors based on position and time
  float r = sin(fx * PI2 + time) * 0.5 + 0.5;
  float g = sin(fy * PI2 + time * 1.3) * 0.5 + 0.5;
  float b = sin((fx + fy) * PI + time * 0.7) * 0.5 + 0.5;
  colors[index] = vec4(r, g, b, 1.0);
}}
"""

################################################################################

class ComputeShaderApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)

    # Enable UI draw mode for onGpuPreFrame support
    self.ezapp.topWidget.enableUiDraw()

    # Create a single SceneGraphViewport
    lg_group = self.ezapp.topLayoutGroup
    self.griditems = lg_group.makeGrid(
      width=1,
      height=1,
      margin=1,
      uiclass=lev2.ui.SceneGraphViewport,
      args=["computeview", vec4(1, 0, 1, 1)]
    )

    self.materials = set()

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)
    self.frame = 0
    self.initialized = False

  ##############################################

  def onGpuInit(self, ctx):
    # Get draw buffer context from ezapp vars
    self.dbufcontext = self.ezapp.vars.dbufcontext
    self.cameralut = self.ezapp.vars.cameras

    ###################################
    # Create scenegraph with Unlit preset
    ###################################
    sg_params = VarMap()
    sg_params.preset = "UNLIT"
    sg_params.dbufcontext = self.dbufcontext

    self.scenegraph = scenegraph.Scene(sg_params)
    self.layer1 = self.scenegraph.createLayer("std_forward")

    ###################################
    # Setup camera
    ###################################
    self.camera, self.uicam = setupUiCameraX(
      cameralut=self.cameralut,
      camname="maincam",
      eye=vec3(10, 10, 10),
      tgt=vec3(0, 0, 0),
      up=vec3(0, 1, 0),
      constrainZ=True
    )

    # Assign scenegraph and camera to viewport
    self.griditems[0].widget.cameraName = "maincam"
    self.griditems[0].widget.scenegraph = self.scenegraph
    self.griditems[0].widget.evhandler = lambda ev: self.onViewportUiEvent(ev)

    ###################################
    # Create SSBO for compute shader data
    ###################################
    self.ssbo = ctx.FXI.createShaderStorageBufferWithLength(SSBO_SIZE)
    print(f"Created SSBO: {self.ssbo} with size {SSBO_SIZE} bytes")

    ###################################
    # Create shader pipeline
    ###################################
    self.pipeline = createPipeline(
      app=self,
      ctx=ctx,
      shadertext=SHADERTEXT,
      blending=tokens.OFF,
      depthtest=tokens.LEQUALS,
      techname="tek_points_fwd",
      rendermodel="Unlit"
    )

    freestylemtl = self.pipeline.sharedMaterial

    param_pntsize = freestylemtl.param("pointsize")
    param_mvp = freestylemtl.param("mvp")

    self.pipeline.bindParam(param_pntsize, float(3.0))
    self.pipeline.bindParam(param_mvp, tokens.RCFD_Camera_MVP_Mono)

    ###################################
    # Get compute shaders
    ###################################
    self.cs_init = freestylemtl.computeShader("cs_init_points")
    self.cs_animate = freestylemtl.computeShader("cs_animate_points")
    print(f"Init compute shader: {self.cs_init}")
    print(f"Animate compute shader: {self.cs_animate}")

    ###################################
    # Get storage block and bind SSBO for graphics shader
    ###################################
    self.storage_block = freestylemtl.storage("sif_points")
    print(f"Storage block: {self.storage_block}")
    ctx.FXI.bindStorageBuffer(self.storage_block, self.ssbo)

    ###################################
    # Create points primitive using SSBO
    ###################################
    self.points_prim = createPointsPrimSSBO(ctx=ctx, numpoints=NUMPOINTS, ssbo=self.ssbo)
    self.points_node = self.points_prim.createNode("points", self.layer1, self.pipeline)
    self.points_node.sortkey = 2

  ##############################################

  def onUpdate(self, updinfo):
    self.scenegraph.updateScene(self.cameralut)
    for g in self.griditems:
      g.widget.setDirty()

  ##############################################

  def onViewportUiEvent(self, uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.uicam.updateMatrices()
      self.camera.copyFrom(self.uicam.cameradata)
    return lev2.ui.HandlerResult()

  ##############################################

  def onGpuPreFrame(self, ctx):
    self.frame += 1
    time_val = self.frame / 60.0

    CI = ctx.CI
    num_workgroups = (NUMPOINTS + 63) // 64  # Round up to cover all points

    # Begin dispatch phase (suspends render pass if active)
    CI.beginDispatchPhase()

    # Initialize points on first frame
    if not self.initialized:
      CI.bindStorageBuffer(self.cs_init, 0, self.ssbo)
      CI.dispatch(self.cs_init, num_workgroups, 1, 1)
      self.initialized = True
      print("Initialized point positions and colors via compute shader")

    # Animate points with compute shader
    CI.bindStorageBuffer(self.cs_animate, 0, self.ssbo)
    CI.dispatch(self.cs_animate, num_workgroups, 1, 1)

    # End dispatch phase (resumes render pass if suspended)
    CI.endDispatchPhase()

###############################################################################

def onRunLoopIteration():
  pass

ComputeShaderApp().ezapp.mainThreadLoop(on_iter=onRunLoopIteration)
