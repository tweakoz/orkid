#!/usr/bin/env ork.python
################################################################################
# NIGHT DISPLAY VISIBILITY gate — the dead of night has to survive the 8-BIT
# STORE, not merely exist in the radiance buffer.
#
# THE DEFECT THIS CLOSES. Every radiance-domain night gate passed while the
# owner's screen showed BLACK. They were all measuring the wrong side of the
# quantizer: a moonless sky reaches the tone stage at ~4.5e-5 of scene-referred
# radiance, and through the ACES curve at the pre-calibration exposure that is
# 0.002 of ONE 8-BIT STEP. Measured on this rig before the calibration: sky mean
# 0.205 of a step (which is the OUTPUT DITHER, not the sky), terrain-vs-sky
# separation 0.003 of a step, and 20 star pixels in a 1280x720 frame. So this
# gate reads the DISPLAY-REFERRED frame and nothing else.
#
# WHERE THE PIXELS COME FROM. ork.scene.materialize.py runs the shipped C++
# player (ork.ecs.player.exe --offscreen --snapshot), whose capture is
# fbi->_main_rtg buffer 0 — the 8-bit surface the desktop window presents. That
# is PAST the ACES tone stage, past the output node, and past the dither: the
# same bytes a monitor gets. A capture off any float RTG upstream of that would
# re-make the exact mistake this gate exists to catch.
#
# THE SCENE is scn_nightcal, the calibration rig: moonless (a risen moon is
# orders of magnitude of extra sky), clock FROZEN (the library day runs ~2 deg of
# sun per wall second, so an unfrozen scene is measured wherever the boot
# happened to end), no cloud decks (their colors are authored constants that do
# not darken), no author grade. Its walker camera puts the skyline across the
# lower fifth with open sky and the star field above it.
#
# FOUR LEGS, five renders:
#   deep       the rig AS SHIPPED at the dead of night — the three display
#              claims below.
#   deep_pre   the SAME hour with the tone stage forced back to the
#              pre-calibration floor anchor. Asserted to FAIL the claims: a
#              display gate that cannot tell the black frame from the fixed one
#              is a rubber stamp. This is the gate's teeth, committed rather
#              than quoted.
#   early      the EARLY-NIGHT anchor, rig as shipped.
#   early_b    the early-night anchor AGAIN, same config, separate process. Not
#              a claim — the BENCH NOISE PROBE, whose only job is to put this
#              machine's own render noise into the verdict line.
#   early_pre  the same hour at the pre-calibration anchor, asserted invariant
#              against `early`. The first half of the night is the band the
#              owner accepted, and the calibration moves the floor anchor, which
#              exists only BELOW the twilight luminance — so the early night may
#              not move. Structural, and proven per-pixel here.
#
# HOW THE INVARIANCE IS ASSERTED — and why it is NOT whole-frame byte identity.
# "Not one byte moved" silently assumes the bench renders the same config to the
# same bytes. That is a property of the MACHINE, not of this engine, and on the
# mac bench it is FALSE: sixteen same-config early-night renders produced three
# distinct frames, differing by up to 15 steps over ~100-1300 pixels of 921600.
# More drain frames does not cure it (measured identically at 300 and at 900), so
# it is not a settling transient. A whole-frame byte claim therefore fails on a
# clean tree — measured, once in three runs — which is a gate reporting a
# machine's noise as an engine regression.
#
# Every one of those noisy pixels was at y-fraction 0.7528 or below the horizon
# line, i.e. TERRAIN. So the leg splits the frame and asserts each half with the
# strongest form that half can carry:
#
#   STRICT, on the band above STRICT_CUT — BYTE IDENTITY, no epsilon. That band
#   was bit-identical across all 120 pairs of those sixteen renders. It is also
#   the band that CARRIES THE CLAIM: the floor anchor is a global exposure term,
#   so a leak cannot move terrain without moving the sky above it. The cut sits a
#   margin above the lowest deviating row ever observed.
#
#   TOLERANT, over the FULL frame — ALL THREE of: per-pixel max |delta| within
#   EPS_MARGIN x the noise `early_b` just measured, luminance-HISTOGRAM identity
#   within that same epsilon (compared as quantile functions — two histograms are
#   equal exactly when their sorted pixel values are), and sky mean equal to
#   MEAN_DECIMALS decimals. This is what keeps the terrain honest without
#   convicting it of the bench's noise.
#
# The mean is the sharp blade of the three: a floor anchor that reaches this hour
# at all moves EVERY sky pixel, so it moves the mean. Measured on the mac bench at
# the dead of night, where the anchor IS reachable: a 1.6% floor nudge (512 ->
# 520) moves the sky mean 4.507 -> 4.599 while its per-pixel max delta is only 2
# steps. An epsilon loose enough to absorb bench noise would swallow that delta;
# the mean will not. That is why all three are ANDed rather than any one taken.
#
# TEETH FOR THE TOLERANT FORM, committed rather than quoted: the same predicate
# is applied to the deep/deep_pre pair — the SAME floor lever at the hour where
# the anchor is reachable — and asserted to REJECT it, both at the epsilon this
# run derived and at the tightest epsilon the leg can ever use. No early-night
# floor value can serve as teeth here, because that is the very thing being
# claimed: the anchor is unreachable at this hour, so 0.65 and 512 render the
# identical frame (test_scene_adaptation_curve_gate proves the unreachability
# analytically, over a dense sweep and without a frame).
#
# WHOSE DEFAULT. The dead-of-night adaptation anchor is authored PER SCENE, not
# engine-wide (PostFxNodeACES.h says why: the value that develops a clear night
# clips a clouded one, because the cloud decks' night radiance is not the sky's).
# So "as shipped" here means scn_nightcal's own NIGHT_FLOOR, which this gate READS
# OUT OF THAT FILE rather than restating — re-tune the rig and the thresholds
# below move with it.
#
# THE THREE DISPLAY CLAIMS, all in 8-bit steps on the sky/terrain bands:
#   a) THE SKY IS THERE      sky-band mean at or above SKY_MEAN_MIN steps, AND
#                            at or above half of what the shipped floor anchor
#                            predicts through the tone curve — so re-tuning the
#                            anchor (Scene.sky(tonemap={"adapt_floor": ...}))
#                            moves this bar with it instead of leaving a stale
#                            number behind.
#   b) THERE IS A SKYLINE    |terrain mean - sky mean| at or above one step.
#   c) THERE ARE STARS       at least STAR_PIXELS_MIN pixels in the upper region
#                            standing STAR_OVER steps above the sky background.
#
# WARMUP: the player's own snapshot discipline plus --snapshot-frame — the
# appearance async (sky/IBL) must drain before ANY capture is issued, the first
# settled-lit frame is then DISCARDED and the capture is taken SNAP_FRAME drain
# frames later, once the walker has come to rest. That settles the SKY band
# completely — ten same-config early-night runs on the mac bench read the same
# sky mean to six decimals and were bit-identical above the skyline. It does not
# always settle the TERRAIN: two of those ten landed 74 and 235 pixels (of
# 921600) up to 12 steps apart, every one of them below y=0.76, i.e. terrain.
# That residue is why the invariance leg measures the bench before it judges the
# engine.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

import math
import shutil
import subprocess
import tempfile

import numpy
from PIL import Image

from ork.testing import verdict

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)

SCENE      = "scn_nightcal"
SCENE_PY   = os.path.join(_ROOT, "ork.data", "scenes", "scn_nightcal.py")
TOD_DEEP   = "0.0"       # the dead of night
TOD_EARLY  = "19.0"      # the early-night anchor (the band the owner accepted)
PRE_FLOOR  = "0.65"      # the pre-calibration dead-of-night adaptation anchor
SNAP_FRAME = 300         # drain frames after first-lit: discard, settle, then grab
LEG_TIMEOUT = 240.0

# frame bands, as fractions of height. The rig frames ~80% sky over ~20% terrain.
SKY_BAND   = (0.10, 0.60)
TERR_BAND  = (0.88, 0.99)
STAR_BAND  = (0.02, 0.70)

STAR_OVER       = 3       # steps above the sky background to count as a star pixel
SKY_MEAN_MIN    = 2.0     # 8-bit steps (measured 4.51 shipped / 0.205 pre)
SKYLINE_MIN     = 1.0     # 8-bit steps (measured 2.57 shipped / 0.003 pre)
STAR_PIXELS_MIN = 5000    # (measured 74889 shipped / 20 pre; 5848 at a floor of 64)

# The invariance leg (see "HOW THE INVARIANCE IS ASSERTED" above). The epsilon is
# NOT a constant — it is this bench's own measured render noise times the margin,
# floored at EPS_MIN so a byte-deterministic bench still gets a real threshold.
EPS_MARGIN    = 2.0
EPS_MIN       = 1
MEAN_DECIMALS = 3

# Rows above this fraction of the height carry the BYTE-IDENTICAL claim. The
# lowest row ever observed to move between two same-config renders on the mac
# bench was 0.7528; this sits a margin above it and still contains the whole sky,
# the skyline and the star field.
STRICT_CUT = 0.70

# What a moonless sky hands the tone stage, scene-referred — the airglow floor
# through the shipped sky exposure. Shared with
# test_scene_adaptation_curve_gate, which makes the same prediction without a
# frame; this gate is where it is checked against actual pixels.
MOONLESS_SKY_RADIANCE = 4.5e-5

failures = []


def check(label, ok, detail=""):
  print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
  if not ok:
    failures.append(label)


def aces(x):
  """the shipped tone curve (framefx.fxv2 lib_aces, Narkowicz fit)."""
  a, b, c, d, e = 2.51, 0.03, 2.43, 0.59, 0.14
  return max(0.0, min(1.0, (x * (a * x + b)) / (x * (c * x + d) + e)))


def shipped_floor():
  """the rig's OWN declared dead-of-night anchor, read out of the scene file.
  By text, not by import: importing a scene pulls the whole eager asset DSL in,
  and this gate wants one number."""
  for line in open(SCENE_PY):
    if line.startswith("NIGHT_FLOOR"):
      return float(line.split("=", 1)[1].split("#")[0].strip())
  raise RuntimeError("night display gate: no NIGHT_FLOOR in %s — the rig stopped "
                     "declaring the calibration this gate is calibrated to" % SCENE_PY)


def render(png, tod, floor=None):
  """one offscreen player run -> a display-referred PNG."""
  tool = shutil.which("ork.scene.materialize.py")
  if tool is None:
    raise RuntimeError("night display gate: ork.scene.materialize.py not on PATH "
                       "(the gate renders through the shipped player, not a hand-rolled boot)")
  env = dict(os.environ)
  env["ORK_NIGHTCAL_TOD"] = tod
  if floor is not None:
    env["ORK_NIGHTCAL_FLOOR"] = floor
  else:
    env.pop("ORK_NIGHTCAL_FLOOR", None)
  proc = subprocess.run([tool, SCENE, "-S", png, "-F", str(SNAP_FRAME)],
                        env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                        timeout=LEG_TIMEOUT)
  log = proc.stdout.decode("utf-8", "replace")
  if (proc.returncode != 0) or (not os.path.isfile(png)):
    sys.stdout.write(log[-3000:])
    raise RuntimeError("night display gate: render failed rc=%d tod=%s floor=%s"
                       % (proc.returncode, tod, floor))
  return png


def measure(png):
  """the three display readings, in 8-bit steps, off the presented frame."""
  rgb = numpy.array(Image.open(png).convert("RGB")).astype(numpy.float64)
  h, w, _ = rgb.shape
  lum = rgb.max(axis=2)                      # brightest channel: a blue sky and a
                                             # warm star both read honestly
  sky  = lum[int(h * SKY_BAND[0]):int(h * SKY_BAND[1]), :]
  terr = lum[int(h * TERR_BAND[0]):int(h * TERR_BAND[1]), :]
  star = lum[int(h * STAR_BAND[0]):int(h * STAR_BAND[1]), :]
  # the background is the MEDIAN of the sky band, not its mean: the mean of a
  # star-filled band is pulled up by the very pixels being counted.
  bg = float(numpy.median(sky))
  return {"sky":   float(sky.mean()),
          "terr":  float(terr.mean()),
          "delta": abs(float(sky.mean()) - float(terr.mean())),
          "stars": int((star >= bg + STAR_OVER).sum()),
          "rgb":   rgb.astype(numpy.int16),
          "bytes": open(png, "rb").read()}


def claims_hold(m):
  """the three display claims, as a triple of bools (shared by leg + teeth)."""
  return (m["sky"] >= SKY_MEAN_MIN,
          m["delta"] >= SKYLINE_MIN,
          m["stars"] >= STAR_PIXELS_MIN)


def frame_delta(a, b):
  """how far apart two frames are, in 8-bit steps, two ways.

  pixel  the largest per-pixel per-channel difference — catches a few pixels
         moving a lot, which is what unsettled streaming looks like.
  hist   the largest difference between the two LUMINANCE HISTOGRAMS, compared
         as quantile functions (sorted pixel values elementwise): blind to
         WHERE a pixel is, so it ignores a reshuffle and reports a shift of the
         distribution itself, which is what an exposure change looks like."""
  pixel = int(numpy.abs(a["rgb"] - b["rgb"]).max())
  qa    = numpy.sort(a["rgb"].max(axis=2).ravel())
  qb    = numpy.sort(b["rgb"].max(axis=2).ravel())
  hist  = int(numpy.abs(qa - qb).max())
  return {"pixel": pixel, "hist": hist,
          "npix":  int((numpy.abs(a["rgb"] - b["rgb"]).max(axis=2) > 0).sum())}


def is_invariant(a, b, eps):
  """the tolerant invariance predicate: all three, or it is not invariant."""
  d = frame_delta(a, b)
  return (d["pixel"] <= eps
          and d["hist"] <= eps
          and round(a["sky"], MEAN_DECIMALS) == round(b["sky"], MEAN_DECIMALS))


def strict_band_identical(a, b):
  """byte identity above STRICT_CUT — the band that carries the claim and that
  this bench renders reproducibly."""
  rows = int(a["rgb"].shape[0] * STRICT_CUT)
  return bool((a["rgb"][:rows] == b["rgb"][:rows]).all())


def main():
  floor = shipped_floor()
  predicted = 255.0 * aces(MOONLESS_SKY_RADIANCE * floor)
  print("scn_nightcal NIGHT_FLOOR=%g -> predicted moonless sky %.3f of an 8-bit step"
        % (floor, predicted), flush=True)

  tmp = tempfile.mkdtemp(prefix="nightdisp_")
  deep      = measure(render(os.path.join(tmp, "deep.png"),      TOD_DEEP))
  deep_pre  = measure(render(os.path.join(tmp, "deep_pre.png"),  TOD_DEEP,  PRE_FLOOR))
  early     = measure(render(os.path.join(tmp, "early.png"),     TOD_EARLY))
  early_b   = measure(render(os.path.join(tmp, "early_b.png"),   TOD_EARLY))
  early_pre = measure(render(os.path.join(tmp, "early_pre.png"), TOD_EARLY, PRE_FLOOR))

  print("=== display readings (8-bit steps) ===", flush=True)
  for name, m in (("deep", deep), ("deep_pre", deep_pre),
                  ("early", early), ("early_b", early_b), ("early_pre", early_pre)):
    print("  %-10s sky=%7.3f terrain=%7.3f |delta|=%7.3f stars=%7d"
          % (name, m["sky"], m["terr"], m["delta"], m["stars"]), flush=True)

  ##########################################################################
  # a/b/c — THE DEAD OF NIGHT IS ON SCREEN
  ##########################################################################
  print("=== the dead of night, at shipped defaults ===", flush=True)
  ok_sky, ok_line, ok_star = claims_hold(deep)
  check("sky_clears_the_quantizer", ok_sky,
        "sky mean %.3f steps (min %.1f)" % (deep["sky"], SKY_MEAN_MIN))
  check("sky_matches_the_declared_floor_anchor", deep["sky"] >= 0.5 * predicted,
        "measured %.3f vs predicted %.3f steps" % (deep["sky"], predicted))
  check("skyline_separates_terrain_from_sky", ok_line,
        "|terrain-sky| %.3f steps (min %.1f)" % (deep["delta"], SKYLINE_MIN))
  check("stars_are_visible", ok_star,
        "%d pixels >= background+%d (min %d)" % (deep["stars"], STAR_OVER, STAR_PIXELS_MIN))

  ##########################################################################
  # TEETH — the pre-calibration frame must FAIL all three
  ##########################################################################
  print("=== teeth: the same hour at the pre-calibration anchor ===", flush=True)
  check("pre_calibration_frame_fails_every_display_claim",
        not any(claims_hold(deep_pre)),
        "sky=%.3f delta=%.3f stars=%d" % (deep_pre["sky"], deep_pre["delta"],
                                          deep_pre["stars"]))

  ##########################################################################
  # THE EARLY NIGHT DID NOT MOVE — per pixel
  ##########################################################################
  print("=== the early night is untouched ===", flush=True)

  # what does THIS bench do when nothing changes at all? Not a claim — this only
  # sets the epsilon and goes into the verdict line, so a cross-leg reader can
  # see what the machine did.
  noise = frame_delta(early, early_b)
  eps   = max(EPS_MIN, int(math.ceil(noise["pixel"] * EPS_MARGIN)))
  print("  bench noise probe (same config, two processes): pixel=%d hist=%d "
        "npix=%d strict_band_identical=%d -> eps=%d steps"
        % (noise["pixel"], noise["hist"], noise["npix"],
           int(strict_band_identical(early, early_b)), eps), flush=True)

  cal = frame_delta(early, early_pre)
  print("  across the calibration: whole_frame_identical=%d pixel=%d hist=%d "
        "npix=%d sky %.6f vs %.6f"
        % (int(early["bytes"] == early_pre["bytes"]), cal["pixel"], cal["hist"],
           cal["npix"], early["sky"], early_pre["sky"]), flush=True)

  # the band that carries the claim gets the strongest form there is.
  check("early_night_sky_band_is_byte_identical_across_the_calibration",
        strict_band_identical(early, early_pre),
        "rows above y=%.2f, no epsilon" % STRICT_CUT)

  # the whole frame gets the form the bench can support, with the noise named.
  check("early_night_full_frame_is_invariant_across_the_calibration",
        is_invariant(early, early_pre, eps),
        "eps=%d from bench noise %d steps/%d px: pixel %d, histogram %d, "
        "sky %.*f vs %.*f"
        % (eps, noise["pixel"], noise["npix"], cal["pixel"], cal["hist"],
           MEAN_DECIMALS, early["sky"], MEAN_DECIMALS, early_pre["sky"]))

  # the tolerant half is not a rubber stamp. Same floor lever, at the hour where
  # the anchor is reachable: rejected at this run's epsilon AND at the tightest
  # epsilon the leg can ever derive.
  teeth = frame_delta(deep, deep_pre)
  check("the_tolerant_invariance_form_rejects_a_real_floor_move",
        (not is_invariant(deep, deep_pre, eps))
        and (not is_invariant(deep, deep_pre, EPS_MIN)),
        "deep vs deep_pre at eps=%d and eps=%d: pixel %d, histogram %d, "
        "sky %.3f vs %.3f"
        % (eps, EPS_MIN, teeth["pixel"], teeth["hist"], deep["sky"], deep_pre["sky"]))

  ok = (len(failures) == 0)
  detail = ("deep sky=%.3f delta=%.3f stars=%d | pre sky=%.3f delta=%.3f stars=%d | "
            "early %.3f (bench noise %dsteps/%dpx eps=%d; skyband_bytes_identical=%d; "
            "full-frame pixel=%d hist=%d) | adapt_floor=%g"
            % (deep["sky"], deep["delta"], deep["stars"],
               deep_pre["sky"], deep_pre["delta"], deep_pre["stars"],
               early["sky"], noise["pixel"], noise["npix"], eps,
               int(strict_band_identical(early, early_pre)),
               cal["pixel"], cal["hist"], floor))
  if failures:
    detail += " failed=" + ",".join(failures)
  verdict(ok, detail)
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main())
