#!/usr/bin/env ork.python

################################################################################
# MicroMesh UV Test
# Demonstrates UV coordinate support in MicroMesh with custom shader visualization
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import math, sys
import numpy as np
from orkengine.core import vec2, vec3, vec4, CrcStringProxy, lev2_pyexdir
from orkengine.lev2 import RigidPrimitive, MicroMesh
from ork.app.application import ComponentizedApplication, ApplicationComponent
from ork.app.std_scenegraph import StandardSceneGraphComponent

lev2_pyexdir.addToSysPath()
from shaders import createPipeline

tokens = CrcStringProxy()

################################################################################
# UV Visualization Shader - shows UVs as colors (R=U, G=V)
################################################################################

UV_SHADER = """
////////////////////////////////////////
fxconfig fxcfg_default { glsl_version = "330"; }
////////////////////////////////////////
uniform_set ublock_vtx {
  mat4 mvp;
}
////////////////////////////////////////
uniform_set ublock_frg {
  vec4 modcolor;
  float time;
}
////////////////////////////////////////
vertex_interface vif_uv : ublock_vtx {
  inputs {
    vec4 pos : POSITION;
    vec4 nrm : NORMAL;
    vec3 binormal : BINORMAL;
    vec2 uv : TEXCOORD0;
    vec4 clr : COLOR0;
  }
  outputs {
    vec2 frg_uv;
    vec3 frg_nrm;
    vec3 frg_pos;
  }
}
////////////////////////////////////////
fragment_interface fif_uv : vif_uv : ublock_frg {
  outputs { layout(location = 0) vec4 out_clr; }
}
////////////////////////////////////////
vertex_shader vs_uv : vif_uv {
  frg_uv = uv;
  frg_nrm = normalize(nrm.xyz);
  frg_pos = pos.xyz;
  gl_Position = mvp * vec4(pos.xyz, 1.0);
}
////////////////////////////////////////
fragment_shader fs_uv_color : fif_uv {
  // Visualize UVs as colors: R=U, G=V, B=0
  out_clr = vec4(frg_uv.x, frg_uv.y, 0.0, 1.0);
}
////////////////////////////////////////
fragment_shader fs_uv_checker : fif_uv {
  // Checkerboard pattern based on UVs
  float scale = 8.0;
  vec2 uv_scaled = frg_uv * scale;
  float checker = mod(floor(uv_scaled.x) + floor(uv_scaled.y), 2.0);
  vec3 color = mix(vec3(0.2, 0.2, 0.2), vec3(1.0, 1.0, 1.0), checker);

  // Add slight UV tint
  color = mix(color, vec3(frg_uv.x, frg_uv.y, 0.5), 0.3);
  out_clr = vec4(color, 1.0);
}
////////////////////////////////////////
fragment_shader fs_uv_gradient : fif_uv {
  // Smooth gradient with normal-based lighting
  vec3 light_dir = normalize(vec3(1.0, 1.0, 1.0));
  float ndotl = max(dot(frg_nrm, light_dir), 0.2);

  // UV gradient colors
  vec3 uv_color = vec3(frg_uv.x, frg_uv.y, 1.0 - (frg_uv.x + frg_uv.y) * 0.5);

  out_clr = vec4(uv_color * ndotl, 1.0);
}
////////////////////////////////////////
technique tek_uv_color {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_uv;
    fragment_shader = fs_uv_color;
    state_block     = default;
  }
}
////////////////////////////////////////
technique tek_uv_checker {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_uv;
    fragment_shader = fs_uv_checker;
    state_block     = default;
  }
}
////////////////////////////////////////
technique tek_uv_gradient {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_uv;
    fragment_shader = fs_uv_gradient;
    state_block     = default;
  }
}
"""

################################################################################
# Create a simple quad mesh with UVs
################################################################################

def create_quad_mesh():
  """Create a simple quad with UV coordinates"""
  vertices = np.array([
    [-1.0, -1.0, 0.0],
    [ 1.0, -1.0, 0.0],
    [ 1.0,  1.0, 0.0],
    [-1.0,  1.0, 0.0],
  ], dtype=np.float32)

  uvs = np.array([
    [0.0, 0.0],
    [1.0, 0.0],
    [1.0, 1.0],
    [0.0, 1.0],
  ], dtype=np.float32)

  # Face list format: [num_verts, idx0, idx1, ...]
  faces = [4, 0, 1, 2, 3]

  return vertices, uvs, faces

################################################################################
# Create a UV sphere mesh
################################################################################

def create_uv_sphere(radius=1.0, slices=32, stacks=16):
  """Create a UV sphere with proper UV coordinates"""
  vertices = []
  uvs = []

  for i in range(stacks + 1):
    v = i / stacks
    phi = math.pi * v

    for j in range(slices + 1):
      u = j / slices
      theta = 2.0 * math.pi * u

      x = radius * math.sin(phi) * math.cos(theta)
      y = radius * math.cos(phi)
      z = radius * math.sin(phi) * math.sin(theta)

      vertices.append([x, y, z])
      uvs.append([u, v])

  vertices = np.array(vertices, dtype=np.float32)
  uvs = np.array(uvs, dtype=np.float32)

  # Build face list (quads)
  faces = []
  for i in range(stacks):
    for j in range(slices):
      v0 = i * (slices + 1) + j
      v1 = v0 + 1
      v2 = v0 + slices + 2
      v3 = v0 + slices + 1

      faces.extend([4, v0, v1, v2, v3])

  return vertices, uvs, faces

################################################################################
# Create a torus mesh with UVs
################################################################################

def create_uv_torus(major_radius=1.0, minor_radius=0.3, major_segments=32, minor_segments=16):
  """Create a torus with proper UV coordinates"""
  vertices = []
  uvs = []

  for i in range(major_segments + 1):
    u = i / major_segments
    theta = 2.0 * math.pi * u

    for j in range(minor_segments + 1):
      v = j / minor_segments
      phi = 2.0 * math.pi * v

      x = (major_radius + minor_radius * math.cos(phi)) * math.cos(theta)
      y = minor_radius * math.sin(phi)
      z = (major_radius + minor_radius * math.cos(phi)) * math.sin(theta)

      vertices.append([x, y, z])
      uvs.append([u, v])

  vertices = np.array(vertices, dtype=np.float32)
  uvs = np.array(uvs, dtype=np.float32)

  # Build face list (quads)
  faces = []
  for i in range(major_segments):
    for j in range(minor_segments):
      v0 = i * (minor_segments + 1) + j
      v1 = v0 + 1
      v2 = v0 + minor_segments + 2
      v3 = v0 + minor_segments + 1

      faces.extend([4, v0, v1, v2, v3])

  return vertices, uvs, faces

################################################################################
# UV Test Component
################################################################################

class UVTestComponent(ApplicationComponent):
  """Component that demonstrates UV coordinate support in MicroMesh"""

  def __init__(self, mesh_type="sphere", shader_mode="color"):
    super().__init__()
    self.mesh_type = mesh_type
    self.shader_mode = shader_mode
    self.mesh_prim = None
    self.mesh_pipe = None
    self.mesh_node = None
    self.micromesh = None
    self.base_uvs = None  # Store original UVs for animation
    self.phi = 0.0

  def _onGpuInit(self, ctx):
    """Initialize GPU resources"""

    # Select technique based on shader mode
    tech_map = {
      "color": "tek_uv_color",
      "checker": "tek_uv_checker",
      "gradient": "tek_uv_gradient",
    }
    techname = tech_map.get(self.shader_mode, "tek_uv_checker")

    # Create pipeline with UV shader
    self.mesh_pipe = createPipeline(
      app=self.app,
      ctx=ctx,
      rendermodel="ForwardPBR",
      shadertext=UV_SHADER,
      techname=techname,
    )

    # Create mesh based on type
    if self.mesh_type == "quad":
      vertices, uvs, faces = create_quad_mesh()
    elif self.mesh_type == "torus":
      vertices, uvs, faces = create_uv_torus()
    else:  # sphere
      vertices, uvs, faces = create_uv_sphere()

    # Store base UVs for animation
    self.base_uvs = uvs.copy()

    # Create MicroMesh with UVs
    self.micromesh = MicroMesh.fromVertAndFaceLists(vertices, faces)
    self.micromesh.updateUVs(uvs)
    self.micromesh.computeNormals()
    self.micromesh.computeBinormals()

    print(f"Created {self.mesh_type} mesh:")
    print(f"  Vertices: {self.micromesh.num_verts}")
    print(f"  Faces: {self.micromesh.num_faces}")
    print(f"  UVs: {self.micromesh.num_uvs}")
    print(f"  Binormals: {self.micromesh.num_binormals}")

    # Create rigid primitive
    self.mesh_prim = RigidPrimitive()
    self.mesh_prim.updateWithMicroMesh(self.micromesh, ctx)

    # Store context for GPU updates
    self._ctx = ctx

  def _onGpuLink(self, ctx):
    """Create scene graph nodes"""
    SGC = self.app.findComponentByName("std_scenegraph")
    self.mesh_node = self.mesh_prim.createNode("uv-mesh-node", SGC.layer1, self.mesh_pipe)
    self.mesh_node.sortkey = 1

  def _onUpdate(self, updinfo):
    """Update logic"""
    self.phi = updinfo.absolutetime

  def _onGpuUpdate(self, ctx):
    """Animate UVs each frame"""
    # Offset UVs based on time
    offset_u = self.phi * 0.2
    offset_v = self.phi * 0.1

    # Create animated UVs (wrap to 0-1 range using fmod)
    animated_uvs = self.base_uvs.copy()
    animated_uvs[:, 0] = np.fmod(self.base_uvs[:, 0] + offset_u, 1.0)
    animated_uvs[:, 1] = np.fmod(self.base_uvs[:, 1] + offset_v, 1.0)

    # Update mesh UVs and rebuild GPU primitive
    self.micromesh.updateUVs(animated_uvs)
    self.mesh_prim.updateWithMicroMesh(self.micromesh, ctx)

################################################################################
# Main Application
################################################################################

class UVTestApp(ComponentizedApplication):

  def __init__(self, mesh_type="sphere", shader_mode="checker"):
    super().__init__()

    # Add components
    self.SGC = self.addComponent("std_scenegraph",
                                 StandardSceneGraphComponent,
                                 eye=vec3(0, 0, 3),
                                 tgt=vec3(0, 0, 0),
                                 up=vec3(0, 1, 0),
                                 grid_variant=None)

    self.UVC = self.addComponent("uv_test",
                                 UVTestComponent,
                                 mesh_type=mesh_type,
                                 shader_mode=shader_mode)

    self.createEzApp(name="MicroMesh UV Test",
                     width=1280,
                     height=720)

################################################################################

if __name__ == "__main__":
  import argparse

  parser = argparse.ArgumentParser(description='MicroMesh UV Test')
  parser.add_argument('-m', '--mesh', choices=['quad', 'sphere', 'torus'],
                      default='sphere', help='Mesh type to display')
  parser.add_argument('-s', '--shader', choices=['color', 'checker', 'gradient'],
                      default='color', help='UV visualization mode')
  args = parser.parse_args()

  print(f"UV Test: mesh={args.mesh}, shader={args.shader}")

  app = UVTestApp(mesh_type=args.mesh, shader_mode=args.shader)
  app.ezapp.mainThreadLoop()
