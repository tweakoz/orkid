#!/usr/bin/env ork.python
###############################################################################
# section_array_bake.py — O3 per-section texture-ARRAY gate (offscreen).
#
# Stage 2: the REAL GPU material bake driver. Exercises the full O3 path end to end:
#   SdfBaked asset: box -> sdf_to_mesh_clean(unwrap=False) -> gid partition (2 sections)
#                   -> section_unwrap (per-section 0-1 UV + layer index in UV0.z)
#   material SectionArray: capture=True — surface_stored() samples ONE sampler2DArray at
#                          ctx.layer (ctx.texArray); a PROCEDURAL grain (self.capture) is
#                          baked per section into that section's array LAYER.
#   prepareSectionBake (C++): renders each section's gid bucket into its own 2D MRT via the
#                          material's FWD_SSBO_CUSTOM_CAPTURE technique + the MoltenVK-safe
#                          section cap-VS (mvp over the 0-1 UV domain), host round-trips each.
#   bake_section_array: assembles the per-section host captures into the array + content-
#                          address-caches them (COLD writes the cache, WARM loads it).
#
# Renders offscreen and asserts the frame is non-black AND multi-colored (the two sections
# sample DIFFERENT array layers with DIFFERENT baked surface content), then proves the cache
# goes COLD (GPU bake) then WARM (load). This JIT-compiles the new shadlang (sampler2DArray +
# ctx.layer + the section cap-VS), which a C++ build does NOT — the runtime proof of the driver.
#
#   run:  MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1 ork.python section_array_bake.py
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, time
import numpy as np
from orkengine.core import vec3, CrcStringProxy, asyncWorkPending, asyncWorkSummary   # core before lev2
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.hypergraph.dflow.hypermesh import make_drawable
from ork.hypergraph.assets.hypermesh.sdf_baked import SdfBaked
from ork.hypergraph.assets.materials.hypermesh.section_array import SectionArray
from ork.hypergraph.ptex3d.section_bake import (
    bake_section_array, placeholder_section_array, section_array_warm, job_content_fn)

tokens = CrcStringProxy()
DIM = 512
BAKE_RES = 256
INSTANCED = os.environ.get("SECTION_ARRAY_INSTANCED", "0") == "1"  # the instanced variant sets this env
N_INSTANCES = 3                        # instanced variant: distinct row of cubes proving multiplicity
INSTANCE_SPACING = 3.5                 # world-X gap between instances (guarantees separated screen clusters)
OUT = os.environ.get("SECTION_ARRAY_OUT", "/tmp/section_array_bake.png")


def _hclusters(mask, min_gap=4, min_cols=3):
  """Horizontally-separated lit-pixel CLUSTERS: per-column occupancy (any lit pixel in the column), then
  contiguous occupied runs separated by >= min_gap empty columns (bridging sub-min_gap anti-alias gaps),
  keeping only runs spanning >= min_cols columns. Returns the list of (col_start, col_end). This is the
  instance-multiplicity oracle — N separated cubes -> N clusters; overlapping/collapsed instances -> 1
  (the exact failure the gate-runner caught by eye). No scipy dependency."""
  occ = mask.any(axis=0)             # per-column: is any row lit?
  runs = []
  start = None
  gap = 0
  for i, c in enumerate(occ):
    if c:
      if start is None:
        start = i
      end = i
      gap = 0
    else:
      if start is not None:
        gap += 1
        if gap >= min_gap:
          runs.append((start, end))
          start = None
  if start is not None:
    runs.append((start, end))
  return [(a, b) for (a, b) in runs if (b - a + 1) >= min_cols]


class App(ComponentizedApplication):
  def __init__(self):
    super().__init__()
    self._frame = 0
    self._done = False
    self._want_exit = False
    self._future = None
    self._capbuf = None
    self._job = None
    self._state = "init"               # init -> cold_wait -> render
    self.result = None
    self.SGC = self.addComponent("std_scenegraph", StandardSceneGraphComponent,
                                 eye=vec3(6, 5, 9), tgt=vec3(0, 0, 0), up=vec3(0, 1, 0),
                                 grid_variant=None)
    self.createEzApp(enable_lockstep_ups=True, enable_lockstep_fps=True,
                     enable_freerun_ups=True, enable_freerun_fps=True,
                     freerun=False, target_ups=60, target_fps=60,
                     width=DIM, height=DIM, use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  def _make_drawable(self, ctx):
    asset = SdfBaked()
    self._live = asset.materialize_live(ctx)
    if INSTANCED:
      # N instances placed on a row along world X (the instanced coverage variant). Bake stays
      # non-instanced. make_drawable REQUIRES COLUMN-MAJOR mat4 floats: the translation is the 4th
      # column (flat indices 12,13,14). (Row-major here would drop tx into the projective bottom-row
      # slot [3], which setupMeshRender extracts to per-instance attrs + ZEROES -> all instances collapse
      # to identity -> one overlapping cube. That was the original G3 miss.)
      inst = []
      for i in range(N_INSTANCES):
        tx = (i - (N_INSTANCES - 1) * 0.5) * INSTANCE_SPACING
        inst += [1.0, 0.0, 0.0, 0.0,     # col 0
                 0.0, 1.0, 0.0, 0.0,     # col 1
                 0.0, 0.0, 1.0, 0.0,     # col 2
                 tx,  0.0, 0.0, 1.0]     # col 3 = translation (tx,0,0)
      cdd, gmtl = make_drawable(self._live, ctx, animated=False, material_cls=SectionArray, instances=inst)
    else:
      cdd, gmtl = make_drawable(self._live, ctx, animated=False, material_cls=SectionArray)
    return asset, cdd, gmtl

  def _onGpuInit(self, ctx):
    self._ctx = ctx
    self.ezapp.topWidget.enableUiDraw()
    asset, cdd, gmtl = self._make_drawable(ctx)
    self._gmtl = gmtl
    # A8: the layer->gid table drives one bake draw per layer via the right gid bucket (NEVER the
    # ascending-gid==layer coincidence). Derived from the assembled mesh.
    layer_gids = list(lev2.hypermesh.sectionLayerGids(self._live, ctx))
    if len(layer_gids) < 1:
      raise RuntimeError("section_array_gate: SectionUnwrap produced no layer->gid table (empty mesh?)")
    num_layers = len(layer_gids)
    self._layer_gids = layer_gids
    self._num_layers = num_layers
    key = "section_array_gate_stage2::SdfBaked::%d" % num_layers
    self._key = key
    warm, cache_dir = section_array_warm(key, num_layers, BAKE_RES)
    self._cache_dir = cache_dir
    self._cold_was_cold = (not warm)
    if warm:
      arr, cache_dir, _ = bake_section_array(ctx, key=key, num_layers=num_layers, bake_res=BAKE_RES)
      gmtl.bindParam(SectionArray.ARRAY_SAMPLER, arr)
      self._arr = arr
      self._state = "render"
    else:
      # COLD: bind a valid gray placeholder (the stored sampler needs a shader-readable array during the
      # few-frame async bake), trigger the C++ in-frame bake one-shot, then finalize when it completes.
      self._arr = placeholder_section_array(ctx, num_layers=num_layers, bake_res=BAKE_RES)
      gmtl.bindParam(SectionArray.ARRAY_SAMPLER, self._arr)
      # prepareSectionBake MUST run before the node is created (the one-shot is copied at node creation).
      self._job = lev2.hypermesh.prepareSectionBake(
          cdd, self._live, gmtl, layer_gids, ctx, bake_res=BAKE_RES, num_targets=1)
      self._state = "cold_wait"
    self.node = self.SGC.layer_fwd.createDrawableNodeFromData("secarray", cdd)
    self.SGC.pbr_common.enable_skybox = False
    print("section_array_gate: materialized verts=%d faces=%d layers=%d gids=%r instanced=%d %s"
          % (self._live.mesh.num_verts, self._live.mesh.num_faces, num_layers, layer_gids,
             int(INSTANCED), "WARM" if warm else "COLD(GPU bake)"), flush=True)

  def _onUpdate(self, updinfo):
    if not self._done:
      self.SGC.scenegraph.updateScene(self.SGC.cameralut)

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    if self._want_exit:
      self._want_exit = False
      self.ezapp.signalExit()
      return
    if self._done:
      return
    self._frame += 1
    if self._state == "cold_wait":
      if self._job is not None and bool(self._job.is_ready):
        arr, cache_dir, warm = bake_section_array(
            ctx, key=self._key, num_layers=self._num_layers, bake_res=BAKE_RES,
            content_fn=job_content_fn(self._job))
        self._gmtl.bindParam(SectionArray.ARRAY_SAMPLER, arr)
        self._arr = arr                 # rebind: the REAL baked array (placeholder drops out)
        self._state = "render"
        self._frame = 0                 # restart the IBL settle window for the capture
        print("section_array_gate: GPU bake COMPLETE (%d layers) -> array rebound" % self._num_layers, flush=True)
      return
    if self._future is None:
      # settle: the async IBL envmap (.xir) GPU upload must COMPLETE before the lit-PBR albedo
      # reads. A fixed frame count RACES the upload on fast GPUs (RADV) — a BLACK capture — so
      # gate on the async-tracker registry (asyncWorkPending, exactly what the offscreen player
      # settles on) in addition to the settle floor. Fail loud if it never drains.
      if self._frame >= 320 and asyncWorkPending() == 0:
        rtg = getattr(self.SGC.SGVPW, "rtgroup", None)
        if rtg is None or rtg.numBuffers < 1:
          return
        self._capbuf = lev2.CaptureBuffer()
        self._future = ctx.FBI.captureAsFormat(rtg.buffer(0), self._capbuf, "RGBA8")
      elif self._frame > 4000:
        raise RuntimeError("section_array_gate: async work never drained after %d frames (pending=%d %s)"
                           % (self._frame, asyncWorkPending(), asyncWorkSummary()))
      return
    if not bool(self._future.is_ready):
      return
    self._finish()

  def _finish(self):
    arr = np.array(self._capbuf, dtype=np.uint8).reshape(self._capbuf.height, self._capbuf.width, 4)
    rgb = arr[..., :3].astype(np.float32)
    mean = float(rgb.mean())
    mx = int(rgb.max())
    # section discrimination: mask the object (non-black px), measure per-channel spread across it.
    lum = rgb.mean(axis=2)
    obj = lum > 8.0
    npx = int(obj.sum())
    spread = 0.0
    if npx > 50:
      hue = rgb[obj]
      # channel ratios differ between the two section layers -> stddev of (R-B) over the object.
      spread = float((hue[:, 0] - hue[:, 2]).std())
    # INSTANCE-MULTIPLICITY oracle: count horizontally-separated lit clusters. The instanced variant MUST
    # show N_INSTANCES separated cubes; a collapsed/single-technique draw shows 1 (fixed-means-observed).
    clusters = _hclusters(obj)
    ncluster = len(clusters)
    width = 0
    if clusters:
      width = int(max(b for _, b in clusters) - min(a for a, _ in clusters) + 1)
    try:
      from PIL import Image
      Image.fromarray(arr[..., :3]).transpose(Image.FLIP_TOP_BOTTOM).save(OUT)
    except Exception as e:
      print("PNG write error: %r" % e, flush=True)
    self.result = dict(mean=mean, mx=mx, npx=npx, spread=spread, ncluster=ncluster, width=width)
    print("section_array_gate: CAPTURE mean=%.2f max=%d objpx=%d section_spread=%.2f clusters=%d objwidth=%d -> %s"
          % (mean, mx, npx, spread, ncluster, width, OUT), flush=True)
    self._done = True
    self._want_exit = True


def main():
  app = App()
  app.ezapp.mainThreadLoop()
  r = app.result or {}
  # verdicts (run-order INDEPENDENT): the frame is non-black AND the two sections sample DIFFERENT
  # array layers (with DIFFERENT baked surface content), AND the content-addressed cache holds one PNG
  # per section. The cold-vs-warm status is INFORMATIONAL (first run GPU-bakes+writes -> COLD; any later
  # run loads -> WARM; both render the same baked content).
  nonblack = r.get("mx", 0) > 0 and r.get("mean", 0.0) > 1.0
  multi = r.get("spread", 0.0) > 2.0          # the 2 sections sample different array layers
  cold = getattr(app, "_cold_was_cold", False)
  npngs = 0
  try:
    import glob
    npngs = len(sorted(glob.glob(os.path.join(app._cache_dir, "layer*.png"))))
  except Exception:
    pass
  cache_ok = npngs >= 2
  # INSTANCE MULTIPLICITY (instanced variant only): the render MUST show >= N_INSTANCES separated cubes.
  # Non-instanced: exactly 1 cube. This oracle FAILS the exact 1-cube collapse the gate-runner caught.
  ncluster = int(r.get("ncluster", 0))
  want_clusters = N_INSTANCES if INSTANCED else 1
  mult_ok = (ncluster >= want_clusters) if INSTANCED else (ncluster == 1)
  passed = nonblack and multi and cache_ok and mult_ok
  print("SECTION_ARRAY_RESULT=%s nonblack=%d multi_section=%d cache_layers=%d clusters=%d/%d bake=%s instanced=%d"
        % ("PASS" if passed else "FAIL", int(nonblack), int(multi), npngs, ncluster, want_clusters,
           "COLD" if cold else "WARM", int(INSTANCED)), flush=True)
  sys.exit(0 if passed else 1)


if __name__ == "__main__":
  main()
