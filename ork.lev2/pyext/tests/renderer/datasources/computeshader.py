#!/usr/bin/env ork.python

################################################################################
# Compute shader water simulation
# Renders a water surface with rain drops creating ripples
# Uses 2D wave equation for realistic wave propagation
# All computation stays on the GPU (no GPU->CPU->GPU transfers)
# Copyright 1996-2025, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

from obt import template 
import math, sys, signal, time
from orkengine.core import vec3, vec4, quat, mtx4, thisdir
from orkengine.core import CrcStringProxy, lev2_pyexdir

tokens = CrcStringProxy()

lev2_pyexdir.addToSysPath()
from lev2utils.shaders import createPipeline
from lev2utils.primitives import createPointsPrimSSBO
from ork.app.application import ComponentizedApplication, ApplicationComponent
from ork.app.std_scenegraph import StandardSceneGraphComponent, StdSpotLight
from ork.app.loggerui import LoggerUIComponent

################################################################################
# Configuration constants
################################################################################

GRID_DIM = 2048             # Grid resolution (GRID_DIM x GRID_DIM points)
GRID_SCALE = 8.0            # World space size of the grid
POINT_SIZE = 4.0            # Size of rendered points
SPRING_K = 0.4              # Spring stiffness (lower = slower propagation) 
DAMPING = 0.9965            # Velocity damping (higher = waves travel further)

################################################################################
# Derived constants
################################################################################

NUMPOINTS = GRID_DIM * GRID_DIM
SIZEOF_FLOAT = 4
SIZEOF_VEC4F = 4 * SIZEOF_FLOAT

# SSBO layout (minimal - positions derived from vertex ID):
#   heights_A[NUMPOINTS]  - float (ping buffer)
#   heights_B[NUMPOINTS]  - float (pong buffer)
#   velocities[NUMPOINTS] - float (vertical velocity)
#   sim_params            - vec4 (time, frame, ping_pong, unused)

HGT_A_OFFSET = 0
HGT_B_OFFSET = HGT_A_OFFSET + NUMPOINTS * SIZEOF_FLOAT
VEL_OFFSET = HGT_B_OFFSET + NUMPOINTS * SIZEOF_FLOAT
SIM_OFFSET = VEL_OFFSET + NUMPOINTS * SIZEOF_FLOAT
SSBO_SIZE = SIM_OFFSET + SIZEOF_VEC4F

################################################################################
################################################################################
################################################################################

class WaterSimComponent(ApplicationComponent):
  
  ##############################################

  def __init__(self):
    super().__init__()
    shader_file_path = thisdir()/"_computeshader.fxv2"
    with open(str(shader_file_path), 'r') as shader_file:
      self.SHADERTEXT = shader_file.read()
      # apply template substitutions
      print(dir(template))
      self.SHADERTEXT = template.template_string(
        self.SHADERTEXT,
        {
          "GRID_DIM": str(GRID_DIM),
          "GRID_SCALE": str(GRID_SCALE),
          "SPRING_K": str(SPRING_K),
          "DAMPING": str(DAMPING),
          "NUMPOINTS": str(NUMPOINTS)
        }
      )
    self.frame = 0
    self.ping_pong = 0
    self.initialized = False

  ##############################################

  def _onGpuInit(self, ctx):

    SGC = self.app.SGC
    SG = SGC.scenegraph

    ###################################
    # Create SSBO for water simulation data
    ###################################

    self.ssbo = ctx.FXI.createShaderStorageBufferWithLength(SSBO_SIZE)

    ###################################
    # Create shader pipeline
    ###################################

    self.pipeline = createPipeline(
      app=self,
      ctx=ctx,
      shadertext=self.SHADERTEXT,
      blending=tokens.OFF,
      depthtest=tokens.LEQUALS,
      techname="tek_water_fwd",
      rendermodel="Unlit"
    )

    freestylemtl = self.pipeline.sharedMaterial

    ###################################
    # Grid params for vertex shader
    ###################################

    param_pntsize = freestylemtl.param("pointsize")
    param_mvp = freestylemtl.param("mvp")
    param_grid_scale = freestylemtl.param("grid_scale")
    param_grid_dim = freestylemtl.param("grid_dim")
    self.param_ping_pong = freestylemtl.param("ping_pong")
    self.gfx_storage_block = freestylemtl.storage("sif_water")

    ###################################
    # Get compute shaders
    ###################################

    self.cs_init = freestylemtl.computeShader("cs_init_water")
    self.cs_simulate = freestylemtl.computeShader("cs_simulate_water")

    ###################################
    # bind graphics parameters
    ###################################

    self.pipeline.bindParam(param_pntsize, float(POINT_SIZE))
    self.pipeline.bindParam(param_mvp, tokens.RCFD_Camera_MVP_Mono)
    self.pipeline.bindParam(param_grid_scale, float(GRID_SCALE))
    self.pipeline.bindParam(param_grid_dim, int(GRID_DIM))
    self.pipeline.bindParam(self.param_ping_pong, lambda: int(self.ping_pong))
    self.pipeline.bindStorage(self.gfx_storage_block, self.ssbo)

    ###################################
    # Create points primitive using SSBO
    ###################################

    self.points_prim = createPointsPrimSSBO(ctx=ctx, numpoints=NUMPOINTS, ssbo=self.ssbo)
    self.points_node = self.points_prim.createNode("water_points", SGC.layer_fwd, self.pipeline)
    self.points_node.sortkey = 2
    self.points_node.worldTransform.translation = vec3(0, 0, 0)
    self.points_node.worldTransform.orientation = quat(vec3(0, 1, 0), 0)
    self.points_node.worldTransform.scale = 1.0

  ##############################################

  def _onGpuUpdate(self, ctx):

    self.frame += 1
    time_val = self.frame / 60.0

    CI = ctx.CI
    FXI = ctx.FXI
    num_workgroups = (NUMPOINTS + 63) // 64  # Round up to cover all points

    # Write sim_params to SSBO: [time, frame, seed, ping_pong]
    sim_params = [time_val, float(self.frame), 12345.0, float(self.ping_pong)]
    FXI.copyDataIntoShaderStorageBuffer(sim_params, self.ssbo, SIM_OFFSET)

    # Begin dispatch phase (uses dedicated compute command buffer)
    CI.beginDispatchPhase()

    # Initialize water surface on first frame
    if not self.initialized:
      CI.bindStorageBuffer(self.cs_init, 0, self.ssbo)
      CI.dispatch(self.cs_init, num_workgroups, 1, 1)
      self.initialized = True
      print("Initialized water surface via compute shader")

    # Run water simulation
    CI.bindStorageBuffer(self.cs_simulate, 0, self.ssbo)
    CI.dispatch(self.cs_simulate, num_workgroups, 1, 1)

    # End dispatch phase (submits compute CB and waits for completion)
    CI.endDispatchPhase()
  

    # Toggle ping-pong for next frame
    self.ping_pong = 1 - self.ping_pong

################################################################################
################################################################################
################################################################################

class WaterSimApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    sg_params = {
      "preset": "UNLIT"
    }
    self.SGC = self.addComponent("std_scenegraph", 
                                 StandardSceneGraphComponent, 
                                 grid_variant=None,
                                 sg_params=sg_params,
                                 eye=vec3(0,12,15))
    #self.LUI = self.addComponent("loggerui", LoggerUIComponent, filter_regex=[".*"]) 
    self.WSC = self.addComponent("water_sim", 
                                 WaterSimComponent)
    self.createEzApp(name="WaterSimApp", ssaa=0, fullscreen=True, fsmouse=True)


###############################################################################
################################################################################
################################################################################

WSA = WaterSimApp()
WSA.ezapp.mainThreadLoop()
