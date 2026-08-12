#!/usr/bin/env ork.python
################################################################################
# FOLIAGE HUD TOGGLE GATE — one scatter, three barks, switched at runtime.
#
# A scene can declare the SAME geometry several times over, each declaration
# tagged with a visgroup (HypermeshComponentData.visgroup), exactly one of them
# visible at launch. The player's HUD FOLIAGE row picks which one draws, by
# sending HypermeshSystem SET_VISGROUP — so switching a bark is a VISIBILITY
# message, never a material edit on a live drawable.
#
# The bench is ONE prototype trunk (the same LsTrunkSections skeleton the forest
# scatters) declared three times:
#
#   foliage:solid    flat Solid constants           — no per-pixel structure
#   foliage:proctex  BarkAlpineLive, the DEFAULT    — the furrow ladder, live
#   foliage:stored   BarkSectionPBR + section bake  — the baked coat
#
# Three offscreen player runs of that ONE .ecs, differing ONLY in how many times
# the HUD row is stepped (--editscript RIGHT, the very call the '=' key makes):
#
#   A  untouched            -> proctex   (the scene's declared launch state)
#   B  one step             -> solid
#   C  two steps            -> stored
#
# WHAT IS MEASURED. The trunk is the only opaque thing in frame, so "not sky" is
# the bark mask, and inside it the gate reads mean luminance and its standard
# deviation — the two numbers a bark treatment cannot help moving:
#
#   * solid has NO per-pixel structure at all, so its spread must be the
#     smallest of the three by a clear margin. This is also the gate's negative
#     control: if the switch did nothing, all three would read identically and
#     every separation bar below would fail at once.
#   * proctex and stored both carry structure, and they are separated on MEAN —
#     the baked coat is a different albedo from the live ladder.
#
# THE ROW ITSELF is asserted too, from the HUD's own stdout dump: the FOLIAGE
# page must exist, and must report the state each run actually selected. A row
# that displays "stored" while the trunks still draw proctex is exactly the
# staleness this pairing catches.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"

import itertools
import re
import shutil
import subprocess
import sys
import tempfile
sys.stdout.reconfigure(line_buffering=True)

import numpy
from PIL import Image as PILImage

from orkengine import core   # core before lev2
from orkengine import ecs
from orkengine.core import vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.dflow.hypermesh import GpuMeshRenderSource
from ork.hypergraph.assets.hypermesh.ls_trunk_baked import (
    LsTrunkSections, GID_BARK, GID_BRANCH)
from ork.hypergraph.assets.materials.terrain.solid import Solid
from ork.hypergraph.assets.materials.bark_alpine import (
    BarkAlpineLive, BarkAlpineStored, BarkSectionPBR, BARK_SPECIES,
    bark_content_key)
from ork.hypergraph.colors import hsv
from ork.testing import verdict, Watchdog

###############################################################################
# the bench scene
###############################################################################

TREE_SCALE  = 10.0     # presentation only (instance matrix) — a ~23 m tree from
                       # the ~2.34 m prototype. TWO instances: a single
                       # instance_matrices entry does not reach the instanced
                       # path (the instCount>1 gate in hmdflow_render.cpp)
TREE_B_X    = 18.0     # world-x of the second instance (metres)
BAKE_RES    = 512      # stored-mode section bake res. The forest budgets 1024
                       # across 16 variants and ren_tree_bark 4096 for close
                       # inspection; 512 is this gate's — the bake is a COLD
                       # GPU pass on first run and res is what it costs
CAM_DIST_M   = 26.0
CAM_HEIGHT_M = 9.0
SNAPSHOT_FRAME = 120   # render frames after first-lit; the scripted row steps
                       # are counted in UPDATE ticks, well before this
EDIT_TICK      = 60    # update tick of the first scripted row step
PLAYER_TIMEOUT = 300.0

# ---- bars (measured on this bench; see the verdict detail line) --------------
HUD_STRIP_FRAC  = 0.14   # bottom fraction of the frame the HUD panel draws in;
                         # cropped out of every probe (the panel's own text
                         # changes between runs and is not bark)
CHANGE_EPS      = 0.01   # luminance move that makes a pixel part of the bark
                         # mask (see bark_mask) — an eighth of solid's own
                         # separation from either textured state
BARK_FRAC_MIN   = 0.03   # of the probed frame; below this the toggle moved
                         # nothing worth measuring (measures 0.095 here)
BARK_FRAC_MAX   = 0.70   # ...and above it something other than the trunks
                         # changed (a black frame reads as all-bark)
SPREAD_RATIO    = 2.00   # textured spread over solid's (measures x3.3 / x3.4)
PAIR_RMS_MIN    = 0.008  # every pair of states must differ by at least this
                         # rms over the mask; the tight pair (proctex/stored)
                         # measures 0.017, the solid pairs ~0.049

OUT_DIR = os.environ.get("FOLIAGE_HUD_OUT", "/tmp/foliage_hud_toggle")

# the three states, in the order the HUD row rings through them. The player
# sorts the declared visgroup suffixes, so this list IS that order and the
# step counts below are derived from it (never hand-counted).
STATES = ["proctex", "solid", "stored"]


def _instances():
  """COLUMN-MAJOR mat4 floats (translation = flat 12,13,14; scale = diag 0,5,10 —
  row-major drops tx into the projective slot and every instance collapses to
  identity)."""
  out = []
  for tx in (0.0, TREE_B_X):
    s = TREE_SCALE
    out += [s,   0.0, 0.0, 0.0,
            0.0, s,   0.0, 0.0,
            0.0, 0.0, s,   0.0,
            tx,  0.0, 0.0, 1.0]
  return out


class FoliageHudBenchScene(Scene):
  """One prototype trunk, three co-resident bark treatments over it. No terrain,
  no leaves, no clouds: every pixel is sky or bark, which is what makes the
  not-sky mask a bark mask."""

  def __init__(self):
    super().__init__()

    self.scenegraph(
        preset            = "ForwardPBR",
        skybox_path       = "<ork_envmaps2>/blender_forest.xir",
        SkyboxIntensity   = 1.0,
        DiffuseIntensity  = 1.0,
        SpecularIntensity = 1.0,
        AmbientLight      = vec3(0.0),
        DepthPrepass      = True,
        msaa              = 0,
        ssaa              = 0)

    self.system_data("HypermeshSystem")

    ##########################################################################
    # materials — one per treatment. Species params are consumed VERBATIM
    # (bark_alpine's same-tree-same-look contract); the stored pair carries the
    # shared content key so a bark edit re-keys its cache.
    ##########################################################################
    live_lo = self.asset.Ptex3d(
        "bench_bark_live",
        dsl_class     = BarkAlpineLive,
        vertex_source = GpuMeshRenderSource(instanced=True),
        env_specular  = 0.15,
        **BARK_SPECIES["furrow"])
    live_hi = self.asset.Ptex3d(
        "bench_bark_live_hi",
        dsl_class     = BarkAlpineLive,
        vertex_source = GpuMeshRenderSource(instanced=True),
        env_specular  = 0.15,
        **BARK_SPECIES["smooth"])
    # the pre-campaign flat constants, verbatim from the forest's own history
    flat_lo = self.asset.Ptex3d(
        "bench_bark_solid",
        dsl_class     = Solid,
        vertex_source = GpuMeshRenderSource(instanced=True),
        albedo        = hsv(30, 0.36, 0.1),
        env_specular  = 0.15)
    flat_hi = self.asset.Ptex3d(
        "bench_bark_solid_hi",
        dsl_class     = Solid,
        vertex_source = GpuMeshRenderSource(instanced=True),
        albedo        = hsv(35, 0.30, 0.14), roughness=0.8,
        env_specular  = 0.15)
    sampler = self.asset.Ptex3d(
        "bench_bark_sampler",
        dsl_class     = BarkSectionPBR,
        vertex_source = GpuMeshRenderSource(instanced=True),
        env_specular  = 0.15)
    ck = bark_content_key()
    baked_lo = self.asset.Ptex3d(
        "bench_bark_stored_lo_" + ck,
        dsl_class     = BarkAlpineStored,
        vertex_source = GpuMeshRenderSource(),
        **BARK_SPECIES["furrow"])
    baked_hi = self.asset.Ptex3d(
        "bench_bark_stored_hi_" + ck,
        dsl_class     = BarkAlpineStored,
        vertex_source = GpuMeshRenderSource(),
        **BARK_SPECIES["smooth"])

    trunk = self.asset.Hypermesh("bench_trunk", dsl_class=LsTrunkSections)
    inst  = _instances()

    def declare(name, group, visible, drawable):
      self.entity(
          name,
          components = [self.declare_component(
              "HypermeshComponent",
              drawabledata = drawable,
              layername    = "std_forward",
              nodename     = "hm_" + name,
              visgroup     = group,
              visible      = visible)])

    # ONE mesh asset, three drawables over it — the same shape the forest uses.
    declare("bench_proctex", "foliage:proctex", True,
            trunk.drawable_data(material  = live_lo,
                                materials = {GID_BRANCH: live_hi},
                                instance_matrices = inst))
    declare("bench_solid", "foliage:solid", False,
            trunk.drawable_data(material  = flat_lo,
                                materials = {GID_BRANCH: flat_hi},
                                instance_matrices = inst))
    declare("bench_stored", "foliage:stored", False,
            trunk.drawable_data(material     = sampler,       # drawn stored sampler
                                materials    = {GID_BARK: baked_lo,   # per-gid BAKE MAP
                                                GID_BRANCH: baked_hi},
                                section_bake = True,
                                bake_res     = BAKE_RES,
                                instance_matrices = inst))


__all__ = ["FoliageHudBenchScene"]


###############################################################################
# the drive
###############################################################################

def author_scene(tmpdir):
  """Serialize the bench in ONE headless GPU-bound process (the generated bark
  materials compose inline — the materialize.py author phase)."""
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  try:
    sd = ecs.SceneData()
    FoliageHudBenchScene().build(sd)
    path = os.path.join(tmpdir, "foliage_hud_bench.ecs")
    with open(path, "w") as f:
      f.write(sd.serializeJson())
  finally:
    ezapp.mainThreadEnd()
    ecs.headless_exit()
  print("[foliagehud] authored %s" % path, flush=True)
  return path


def play(ecs_path, png_path, steps=0):
  """One offscreen player run, opening straight onto the FOLIAGE page and
  stepping its row `steps` times — the same handler the '=' key drives."""
  runner = shutil.which("ork.ecs.player.exe")
  if runner is None:
    raise RuntimeError("ork.ecs.player.exe not on PATH")
  if os.path.isfile(png_path):
    os.remove(png_path)
  env = dict(os.environ)
  env["ORKID_PERFHUD"] = "foliage"          # open ON the page (no keyboard offscreen)
  # ...and dump every page to stdout. The PERIOD is short on purpose: this bench
  # settles in a couple of seconds, and a 2 s dump interval can miss the window
  # entirely — the row would then be unreadable rather than wrong.
  env["ORKID_PLAYER_HUD_STDOUT"] = "0.25"
  cmd = [runner, ecs_path, "--offscreen",
         "--snapshot", png_path,
         "--snapshot-frame", str(SNAPSHOT_FRAME),
         "--camdist", str(CAM_DIST_M),
         "--camheight", str(CAM_HEIGHT_M)]
  if steps:
    cmd += ["--editscript",
            ",".join("RIGHT:%d" % (EDIT_TICK + i) for i in range(steps))]
  r = subprocess.run(cmd, capture_output=True, text=True, env=env,
                     timeout=PLAYER_TIMEOUT)
  out = r.stdout or ""
  ok = os.path.isfile(png_path)
  if not ok:
    print("[foliagehud] player FAILED rc=%d\n%s"
          % (r.returncode, "\n".join(out.splitlines()[-25:])), flush=True)
  return ok, out


###############################################################################
# observables
###############################################################################

_ANSI    = re.compile(r"\x1b\[[0-9;]*[A-Za-z]|\[[0-9;]+m")
_ROW_RE  = re.compile(r"^[> ]\s*bark\s+(\S+)\s+(\d+)/(\d+)\s*$")
_VISG_RE = re.compile(r"SET_VISGROUP<foliage:(\w+)>\s*->\s*(on|off)")


def hud_row(log):
  """The LAST FOLIAGE row the HUD printed: (label, index, count). The last one
  is what the snapshot frame was rendered under — earlier dumps predate the
  scripted step."""
  seen = None
  for line in log.splitlines():
    m = _ROW_RE.match(_ANSI.sub("", line).rstrip())
    if m:
      seen = (m.group(1), int(m.group(2)), int(m.group(3)))
  return seen


def visgroups_on(log):
  """The set of groups left ENABLED by the last SET_VISGROUP burst."""
  state = {}
  for m in _VISG_RE.finditer(_ANSI.sub("", log)):
    state[m.group(1)] = (m.group(2) == "on")
  return {g for g, on in state.items() if on}


def luminance(png_path):
  """Rec.709 luminance of the frame, with the HUD panel's lower strip cropped
  off — the panel is the one part of the image the bark does not own."""
  im = numpy.asarray(PILImage.open(png_path).convert("RGB"), dtype=numpy.float32) / 255.0
  im = im[: int(im.shape[0] * (1.0 - HUD_STRIP_FRAC)), :, :]
  return 0.2126 * im[..., 0] + 0.7152 * im[..., 1] + 0.0722 * im[..., 2]


def bark_mask(lums):
  """THE BARK, FOUND BY THE TOGGLE ITSELF. Every run shares one camera, one
  skybox and one mesh, so the pixels that MOVE between the three states are
  exactly the pixels the bark shader owns — no brightness threshold to tune
  against a busy skybox, and no rectangle to re-measure when the framing moves.
  An inert toggle yields an empty mask, which is a loud failure rather than a
  quiet pass."""
  stack = numpy.stack(list(lums.values()))
  return (stack.max(0) - stack.min(0)) > CHANGE_EPS


###############################################################################

def main():
  wd = Watchdog(600.0, label="foliage_hud_toggle").arm()
  os.makedirs(OUT_DIR, exist_ok=True)
  tmpdir = tempfile.mkdtemp(prefix="foliage_hud_")
  failures = []
  lums = {}

  def check(label, ok, detail=""):
    print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
    if not ok:
      failures.append(label)

  try:
    scene = author_scene(tmpdir)

    for want in STATES:
      steps = STATES.index(want)
      png   = os.path.join(OUT_DIR, "%s.png" % want)
      ok, log = play(scene, png, steps=steps)
      # the run's own stdout is an exhibit: it carries the HUD dump and the
      # SET_VISGROUP trace this gate reads its verdict out of
      with open(os.path.join(OUT_DIR, "%s.log" % want), "w") as f:
        f.write(log)
      if not ok:
        wd.disarm()
        return verdict(False, "player produced no %s snapshot" % want)
      row = hud_row(log)
      check("%s_row_reads_back" % want,
            row is not None and row[0] == want and row[2] == len(STATES),
            "HUD row %s (asked %s, %d states)" % (row, want, len(STATES)))
      check("%s_is_the_only_group_on" % want, visgroups_on(log) == {want},
            "enabled groups %s" % sorted(visgroups_on(log)))
      lums[want] = luminance(png)

    ##########################################################################
    # three DIFFERENT barks over one mesh
    ##########################################################################
    mask = bark_mask(lums)
    frac = float(mask.mean())
    check("the_toggle_moved_pixels", BARK_FRAC_MIN <= frac <= BARK_FRAC_MAX,
          "bark mask %.4f of frame (window %.2f..%.2f)"
          % (frac, BARK_FRAC_MIN, BARK_FRAC_MAX))
    if failures:
      wd.disarm()
      return verdict(False, "setup: " + ",".join(failures))

    sd = {s: float(lums[s][mask].std()) for s in STATES}
    # solid is a CONSTANT albedo lit by one sky: whatever spread it carries is
    # shading, and a treatment that puts structure on the surface must beat it.
    for textured in ("proctex", "stored"):
      ratio = sd[textured] / max(sd["solid"], 1e-6)
      check("%s_has_structure_solid_lacks" % textured, ratio >= SPREAD_RATIO,
            "bark spread %.4f vs solid %.4f (x%.2f, floor %.2f)"
            % (sd[textured], sd["solid"], ratio, SPREAD_RATIO))
    # ...and no two states are the same picture. The proctex/stored pair is the
    # tight one (both are the same authored ladder, one evaluated live and one
    # resampled off a baked chart), so it is what this bar is set against.
    rms = {}
    for a, b in itertools.combinations(STATES, 2):
      d = lums[a][mask] - lums[b][mask]
      rms[(a, b)] = float(numpy.sqrt((d * d).mean()))
      check("%s_differs_from_%s" % (a, b), rms[(a, b)] >= PAIR_RMS_MIN,
            "bark-mask rms %.4f (floor %.4f)" % (rms[(a, b)], PAIR_RMS_MIN))
  finally:
    shutil.rmtree(tmpdir, ignore_errors=True)

  detail = ("mask=%.4f spread " % frac
            + " ".join("%s=%.4f" % (s, sd[s]) for s in STATES)
            + " rms " + " ".join("%s/%s=%.4f" % (a, b, v) for (a, b), v in rms.items()))
  if failures:
    detail += " failed=" + ",".join(failures)
  wd.disarm()
  return verdict(len(failures) == 0, detail)


if __name__ == "__main__":
  sys.exit(main())
