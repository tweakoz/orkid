#!/usr/bin/env ork.python
################################################################################
# SCENE ADAPTATION CURVE gate (procsky wave4 slice W4-S1).
#
# The adaptation term is the CONTENT half of exposure: a function of the
# measured available light, evaluated per frame on the ACES stage. This gate
# exercises that function directly (PostFxNodeACES.sceneAdaptation is pure), so
# it needs no device, no sky and no frame — which is why it can afford to sweep
# it densely instead of sampling two points.
#
# FOUR CLAIMS:
#
#  1 SIGN. Over the FLOOR..TWILIGHT limb — the one a moonrise travels — a
#    BRIGHTER environment must get LESS gain. A sign error here is the classic
#    invisible bug: the picture stays plausible (it just looks punchy) while the
#    adaptation is fighting the scene instead of following it.
#
#  2 THE OWNER'S ANCHORS. Day reads 1.0 (the identity: a daylit frame is graded
#    by the tone curve alone). Early night lands in 0.5..0.7 — the EARLY night
#    sits BELOW day, compressed, NOT lifted. The dead of night, where the only
#    light left is the night-emission floor, gets MORE gain than early night.
#
#    IT MAY ALSO GET MORE THAN DAY, and that is a calibration, not a slip: a
#    moonless sky reaches the tone stage at ~4.5e-5 of radiance, so at a gain
#    near unity the ACES curve quantizes it to 0.002 of one 8-bit step — black
#    on any monitor. The floor anchor is the dark-adaptation opening the renderer
#    has no other model for, and a clear-sky scene authors it in the hundreds
#    (scn_nightcal). So the claims here are RATIOS and shapes, never ceilings.
#    The early-night band is the invariant, and it is EXACTLY invariant: the
#    floor anchor exists only on the FLOOR..TWILIGHT limb, so nothing at or above
#    the twilight luminance can read it — swept and asserted below, which is what
#    lets that anchor be re-tuned by eye without re-checking the first half of
#    the night.
#
#  3 CONTINUITY. No step anywhere across four and a half decades of luminance,
#    including across the anchor joins where two smoothsteps hand over. Measured
#    RELATIVE to the local value: the curve now spans three decades, and an
#    absolute step budget would read the floor limb's honest slope as a break.
#
#  4 THE ELEVATION RAMP IS GONE, not bypassed: the sun-elevation curve module
#    and its per-tick sim drive must not exist on disk, and the notify channel
#    that carried it must not exist in the engine. Adaptation is a function of
#    MEASURED LIGHT; an elevation reading may only ever SEED the warm-up window
#    before the first measurement, which claim 5 pins down.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import math
from orkengine.core import vec3
from orkengine import lev2
from ork.testing import verdict

# the measured ladder this engine reports for those skies
# (test_scene_adaptation_luminance_gate).
L_DAY            = 9.37e-2
L_TWILIGHT       = 5.49e-4
L_NIGHT_MOONLESS = 1.79e-5

# What a MOONLESS sky hands the tone stage, scene-referred — the airglow floor
# through the shipped sky exposure, read off the rendered frame the display gate
# measures (test_night_display_visibility_gate). Radiance, not the measured
# ambient luminance above: this is the number the ACES curve is applied to.
MOONLESS_SKY_RADIANCE = 4.5e-5

# The shipped tone curve (framefx.fxv2 lib_aces, the Narkowicz fit), duplicated
# here for one reason: this gate's business is the EXPOSURE the night is
# developed at, and an exposure claim that never touches the curve it is
# developed through is not a display claim at all.
def _aces(x):
  a, b, c, d, e = 2.51, 0.03, 2.43, 0.59, 0.14
  return max(0.0, min(1.0, (x * (a * x + b)) / (x * (c * x + d) + e)))

# claim 4's removal list, workspace-relative.
REMOVED_FILES = [
    "obt.project/scripts/ork/hypergraph/ecs/scene/_display_exposure.py",
    "obt.project/scripts/ork/hypergraph/ecs/scene/_display_exposure_system.py",
    "obt.project/unittests/display_exposure.py",
]
REMOVED_SYMBOL_FILES = [
    ("ork.ecs/src/scenegraph/SceneGraphSystem.cpp", "SetDisplayExposure"),
    ("ork.ecs/inc/ork/ecs/SceneGraphComponent.h", "_warnedNoDisplayExposureNode"),
]

failures = []


def check(label, ok, detail=""):
  print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
  if not ok:
    failures.append(label)


def main():
  node = lev2.PostFxNodeACES()
  A = lambda L: float(node.sceneAdaptation(L))

  print("=== anchors ===", flush=True)
  a_day   = A(L_DAY)
  a_twi   = A(L_TWILIGHT)
  a_floor = A(L_NIGHT_MOONLESS)
  print("  day        L=%.6g -> %.6f" % (L_DAY, a_day), flush=True)
  print("  twilight   L=%.6g -> %.6f" % (L_TWILIGHT, a_twi), flush=True)
  print("  deep night L=%.6g -> %.6f" % (L_NIGHT_MOONLESS, a_floor), flush=True)

  ##########################################################################
  # 1 SIGN, on the limb the anchors define as decreasing.
  ##########################################################################
  dark, bright = 3.0e-5, 3.0e-4
  a_dark, a_bright = A(dark), A(bright)
  print("=== sign ===", flush=True)
  print("  dark L=%.3g -> %.6f   bright L=%.3g -> %.6f" % (dark, a_dark, bright, a_bright),
        flush=True)
  check("adaptation_decreases_for_the_brighter_environment", a_bright < a_dark,
        "bright=%.6f dark=%.6f" % (a_bright, a_dark))

  ##########################################################################
  # 2 THE OWNER'S ANCHORS
  ##########################################################################
  check("day_is_identity", abs(a_day - 1.0) < 0.02, "%.6f" % a_day)
  check("early_night_in_owner_band", 0.5 <= a_twi <= 0.7, "%.6f" % a_twi)
  check("early_night_sits_below_day", a_twi < a_day,
        "early=%.6f day=%.6f" % (a_twi, a_day))
  check("dead_of_night_more_gain_than_early_night", a_floor > a_twi,
        "deep=%.6f early=%.6f" % (a_floor, a_twi))
  # WHAT THE FLOOR ANCHOR IS WORTH ON A MONITOR, printed rather than asserted:
  # the moonless sky lands on the tone stage at MOONLESS_SKY_RADIANCE, and the
  # product through the ACES curve is what the 8-bit store rounds. The DISPLAY
  # claim belongs on pixels and lives in test_night_display_visibility_gate;
  # this line exists so a reader of the curve can see, without rendering, which
  # side of the quantizer the shipped anchor is on.
  disp = 255.0 * _aces(MOONLESS_SKY_RADIANCE * a_floor)
  print("  moonless sky %.3g x A=%.3f -> %.4f of an 8-bit step "
        "(scn_nightcal declares 512, which is 3.33)" %
        (MOONLESS_SKY_RADIANCE, a_floor, disp), flush=True)
  # THE EARLY NIGHT IS UNTOUCHED BY THE FLOOR ANCHOR — structurally, not
  # numerically: sweep the floor over four decades and the twilight reading must
  # not move by a bit. This is the assertion that lets the floor be re-tuned by
  # eye without anyone re-checking the first half of the night.
  twi_invariant = True
  for probe in (0.65, 64.0, 512.0, 4096.0):
    n2 = lev2.PostFxNodeACES()
    n2.adapt_floor = probe
    twi_invariant = twi_invariant and (float(n2.sceneAdaptation(L_TWILIGHT)) == a_twi)
  check("early_night_is_invariant_under_the_floor_anchor", twi_invariant,
        "twilight reads %.9g for adapt_floor in 0.65..4096" % a_twi)

  ##########################################################################
  # 3 CONTINUITY across the whole measured range, log-swept.
  ##########################################################################
  N = 2000
  lo, hi = -6.0, 0.0            # 1e-6 .. 1.0, four decades wider than measured

  def worst_step(n):
    """largest adjacent step, RELATIVE to the local value. Relative because the
    curve spans three decades: an absolute budget would read the floor limb's
    honest slope as a break."""
    v = [A(10.0 ** (lo + (hi - lo) * i / float(n))) for i in range(n + 1)]
    return max(abs(v[i + 1] - v[i]) / max(v[i], v[i + 1]) for i in range(n)), v

  worst, vals = worst_step(N)
  worst2, _   = worst_step(2 * N)
  # THE TEST IS RESOLUTION SCALING, not a step budget. Sampling a SMOOTH curve
  # twice as densely halves its worst step; sampling a curve with a JUMP in it
  # does not move that jump at all. So the ratio separates the two with no
  # constant to re-tune whenever an anchor moves — which matters here, because
  # the honest tail of a 1000:1 span really is 0.1 of the local value per sample.
  ratio = worst2 / max(worst, 1e-12)
  print("=== continuity ===", flush=True)
  print("  worst adjacent step %.3g of the local value at %d samples, %.3g at %d "
        "(ratio %.3f; smooth -> ~0.5, a jump -> ~1.0)"
        % (worst, N + 1, worst2, 2 * N + 1, ratio), flush=True)
  check("curve_is_continuous", ratio < 0.7, "refinement ratio %.3f" % ratio)
  # bounded BY THE ANCHORS it was built from — the curve may never overshoot the
  # largest one, which is what a bad smoothstep or a sign error would do.
  ceiling = max(float(node.adapt_day), float(node.adapt_twilight), float(node.adapt_floor))
  check("curve_stays_within_its_anchors",
        min(vals) > 0.0 and max(vals) <= ceiling + 1e-6,
        "min=%.6f max=%.6f ceiling=%.6f" % (min(vals), max(vals), ceiling))

  ##########################################################################
  # 4 THE ELEVATION RAMP IS REMOVED, not bypassed
  ##########################################################################
  print("=== elevation-ramp removal ===", flush=True)
  for rel in REMOVED_FILES:
    p = os.path.join(_ROOT, rel)
    check("removed_" + os.path.basename(rel), not os.path.exists(p), rel)
  for rel, sym in REMOVED_SYMBOL_FILES:
    p = os.path.join(_ROOT, rel)
    txt = open(p).read() if os.path.exists(p) else ""
    check("removed_symbol_" + sym, sym not in txt, "%s in %s" % (sym, rel))

  ##########################################################################
  # 5 THE SEED is a bootstrap, and only that: it applies ONLY where there is
  #   no measurement, and it never bends a measured reading.
  ##########################################################################
  print("=== elevation seed ===", flush=True)
  seed_up   = float(node.sceneAdaptation(-1.0, 1.0))
  seed_down = float(node.sceneAdaptation(-1.0, -1.0))
  print("  unmeasured, sun up -> %.6f   sun down -> %.6f" % (seed_up, seed_down), flush=True)
  check("seed_sun_up_is_day_anchor", abs(seed_up - float(node.adapt_day)) < 1e-6,
        "%.6f" % seed_up)
  check("seed_sun_down_is_floor_anchor", abs(seed_down - float(node.adapt_floor)) < 1e-6,
        "%.6f" % seed_down)
  same = all(abs(float(node.sceneAdaptation(10.0 ** e, 1.0))
                 - float(node.sceneAdaptation(10.0 ** e, -1.0))) < 1e-9
             for e in (-6.0, -4.5, -3.0, -1.5, 0.0))
  check("measured_reading_ignores_the_seed", same)

  ok = (len(failures) == 0)
  detail = ("day=%.4f early=%.4f deep=%.4f sign(bright=%.4f<dark=%.4f) worststep=%.3g"
            % (a_day, a_twi, a_floor, a_bright, a_dark, worst))
  if failures:
    detail += " failed=" + ",".join(failures)
  verdict(ok, detail)
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main())
