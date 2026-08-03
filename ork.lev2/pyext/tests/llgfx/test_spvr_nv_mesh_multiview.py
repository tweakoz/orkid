#!/usr/bin/env ork.python
################################################################################
# SINGLE-PASS STEREO, MESH STAGE: the terrain mesh-shader path inside a MULTIVIEW
# pass (viewMask=0b11) into a 2-LAYER color+depth target — two CORRECT views out
# of ONE draw, no device rejection.
#
# WHY IT IS A GATE AND NOT A ONE-OFF: single-pass stereo is only worth building on
# if the mesh stage survives it. The capability bit (multiviewMeshShader) says the
# driver MAY do this; this test says it DOES, and keeps saying it — a driver update,
# a codegen change, or an engine change to the layered-RTG plumbing that breaks the
# second view has nowhere else to be caught.
#
# WHAT IT RENDERS: the generator's mesh-shader terrain (TerrainChunkVertexSource,
# fixed-grid mode-1 decode — the same text the A/B gate and the C++ chunk drawable
# use), over a procedural heightfield in one SSBO. The ONLY thing this test adds
# to that path is the per-view clip matrix: the mesh stage's mvp expression is
# s_vp[ofx_viewIndex] out of a second SSBO (two matrices, host-written), so view 0
# and view 1 differ by a real stereo baseline and nothing about the eye separation
# is baked into shader text.
#
# THE EVIDENCE (four renders, three comparisons):
#   multiview : ONE draw into the 2-layer multiview RTG; layer 0 and layer 1 read
#               back separately (capbuf.capture_layer).
#   mono      : the SAME geometry drawn TWICE into an ordinary 1-layer RTG, once
#               per eye matrix, by a SEPARATE material whose mesh stage reads
#               s_vp[0] and never mentions ofx_viewIndex.
#   -> layer0 must match mono-left and layer1 must match mono-right (the views are
#      CORRECT, not merely present), the two layers must NOT match each other, and
#      the horizontal disparity between the layers must equal the disparity the
#      mono pair independently shows (the parallax is REAL, not a smear).
#
# PIPELINE-CACHE COLLISION DODGE: layoutBits() keys the pipeline hash on attachment
# FORMATS + msaa only — it does not fold viewMask, so one material drawn into both a
# multiview and a non-multiview RTG of the same formats would be handed one pipeline
# for both passes. The mono control therefore lives in its OWN FreestyleMaterial (a
# distinct shader program => a distinct pipeline hash), and neither material is ever
# bound to the other's target.
#
# SKIP is a real caps query, never a try/except: a device without VK_EXT_mesh_shader,
# without core multiview, without multiviewMeshShader, or whose enabled view budget is
# under 2 has no subject here and the test says so and passes — the whole fleet stays
# green while the capability spreads.
#
# Fully synthetic: procedural heightfield, inline shader text, no assets, no window.
# Runs standalone in well under a minute:
#   ./ork.lev2/pyext/tests/llgfx/test_spvr_nv_mesh_multiview.py
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
# SELF-CONFIGURED, set before engine init — the bare invocation needs no external env.
# fidelity with the shipped mesh toggle; the generator below is constructed with
# mesh=True explicitly, so this cannot change what is compiled.
os.environ.setdefault("ORKID_TERRAIN_MESHSHADER", "1")
# validation ARMED in continue mode: a driver that merely tolerates this pass while the
# layer screams is not a pass. 2 (not 1) so one run surfaces every defect instead of
# dying on the first, and the count is read back off the context at verdict time.
os.environ.setdefault("ORKID_VULKAN_VALIDATE", "2")
import sys
sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import numpy

from orkengine import core   # core before lev2
from orkengine import lev2
from ork.testing import headless_app, verdict
from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource

tokens = core.CrcStringProxy()

DIM    = 1024      # heightfield grid (procedural)
EXTENT = 1000.0    # world meters per side
HMAX   = 90.0      # world meters of relief
CHUNK  = 128
W, H   = 512, 512
FOV    = 45.0 * 3.14159265 / 180.0

EYE      = core.vec3(0.0, 420.0, 900.0)
TGT      = core.vec3(0.0,   0.0,   0.0)
BASELINE = 60.0    # metres between the two eyes — host-written into the SSBO, never in shader text

NVIEWS   = 2

################################################################################

def synth_heights(dim, hmax):
  """Procedural heightfield in TRUE METERS — the A/B gate's field, unchanged."""
  xs = (numpy.arange(dim, dtype=numpy.float32) + 0.5) / float(dim) - 0.5
  X, Z = numpy.meshgrid(xs, xs, indexing="xy")
  h = (0.55
       + 0.25 * numpy.sin(X * 14.0) * numpy.cos(Z * 11.0)
       + 0.20 * numpy.exp(-((X * 2.4) ** 2 + (Z * 2.4) ** 2)))
  return (numpy.clip(h, 0.0, 1.0) * hmax).astype(numpy.float32).reshape(-1)


def _indent(text, n):
  pad = " " * n
  return "\n".join(pad + ln for ln in text.strip().splitlines())

################################################################################

def build_shader(vs, mvp):
  """One mesh technique over the generator's geometry. `mvp` is the clip-matrix
  EXPRESSION handed to the generator — the only difference between the multiview
  program (s_vp[ofx_viewIndex]) and the mono control (s_vp[0])."""
  return """
fxconfig fxcfg_default {}
////////////////////////////////////////
storage_interface sif_ptex_vtx (descriptor_set 0) {
  buffer layout(std430) ptex_vtx_data {
%(LAYOUT)s
  };
}
// per-view clip matrices: one per view of the multiview pass, host-written.
storage_interface sif_stereo (descriptor_set 0) {
  buffer layout(std430) stereo_data {
    mat4 s_vp[%(NVIEWS)d];
  };
}
libblock lib_terr {
%(LIB)s
  vec4 terr_shade(vec3 nrm, vec2 uv) {
    float lam = clamp(dot(normalize(nrm), normalize(vec3(0.4, 0.85, 0.3))), 0.0, 1.0);
    return vec4(lam, 0.35 + 0.5 * uv.x, 0.25 + 0.5 * uv.y, 1.0);
  }
}
////////////////////////////////////////
// carries the fragment stage's varying declarations (no vertex_shader uses it)
vertex_interface vif_terrain : sif_ptex_vtx {
  outputs { vec4 frg_clr; }
}
%(MESHIFACE)s
fragment_interface fif_terrain : vif_terrain {
  outputs { layout(location = 0) vec4 out_clr; }
}
////////////////////////////////////////
mesh_shader ms_terrain : extension(GL_EXT_mesh_shader) : vif_terrain_mesh : lib_terr {
%(MESHBODY)s
}
fragment_shader ps_terrain : fif_terrain {
  out_clr = frg_clr;
}
////////////////////////////////////////
technique tek_mesh {
  fxconfig = fxcfg_default;
  pass p0 { mesh_shader = ms_terrain; fragment_shader = ps_terrain; state_block = default; }
}
""" % dict(NVIEWS=NVIEWS,
           LAYOUT=_indent(vs.layout, 4),
           LIB=_indent(vs.lib, 2),
           MESHIFACE=vs.mesh_interface(outputs="vec4 frg_clr;",
                                       inherits=("sif_ptex_vtx", "sif_stereo")),
           MESHBODY=_indent(vs.mesh_body(mvp=mvp, compacted=False,
                                         varying_writes="frg_clr[$V] = terr_shade(normal, uv0);"), 2))

################################################################################

def view_proj(eye, tgt):
  proj = core.mtx4.perspective(FOV, float(W) / float(H), 1.0, 4000.0)
  view = core.mtx4.lookAt(eye, tgt, core.vec3(0, 1, 0))
  return proj * view   # orkid operator* is rtol


def eye_pair():
  """Two PARALLEL views separated by BASELINE along the camera right axis (no toe-in,
  so the disparity is purely horizontal)."""
  fwd = (TGT - EYE).normalized
  right = fwd.cross(core.vec3(0, 1, 0)).normalized
  half = right * (0.5 * BASELINE)
  return [(EYE - half, TGT - half), (EYE + half, TGT + half)]

################################################################################

class Stereo:

  def __init__(self, app):
    self.app = app
    self.ctx = ctx = app.ctx
    fxi = ctx.FXI

    self.vs = TerrainChunkVertexSource(dim=DIM, extent_m=EXTENT, chunk=CHUNK,
                                       mesh=True, mesh_indirect=False, mesh_cull_chunk=False)
    heights = synth_heights(DIM, HMAX)
    self.ssbo = fxi.createShaderStorageBufferWithLength(self.vs.TOTAL)
    fxi.copyDataIntoShaderStorageBuffer(heights, self.ssbo, self.vs.HEIGHTS_OFF)
    self.vs.upload_dim(fxi, self.ssbo)
    fxi.copyDataIntoShaderStorageBuffer(
        numpy.array([float(heights.min()), float(heights.max())], dtype=numpy.float32),
        self.ssbo, self.vs.YB_OFF)
    # per-view clip matrices (16 floats each)
    self.stereo_ssbo = fxi.createShaderStorageBufferWithLength(NVIEWS * 64)

    self.mtl_mv   = self._material("terrain_mv",   "s_vp[ofx_viewIndex]")
    self.mtl_mono = self._material("terrain_mono", "s_vp[0]")

    # 2-layer multiview target: numLayers/multiview BOTH set before any buffer exists.
    self.rtg_mv = lev2.RtGroup(self.ctx, W, H)
    self.rtg_mv.numLayers = NVIEWS
    self.rtg_mv.multiview = True
    self.rtb_mv = self.rtg_mv.createBuffer(tokens.RGBA8, tokens.color)
    self.rtb_mv.clearColor = core.vec4(0, 0, 0, 1)
    self.rtg_mv.createDepthBuffer(tokens.Z32F, True)
    self.rtg_mv.autoclear = True

    # ordinary single-view control target
    self.rtg_mono = lev2.RtGroup(self.ctx, W, H)
    self.rtb_mono = self.rtg_mono.createBuffer(tokens.RGBA8, tokens.color)
    self.rtb_mono.clearColor = core.vec4(0, 0, 0, 1)
    self.rtg_mono.createDepthBuffer(tokens.Z32F, True)
    self.rtg_mono.autoclear = True

    self.groups = self.vs.mesh_groups()

  def _material(self, name, mvp):
    mtl = lev2.FreestyleMaterial()
    mtl.gpuInitFromShaderText(self.ctx, name, build_shader(self.vs, mvp))
    mtl.rasterstate.culltest = tokens.OFF
    mtl.rasterstate.depthtest = tokens.LEQUALS
    permu = lev2.FxPipelinePermutation()
    permu.rendermodel = "CUSTOM"
    permu.technique = mtl.shader.technique("tek_mesh")
    assert permu.technique, "technique tek_mesh not found (%s)" % name
    pipe = mtl.fxcache.findPipeline(permu)
    assert pipe, "no pipeline for %s" % name
    pipe.bindStorage(mtl.storage("sif_ptex_vtx"), self.ssbo)
    pipe.bindStorage(mtl.storage("sif_stereo"), self.stereo_ssbo)
    return dict(mtl=mtl, tek=permu.technique, pipe=pipe)

  ##############################################

  def set_cull_view(self, eye, tgt):
    """The CamBlk the mesh stage's frustum SELF-CULL reads. Mono (the left eye's frustum)
    for both views — a stereo implementation would use the view union; at this baseline
    the two frusta differ by well under a chunk, and the cull bound is what both the
    multiview draw and the mono controls share, so the comparison stays apples-to-apples."""
    fxi = self.ctx.FXI
    vp = view_proj(eye, tgt)
    fxi.copyDataIntoShaderStorageBuffer(numpy.array(vp, dtype=numpy.float32).reshape(-1),
                                        self.ssbo, self.vs.CAM_OFF)
    fxi.copyDataIntoShaderStorageBuffer(numpy.array(vp.inverse, dtype=numpy.float32).reshape(-1),
                                        self.ssbo, self.vs.CAM_OFF + 64)
    fxi.copyDataIntoShaderStorageBuffer(numpy.array([eye.x, eye.y, eye.z, 1.0], dtype=numpy.float32),
                                        self.ssbo, self.vs.CAM_OFF + 128)
    # misc.x = exact frustum scale, misc.y = 0 -> HZB occlusion off
    fxi.copyDataIntoShaderStorageBuffer(numpy.array([1.0, 0.0, 0.0, 0.0], dtype=numpy.float32),
                                        self.ssbo, self.vs.CAM_OFF + 144)

  def set_view_matrices(self, mats):
    fxi = self.ctx.FXI
    for i, m in enumerate(mats):
      fxi.copyDataIntoShaderStorageBuffer(numpy.array(m, dtype=numpy.float32).reshape(-1),
                                          self.stereo_ssbo, i * 64)

  ##############################################

  def render(self, which, rtg, rtb, layer):
    ctx = self.ctx
    slot = self.mtl_mv if which == "mv" else self.mtl_mono
    capbuf = lev2.CaptureBuffer()
    capbuf.capture_layer = layer
    ctx.beginFrame()
    ctx.FBI.rtGroupPush(rtg)
    ctx.FBI.rtGroupClear(rtg)
    RCFD = lev2.RenderContextFrameData(ctx)
    RCID = lev2.RenderContextInstData(RCFD)
    RCID.forceTechnique(slot["tek"])
    RCID.genMatrix(lambda: core.mtx4())
    gx, gy, gz = self.groups
    slot["pipe"].wrappedDrawCall(RCID, lambda: ctx.GBI.drawMeshTasks(gx, gy, gz))
    future = ctx.FBI.captureAsFormat(rtb, capbuf, "RGBA8")
    ctx.FBI.rtGroupPop()
    ctx.endFrame()
    frames = 0
    while not future.is_ready:
      self.app.run_frames(1)
      frames += 1
      assert frames < 600, "capture never became ready (%s layer %d)" % (which, layer)
    return numpy.array(capbuf, dtype=numpy.uint8).reshape(capbuf.height, capbuf.width, 4)

################################################################################

def coverage(img):
  return float((img[..., :3].sum(axis=2) > 0).mean())


def exact_match(a, b):
  return float((a[..., :3] == b[..., :3]).all(axis=2).mean())


def best_shift(a, b, span=140):
  """Horizontal disparity: the roll of `b` (in pixels, positive = content moved RIGHT)
  that best matches `a`, plus the improvement over no roll. Measured on luma over the
  interior so the roll's wrapped edge cannot dominate."""
  la = a[..., :3].astype(numpy.float32).mean(axis=2)
  lb = b[..., :3].astype(numpy.float32).mean(axis=2)
  m = span + 4
  best_s, best_e = 0, None
  e0 = None
  for s in range(-span, span + 1):
    e = float(numpy.abs(la[:, m:-m] - numpy.roll(lb, s, axis=1)[:, m:-m]).mean())
    if s == 0:
      e0 = e
    if best_e is None or e < best_e:
      best_s, best_e = s, e
  return best_s, best_e, e0

################################################################################

def main():
  code = 1
  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    ctx = app.ctx
    print("SPVRMV-CAPS mesh=%d multiview=%d mesh_multiview=%d max_mesh_views=%d"
          % (int(ctx.supports_mesh_shader), int(ctx.supports_multiview),
             int(ctx.supports_multiview_mesh_shader), int(ctx.max_mesh_multiview_views)))
    reason = None
    if not ctx.supports_mesh_shader:
      reason = "no VK_EXT_mesh_shader"
    elif not ctx.supports_multiview:
      reason = "no core multiview"
    elif not ctx.supports_multiview_mesh_shader:
      reason = "multiviewMeshShader not enabled on this device"
    elif ctx.max_mesh_multiview_views < NVIEWS:
      reason = "maxMeshMultiviewViewCount %d < %d" % (ctx.max_mesh_multiview_views, NVIEWS)
    if reason:
      print("SKIP: %s" % reason)
      sys.exit(verdict(True, "mesh-stage multiview gate SKIPPED — %s" % reason))

    sp = Stereo(app)
    print("SPVRMV-CFG view_mask=0x%x layers=%d mono_view_mask=0x%x groups=%s baseline=%.1f"
          % (sp.rtg_mv.view_mask, sp.rtg_mv.numLayers, sp.rtg_mono.view_mask,
             sp.groups, BASELINE))
    assert sp.rtg_mv.view_mask == 0b11, "multiview RTG did not report viewMask 0b11"
    assert sp.rtg_mono.view_mask == 0, "control RTG must not be multiview"

    (eyeL, tgtL), (eyeR, tgtR) = eye_pair()
    vpL, vpR = view_proj(eyeL, tgtL), view_proj(eyeR, tgtR)

    # ---- multiview: ONE draw, both views, read back layer by layer ----
    sp.set_cull_view(eyeL, tgtL)
    sp.set_view_matrices([vpL, vpR])
    mv0 = sp.render("mv", sp.rtg_mv, sp.rtb_mv, 0)
    mv1 = sp.render("mv", sp.rtg_mv, sp.rtb_mv, 1)

    # ---- mono controls: same geometry, one eye per pass, separate material ----
    sp.set_view_matrices([vpL, vpL])
    monoL = sp.render("mono", sp.rtg_mono, sp.rtb_mono, 0)
    sp.set_view_matrices([vpR, vpR])
    monoR = sp.render("mono", sp.rtg_mono, sp.rtb_mono, 0)

    cov = [coverage(x) for x in (mv0, mv1, monoL, monoR)]
    m_l  = exact_match(mv0, monoL)
    m_r  = exact_match(mv1, monoR)
    m_ll = exact_match(mv0, mv1)
    s_mv, e_mv, e0_mv     = best_shift(mv0, mv1)
    s_mono, e_mono, e0_mono = best_shift(monoL, monoR)

    print("SPVRMV-COV  mv0=%.4f mv1=%.4f monoL=%.4f monoR=%.4f" % tuple(cov))
    print("SPVRMV-MATCH mv0_vs_monoL=%.4f mv1_vs_monoR=%.4f mv0_vs_mv1=%.4f" % (m_l, m_r, m_ll))
    print("SPVRMV-PARALLAX multiview_shift=%d err=%.3f err0=%.3f | mono_shift=%d err=%.3f err0=%.3f"
          % (s_mv, e_mv, e0_mv, s_mono, e_mono, e0_mono))

    fails = []
    for name, c in zip(("mv0", "mv1", "monoL", "monoR"), cov):
      if not (0.10 < c < 0.98):
        fails.append("%s coverage %.4f outside terrain-shaped range" % (name, c))
    if m_l < 0.98:
      fails.append("layer0 does not match the left mono view (%.4f)" % m_l)
    if m_r < 0.98:
      fails.append("layer1 does not match the right mono view (%.4f)" % m_r)
    if m_ll > 0.90:
      fails.append("the two layers are the same image (%.4f) — no parallax" % m_ll)
    if s_mv == 0 or e_mv >= e0_mv:
      fails.append("no horizontal disparity between layers (shift=%d)" % s_mv)
    if s_mv != s_mono:
      fails.append("layer disparity %d != mono-pair disparity %d" % (s_mv, s_mono))

    # the layer's own verdict on the pass, read off the context (see validation_armed)
    armed, verrs = bool(ctx.validation_armed), int(ctx.validation_errors)
    print("SPVRMV-VALIDATION armed=%d errors=%d" % (int(armed), verrs))
    if not armed:
      fails.append("validation layer not loaded — the zero-error claim is uncovered")
    elif verrs:
      fails.append("%d validation error(s) during the multiview mesh pass" % verrs)

    detail = ("mesh-stage multiview gate | viewMask=0x%x match(L=%.4f R=%.4f LL=%.4f) "
              "disparity mv=%d mono=%d validation(armed=%d errors=%d)"
              % (sp.rtg_mv.view_mask, m_l, m_r, m_ll, s_mv, s_mono, int(armed), verrs))
    code = verdict(not fails, detail if not fails else detail + " | " + "; ".join(fails))

  sys.exit(code)


main()
