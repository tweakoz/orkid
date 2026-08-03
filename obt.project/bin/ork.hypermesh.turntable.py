#!/usr/bin/env ork.python
################################################################################
# Copyright 1996-2026, Michael T. Mayers. MIT License.
################################################################################
# ork.hypermesh.turntable.py — hypermesh TURNTABLE instrument (offscreen grader + visual viewer).
#
# Render a hypermesh recipe/asset ALONE (origin-centered, isolated) on a turntable
# under a FIXED, DETERMINISTIC lighting rig, from every azimuth x a few elevation
# rings, and grade each angle for RESIDUAL BLACK inside the object silhouette. The
# immediate consumer is the section-baked-building "residual black" hunt: a baked-
# mode turntable that FAILS is a SUCCESS (the instrument saw the black).
#
#   ork.hypermesh.turntable.py box                     # offscreen grade -> contact sheet + verdict
#   ork.hypermesh.turntable.py bld_pueblo_stepped --baked
#   ork.hypermesh.turntable.py box --view              # OPEN A WINDOW, watch it spin (no metrics)
#   ork.hypermesh.turntable.py bld_pueblo_stepped --baked --view
#   ork.hypermesh.turntable.py --selftest              # oracle negative-proof (no GPU)
#   ork.hypermesh.turntable.py box --frames 24 --elevations 0,30,60 --res 512
#
# TWO PRESENTERS, ONE RENDER CORE: the offscreen grader and the --view window share
# _setup_scene()/_install_rig()/_center_and_size()/_build_drawable()/_pump_bake()/
# _orbit_eye() verbatim — the same asset, same rig, same rotation math. --view only
# swaps the presentation (real window + continuous auto-spin) and runs NO metrics.
#
# OFFSCREEN OUTPUTS (under --out, default <OBT_STAGE>/turntable/<asset>_<mode>/):
#   contact_sheet.png   grid of every angle, labeled az/el (eyeball review)
#   frame_azNNN_elNN.png  per-frame renders
#   metrics.txt         per-angle black-fraction / mean-luma / lit-fraction
#   turntable.mp4       optional (--movie), best-effort via ffmpeg
# and a machine-parseable verdict line:
#   TURNTABLE_RESULT=PASS|FAIL frames=N worst_black=<frac>@azA elE mean_lit=<x>
#
# ---- IMPORT PROVENANCE (STEP 0) ----------------------------------------------
# This tool lives in a lane WORKTREE and must run its OWN copies of the ork.*
# python (not the installed staging). It PREPENDS <worktree>/obt.project/scripts
# to sys.path[0] BEFORE importing any ork.hypergraph module, then ASSERTS the
# resolved module __file__ is under the worktree (fail-loud otherwise). Verified:
# ork.python does NOT pre-import `ork`, so a plain sys.path.insert wins cleanly.
################################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, math, time, argparse, glob

# --- provenance: force the worktree's ork.* python onto the front of the path ---
_BIN_DIR   = os.path.dirname(os.path.abspath(__file__))
_WORKTREE  = os.path.dirname(_BIN_DIR)                       # <worktree>/obt.project
_SCRIPTS   = os.path.join(_WORKTREE, "scripts")
if _SCRIPTS not in sys.path:
  sys.path.insert(0, _SCRIPTS)


def _assert_provenance():
  import ork.hypergraph.dflow.hypermesh as _h
  resolved = os.path.abspath(_h.__file__)
  if not resolved.startswith(os.path.abspath(_SCRIPTS)):
    raise SystemExit("PROVENANCE FAIL: ork.hypergraph resolved to %s (expected under %s) — the tool is "
                     "running the installed staging, not this worktree." % (resolved, _SCRIPTS))
  return resolved


################################################################################
# LIGHTING RIG — fixed + documented (A8: these are the instrument's structural
# constants, not user tweakables of a shipped material).
#
# The residual-black metric needs a KNOWN, NON-BLACK background so the object
# silhouette (mask = pixels differing from bg) captures even BLACK-shaded object
# pixels — a black object over a black background is invisible and would be
# UNDERCOUNTED. The PBR forward path clears to BLACK when the skybox draw is off,
# so instead of a .xir envmap-as-background (whose gradient defeats the trivial
# mask) the rig uses a FLAT PROCEDURAL environment: one solid color that is BOTH
# the drawn background AND the (uniform) IBL source. Deterministic, no async .xir
# load lag, trivial mask. See the report's "deviation" note.
#
# LIT rig:  slate-blue solid environment (chromatically distinct from warm adobe/
#           timber so even a luma-matched object pixel differs by channel), a low
#           IBL fill + a small constant ambient floor (so legitimately shadowed
#           REAL geometry never dips below the black threshold, while a genuinely
#           broken zero-contribution / black-albedo region stays ~0 and is caught),
#           and ONE directional sun at a fixed el/az for directional form.
# UNLIT rig: neutral-gray solid environment at full diffuse, no sun, no ambient
#           tint contribution beyond the flat env -> ~pure albedo (× flat gray).
#           The discriminator: black in BOTH lit and unlit == a DATA problem
#           (black albedo/unbaked texture); black only in lit == shading.
################################################################################
from orkengine.core import vec3

LIT_ENV_COLOR   = vec3(0.09, 0.12, 0.22)   # solid environment = drawn background (slate blue)
LIT_DIFFUSE     = 0.35                      # IBL diffuse contribution
LIT_SPECULAR    = 0.30                      # IBL specular contribution
LIT_AMBIENT     = vec3(0.05)               # constant additive floor (lifts real shadowed geometry)
LIT_SKYBOXLEVEL = 1.0                       # background draw brightness

SUN_COLOR       = vec3(1.0, 0.97, 0.92)    # warm white
SUN_INTENSITY   = 3.5
SUN_ELEVATION   = 50.0                      # degrees above horizon
SUN_AZIMUTH     = 140.0                     # degrees

UNLIT_ENV_COLOR = vec3(0.60, 0.60, 0.60)   # neutral gray solid env (background + flat lighting)
UNLIT_DIFFUSE   = 1.0
UNLIT_SPECULAR  = 0.0
UNLIT_AMBIENT   = vec3(0.0)

VIEW_SPIN_DEG_S = 40.0   # --view azimuth spin rate (deg/sec); ~9s per revolution
FRAMING_FOVY_DEG = 45.0  # the scene camera's vertical FOV (std_scenegraph -> setupUiCameraX default).
                         # Framing sizes the orbit against this; for a non-square window the SMALLER
                         # dimension is height (landscape), so the vertical FOV governs the fill.

# ---- metric thresholds -------------------------------------------------------
BG_DIFF_TOL   = 16     # per-channel |px-bg| above which a pixel is "object" (in the silhouette mask)
BLACK_LUMA    = 16.0   # in-silhouette luma below this == "residual black" (0..255)
LIT_LUMA      = 60.0   # in-silhouette luma above this == "lit"
MIN_SILHOUETTE = 200   # a frame with fewer object pixels than this is treated as empty (BLANK)


################################################################################
# METRIC — pure numpy, no GPU. Shared by the live capture path and --selftest so
# the exact code that grades real frames is negative-proofed.
################################################################################

def _estimate_bg(rgb):
  """Background color = median of the 4 corner regions (object is centered -> corners are empty)."""
  import numpy as np
  h, w, _ = rgb.shape
  s = max(4, min(h, w) // 16)
  corners = np.concatenate([
      rgb[:s, :s].reshape(-1, 3), rgb[:s, -s:].reshape(-1, 3),
      rgb[-s:, :s].reshape(-1, 3), rgb[-s:, -s:].reshape(-1, 3)], axis=0)
  return np.median(corners, axis=0)


def grade_frame(rgb, bg=None):
  """Grade one RGB frame (HxWx3 uint8) for residual black inside the object silhouette.

  silhouette = pixels whose max per-channel abs-diff from the background exceeds BG_DIFF_TOL.
  Returns dict(black_frac, mean_luma, lit_frac, silhouette_px, bg). black_frac is the fraction
  of SILHOUETTE pixels whose luma < BLACK_LUMA (the residual-black measure)."""
  import numpy as np
  rgb = rgb.astype(np.float32)
  if bg is None:
    bg = _estimate_bg(rgb)
  bg = np.asarray(bg, dtype=np.float32)
  diff = np.abs(rgb - bg[None, None, :]).max(axis=2)
  mask = diff > BG_DIFF_TOL
  npx = int(mask.sum())
  luma = 0.299 * rgb[..., 0] + 0.587 * rgb[..., 1] + 0.114 * rgb[..., 2]
  if npx < MIN_SILHOUETTE:
    return dict(black_frac=0.0, mean_luma=0.0, lit_frac=0.0, silhouette_px=npx, bg=bg, empty=True)
  ml = luma[mask]
  black_frac = float((ml < BLACK_LUMA).sum()) / float(npx)
  lit_frac   = float((ml > LIT_LUMA).sum()) / float(npx)
  return dict(black_frac=black_frac, mean_luma=float(ml.mean()), lit_frac=lit_frac,
              silhouette_px=npx, bg=bg, empty=False)


def _selftest():
  """Negative-proof the oracle: a known-good frame (bright object, no interior black) must grade
  below the FAIL threshold; a known-bad frame (a black patch composited INSIDE the silhouette)
  must grade above it. Prints SELFTEST=PASS iff the metric discriminates."""
  import numpy as np
  H = W = 256
  bg = np.array([23, 31, 56], dtype=np.uint8)                 # a slate background (like the lit rig)
  frame = np.zeros((H, W, 3), dtype=np.uint8) + bg[None, None, :]
  # a bright, fully-shaded object disc in the middle (no interior black)
  yy, xx = np.mgrid[0:H, 0:W]
  r = np.sqrt((xx - W / 2) ** 2 + (yy - H / 2) ** 2)
  obj = r < 80
  good = frame.copy()
  good[obj] = np.array([180, 150, 120], dtype=np.uint8)       # warm lit surface
  g = grade_frame(good)
  # known-bad: same object, but stamp a black square INSIDE the disc
  bad = good.copy()
  bad[110:150, 110:150] = 0                                    # ~40x40 black patch, inside r<80
  b = grade_frame(bad)
  thresh = 0.05
  good_ok = (not g["empty"]) and g["black_frac"] <= thresh
  bad_ok  = (not b["empty"]) and b["black_frac"] >  thresh
  print("selftest: known-good black_frac=%.4f (want<=%.2f) sil=%d  | known-bad black_frac=%.4f (want>%.2f) sil=%d"
        % (g["black_frac"], thresh, g["silhouette_px"], b["black_frac"], thresh, b["silhouette_px"]), flush=True)
  ok = good_ok and bad_ok
  print("SELFTEST=%s" % ("PASS" if ok else "FAIL"), flush=True)
  return 0 if ok else 1


################################################################################
# TURNTABLE SCHEDULE + ORBIT MATH (shared by both presenters)
################################################################################

POLE_TOP = 90.0    # top-down frame (el=+90); azimuth degenerates -> ONE frame is full coverage
POLE_BOT = -90.0   # bottom-up frame (el=-90)


def _schedule(frames, elevations, poles=True):
  """(az_deg, el_deg) pairs: for each SIGNED elevation ring, `frames` azimuth steps over 360deg.
  If `poles`, append two single-frame pole caps (el=+90 top-down, el=-90 bottom-up) — one azimuth
  each, since azimuth degenerates at a pole. Poles come LAST so ring indexing is unaffected."""
  out = []
  for el in elevations:
    for i in range(frames):
      out.append((360.0 * i / frames, float(el)))
  if poles:
    out.append((0.0, POLE_TOP))
    out.append((0.0, POLE_BOT))
  return out


def _up_for_el(el_deg):
  """Camera up-vector, guarding the degenerate look-straight-down/up at the poles (the impostor
  bake idiom: when the view direction is near-vertical, swap the world-up to +Z)."""
  if abs(math.sin(math.radians(el_deg))) > 0.99:
    return vec3(0, 0, 1)
  return vec3(0, 1, 0)


def _orbit_eye(center, radius, az_deg, el_deg):
  a = math.radians(az_deg)
  e = math.radians(el_deg)
  return vec3(center.x + radius * math.cos(e) * math.cos(a),
              center.y + radius * math.sin(e),
              center.z + radius * math.cos(e) * math.sin(a))


def _frame_distance(r, fill, fovy_deg=FRAMING_FOVY_DEG):
  """CONSTANT orbit distance (same for every azimuth AND elevation) so a bounding SPHERE of radius
  r subtends `fill` of the smaller view dimension. Exact perspective projection: the sphere's
  silhouette has angular radius asin(r/d); its projected-radius fraction of the half-view is
  tan(asin(r/d)) / tan(fovy/2). Setting that == fill and solving for d:
      K = fill * tan(fovy/2);  s = r/d = K / sqrt(1+K^2);  d = r / s.
  Bounding-sphere (not per-axis) framing is the correct 'fill if possible' for tall/flat meshes:
  the long axis fills, the short axis shows margin, and the sphere fits at EVERY angle by
  construction — so the turntable never breathes."""
  half = math.radians(fovy_deg) * 0.5
  K = max(1e-6, fill) * math.tan(half)
  s = K / math.sqrt(1.0 + K * K)
  return r / max(1e-9, s)


def _subtended_fraction(r, d, fovy_deg=FRAMING_FOVY_DEG):
  """The fill fraction actually achieved at distance d (inverse of _frame_distance) — for logging."""
  half = math.radians(fovy_deg) * 0.5
  if d <= r:
    return 1.0
  return math.tan(math.asin(r / d)) / math.tan(half)


################################################################################
# SHARED RENDER CORE — the ONE scene builder both presenters call. Operates on an
# `app` that carries: app.SGC, app._args, app._stored, app._unlit, app._label. It
# sets app._asset/_live/node/_center/_radius/_aabb_r/_gmtl (+ baked-mode bake state),
# so there is a single implementation of asset->drawable->center->rig with no fork
# that can drift between the grader and the viewer.
################################################################################

def _setup_scene(app, ctx):
  from ork.hypergraph.assets.hypermesh._resolve import resolve_asset, load_asset_from_path
  import inspect
  args = app._args
  asset_cls = (load_asset_from_path(args.input) if args.input else resolve_asset(args.asset))
  # --baked passthrough: baked mode asks the recipe to section-unwrap its geometry (kwarg where
  # the constructor accepts it — e.g. Pueblo(sectioned=True)); proc leaves it byte-identical.
  kw = {}
  if app._stored:
    try:
      params = inspect.signature(asset_cls.__init__).parameters
      if "sectioned" in params or any(p.kind == inspect.Parameter.VAR_KEYWORD for p in params.values()):
        kw["sectioned"] = True
    except (ValueError, TypeError):
      pass
  app._asset = asset_cls(**kw)
  app._live  = app._asset.materialize_live(ctx)
  _build_drawable(app, ctx)
  _center_and_size(app, ctx)
  _install_rig(app, ctx)


def _build_drawable(app, ctx):
  from orkengine import lev2
  from ork.hypergraph.dflow.hypermesh import make_drawable
  args = app._args
  if app._stored:
    from ork.hypergraph.assets.materials.hypermesh.section_array import SectionArray
    from ork.hypergraph.ptex3d.section_bake import (
        bake_section_array, placeholder_section_array, section_array_warm, job_content_fn)
    app._SectionArray = SectionArray
    app._bake_section_array = bake_section_array
    app._job_content_fn = job_content_fn
    cdd, gmtl = make_drawable(app._live, ctx, animated=False, material_cls=SectionArray)
    app._gmtl = gmtl
    layer_gids = list(lev2.hypermesh.sectionLayerGids(app._live, ctx))
    if len(layer_gids) < 1:
      raise RuntimeError("turntable baked: SectionUnwrap produced no layer->gid table — the asset "
                         "is not section-unwrapped (needs --baked geometry, e.g. sectioned=True)")
    app._layer_gids = layer_gids
    app._num_layers = len(layer_gids)
    app._bake_res   = int(args.bake_res)
    app._key = "turntable::%s::%d" % (app._label, app._num_layers)
    warm, cache_dir = section_array_warm(app._key, app._num_layers, app._bake_res)
    app._cache_dir  = cache_dir
    app._cold       = not warm
    if warm:
      arr, _cd, _ = bake_section_array(ctx, key=app._key, num_layers=app._num_layers,
                                       bake_res=app._bake_res)
      gmtl.bindParam(SectionArray.ARRAY_SAMPLER, arr)
      app._arr = arr
      app._bake_done = True
    else:
      app._arr = placeholder_section_array(ctx, num_layers=app._num_layers, bake_res=app._bake_res)
      gmtl.bindParam(SectionArray.ARRAY_SAMPLER, app._arr)
      app._job = lev2.hypermesh.prepareSectionBake(
          cdd, app._live, gmtl, app._layer_gids, ctx, bake_res=app._bake_res, num_targets=1)
      app._bake_done = False
    app.node = app.SGC.layer_fwd.createDrawableNodeFromData("turntable", cdd)
    print("turntable: BAKED verts=%d faces=%d layers=%d gids=%r %s"
          % (app._live.mesh.num_verts, app._live.mesh.num_faces, app._num_layers,
             app._layer_gids, "WARM" if warm else "COLD(GPU bake)"), flush=True)
  else:
    # PROC mode: use the asset's OWN materials (primary + per-gid buckets), like the viewer.
    from ork.hypergraph.dflow.hypermesh import HmMaterial as _HmMat
    try:    mats = list(app._asset.materials() or [])
    except Exception: mats = []
    primary = next((m for m in mats if m.gid in (None, 0)), None)
    gid_mats = {int(m.gid): m for m in mats if m.gid not in (None, 0)}
    if primary is None:
      primary = _HmMat(type(app._asset).MATERIAL_CLASS, albedo=vec3(0.70, 0.74, 0.80), roughness=0.5)
    cdd, app._gmtl = make_drawable(
        app._live, ctx, animated=False,
        material_cls=primary.material_cls, albedo=primary.albedo,
        roughness=primary.roughness, metallic=primary.metallic,
        gid_materials=(gid_mats or None),
        instances=getattr(app._asset, "instances", None), instance_from=app._live)
    app.node = app.SGC.layer_fwd.createDrawableNodeFromData("turntable", cdd)
    app._bake_done = True
    print("turntable: PROC verts=%d faces=%d mats=%d(+%dgid)"
          % (app._live.mesh.num_verts, app._live.mesh.num_faces, 1, len(gid_mats)), flush=True)


def _center_and_size(app, ctx):
  from orkengine import lev2
  import tempfile
  objpath = os.path.join(tempfile.gettempdir(), "turntable_bounds_%d.obj" % os.getpid())
  lev2.hypermesh.dump_obj(app._live.mesh, ctx, objpath)
  lo = [1e30, 1e30, 1e30]; hi = [-1e30, -1e30, -1e30]
  with open(objpath) as f:
    for line in f:
      if line.startswith("v "):
        p = line.split()
        x, y, z = float(p[1]), float(p[2]), float(p[3])
        lo[0] = min(lo[0], x); lo[1] = min(lo[1], y); lo[2] = min(lo[2], z)
        hi[0] = max(hi[0], x); hi[1] = max(hi[1], y); hi[2] = max(hi[2], z)
  try: os.remove(objpath)
  except OSError: pass
  if lo[0] > hi[0]:
    raise RuntimeError("turntable: mesh has no vertices — cannot size the orbit")
  cx, cy, cz = (lo[0] + hi[0]) * 0.5, (lo[1] + hi[1]) * 0.5, (lo[2] + hi[2]) * 0.5
  r = 0.5 * math.sqrt((hi[0] - lo[0]) ** 2 + (hi[1] - lo[1]) ** 2 + (hi[2] - lo[2]) ** 2)
  r = max(r, 1e-3)
  app._center = vec3(0, 0, 0)                        # camera orbits the ORIGIN (mesh moved to it)
  app._aabb_r = r
  # FILL framing: one constant distance so the bounding sphere subtends `--fill` of the view.
  app._radius = app._args.margin * _frame_distance(r, app._args.fill)
  app._fill_actual = _subtended_fraction(r, app._radius)
  app.node.worldTransform.translation = vec3(-cx, -cy, -cz)   # AABB center -> origin


def _install_rig(app, ctx):
  from orkengine import lev2
  pc = app.SGC.pbr_common
  env_color = UNLIT_ENV_COLOR if app._unlit else LIT_ENV_COLOR
  rad = lev2.PbrCommon.makeProceduralRadianceMaps(ctx)
  lev2.PbrCommon.updateRadianceMapsGradient(rad, [(0.0, env_color)], ctx)   # single stop == solid
  pc.RadianceMaps = rad
  pc.enable_skybox = True
  pc.skyboxLevel   = LIT_SKYBOXLEVEL
  if app._unlit:
    pc.diffuseLevel  = UNLIT_DIFFUSE
    pc.specularLevel = UNLIT_SPECULAR
    pc.ambientLevel  = UNLIT_AMBIENT
  else:
    pc.diffuseLevel  = LIT_DIFFUSE
    pc.specularLevel = LIT_SPECULAR
    pc.ambientLevel  = LIT_AMBIENT
    sun = lev2.DynamicDirectionalLight()
    sun.data.color     = SUN_COLOR
    sun.data.intensity = SUN_INTENSITY
    sun.shadowCaster   = False                # isolation: no ground plane to receive shadows
    sun_dir = _orbit_eye(vec3(0), 1.0, SUN_AZIMUTH, SUN_ELEVATION)   # unit dir toward the sun
    sun.lookAt(sun_dir, vec3(0, 0, 0), vec3(0, 1, 0))               # light travels sun->origin
    app._sun = sun
    app._sun_node = app.SGC.layer_fwd.createLightNode("sun", sun)
  app.SGC.scenegraph.lightingmanager.gpuInit(ctx)


def _pump_bake(app, ctx):
  """Baked COLD path: complete the in-frame GPU section bake, rebind the real array. Returns True
  once the sampler holds baked content (always True for proc/warm). GPU-thread only (needs ctx)."""
  if app._bake_done:
    return True
  if app._stored and app._job is not None and bool(app._job.is_ready):
    arr, _cd, _warm = app._bake_section_array(
        ctx, key=app._key, num_layers=app._num_layers, bake_res=app._bake_res,
        content_fn=app._job_content_fn(app._job))
    app._gmtl.bindParam(app._SectionArray.ARRAY_SAMPLER, arr)
    app._arr = arr
    app._bake_done = True
    print("turntable: GPU section bake COMPLETE (%d layers)" % app._num_layers, flush=True)
  return app._bake_done


def _apply_camera(app, eye, up=None):
  app.SGC.uicam.lookAt(eye, app._center, up if up is not None else vec3(0, 1, 0))
  app.SGC.uicam.updateMatrices()
  app.SGC.camera.copyFrom(app.SGC.uicam.cameradata)


def _mode_str(stored, unlit):
  return "unlit" if unlit else ("baked" if stored else "proc")


################################################################################
# PRESENTER 1 — OFFSCREEN GRADER (metrics + contact sheet + verdict)
################################################################################

def run_turntable(args):
  from orkengine import lev2
  from ork.app.application import ComponentizedApplication
  from ork.app.std_scenegraph import StandardSceneGraphComponent

  DIM        = int(args.res)
  UNLIT      = bool(args.unlit)
  STORED     = bool(args.baked)
  elevations = [float(x) for x in str(args.elevations).split(",") if x.strip() != ""]
  sched      = _schedule(int(args.frames), elevations)
  n_total    = len(sched)
  SETTLE0    = 40                     # initial frames to let the scene render + (baked) IBL settle
  PERANGLE   = max(2, int(args.frames_per_angle))

  label = args.asset if args.asset else os.path.splitext(os.path.basename(args.input))[0]
  mode  = _mode_str(STORED, UNLIT)
  import tempfile
  _base = os.environ.get("OBT_STAGE") or tempfile.gettempdir()
  outdir = args.out or os.path.join(_base, "turntable", "%s_%s" % (label, mode))
  os.makedirs(outdir, exist_ok=True)

  class TurntableApp(ComponentizedApplication):
    def __init__(self):
      super().__init__()
      self._args      = args
      self._stored    = STORED
      self._unlit     = UNLIT
      self._label     = label
      self._frame     = 0
      self._done      = False
      self._want_exit = False
      self._state     = "settle"     # settle -> (baked: coldwait ->) capture -> done
      self._sched_i   = 0
      self._angle_f   = 0
      self._future    = None
      self._capbuf    = None
      self._results   = {}           # sched_i -> grade dict
      self._frames_np = {}           # sched_i -> HxWx3 uint8 (for the contact sheet / movie)
      self._error     = None
      self._job       = None
      self._center    = vec3(0)
      self._radius    = 5.0
      self._cur_eye   = _orbit_eye(vec3(0), 5.0, 0.0, elevations[0] if elevations else 0.0)
      self._cur_up    = vec3(0, 1, 0)
      self.SGC = self.addComponent("std_scenegraph", StandardSceneGraphComponent,
                                   eye=self._cur_eye, tgt=vec3(0, 0, 0), up=vec3(0, 1, 0),
                                   grid_variant=None)
      self.createEzApp(enable_lockstep_ups=True, enable_lockstep_fps=True,
                       enable_freerun_ups=True, enable_freerun_fps=True,
                       freerun=False, target_ups=60, target_fps=60,
                       width=DIM, height=DIM,
                       use_subsystems=['opq', 'core', 'gpu', 'lev2'])

    def _onGpuInit(self, ctx):
      self._ctx = ctx
      self.ezapp.topWidget.enableUiDraw()
      try:
        _setup_scene(self, ctx)
        self._set_pose(sched[0])
        print("turntable: orbit radius=%.3f (aabb_r=%.3f fill=%.2f->%.2f margin=%.2f) frames=%d "
              "(%d az x %d rings + 2 poles)"
              % (self._radius, getattr(self, "_aabb_r", 0.0), args.fill,
                 getattr(self, "_fill_actual", 0.0), args.margin, n_total,
                 int(args.frames), len(elevations)), flush=True)
      except Exception as e:
        import traceback; traceback.print_exc()
        self._error = str(e)
        self._done = True
        self._want_exit = True

    def _set_pose(self, azel):
      az, el = azel
      self._cur_eye = _orbit_eye(self._center, self._radius, az, el)
      self._cur_up  = _up_for_el(el)

    def _onUpdate(self, updinfo):
      if self._done:
        return
      _apply_camera(self, self._cur_eye, self._cur_up)
      self.SGC.scenegraph.updateScene(self.SGC.cameralut)

    def _rtg(self):
      return getattr(self.SGC.SGVPW, "rtgroup", None)

    def onGpuPostFrame(self, ctx):
      super().onGpuPostFrame(ctx)
      if self._want_exit:
        self._want_exit = False
        self.ezapp.signalExit()
        return
      if self._done:
        return
      self._frame += 1
      try:
        if self._state == "settle":
          if STORED and not self._bake_done:           # baked COLD: finish the GPU bake first
            if _pump_bake(self, ctx):
              self._frame = 0                           # restart the settle window post-bake
            return
          if self._frame >= SETTLE0:
            self._state = "capture"
            self._angle_f = 0
            self._set_pose(sched[0])
          return

        if self._state == "capture":
          rtg = self._rtg()
          if rtg is None or rtg.numBuffers < 1:
            return
          if self._future is None:
            self._angle_f += 1
            if self._angle_f < PERANGLE:            # let the new camera settle (TAA/prepass converge)
              return
            self._capbuf = lev2.CaptureBuffer()
            self._future = ctx.FBI.captureAsFormat(rtg.buffer(0), self._capbuf, "RGBA8")
            return
          if not bool(self._future.is_ready):
            return
          # readback ready: store this angle's frame
          import numpy as np
          w, h = self._capbuf.width, self._capbuf.height
          arr = np.array(self._capbuf, dtype=np.uint8).reshape(h, w, 4)
          rgb = np.ascontiguousarray(arr[..., :3][::-1])     # flip to top-down orientation
          self._frames_np[self._sched_i] = rgb
          self._results[self._sched_i] = grade_frame(rgb)
          self._future = None
          self._capbuf = None
          self._sched_i += 1
          if self._sched_i >= n_total:
            self._state = "done"
            self._done = True
            self._want_exit = True
            return
          self._angle_f = 0
          self._set_pose(sched[self._sched_i])
          return
      except Exception as e:
        import traceback; traceback.print_exc()
        self._error = str(e)
        self._done = True
        self._want_exit = True

  app = TurntableApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()

  if app._error:
    print("TURNTABLE_RESULT=FAIL frames=0 worst_black=1.000@az0 el0 mean_lit=0.000 error=%r" % app._error,
          flush=True)
    return 3

  # ------- write per-frame PNGs, contact sheet, metrics, verdict -------
  return _finalize(app, sched, outdir, label, mode, args)


################################################################################
# PRESENTER 2 — VISUAL VIEWER (--view): a real window, continuous auto-spin, NO metrics.
################################################################################

def run_view(args):
  from orkengine import lev2
  from orkengine.core import CrcStringProxy
  from ork.app.application import ComponentizedApplication
  from ork.app.std_scenegraph import StandardSceneGraphComponent

  tokens = CrcStringProxy()
  UNLIT      = bool(args.unlit)
  STORED     = bool(args.baked)
  rings      = sorted(float(x) for x in str(args.elevations).split(",") if x.strip() != "")
  rings      = [e for e in rings if abs(e) < 89.9] or [0.0]
  # the UP/DOWN stop list: bottom pole, the signed rings, top pole (poles at the ends).
  view_stops = [POLE_BOT] + rings + [POLE_TOP]
  label = args.asset if args.asset else os.path.splitext(os.path.basename(args.input))[0]

  class TurntableViewApp(ComponentizedApplication):
    def __init__(self):
      super().__init__()
      self._args     = args
      self._stored   = STORED
      self._unlit    = UNLIT
      self._label    = label
      self._stops    = view_stops
      self._el_idx   = min(range(len(view_stops)), key=lambda i: abs(view_stops[i]))  # nearest 0
      self._az       = 0.0
      self._paused   = False
      self._last_t   = None
      self._center   = vec3(0)
      self._radius   = 5.0
      self._job      = None
      self._bake_done = True
      self.SGC = self.addComponent("std_scenegraph", StandardSceneGraphComponent,
                                   eye=_orbit_eye(vec3(0), 5.0, 0.0, view_stops[self._el_idx]),
                                   tgt=vec3(0, 0, 0), up=vec3(0, 1, 0), grid_variant=None)
      self.createEzApp(name="OrkHypermeshTurntable")

    def _onGpuInit(self, ctx):
      self._ctx = ctx
      try:
        _setup_scene(self, ctx)
      except Exception as e:
        import traceback; traceback.print_exc()
        print("turntable --view: setup FAILED: %s" % e, flush=True)
        self.ezapp.signalExit()
        return
      el0 = self._stops[self._el_idx]
      _apply_camera(self, _orbit_eye(self._center, self._radius, self._az, el0), _up_for_el(el0))
      print("=" * 68, flush=True)
      print("turntable --view: %s  mode=%s  orbit_r=%.2f  fill=%.2f->%.2f  (NO metrics — visual only)"
            % (label, _mode_str(STORED, UNLIT), self._radius, args.fill,
               getattr(self, "_fill_actual", 0.0)), flush=True)
      print("  SPACE      pause / resume rotation")
      print("  UP / DOWN  step elevation stop (%s deg — poles at the ends)"
            % ",".join("%g" % e for e in self._stops))
      print("  ESC / Q    quit")
      print("=" * 68, flush=True)

    def _onGpuUpdate(self, ctx):
      _pump_bake(self, ctx)                              # baked COLD: finish the GPU bake while spinning

    def _onUpdate(self, updinfo):
      t = updinfo.absolutetime
      if self._last_t is None:
        self._last_t = t
      dt = t - self._last_t
      self._last_t = t
      if not self._paused:
        self._az = (self._az + VIEW_SPIN_DEG_S * dt) % 360.0
      el = self._stops[self._el_idx]
      eye = _orbit_eye(self._center, self._radius, self._az, el)
      _apply_camera(self, eye, _up_for_el(el))
      self.SGC.scenegraph.updateScene(self.SGC.cameralut)
      self.SGC.SGVP.widget.setDirty()

    def _onUiEvent(self, uievent):
      if uievent.code == tokens.KEY_DOWN.hashed:
        kc = uievent.keycode
        if kc == 32:                                     # SPACE
          self._paused = not self._paused
          print("turntable --view: %s" % ("PAUSED" if self._paused else "spinning"), flush=True)
        elif kc == 265:                                  # UP arrow
          self._el_idx = min(self._el_idx + 1, len(self._stops) - 1)
          el = self._stops[self._el_idx]
          tag = "  (TOP pole)" if el >= 89.9 else ("  (BOTTOM pole)" if el <= -89.9 else "")
          print("turntable --view: elevation -> %g deg%s" % (el, tag), flush=True)
        elif kc == 264:                                  # DOWN arrow
          self._el_idx = max(self._el_idx - 1, 0)
          el = self._stops[self._el_idx]
          tag = "  (TOP pole)" if el >= 89.9 else ("  (BOTTOM pole)" if el <= -89.9 else "")
          print("turntable --view: elevation -> %g deg%s" % (el, tag), flush=True)
        elif kc == 256 or kc == ord("Q"):                # ESC / Q
          print("turntable --view: quit", flush=True)
          self.ezapp.signalExit()
      return lev2.ui.HandlerResult()

  print("turntable --view: opening window (visual acceptance only, no oracle) ...", flush=True)
  app = TurntableViewApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  return 0


def _finalize(app, sched, outdir, label, mode, args):
  import numpy as np
  try:
    from PIL import Image, ImageDraw
  except Exception as e:
    print("TURNTABLE_RESULT=FAIL frames=0 worst_black=1.000@az0 el0 mean_lit=0.000 error='no PIL: %r'" % e,
          flush=True)
    return 3

  frames = app._frames_np
  results = app._results
  n = len(frames)
  if n == 0:
    print("TURNTABLE_RESULT=FAIL frames=0 worst_black=1.000@az0 el0 mean_lit=0.000 error='no frames captured'",
          flush=True)
    return 3

  # per-frame PNGs
  for i, (az, el) in enumerate(sched):
    if i not in frames:
      continue
    Image.fromarray(frames[i]).save(os.path.join(outdir, "frame_az%03d_el%02d.png" % (int(az), int(el))))

  # contact sheet: one row per elevation (TOP pole highest, BOTTOM pole lowest); columns = azimuth
  # steps within that ring. The two pole rows carry a single frame each, labeled TOP / BOTTOM.
  by_el = {}
  for i, (az, el) in enumerate(sched):
    if i in frames:
      by_el.setdefault(el, []).append((az, i))
  row_els = sorted(by_el.keys(), reverse=True)          # +90 (TOP) ... -90 (BOTTOM)
  az_count = int(args.frames)
  cell = 120
  pad_top = 18
  cols = max(az_count, 1)
  rows = len(row_els)
  sheet = Image.new("RGB", (cols * cell, rows * (cell + pad_top)), (20, 20, 24))
  draw = ImageDraw.Draw(sheet)
  worst = dict(black=-1.0, az=0, el=0)
  lit_vals = []
  for r, el in enumerate(row_els):
    y0 = r * (cell + pad_top)
    is_top = el >= 89.9
    is_bot = el <= -89.9
    for c, (az, i) in enumerate(sorted(by_el[el])):
      thumb = Image.fromarray(frames[i]).resize((cell, cell))
      sheet.paste(thumb, (c * cell, y0 + pad_top))
      g = results[i]
      bf = g["black_frac"]
      lit_vals.append(g["lit_frac"])
      col = (255, 90, 90) if bf > args.black_thresh else (170, 200, 170)
      if is_top:   lbl = "TOP e+90 b%.02f" % bf
      elif is_bot: lbl = "BOTTOM e-90 b%.02f" % bf
      else:        lbl = "a%03d e%+03d b%.02f" % (int(az), int(el), bf)
      draw.text((c * cell + 2, y0 + 3), lbl, fill=col)
      if bf > worst["black"]:
        worst = dict(black=bf, az=int(az), el=int(el))
  sheet_path = os.path.join(outdir, "contact_sheet.png")
  sheet.save(sheet_path)

  # metrics.txt
  metrics_path = os.path.join(outdir, "metrics.txt")
  with open(metrics_path, "w") as f:
    f.write("# turntable metrics — asset=%s mode=%s res=%d black_thresh=%.3f\n"
            % (label, mode, int(args.res), args.black_thresh))
    f.write("# az   el   black_frac  mean_luma  lit_frac  silhouette_px  verdict\n")
    for i, (az, el) in enumerate(sched):
      if i not in results:
        continue
      g = results[i]
      v = "FAIL" if g["black_frac"] > args.black_thresh else "ok"
      f.write("%4d %4d   %.4f     %7.2f   %.4f    %8d       %s\n"
              % (int(az), int(el), g["black_frac"], g["mean_luma"], g["lit_frac"],
                 g["silhouette_px"], v))

  # optional movie (best-effort, ffmpeg)
  if args.movie:
    _write_movie(app, sched, outdir, args)

  mean_lit = float(np.mean(lit_vals)) if lit_vals else 0.0
  n_fail = sum(1 for i in results if results[i]["black_frac"] > args.black_thresh)
  passed = (n_fail == 0)
  print("turntable: contact sheet -> %s" % sheet_path, flush=True)
  print("turntable: metrics       -> %s" % metrics_path, flush=True)
  print("turntable: %d/%d angles FAIL (black_frac > %.3f)" % (n_fail, n, args.black_thresh), flush=True)
  print("TURNTABLE_RESULT=%s frames=%d worst_black=%.3f@az%d el%d mean_lit=%.3f"
        % ("PASS" if passed else "FAIL", n, worst["black"], worst["az"], worst["el"], mean_lit), flush=True)
  return 0 if passed else 1


def _write_movie(app, sched, outdir, args):
  import shutil, subprocess, tempfile
  ff = shutil.which("ffmpeg")
  if not ff:
    print("turntable: --movie requested but ffmpeg not found — skipping", flush=True)
    return
  try:
    from PIL import Image
  except Exception:
    return
  # write az-ordered frames of the FIRST elevation ring as the movie (a clean single-ring spin)
  el0 = sched[0][1]
  tmpd = tempfile.mkdtemp(prefix="turntable_mov_")
  k = 0
  for i, (az, el) in enumerate(sched):
    if el != el0 or i not in app._frames_np:
      continue
    Image.fromarray(app._frames_np[i]).save(os.path.join(tmpd, "f%04d.png" % k))
    k += 1
  if k == 0:
    return
  outmp4 = args.movie if str(args.movie).endswith(".mp4") else os.path.join(outdir, "turntable.mp4")
  subprocess.run([ff, "-y", "-framerate", "24", "-i", os.path.join(tmpd, "f%04d.png"),
                  "-pix_fmt", "yuv420p", outmp4], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
  print("turntable: movie -> %s" % outmp4, flush=True)


################################################################################

def main():
  ap = argparse.ArgumentParser(description="hypermesh turntable instrument (offscreen grader + --view window)")
  ap.add_argument("asset", nargs="?", help="hypermesh asset filename stem (search mode)")
  ap.add_argument("-i", "--input", default=None, help="explicit path to an asset .py (overrides search)")
  ap.add_argument("--baked", action="store_true",
                  help="render via the section-baked texture-array sampler path (absent = the asset's "
                       "own procedural materials)")
  ap.add_argument("--view", action="store_true",
                  help="OPEN A WINDOW and watch the mesh auto-spin on the turntable (same scene + rig as "
                       "the offscreen grader). VISUAL ONLY — runs NO metrics/oracle. Controls: SPACE "
                       "pause, UP/DOWN step elevation (incl. TOP/BOTTOM poles at the ends), ESC/Q quit.")
  ap.add_argument("--frames", type=int, default=36, help="azimuth steps per elevation ring (default 36 = 10deg)")
  ap.add_argument("--elevations", default="-60,-30,0,30,60",
                  help="comma SIGNED elevation rings in degrees (default -60,-30,0,30,60; negatives = "
                       "below the horizon). TWO pole caps (el=+90 top-down, el=-90 bottom-up) are ALWAYS "
                       "added automatically for full top/bottom coverage. NOTE: bottom/pole coverage "
                       "assumes the mesh is baked/textured on its UNDERSIDES — a black underside is a REAL "
                       "isolation-QA finding (that is the point of covering it), not a tool error.")
  ap.add_argument("--fill", type=float, default=0.92,
                  help="target fraction of the smaller view dimension the bounding sphere fills (default "
                       "0.92). Framing is CONSTANT distance across the whole sweep — no per-frame zoom.")
  ap.add_argument("--res", type=int, default=512, help="offscreen render dimension (default 512)")
  ap.add_argument("--margin", type=float, default=1.0,
                  help="extra orbit-distance multiplier applied AFTER --fill framing (1.0 = fill exactly; "
                       ">1 pulls back for more padding)")
  ap.add_argument("--frames-per-angle", type=int, default=6, help="render frames to settle before capturing an angle")
  ap.add_argument("--unlit", action="store_true", help="flat/ambient-only rig (pure-ish albedo, no sun)")
  ap.add_argument("--black-thresh", type=float, default=0.05, help="FAIL if in-silhouette black-fraction exceeds this")
  ap.add_argument("--bake-res", type=int, default=256, help="baked-mode per-section bake resolution")
  ap.add_argument("--movie", nargs="?", const="", default=None, help="also write an mp4 (needs ffmpeg)")
  ap.add_argument("--out", default=None, help="output dir (default <OBT_STAGE>/turntable/<asset>_<mode>/)")
  ap.add_argument("--selftest", action="store_true", help="run the oracle negative-proof (no GPU) and exit")
  args = ap.parse_args()

  if args.selftest:
    return _selftest()

  if not args.asset and not args.input:
    print("usage: ork.hypermesh.turntable.py <ASSET> | -i <path.py>  [--baked] [--view] "
          "[--fill F] [--frames N] [--elevations a,b,c] [--res R] [--unlit] [--movie] | --selftest", flush=True)
    return 2

  print("turntable: provenance ork.hypergraph -> %s" % _assert_provenance(), flush=True)
  if args.view:
    return run_view(args)
  return run_turntable(args)


if __name__ == "__main__":
  sys.exit(main())
