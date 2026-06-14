#!/usr/bin/env ork.python

################################################################################
# Chunked-upload TXI: 2D TEXTURE ARRAY with N solid-color slices, each uploaded
# via a separate uploadTextureRegion call (array_layer=k).
#
# Visualization is a single textured quad sampling sampler2DArray with the
# slice index driven by a uniform. Number keys clamp the sampler to a
# specific slice so you can flip through them.
#
#   1..N maps to slices 0..N-1
#   0    maps to slice (N-1)  (consistent with chunked_2d_per_mip.py rotation)
#
# The N=8 default uses HSV-spaced solid colors:
#   slice 0 red, 1 orange, 2 yellow, 3 green, 4 cyan, 5 blue, 6 violet, 7 magenta
#
# Pass: pressing a number key changes the quad's color to match the slice's
#       expected color. If the color doesn't change, slice routing through
#       array_layer is broken (or the descriptor doesn't carry the array view).
################################################################################

import math, sys, signal
import numpy as np
from orkengine.core import vec3, vec4, mtx4, CrcStringProxy
from orkengine import lev2
from orkengine.lev2 import lev2exdir

sys.path.append(lev2exdir().as_string + "/python")
from lev2utils.cameras import setupUiCamera
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()

DIM        = 256                  # per-slice pixel dimension (power of 2)
NUM_SLICES = 8                    # array layer count
NUM_MIPS   = int(math.log2(DIM)) + 1   # full mip pyramid (9 levels at 256)

# HSV-spaced solid colors — one per slice (rgb 0..255)
SLICE_COLORS = [
    (255,   0,   0),  # red
    (255, 128,   0),  # orange
    (255, 255,   0),  # yellow
    (  0, 255,   0),  # green
    (  0, 255, 255),  # cyan
    (  0,   0, 255),  # blue
    (128,   0, 255),  # violet
    (255,   0, 255),  # magenta
]

ARRAY_SHADER = """
fxconfig fxcfg_default { glsl_version = "330"; }

sampler_set samplers(descriptor_set 0) {
  sampler2DArray ColorMap;
}

uniform_set ublock_vtx {
  mat4 mvp;
}

uniform_block ublock_frg (descriptor_set 0) {
  vec4 modcolor;
  float SliceIndex;
}

vertex_interface iface_vtx : ublock_vtx {
  inputs {
    vec4 pos : POSITION;
    vec2 uv0 : TEXCOORD0;
  }
  outputs {
    vec2 frg_uv;
  }
}

fragment_interface iface_frg : ublock_frg {
  inputs {
    vec2 frg_uv;
  }
  outputs { layout(location = 0) vec4 out_clr; }
}

vertex_shader vs_x : iface_vtx {
  gl_Position = mvp * pos;
  frg_uv = uv0;
}

fragment_shader fs_x : iface_frg : samplers {
  vec4 sampled = texture(ColorMap, vec3(frg_uv, SliceIndex));
  out_clr = vec4(sampled.rgb * modcolor.rgb, 1.0);
}

state_block sb_default : default {
  DepthTest = LEQUALS;
  DepthMask = ON;
}

technique tek_x {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_x;
    fragment_shader = fs_x;
    state_block     = sb_default;
  }
}
"""

def buildSliceRGBA8(w, h, r, g, b):
  """Mip-0 checker (vectorized via numpy so the chain helper has uint8
     ndarrays to downsample)."""
  cell = max(1, w // 8)
  yy = np.arange(h)[:, None] // cell
  xx = np.arange(w)[None, :] // cell
  on = ((xx ^ yy) & 1).astype(np.uint8)
  out = np.zeros((h, w, 4), dtype=np.uint8)
  out[..., 0] = np.where(on, r, r // 3)
  out[..., 1] = np.where(on, g, g // 3)
  out[..., 2] = np.where(on, b, b // 3)
  out[..., 3] = 255
  return out

def buildMipChain(mip0):
  """Generate the full mip pyramid from a mip-0 uint8 RGBA image via
     2x2 box filter. chain[0] = mip0; chain[k] = mip k (downsampled k times)."""
  chain = [mip0]
  cur = mip0
  while min(cur.shape[:2]) > 1:
    H, W, C = cur.shape
    cur = (cur.astype(np.uint16).reshape(H // 2, 2, W // 2, 2, C).sum(axis=(1, 3)) // 4) \
              .astype(np.uint8)
    chain.append(cur)
  return chain

def buildQuadMesh():
  vertices = np.array([
    [-1.0, -1.0, 0.0],
    [ 1.0, -1.0, 0.0],
    [ 1.0,  1.0, 0.0],
    [-1.0,  1.0, 0.0],
  ], dtype=np.float32)
  uvs = np.array([
    [0.0, 1.0],
    [1.0, 1.0],
    [1.0, 0.0],
    [0.0, 0.0],
  ], dtype=np.float32)
  faces = [4, 0, 1, 2, 3]
  return vertices, uvs, faces

class App:
  def __init__(self):
    self.ezapp = lev2.OrkEzApp.create(self, height=720, width=960)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    setupUiCamera(app=self, eye=vec3(0, 0, 3), tgt=vec3(0, 0, 0))
    signal.signal(signal.SIGINT, lambda s, f: self.ezapp.signalExit())
    self.time = 0.0
    self.current_slice = 0

  def onGpuInit(self, ctx):
    self.ctx = ctx
    txi = ctx.TXI
    createSceneGraph(app=self)

    # 1) Reserve a mipmapped TextureArray
    self.tex_array = lev2.TextureArray(w=DIM, h=DIM, slices=NUM_SLICES,
                                       fmt=tokens.RGBA8, mipmapped=True)
    txi.reserveTextureArray(tarr=self.tex_array,
                            w=DIM, h=DIM,
                            num_slices=NUM_SLICES, num_mips=NUM_MIPS,
                            fmt=tokens.RGBA8)

    # 2) For each slice: build mip 0 checker, generate full mip chain,
    #    upload every (mip_level, array_layer) via uploadTextureRegion.
    for i in range(NUM_SLICES):
      r, g, b = SLICE_COLORS[i % len(SLICE_COLORS)]
      mip0 = buildSliceRGBA8(DIM, DIM, r, g, b)
      chain = buildMipChain(mip0)
      for k, mip in enumerate(chain):
        mh, mw, _ = mip.shape
        txi.uploadTextureRegion(
            tex=self.tex_array.tex,
            mip_level=k, array_layer=i,
            offset_x=0, offset_y=0, offset_z=0,
            extent_w=mw, extent_h=mh, extent_d=1,
            data=mip.tobytes())

    txi.finalizeUpload(tex=self.tex_array.tex)

    # 3) Pipeline with sampler2DArray
    mtl = lev2.FreestyleMaterial()
    mtl.gpuInitFromShaderText(ctx, "chunked_array_shader", ARRAY_SHADER)
    mtl.rasterstate.culltest  = tokens.OFF
    mtl.rasterstate.depthtest = tokens.LEQUALS

    permu = lev2.FxPipelinePermutation(rendermodel="ForwardPBR")
    permu.technique = mtl.shader.technique("tek_x")

    self.pipeline = mtl.fxcache.findPipeline(permu)
    self.pipeline.bindParam(mtl.param("mvp"),      tokens.RCFD_Camera_MVP_Mono)
    self.pipeline.bindParam(mtl.param("modcolor"), vec4(1, 1, 1, 1))
    self.pipeline.bindParam(mtl.param("ColorMap"), self.tex_array)
    self.slice_param = mtl.param("SliceIndex")
    self.pipeline.bindParam(self.slice_param, float(0.0))
    self.pipeline.sharedMaterial = mtl

    # 4) Quad in the scenegraph
    verts, uvs, faces = buildQuadMesh()
    self.mesh = lev2.MicroMesh.fromVertAndFaceLists(verts, faces)
    self.mesh.updateUVs(uvs)
    self.mesh.computeNormals()
    self.prim = lev2.RigidPrimitive()
    self.prim.updateWithMicroMesh(self.mesh, ctx, tokens.TRIANGLES)
    self.node = self.prim.createNode("quad", self.layer1, self.pipeline)
    self.node.worldTransform.scale = 1.0

    self.scene.lightingmanager.gpuInit(ctx)

    self._printLegend()

  def _printLegend(self):
    name_for = {0:"red",1:"orange",2:"yellow",3:"green",
                4:"cyan",5:"blue",6:"violet",7:"magenta"}
    expect = name_for.get(self.current_slice, "?")
    # Key 'k' maps to slice (k-1) (with '0' meaning slice 9). With
    # NUM_SLICES=8, only keys '1'..'8' do anything; '9' and '0' are
    # filtered as out-of-range.
    high_key = min(NUM_SLICES, 9)
    print(f"chunked_array_slices — slice {self.current_slice} "
          f"(expected color: {expect})   "
          f"press 1..{high_key} to cycle "
          f"(1=slice0, {high_key}=slice{high_key-1})")

  def onUiEvent(self, uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom(self.uicam.cameradata)
      return lev2.ui.HandlerResult()
    if uievent.code == tokens.KEY_DOWN.hashed:
      kc = uievent.keycode
      if kc >= ord('0') and kc <= ord('9'):
        # Rotate: '1'->slice0, '2'->slice1, ..., '9'->slice8, '0'->slice9.
        # Constant offset of 9 — independent of NUM_SLICES — gives the
        # same keymap as chunked_2d_per_mip.py. Out-of-range slices
        # (e.g. '9' or '0' when NUM_SLICES<10) are filtered by the
        # `0 <= idx < NUM_SLICES` check.
        idx = (kc - ord('0') + 9) % 10
        if 0 <= idx < NUM_SLICES:
          self.current_slice = idx
          self._printLegend()
        return lev2.ui.HandlerResult()
    return lev2.ui.HandlerResult()

  def onUpdate(self, updinfo):
    self.time = updinfo.absolutetime
    self.scene.updateScene(self.cameralut)

  def onGpuUpdate(self, ctx):
    # Rebind SliceIndex each frame from current_slice. Matches the pattern
    # in primitive_types.py (rebinding > lambda for varying scalars).
    self.pipeline.bindParam(self.slice_param, float(self.current_slice))

App().ezapp.mainThreadLoop()
