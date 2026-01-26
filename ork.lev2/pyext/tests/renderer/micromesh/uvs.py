#!/usr/bin/env ork.python

################################################################################
# MicroMesh UV Test
# Demonstrates UV coordinate support in MicroMesh with custom shader visualization
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import math, sys
import numpy as np
from obt import path as obt_path
from orkengine.core import vec2, vec3, vec4, CrcStringProxy, lev2_pyexdir
from orkengine import lev2
from orkengine.lev2 import RigidPrimitive, MicroMesh
from ork.app.application import ComponentizedApplication, ApplicationComponent
from ork.app.std_scenegraph import StandardSceneGraphComponent

lev2_pyexdir.addToSysPath()
from shaders import createPipeline

tokens = CrcStringProxy()

################################################################################
# Build movie shortname map from filesystem
################################################################################

def build_movie_shortname_map():
  """Scan assetcache/movies directory and build shortname -> path map"""
  shortname_to_path = {}

  movies_dir = obt_path.stage() / "assetcache" / "movies"
  if not movies_dir.exists():
    return shortname_to_path

  # Scan for video files
  video_extensions = [".mp4", ".mov", ".mkv", ".avi", ".webm", ".m4v"]
  for ext in video_extensions:
    for video_file in movies_dir.glob(f"*{ext}"):
      shortname = video_file.stem  # filename without extension
      shortname_to_path[shortname] = video_file.name  # just the filename

  return shortname_to_path

################################################################################
# UV Visualization Shader - shows UVs as colors (R=U, G=V)
################################################################################

MOVIE_SHADER = """
////////////////////////////////////////
fxconfig fxcfg_default { glsl_version = "330"; }
////////////////////////////////////////
uniform_set ublock_vtx {
  mat4 mvp;
}
////////////////////////////////////////
sampler_set ublock_frg (descriptor_set 0) {
  sampler2D ColorMap;
}
////////////////////////////////////////
vertex_interface vif_movie : ublock_vtx {
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
  }
}
////////////////////////////////////////
fragment_interface fif_movie : vif_movie : ublock_frg {
  outputs { layout(location = 0) vec4 out_clr; }
}
////////////////////////////////////////
// Lanczos-3 filtering library
////////////////////////////////////////
libblock lib_lanczos3 {
  // Original sinc function
  float sinc(float x) {
    if (abs(x) < 0.0001) return 1.0;
    float pix = PI * x;
    return sin(pix) / pix;
  }

  // Lanczos-3 kernel (a=3 gives excellent sharpness with minimal ringing)
  float lanczos3(float x) {
    if (abs(x) >= 3.0) return 0.0;
    return sinc(x) * sinc(x / 3.0);
  }
}
////////////////////////////////////////
vertex_shader vs_movie : vif_movie {
  frg_uv = uv;
  frg_nrm = normalize(nrm.xyz);
  gl_Position = mvp * vec4(pos.xyz, 1.0);
}
////////////////////////////////////////
fragment_shader fs_movie : fif_movie {
  vec4 tex_color = texture(ColorMap, frg_uv);
  out_clr = tex_color;
}
////////////////////////////////////////
// Adaptive Lanczos-3 downsampling fragment shader
////////////////////////////////////////
fragment_shader fs_movie_aa : fif_movie : lib_lanczos3 {
  // Get texture dimensions
  vec2 texSize = vec2(textureSize(ColorMap, 0));
  vec2 texelSize = 1.0 / texSize;

  // Calculate anisotropic footprint using screen-space derivatives
  vec2 duvdx = dFdx(frg_uv) * texSize;
  vec2 duvdy = dFdy(frg_uv) * texSize;

  // Anisotropic footprint: major and minor axes
  float footprintX = max(abs(duvdx.x), abs(duvdy.x));
  float footprintY = max(abs(duvdx.y), abs(duvdy.y));
  float maxFootprint = max(footprintX, footprintY);

  // If minimal downsampling, use hardware filtering
  if (maxFootprint <= 1.5) {
    out_clr = texture(ColorMap, frg_uv);
  } else {
    // Lanczos-3: 16x16 grid = 256 samples
    const int GRID_SIZE = 16;
    const float HALF_GRID = 8.0;

    vec2 footprint = vec2(footprintX, footprintY);
    vec2 centerTexel = frg_uv * texSize;

    vec4 colorSum = vec4(0.0);
    float weightSum = 0.0;

    for (int iy = 0; iy < GRID_SIZE; iy++) {
      float fy = (float(iy) - HALF_GRID + 0.5) / HALF_GRID * 3.0;

      for (int ix = 0; ix < GRID_SIZE; ix++) {
        float fx = (float(ix) - HALF_GRID + 0.5) / HALF_GRID * 3.0;

        float weight = lanczos3(fx) * lanczos3(fy);

        vec2 sampleOffset = vec2(fx, fy) * footprint;
        vec2 sampleUV = (centerTexel + sampleOffset) * texelSize;

        colorSum += texture(ColorMap, sampleUV) * weight;
        weightSum += weight;
      }
    }

    out_clr = colorSum / max(weightSum, 0.0001);
  }
}
////////////////////////////////////////
state_block sb_movie : default {
  CullTest = OFF;
}
////////////////////////////////////////
technique tek_movie {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_movie;
    fragment_shader = fs_movie;
    state_block     = sb_movie;
  }
}
////////////////////////////////////////
technique tek_movie_aa {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_movie;
    fragment_shader = fs_movie_aa;
    state_block     = sb_movie;
  }
}
"""

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
state_block sb_uvc : default {
  CullTest = OFF;
}
////////////////////////////////////////
technique tek_uv_color {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_uv;
    fragment_shader = fs_uv_color;
    state_block     = sb_uvc;
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
state_block sb_uvg : default {
  CullTest = OFF;
}
////////////////////////////////////////
technique tek_uv_gradient {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_uv;
    fragment_shader = fs_uv_gradient;
    state_block     = sb_uvg;
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

def create_uv_sphere(radius=10.0, slices=64, stacks=32):
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

  def __init__(self, mesh_type="sphere", shader_mode="color", movie_file=None, antialias=False, shm_name=None):
    super().__init__()
    self.mesh_type = mesh_type
    self.shader_mode = shader_mode
    self.movie_file = movie_file
    self.antialias = antialias
    self.shm_name = shm_name
    self.mesh_prim = None
    self.mesh_pipe = None
    self.mesh_node = None
    self.micromesh = None
    self.base_uvs = None  # Store original UVs for animation
    self.movie = None
    self.movie_material = None
    self.shm_consumer = None
    self.shm_material = None
    self.phi = 0.0

  def _onGpuInit(self, ctx):
    """Initialize GPU resources"""

    # Check if we're in SHM consumer mode
    if self.shm_name:
      print(f"Connecting to SHM producer: {self.shm_name}")
      try:
        self.shm_consumer = lev2.ShmTexConsumer.create(self.shm_name)
        print(f"SHM Consumer connected: {self.shm_consumer.width}x{self.shm_consumer.height}")
      except Exception as e:
        print(f"Failed to connect to SHM producer '{self.shm_name}': {e}")
        print("Make sure the producer is running first.")
        sys.exit(1)

      # Create pipeline with movie shader (reuse for SHM texture)
      self.shm_material = lev2.FreestyleMaterial()
      self.shm_material.gpuInitFromShaderText(ctx, "shm_shader", MOVIE_SHADER)
      self.shm_material.rasterstate.culltest = tokens.PASS_FRONT
      self.shm_material.rasterstate.depthtest = tokens.LEQUALS

      techname = "tek_movie_aa" if self.antialias else "tek_movie"
      if self.antialias:
        print("Antialiasing: Adaptive Lanczos-3 enabled")

      permu = lev2.FxPipelinePermutation(rendermodel="ForwardPBR")
      permu.technique = self.shm_material.shader.technique(techname)

      self.mesh_pipe = self.shm_material.fxcache.findPipeline(permu)
      self.mesh_pipe.bindParam(self.shm_material.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
      # ColorMap will be bound dynamically in _onGpuUpdate when texture is available
      self.mesh_pipe.sharedMaterial = self.shm_material

    # Check if we're in movie mode
    elif self.movie_file:
      # Initialize movie playback
      import time
      movie_path = str(obt_path.stage() / "assetcache" / "movies" / self.movie_file)
      print(f"Loading movie: {movie_path}")

      self.movie = lev2.MoviePlaybackContext()
      self.movie.init(
        movie_path,
        backend=lev2.MovieBackend.VIDEOTOOLBOX,
        format=lev2.MoviePixelFormat.AUTO
      )
      time.sleep(0.5)  # Wait for initialization

      print(f"Movie: {self.movie.width}x{self.movie.height} @ {self.movie.fps:.2f}fps")

      # Create pipeline with movie shader
      self.movie_material = lev2.FreestyleMaterial()
      self.movie_material.gpuInitFromShaderText(ctx, "movie_shader", MOVIE_SHADER)
      self.movie_material.rasterstate.culltest = tokens.PASS_FRONT
      self.movie_material.rasterstate.depthtest = tokens.LEQUALS

      # Select technique based on antialias setting
      techname = "tek_movie_aa" if self.antialias else "tek_movie"
      if self.antialias:
        print("Antialiasing: Adaptive Lanczos-3 enabled")

      permu = lev2.FxPipelinePermutation(rendermodel="ForwardPBR")
      permu.technique = self.movie_material.shader.technique(techname)

      self.mesh_pipe = self.movie_material.fxcache.findPipeline(permu)
      self.mesh_pipe.bindParam(self.movie_material.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
      self.mesh_pipe.bindParam(self.movie_material.param("ColorMap"), self.movie.texture )
      self.mesh_pipe.sharedMaterial = self.movie_material

      # Start playback
      self.movie.play()
    else:
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
    """Animate UVs each frame (only when not in movie/shm mode)"""
    # In SHM consumer mode, update texture from shared memory
    if self.shm_name:
      if self.shm_consumer:
        if self.shm_consumer.update(ctx):
          tex = self.shm_consumer.texture
          if tex and self.shm_material:
            # Bind the texture to the pipeline
            self.mesh_pipe.bindParam(self.shm_material.param("ColorMap"), tex)
      return

    # In movie mode, poll the texture update provider to trigger frame updates
    if self.movie_file:
      if self.movie and self.movie.texture:
        tex = self.movie.texture
        if tex.update_provider:
          tex.update_provider.getTexture()
      return

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

  def __init__(self, mesh_type="sphere", shader_mode="checker", movie_file=None, fullscreen=False, antialias=False, shm_name=None):
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
                                 shader_mode=shader_mode,
                                 movie_file=movie_file,
                                 antialias=antialias,
                                 shm_name=shm_name)

    self.createEzApp(name="MicroMesh UV Test",
                     width=1280,
                     height=720,
                     fullscreen=fullscreen,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

################################################################################

if __name__ == "__main__":
  import argparse

  # Build shortname map before parsing args
  shortname_map = build_movie_shortname_map()

  parser = argparse.ArgumentParser(description='MicroMesh UV Test')
  parser.add_argument('-m', '--mesh', choices=['quad', 'sphere', 'torus'],
                      default='sphere', help='Mesh type to display')
  parser.add_argument('-s', '--shader', choices=['color', 'checker', 'gradient'],
                      default='color', help='UV visualization mode')
  parser.add_argument('-M', '--movie', type=str, default=None,
                      help='Movie file or shortname to use as texture')
  parser.add_argument('-l', '--list', action='store_true',
                      help='List available movie shortnames')
  parser.add_argument('-f', '--fullscreen', action='store_true',
                      help='Run in fullscreen mode')
  parser.add_argument('-A', '--aa', action='store_true',
                      help='Enable adaptive Lanczos antialiasing')
  parser.add_argument('-S', '--shm', type=str, default=None,
                      help='SHM texture name to consume (e.g., shmtex_demo)')
  args = parser.parse_args()

  # Handle --list option
  if args.list:
    print("\nAvailable movies:")
    print("=" * 60)

    if not shortname_map:
      print("  (no movies found in assetcache/movies)")
    else:
      # Sort by shortname and display in columns
      sorted_names = sorted(shortname_map.keys())
      col_width = max(len(n) for n in sorted_names) + 2
      cols = max(1, 60 // col_width)

      for i in range(0, len(sorted_names), cols):
        row = sorted_names[i:i+cols]
        line = "  " + "".join(f"{n:<{col_width}}" for n in row)
        print(line)

    print("=" * 60)
    print(f"Total: {len(shortname_map)} movies")
    print("Usage: uvs.py -M <shortname> [-m mesh] [-s shader]")
    sys.exit(0)

  # Resolve movie shortname to filename
  movie_file = args.movie
  if movie_file:
    if movie_file in shortname_map:
      movie_file = shortname_map[movie_file]
      print(f"Resolved shortname '{args.movie}' -> {movie_file}")
    elif not any(movie_file.endswith(ext) for ext in [".mp4", ".mov", ".mkv", ".avi", ".webm", ".m4v"]):
      # Try adding .mp4 extension
      if args.movie + ".mp4" in [shortname_map.get(k, "") for k in shortname_map]:
        movie_file = args.movie + ".mp4"

  if args.shm:
    print(f"UV Test: mesh={args.mesh}, shm={args.shm}")
  elif movie_file:
    print(f"UV Test: mesh={args.mesh}, movie={movie_file}")
  else:
    print(f"UV Test: mesh={args.mesh}, shader={args.shader}")

  app = UVTestApp(mesh_type=args.mesh, shader_mode=args.shader, movie_file=movie_file,
                  fullscreen=args.fullscreen, antialias=args.aa, shm_name=args.shm)
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
