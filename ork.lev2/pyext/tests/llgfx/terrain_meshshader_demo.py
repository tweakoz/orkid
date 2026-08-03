#!/usr/bin/env ork.python
################################################################################
# WINDOWED terrain mesh-shader demo — the owner's eyeball vehicle for the §12
# prototype. Same procedural heightfield, same SSBO layout, same generated
# geometry (TerrainChunkVertexSource) as the offscreen A/B gate
# test_terrain_meshshader_ab.py; the difference is that this one presents to a
# real display and prints live frame timing instead of asserting pixel parity.
#
#   baseline (compute cull -> v_list + VkDrawIndirectCommand -> DrawIndirectEML):
#     ./ork.lev2/pyext/tests/llgfx/terrain_meshshader_demo.py
#
#   mesh shader (taskless VK_EXT_mesh_shader, fixed workgroup grid, self-cull):
#     ./ork.lev2/pyext/tests/llgfx/terrain_meshshader_demo.py --mesh
#
# on macOS the mesh path needs the PR#2777 MoltenVK ICD in front of it:
#   env VK_ICD_FILENAMES=<moltenvk-build>/Package/Latest/MoltenVK/dylib/macOS/MoltenVK_icd.json ./ork.lev2/pyext/tests/llgfx/terrain_meshshader_demo.py --mesh --seconds 60
#
# ORKID_TERRAIN_MESHSHADER=1 selects the mesh path too (same env the C++ chunk
# drawable's toggle reads). Path selection is ALWAYS gated on the live caps
# query: mesh requested on a device without the extension prints the fallback
# banner and runs baseline.
#
# Platform: no platform conditionals here — a plain windowed OrkEzApp. On linux
# ORKID_DRM_MODE in the environment is picked up by
# AppInitData::finalizeInitialization (linux-guarded, C++ side) and the app scans
# out on that display; on macOS it is an ordinary window. Ctrl-C exits cleanly;
# --seconds N self-exits through the same signalExit path, for remote-driven runs
# where Ctrl-C is unavailable.
#
# NOT a gate (no verdict line, no committed-test obligations, unbounded by
# default) — the pixel-parity/timing gate is test_terrain_meshshader_ab.py. The
# ork.testing harness is offscreen-only, hence the hand-rolled windowed boot.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)
import argparse, math, signal, time

# repo root = five levels up; prepend THIS checkout's scripts dir so the terrain
# generator resolves from the same tree as this script.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import numpy

from orkengine import core   # core before lev2
from orkengine import lev2
from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource

tokens = core.CrcStringProxy()

DIM    = 1024      # heightfield grid (procedural)
EXTENT = 2000.0    # world meters per side
HMAX   = 180.0     # world meters of relief
CHUNK  = 128       # -> 8x8 chunks, 12x12 meshlets each

# slow orbit: one revolution per ORBIT_SECS, with a gentle altitude breathe so the
# frustum cull visibly opens and closes over the run.
ORBIT_SECS   = 90.0
ORBIT_RADIUS = 0.62 * EXTENT
ORBIT_HEIGHT = 0.22 * EXTENT
ORBIT_TGT    = core.vec3(0.0, 0.25 * HMAX, 0.0)

################################################################################

def synth_heights(dim, hmax):
  """Procedural heightfield in TRUE METERS (ridges + a central dome) — the SSBO's
  heights[] contract. Identical to the A/B gate's field; only EXTENT/HMAX differ."""
  xs = (numpy.arange(dim, dtype=numpy.float32) + 0.5) / float(dim) - 0.5
  X, Z = numpy.meshgrid(xs, xs, indexing="xy")
  h = (0.55
       + 0.25 * numpy.sin(X * 14.0) * numpy.cos(Z * 11.0)
       + 0.20 * numpy.exp(-((X * 2.4) ** 2 + (Z * 2.4) ** 2)))
  return (numpy.clip(h, 0.0, 1.0) * hmax).astype(numpy.float32).reshape(-1)

################################################################################

def _indent(text, n):
  pad = " " * n
  return "\n".join(pad + ln for ln in text.strip().splitlines())


MESH_TEXT = """
mesh_shader ms_terrain : extension(GL_EXT_mesh_shader) : vif_terrain_mesh : lib_terr {
%(MESHBODY)s
}
////////////////////////////////////////
technique tek_mesh {
  fxconfig = fxcfg_default;
  pass p0 { mesh_shader = ms_terrain; fragment_shader = ps_terrain; state_block = default; }
}
"""


def build_shader(vs, with_mesh):
  """The A/B gate's program, minus the parity apparatus: every geometry line comes from
  the generator; only the interfaces, the shade helper and the technique declarations are
  local. The MESH stage is spliced in ONLY when the device advertises VK_EXT_mesh_shader —
  a mesh-carrying program must never reach shader-module creation on a device without it."""
  meshiface = vs.mesh_interface(outputs="vec4 frg_clr;") if with_mesh else ""
  meshtek = (MESH_TEXT % dict(
      MESHBODY=_indent(vs.mesh_body(mvp="c_vp",
                                    varying_writes="frg_clr[$V] = terr_shade(normal, uv0);"), 2))
      ) if with_mesh else ""
  return """
fxconfig fxcfg_default {}
////////////////////////////////////////
storage_interface sif_ptex_vtx (descriptor_set 0) {
  buffer layout(std430) ptex_vtx_data {
%(LAYOUT)s
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
vertex_interface vif_terrain : sif_ptex_vtx {
  outputs { vec4 frg_clr; }
}
%(MESHIFACE)s
fragment_interface fif_terrain : vif_terrain {
  outputs { layout(location = 0) vec4 out_clr; }
}
////////////////////////////////////////
vertex_shader vs_terrain : vif_terrain : lib_terr {
%(VSBODY)s
  gl_Position = c_vp * position;
  frg_clr = terr_shade(normal, uv0);
}
////////////////////////////////////////
fragment_shader ps_terrain : fif_terrain {
  out_clr = frg_clr;
}
////////////////////////////////////////
technique tek_vtx {
  fxconfig = fxcfg_default;
  pass p0 { vertex_shader = vs_terrain; fragment_shader = ps_terrain; state_block = default; }
}
%(MESHTEK)s
////////////////////////////////////////
%(COMPUTE)s
""" % dict(LAYOUT=_indent(vs.layout, 4),
           LIB=_indent(vs.lib, 2),
           MESHIFACE=meshiface,
           VSBODY=_indent(vs.vs_body, 2),
           MESHTEK=meshtek,
           COMPUTE=vs.compute)

################################################################################

class TerrainDemoApp:
  """Windowed ezapp: SSBO + material built in onGpuInit, camera upload + (baseline)
  cull dispatch in onGpuUpdate (outside beginFrame, where compute belongs), one terrain
  pass into main_RTG in onDraw."""

  def __init__(self, args):
    self._args = args
    self._want_mesh = bool(args.mesh) or (os.environ.get("ORKID_TERRAIN_MESHSHADER", "") == "1")
    self.ezapp = lev2.OrkEzApp.create(self, width=args.width, height=args.height,
                                      fullscreen=args.fullscreen, ssaa=0)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    signal.signal(signal.SIGINT, lambda s, f: self._requestExit("SIGINT"))
    self.ssbo = None
    self.path = "vtx"
    self._clear_set = False
    self._exiting = False
    self._t0 = None        # first gpu-update timestamp (runtime clock + orbit phase)
    self._t_bin = None     # start of the current 1-second reporting bin
    self._bin_frames = 0
    self._bin_cull = 0.0

  def _requestExit(self, why):
    """The ONE exit path (SIGINT and --seconds share it). _exiting also stops onGpuUpdate
    from issuing new GPU work: OrkEzApp::joinUpdate keeps pumping the gpu-update hook
    while it drains the update thread, so this callback still fires during teardown."""
    if self._exiting:
      return
    self._exiting = True
    print("[TERRAIN-DEMO] exit requested (%s)" % why)
    self.ezapp.signalExit()

  ##############################################

  def onGpuInit(self, ctx):
    self.ctx = ctx
    # mesh_indirect=False FORCED: this demo issues the DIRECT drawMeshTasks over the fixed grid,
    # so it must get the dense chunk decode even when the ambient ORKID_TERRAIN_MESHSHADER asks
    # for mode 2 (the compacted decode reads a visible list this demo never fills -> wrong image).
    self.vs = TerrainChunkVertexSource(dim=DIM, extent_m=EXTENT, chunk=CHUNK, mesh_indirect=False)
    fxi = ctx.FXI

    ############################################
    # the SSBO: heights + u_dim + the global height bounds. chunk_y[] stays zeroed —
    # it feeds the HZB occlusion test only, and c_misc.y = 0 disables that.
    ############################################
    heights = synth_heights(DIM, HMAX)
    self.ssbo = fxi.createShaderStorageBufferWithLength(self.vs.TOTAL)
    fxi.copyDataIntoShaderStorageBuffer(heights, self.ssbo, self.vs.HEIGHTS_OFF)
    self.vs.upload_dim(fxi, self.ssbo)
    fxi.copyDataIntoShaderStorageBuffer(
        numpy.array([float(heights.min()), float(heights.max())], dtype=numpy.float32),
        self.ssbo, self.vs.YB_OFF)

    ############################################
    # path selection against the LIVE caps query
    ############################################
    self.caps_mesh = bool(ctx.supports_mesh_shader)
    self.path = "mesh" if (self._want_mesh and self.caps_mesh) else "vtx"

    ############################################
    # material + the selected pipeline
    ############################################
    self.mtl = lev2.FreestyleMaterial()
    self.mtl.gpuInitFromShaderText(ctx, "terrain_demo", build_shader(self.vs, self.caps_mesh))
    self.mtl.rasterstate.culltest = tokens.OFF        # one winding shared by both paths
    self.mtl.rasterstate.depthtest = tokens.LEQUALS   # a heightfield self-occludes

    tekname = "tek_mesh" if self.path == "mesh" else "tek_vtx"
    self.tek = self.mtl.shader.technique(tekname)
    assert self.tek, "technique %s not found" % tekname
    permu = lev2.FxPipelinePermutation()
    permu.rendermodel = "CUSTOM"
    permu.technique = self.tek
    self.pipe = self.mtl.fxcache.findPipeline(permu)
    assert self.pipe, "no pipeline for %s" % tekname
    self.pipe.bindStorage(self.mtl.storage("sif_ptex_vtx"), self.ssbo)

    self.cs = {}
    if self.path == "vtx":
      for (cs_name, _gx, _gy, _gz) in self.vs.compute_passes():
        shader = self.mtl.computeShader(cs_name)
        assert shader, "compute shader %s missing" % cs_name
        self.cs[cs_name] = shader
    self.groups = self.vs.mesh_groups()

    ############################################
    # LOUD banner
    ############################################
    gx, gy, gz = self.groups
    print("=" * 96)
    if self._want_mesh and not self.caps_mesh:
      print("[TERRAIN-DEMO] *** MESH PATH REQUESTED BUT UNSUPPORTED "
            "(supports_mesh_shader=False) -> FALLING BACK TO BASELINE compute+indirect ***")
    print("[TERRAIN-DEMO] PATH=%s  supports_mesh_shader=%s  dim=%d extent=%.0fm relief=%.0fm "
          "chunks=%d  %s"
          % (self.path.upper(), self.caps_mesh, DIM, EXTENT, HMAX, self.vs.nchunk,
             ("mesh grid <%d,%d,%d> = %d workgroups, no compute" % (gx, gy, gz, gx * gy * gz))
             if self.path == "mesh"
             else ("%d verts/chunk pulled indirect + 4 compute passes" % self.vs.vpc)))
    print("=" * 96)

  ##############################################

  def _setView(self, ctx, t):
    """Write the CamBlk that the cull compute AND both draw paths read (c_vp is the only
    clip-space matrix in play — there are no uniform blocks here)."""
    theta = (t / ORBIT_SECS) * 2.0 * math.pi
    breathe = 1.0 + 0.25 * math.sin(t * 0.13)
    eye = core.vec3(math.sin(theta) * ORBIT_RADIUS,
                    ORBIT_HEIGHT * breathe,
                    math.cos(theta) * ORBIT_RADIUS)
    w = max(1, ctx.mainSurfaceWidth())
    h = max(1, ctx.mainSurfaceHeight())
    # near/far ratio 4000:1, same as the A/B gate's — the camera never gets within ~800m
    # of the field, so a tight near plane would only cost depth precision.
    proj = core.mtx4.perspective(45.0 * math.pi / 180.0, float(w) / float(h), 2.0, 4.0 * EXTENT)
    view = core.mtx4.lookAt(eye, ORBIT_TGT, core.vec3(0, 1, 0))
    vp = proj * view   # orkid operator* is rtol; matches CameraMatrices::multiply_ltor(v,p)
    fxi = ctx.FXI
    fxi.copyDataIntoShaderStorageBuffer(numpy.array(vp, dtype=numpy.float32).reshape(-1),
                                        self.ssbo, self.vs.CAM_OFF)
    fxi.copyDataIntoShaderStorageBuffer(numpy.array(vp.inverse, dtype=numpy.float32).reshape(-1),
                                        self.ssbo, self.vs.CAM_OFF + 64)
    fxi.copyDataIntoShaderStorageBuffer(numpy.array([eye.x, eye.y, eye.z, 1.0], dtype=numpy.float32),
                                        self.ssbo, self.vs.CAM_OFF + 128)
    # misc.x = CullFrustumScale 1.0 (exact frustum), misc.yzw = 0 -> HZB occlusion off
    fxi.copyDataIntoShaderStorageBuffer(numpy.array([1.0, 0.0, 0.0, 0.0], dtype=numpy.float32),
                                        self.ssbo, self.vs.CAM_OFF + 144)

  def _runCompute(self, ctx):
    """The baseline's per-frame GPU work: reset -> cull -> sort -> finalize in one dispatch
    phase (endDispatchPhase submits AND waits, exactly as ComputeDrawable::onPreRender does),
    so the wall-clock around it is a real per-frame cull cost."""
    ci = ctx.CI
    passes = self.vs.compute_passes()
    ci.beginDispatchPhase()
    for i, (cs_name, gx, gy, gz) in enumerate(passes):
      shader = self.cs[cs_name]
      ci.bindStorageBuffer(shader, 0, self.ssbo)
      ci.bindStorageBuffer(shader, 1, self.ssbo)  # sif_hzb stand-in; misc.y == 0 -> never read
      ci.dispatch(shader, gx, gy, gz)
      if (i + 1) < len(passes):
        ci.storageBarrier()
    ci.endDispatchPhase()

  def _visibleChunks(self, ctx):
    """The cull funnel the baseline just wrote: (visible, frustum-pass, total)."""
    import struct
    m = ctx.FXI.mapStorageBuffer(self.ssbo, self.vs.VIS_OFF, 16, tokens.READ_ONLY)
    vals = struct.unpack("<4I", bytes(m.data)[:16])
    ctx.FXI.unmapStorageBuffer(m)
    return vals[0], vals[1], vals[3]

  ##############################################

  def onGpuUpdate(self, ctx):
    if self.ssbo is None or self._exiting:
      return   # gpu init has not landed yet / teardown in progress (see _requestExit)
    now = time.perf_counter()
    if self._t0 is None:
      self._t0, self._t_bin = now, now
    self._setView(ctx, now - self._t0)
    if self.path == "vtx":
      t = time.perf_counter()
      self._runCompute(ctx)
      self._bin_cull += (time.perf_counter() - t) * 1000.0
    self._bin_frames += 1
    ############################################
    # once-per-second stdout line (the owner watches the terminal too). Frame time is
    # PRESENT-paced — vsync/refresh bounds it; the cull column is a real submit+wait.
    ############################################
    binlen = now - self._t_bin
    if binlen >= 1.0 and self._bin_frames > 0:
      n = self._bin_frames
      extra = ""
      if self.path == "vtx":
        vis, _frustum, total = self._visibleChunks(ctx)
        extra = "  cull=%.3fms  chunks=%d/%d" % (self._bin_cull / n, vis, total)
      else:
        extra = "  meshgroups=%d" % (self.groups[0] * self.groups[1] * self.groups[2])
      print("[TERRAIN-DEMO] path=%-4s  fps=%6.1f  frame=%6.2fms%s"
            % (self.path, n / binlen, binlen * 1000.0 / n, extra))
      self._t_bin, self._bin_frames, self._bin_cull = now, 0, 0.0
    ############################################
    if self._args.seconds > 0.0 and (now - self._t0) >= self._args.seconds:
      self._requestExit("--seconds %g elapsed" % self._args.seconds)

  def onDraw(self, drawevent):
    # the ezapp already did beginFrame (EzTopWidget::DoDraw) and does endFrame + present
    # after this returns. On a WINDOW target the push IS the clear — autoclear puts
    # loadOp=CLEAR on color+depth — and a second rtGroupClear of the already-active
    # main_RTG would only end and resume the pass (VkFBI::_pushRtGroup: a redundant push
    # on a WINDOW target does not begin a new pass), so unlike the offscreen A/B there is
    # no explicit clear call here.
    ctx = drawevent.context
    rtg = ctx.FBI.main_RTG
    if not self._clear_set:
      rtg.buffer(0).clearColor = core.vec4(0.06, 0.09, 0.15, 1.0)   # sky-ish, so cull shows
      self._clear_set = True
    ctx.FBI.rtGroupPush(rtg)
    if self.ssbo is not None:
      RCFD = lev2.RenderContextFrameData(ctx)
      RCID = lev2.RenderContextInstData(RCFD)
      RCID.forceTechnique(self.tek)
      RCID.genMatrix(lambda: core.mtx4())
      if self.path == "vtx":
        self.pipe.wrappedDrawCall(RCID, lambda: ctx.GBI.drawIndirect(
            args=self.ssbo, primtype=tokens.TRIANGLES, args_offset=self.vs.ARGS_OFF))
      else:
        gx, gy, gz = self.groups
        self.pipe.wrappedDrawCall(RCID, lambda: ctx.GBI.drawMeshTasks(gx, gy, gz))
    ctx.FBI.rtGroupPop()

################################################################################

def main():
  parser = argparse.ArgumentParser(description="windowed terrain mesh-shader demo")
  parser.add_argument("--mesh", action="store_true",
                      help="use the taskless mesh-shader path (default: compute+indirect baseline)")
  parser.add_argument("--seconds", type=float, default=0.0,
                      help="self-exit after N seconds (0 = run until SIGINT)")
  parser.add_argument("--width", type=int, default=1920)
  parser.add_argument("--height", type=int, default=1080)
  parser.add_argument("--fullscreen", action="store_true")
  app = TerrainDemoApp(parser.parse_args())
  app.ezapp.mainThreadLoop()


main()
