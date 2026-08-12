#!/usr/bin/env ork.python
################################################################################
# CLOUD HUD CONTROLS GATE — the player's CLOUDS page moves the decks, and moves
# BOTH halves of a deck base.
#
# Deck altitude IS the coverage encoding: the deck material decodes
#   t = (shell_apex_y - CgAltLo) * CgInvSweep
# and CgAltLo is baked at author time from the deck's spec altitude plus the
# scene's lift, over a sweep of 600 m. So a live "cloud base" control that only
# moves the shells (the transform half, which the deck script owns) shifts t by
# metres/sweep and destroys the coverage the user asked for — a kilometre pegs
# the band and empties the sky. The player therefore drives the transform half
# through the CloudSet message AND rebinds CgAltLo on every deck material from
# the same number, and this gate is what keeps the pair together.
#
# Four captures of ONE bench scene, all through the shipped offscreen player:
#
#   A  base at the scene's own lift, nothing touched         (the reference)
#   B  the "cloud base" ROW driven +LIFT_M through --editscript — the same calls
#      a keypress makes, so what is measured is the shipped control surface
#   C  the SAME lift sent as a bare CloudSet (--pysysnotify): the transform half
#      alone, with no band rebind. THE CORRUPTION ORACLE — it must collapse the
#      sky, or leg B proves nothing (a compensation that does nothing passes an
#      A/B comparison trivially).
#   D  the "cloud gain" ROW driven to GAIN_TO: the radiance half of the page,
#      whose whole mechanism is the same per-draw rebind of the material's baked
#      lit/shadow colors.
#
# WHAT IS MEASURED, in the sky region only (the HUD panel itself draws in the
# lower-left of B and D): the cloud-pixel fraction (bright + low-saturation
# against blue sky), the vertical centre of the cloud mass and the region's mean
# luminance. Every bar below is quoted with the value this scene measures.
#
# The deck's wind scroll and its B-channel evolution are FROZEN (both are
# author-time multipliers baked into the material): four separate processes must
# otherwise be four different clouds, and none of these comparisons would hold.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
os.environ["ORK_CLOUDGAUGE_WINDX"] = "0.0"   # frozen UV scroll  (baked at author time)
os.environ["ORK_CLOUDGAUGE_EVO"]   = "0.0"   # frozen silhouette churn

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
from ork.hypergraph.ecs.scene import Scene
from ork.testing import verdict, Watchdog

###############################################################################
# the bench scene + the drive
###############################################################################

TOD_NOON     = 12.5     # lit decks; the night floor is a different gate
DECK_COVER   = 0.35     # scattered-to-broken: cloud AND sky in every frame, so
                        # a collapse in either direction is visible
LIFT_M       = 600.0    # 12 presses of the row's 50 m step. ONE FULL SWEEP: an
                        # uncompensated lift of a whole sweep pegs the coverage
                        # band and cannot half-survive, while a compensated one
                        # is still a small enough move that the same frustum
                        # shows a comparable amount of the same deck
BASE_STEP_M  = 50.0     # the row's step (main.cpp) — the press count follows it
GAIN_TO      = 2.0      # 20 presses of the gain row's 0.05 step from 1.0
GAIN_STEP    = 0.05

CAM_DIST_M     = 600.0
CAM_HEIGHT_M   = 60.0
SNAPSHOT_FRAME = 900    # render frames after first-lit: the scripted edits are
                        # counted in UPDATE ticks, so the capture waits well past
                        # them (the log ordering is asserted, not assumed)
PLAYER_TIMEOUT = 150.0

# ---- bars (measured on this scene; see the verdict detail line) --------------
#
# THE PRIMARY A/B IS B AGAINST C, not against A: B and C lift the decks by the
# SAME kilometre from the same camera, and differ only in whether the material's
# band came with them. Comparing either against A would fold in a perspective
# change (a deck a kilometre higher genuinely shows a different part of itself
# through the same frustum), which is a real difference and not a defect — so A
# sets the floor for "there were clouds to begin with" and B/C carry the claim.
COVER_MIN        = 0.05   # clouds in the sky region at all — A and B both
COVER_SURVIVE    = 0.40   # (B) a compensated lift keeps at least this fraction
                          # of A's coverage. Measured: 0.57
COLLAPSE_MAX     = 0.10   # (C) an uncompensated lift keeps at most this
                          # fraction of B's. Measured: 0.00
LIFT_ROW_MIN     = 0.04   # (B) the deck demonstrably MOVED: the cloud mass'
                          # vertical centre shifts by this fraction of the
                          # region's height (either way — where it goes is the
                          # frustum's business). Measured: 0.08
GAIN_LUM_MIN     = 1.20   # (D) mean luminance rise at gain 2.0. Measured: 1.47
OUT_DIR = os.environ.get("CLOUD_HUD_OUT", "/tmp/cloud_hud_controls")

_CFG = {}


class CloudHudBenchScene(Scene):
  """The library's default deck set under the procedural sky on a frozen clock.
  No terrain: every pixel above the skyline is sky or deck, which is what makes
  a bright-and-unsaturated pixel a cloud pixel."""

  def __init__(self):
    super().__init__()
    self.sky(
        time_of_day = TOD_NOON,
        time_scale  = 0.0,       # FROZEN: four processes must be one sky
        moon        = False,
        stars       = False,
        clouds      = True,
        cloud_cover = DECK_COVER,
        skybox_path = "<ork_envmaps2>/desert4k.xir",   # IBL feed only
        msaa        = 0,
        ssaa        = 0)


__all__ = ["CloudHudBenchScene"]


def author_scene(tmpdir):
  """Serialize the bench scene in ONE headless GPU-bound process (the deck's
  generated material composes inline — the materialize.py author phase)."""
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  try:
    sd = ecs.SceneData()
    CloudHudBenchScene().build(sd)
    path = os.path.join(tmpdir, "cloud_hud_bench.ecs")
    with open(path, "w") as f:
      f.write(sd.serializeJson())
  finally:
    ezapp.mainThreadEnd()
    ecs.headless_exit()
  print("[cloudhud] authored %s" % path, flush=True)
  return path


def play(ecs_path, png_path, notify=(), edits=None):
  """One offscreen player run. Returns (ok, stdout). `notify` entries JOIN the
  hour message into the ONE --pysysnotify the player accepts (a second copy of
  the option is a parse error, not a second script)."""
  runner = shutil.which("ork.ecs.player.exe")
  if runner is None:
    raise RuntimeError("ork.ecs.player.exe not on PATH")
  if os.path.isfile(png_path):
    os.remove(png_path)
  # the clock is frozen in the scene; the hour is re-sent so a scene library
  # default can never decide which sky this gate measures
  msgs = ["0.5:SkyTimeSet:hour=%.2f" % TOD_NOON] + list(notify)
  cmd = [runner, ecs_path, "--offscreen",
         "--snapshot", png_path,
         "--snapshot-frame", str(SNAPSHOT_FRAME),
         "--camdist", str(CAM_DIST_M),
         "--camheight", str(CAM_HEIGHT_M),
         "--pysysnotify", ";".join(msgs)]
  if edits:
    cmd += ["--editscript", edits]
  r = subprocess.run(cmd, capture_output=True, text=True, timeout=PLAYER_TIMEOUT)
  out = r.stdout or ""
  ok = os.path.isfile(png_path)
  if not ok:
    print("[cloudhud] player FAILED rc=%d\n%s"
          % (r.returncode, "\n".join(out.splitlines()[-25:])), flush=True)
  return ok, out


def editscript(page_index, row, presses):
  """The scripted HUD input that walks to CLOUDS, selects `row` and presses the
  increment `presses` times — the very calls ` [ ] and = make. Ticks are spaced
  well past registration (the page ring only exists after the scene binds)."""
  ev = ["PAGE:%d" % (50 + i) for i in range(page_index)]
  ev += ["DOWN:%d" % (60 + i) for i in range(row)]
  ev += ["RIGHT:%d" % (70 + i) for i in range(presses)]
  return ",".join(ev)


###############################################################################
# observables
###############################################################################

def probe(png_path):
  im = numpy.asarray(PILImage.open(png_path).convert("RGB"), dtype=numpy.float32) / 255.0
  h = im.shape[0]
  # SKY REGION = the top 30% of the frame: above the HUD panel (lower-left) and
  # above the horizon's bright haze band, which is unsaturated and bright enough
  # to pass for cloud and would report coverage in a frame that has none.
  sky = im[:int(h * 0.30), :, :]
  lum = 0.2126 * sky[:, :, 0] + 0.7152 * sky[:, :, 1] + 0.0722 * sky[:, :, 2]
  sat = (sky.max(2) - sky.min(2)) / numpy.maximum(sky.max(2), 1e-6)
  cloud = (lum > 0.32) & (sat < 0.30)   # bright + unsaturated = deck, not blue sky
  rows = numpy.repeat(numpy.arange(cloud.shape[0])[:, None], cloud.shape[1], 1)[cloud]
  return dict(cover=float(cloud.mean()),
              lum=float(lum.mean()),
              row=(float(rows.mean() / cloud.shape[0]) if rows.size else -1.0))


def deck_count(log):
  """(band, radiance) rebind counts the player reports for the deck materials."""
  m = re.search(r"cloud deck materials: band rebind on (\d+), radiance rebind on (\d+)", log)
  return (int(m.group(1)), int(m.group(2))) if m else (0, 0)


def clouds_page_index(log):
  """Where CLOUDS sits in the HUD page ring, derived from the player's own
  registration line: page 0 is OFF, 1..4 are the built-in pages, and the editor
  pages follow in registration order (a scene with no atmosphere registers no
  SKY pages at all, which would otherwise shift the ring under this gate)."""
  m = re.search(r"HUD editor pages: SKY-MAIN<(\d+) rows> SKY-HAZE<(\d+) rows> "
                r"CLOUDS<(\d+) rows>", log)
  if not m:
    return 0, 0
  before = (1 if int(m.group(1)) else 0) + (1 if int(m.group(2)) else 0)
  return 5 + before, int(m.group(3))


def reported_base(log):
  """The deck script's own last report of the lift it is applying (metres)."""
  hits = re.findall(r"base=([-+][0-9.]+)m", log)
  return float(hits[-1]) if hits else None


###############################################################################

def main():
  wd = Watchdog(420.0, label="cloud_hud_controls").arm()
  os.makedirs(OUT_DIR, exist_ok=True)
  tmpdir = tempfile.mkdtemp(prefix="cloud_hud_")
  failures = []

  def check(label, ok, detail=""):
    print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
    if not ok:
      failures.append(label)

  try:
    scene = author_scene(tmpdir)

    # ---- A: the reference, and the run the drive is derived from ------------
    png_a = os.path.join(OUT_DIR, "a_reference.png")
    ok, log_a = play(scene, png_a)
    if not ok:
      wd.disarm()
      return verdict(False, "player produced no reference snapshot")
    n_band, n_rad = deck_count(log_a)
    page, n_rows = clouds_page_index(log_a)
    check("material_half_bound", n_band >= 1 and n_rad == 2 * n_band,
          "band rebind on %d deck material(s), radiance on %d" % (n_band, n_rad))
    check("clouds_page_present", page >= 5 and n_rows == 4,
          "page %d, %d rows (cover/tile/base/gain)" % (page, n_rows))
    if failures:
      wd.disarm()
      return verdict(False, "setup: " + ",".join(failures))
    a = probe(png_a)
    check("reference_has_clouds", a["cover"] >= COVER_MIN,
          "cover=%.4f (floor %.2f)" % (a["cover"], COVER_MIN))

    # ---- B: the base row, both halves --------------------------------------
    png_b = os.path.join(OUT_DIR, "b_base_row.png")
    steps = int(round(LIFT_M / BASE_STEP_M))
    ok, log_b = play(scene, png_b, edits=editscript(page, 2, steps))
    if not ok:
      wd.disarm()
      return verdict(False, "player produced no base-row snapshot")
    b = probe(png_b)
    check("base_row_reached_the_decks", reported_base(log_b) == LIFT_M,
          "script reports base=%s (asked %.0f)" % (reported_base(log_b), LIFT_M))
    check("base_holds_coverage",
          b["cover"] >= COVER_MIN and b["cover"] >= COVER_SURVIVE * a["cover"],
          "cover %.4f -> %.4f (%.2f of reference, floor %.2f)"
          % (a["cover"], b["cover"], b["cover"] / max(a["cover"], 1e-6), COVER_SURVIVE))
    check("base_moved_the_decks", abs(a["row"] - b["row"]) >= LIFT_ROW_MIN,
          "cloud-mass row %.3f -> %.3f (moved %.3f, floor %.3f)"
          % (a["row"], b["row"], abs(a["row"] - b["row"]), LIFT_ROW_MIN))

    # ---- C: the corruption oracle — transform half alone -------------------
    png_c = os.path.join(OUT_DIR, "c_transform_only.png")
    ok, log_c = play(scene, png_c,
                     notify=["1.0:CloudSet:alt_offset=%.0f" % LIFT_M])
    if not ok:
      wd.disarm()
      return verdict(False, "player produced no transform-only snapshot")
    c = probe(png_c)
    check("uncompensated_lift_corrupts", c["cover"] <= COLLAPSE_MAX * b["cover"],
          "same lift, no band rebind: cover %.4f -> %.4f (%.2f of the compensated "
          "lift, ceiling %.2f)"
          % (b["cover"], c["cover"], c["cover"] / max(b["cover"], 1e-6), COLLAPSE_MAX))

    # ---- D: the radiance row ----------------------------------------------
    png_d = os.path.join(OUT_DIR, "d_gain_row.png")
    gsteps = int(round((GAIN_TO - 1.0) / GAIN_STEP))
    ok, log_d = play(scene, png_d, edits=editscript(page, 3, gsteps))
    if not ok:
      wd.disarm()
      return verdict(False, "player produced no gain-row snapshot")
    d = probe(png_d)
    lum_ratio = d["lum"] / max(a["lum"], 1e-6)
    check("gain_brightens_the_decks", lum_ratio >= GAIN_LUM_MIN,
          "sky-region luminance %.4f -> %.4f (x%.2f, floor %.2f)"
          % (a["lum"], d["lum"], lum_ratio, GAIN_LUM_MIN))
    check("gain_left_the_base_alone", reported_base(log_d) == 0.0,
          "script reports base=%s (the gain row must not move the lift)"
          % reported_base(log_d))
  finally:
    shutil.rmtree(tmpdir, ignore_errors=True)

  detail = ("decks=%d cover A=%.4f B=%.4f C=%.4f row A=%.3f B=%.3f lum A=%.4f D=%.4f"
            % (n_band, a["cover"], b["cover"], c["cover"], a["row"], b["row"],
               a["lum"], d["lum"]))
  if failures:
    detail += " failed=" + ",".join(failures)
  wd.disarm()
  return verdict(len(failures) == 0, detail)


if __name__ == "__main__":
  sys.exit(main())
