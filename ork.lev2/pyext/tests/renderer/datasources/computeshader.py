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

import math, sys, signal, time
from orkengine.core import vec3, vec4, quat, mtx4, Transform, VarMap
from orkengine.core import CrcStringProxy, lev2_pyexdir
from orkengine import lev2
from orkengine.lev2 import scenegraph

tokens = CrcStringProxy()

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera, setupUiCameraX
from lev2utils.shaders import createPipeline
from lev2utils.primitives import createPointsPrimSSBO
from ork.app.application import ComponentizedApplication, ApplicationComponent
from ork.app.std_scenegraph import StandardSceneGraphComponent, StdSpotLight
from ork.app.loggerui import LoggerUIComponent

################################################################################
# Configuration constants
################################################################################

GRID_DIM = 2048              # Grid resolution (GRID_DIM x GRID_DIM points)
GRID_SCALE = 8.0            # World space size of the grid
POINT_SIZE = 4.0            # Size of rendered points

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

class WaterSimComponent(ApplicationComponent):
  
  ##############################################

  def __init__(self, shadertext=None):
    super().__init__()
    self.SHADERTEXT = shadertext
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

    # Update vertex shader ping_pong uniform (reads opposite buffer from compute)

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

  def __init__(self,SHADERTEXT):
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
                                 WaterSimComponent, 
                                 shadertext = SHADERTEXT)
    self.createEzApp(name="WaterSimApp", ssaa=0, fullscreen=True, fsmouse=True)


###############################################################################

SHADERTEXT = f"""
////////////////////////////////////////
import "orkshader://mathtools.i2";
////////////////////////////////////////
fxconfig fxcfg_default {{}}
////////////////////////////////////////
uniform_block ublock_vtx (descriptor_set 0) {{
  mat4 mvp;
  float pointsize;
  float grid_scale;
  int grid_dim;
  int ping_pong;  // 0 = read A, 1 = read B
}}
////////////////////////////////////////
// Storage interface - minimal, positions derived from vertex ID
storage_interface sif_water (descriptor_set 0) {{
  buffer layout(std430) water_data {{
    float heights_A[{NUMPOINTS}];   // ping buffer
    float heights_B[{NUMPOINTS}];   // pong buffer
    float velocities[{NUMPOINTS}];
    vec4 sim_params;  // x=time, y=frame, z=seed, w=ping_pong
  }};
}}
////////////////////////////////////////
vertex_interface iface_vtx_water //
  : ublock_vtx    //
  : sif_water {{ //
  outputs {{
    vec3 frg_normal;
    vec3 frg_worldpos;
    float frg_height;
  }}
}}
////////////////////////////////////////
fragment_interface iface_frg_water {{
  inputs {{
    vec3 frg_normal;
    vec3 frg_worldpos;
    float frg_height;
  }}
  outputs {{ layout(location = 0) vec4 out_clr; }}
}}
////////////////////////////////////////
vertex_shader vs_water : iface_vtx_water {{
  // Derive grid position from vertex ID
  int xi = gl_VertexID % grid_dim;
  int yi = gl_VertexID / grid_dim;
  float fx = (float(xi) / float(grid_dim - 1) - 0.5) * 2.0;
  float fy = (float(yi) / float(grid_dim - 1) - 0.5) * 2.0;

  // Read height from appropriate buffer (render uses latest written)
  float height = (ping_pong == 0) ? heights_B[gl_VertexID] : heights_A[gl_VertexID];
  // Guard against bad height values
  if (isnan(height) || isinf(height)) height = 0.0;
  height = clamp(height, -100.0, 100.0);

  // Sample neighbor heights for normal computation
  int left_idx  = (xi > 0) ? gl_VertexID - 1 : gl_VertexID;
  int right_idx = (xi < grid_dim - 1) ? gl_VertexID + 1 : gl_VertexID;
  int up_idx    = (yi > 0) ? gl_VertexID - grid_dim : gl_VertexID;
  int down_idx  = (yi < grid_dim - 1) ? gl_VertexID + grid_dim : gl_VertexID;

  float h_left  = (ping_pong == 0) ? heights_B[left_idx]  : heights_A[left_idx];
  float h_right = (ping_pong == 0) ? heights_B[right_idx] : heights_A[right_idx];
  float h_up    = (ping_pong == 0) ? heights_B[up_idx]    : heights_A[up_idx];
  float h_down  = (ping_pong == 0) ? heights_B[down_idx]  : heights_A[down_idx];

  // Compute normal from height gradient (with clamping to prevent numerical issues)
  float grid_spacing = (grid_scale * 2.0) / float(grid_dim - 1);
  float dhdx = clamp((h_right - h_left) / (2.0 * grid_spacing), -10.0, 10.0);
  float dhdz = clamp((h_down - h_up) / (2.0 * grid_spacing), -10.0, 10.0);
  vec3 normal = normalize(vec3(-dhdx, 1.0, -dhdz));

  vec3 pos = vec3(fx * grid_scale, height, fy * grid_scale);
  gl_PointSize = pointsize;
  gl_Position = mvp * vec4(pos, 1.0);

  frg_normal = normal;
  frg_worldpos = pos;
  frg_height = height;
}}
////////////////////////////////////////
fragment_shader ps_water : iface_frg_water {{
  // Bias normal from [-1,1] to [0,1] range for visualization
  vec3 normal_color = frg_normal * 0.5 + 0.5;
  out_clr = vec4(normal_color, 1.0);
}}
////////////////////////////////////////
technique tek_water_fwd {{
  fxconfig = fxcfg_default;
  pass p0 {{
    vertex_shader   = vs_water;
    fragment_shader = ps_water;
    state_block     = default;
  }}
}}
////////////////////////////////////////
// Compute interface for water simulation
compute_interface iface_compute {{
  storage {{ sif_water }}
  inputs {{
    layout(local_size_x = 64, local_size_y = 1, local_size_z = 1);
  }}
}}
////////////////////////////////////////
// Initialize water surface to flat
compute_shader cs_init_water : iface_compute {{
  int index = int(gl_GlobalInvocationID.x);
  if (index >= {NUMPOINTS}) return;

  heights_A[index] = 0.0;
  heights_B[index] = 0.0;
  velocities[index] = 0.0;
}}
////////////////////////////////////////
// Damped 2D spring mesh simulation with rain drops
// Reads from one height buffer, writes to the other (ping-pong)
compute_shader cs_simulate_water : iface_compute : lib_math {{
  int index = int(gl_GlobalInvocationID.x);
  if (index >= {NUMPOINTS}) return;

  int xi = index % {GRID_DIM};
  int yi = index / {GRID_DIM};

  float time = sim_params.x;
  float frame = sim_params.y;
  float seed = sim_params.z;
  int ping_pong = int(sim_params.w);

  // Spring mesh parameters
  float spring_k = 0.4;      // Spring stiffness (lower = slower propagation)
  float damping = 0.9965;       // Velocity damping (higher = waves travel further)
  float dt = 1.0;

  // Read from current buffer (ping_pong: 0=read A, 1=read B)
  float height = (ping_pong == 0) ? heights_A[index] : heights_B[index];
  float vel = velocities[index];

  // Sample neighbor heights (reflect at boundaries for wave bounce)
  int left  = (xi > 0) ? index - 1 : index + 1;
  int right = (xi < {GRID_DIM} - 1) ? index + 1 : index - 1;
  int up    = (yi > 0) ? index - {GRID_DIM} : index + {GRID_DIM};
  int down  = (yi < {GRID_DIM} - 1) ? index + {GRID_DIM} : index - {GRID_DIM};

  float h_left  = (ping_pong == 0) ? heights_A[left]  : heights_B[left];
  float h_right = (ping_pong == 0) ? heights_A[right] : heights_B[right];
  float h_up    = (ping_pong == 0) ? heights_A[up]    : heights_B[up];
  float h_down  = (ping_pong == 0) ? heights_A[down]  : heights_B[down];

  // Spring force from 4 neighbors (each pulls toward neighbor height)
  float force = (h_left - height) + (h_right - height) + (h_up - height) + (h_down - height);
  force *= spring_k;

  // Update velocity with spring force
  vel += force * dt;

  // Apply damping
  vel *= damping;

  // Rain drop spawning - deterministic based on frame
  for (int drop = 0; drop < 3; drop++) {{
    float drop_hash = hash1(frame * 7.0 + float(drop) * 13.0 + seed);

    if (drop_hash < 0.015) {{
      float dx = hash1(frame * 11.0 + float(drop) * 17.0 + seed);
      float dy = hash1(frame * 23.0 + float(drop) * 31.0 + seed);

      int drop_x = int(dx * float({GRID_DIM}));
      int drop_y = int(dy * float({GRID_DIM}));

      int dist_x = abs(xi - drop_x);
      int dist_y = abs(yi - drop_y);
      float dist = sqrt(float(dist_x * dist_x + dist_y * dist_y));

      if (dist < 24.0) {{
        float splash = (1.0 - dist / 24.0) * 0.1;
        vel += splash;  // Push up (raindrop splash from above)
      }}
    }}
  }}

  // Update height
  height += vel * dt;

  // Clamp to prevent numerical instability
  height = clamp(height, -100.0, 100.0);
  vel = clamp(vel, -1.0, 1.0);

  // Write to opposite buffer
  if (ping_pong == 0) {{
    heights_B[index] = height;
  }} else {{
    heights_A[index] = height;
  }}
  velocities[index] = vel;
}}
"""

WSA = WaterSimApp(SHADERTEXT)
WSA.ezapp.mainThreadLoop()
