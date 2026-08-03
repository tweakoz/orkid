#!/usr/bin/env ork.python
################################################################################
# STAR VISIBILITY MONOTONICITY gate (procsky wave4 slice W4-S1).
#
# THE DEFECT THIS CLOSES. Star visibility used to be max() of two independent
# "is it night" readings — a sun-elevation band and a light-intensity reading.
# Neither was anchored to the other, and they CROSSED between sun elevation
# -4.5 and -7 degrees, so the field briefly went OUT as the sun went down
# (0.62 of full at -4.5, 0.46 at -7). The second reading existed only because
# the sun's uniform block carries the MOON after the night caster handoff, i.e.
# it was keyed to the wrong body; deleting it naively would break the night.
#
# THE FIX IS BY CONSTRUCTION, which is what this gate pins. Visibility is a
# CONTRAST THRESHOLD against the MEASURED sky background: a star is visible
# when it out-shines the sky behind it. That is monotone in the background by
# construction, so nothing can go backwards; it orders stars by their own
# luminance, so the bright ones cross first; and it is not keyed to any body,
# so the handoff is not an event.
#
# THE ORACLE is the law itself, evaluated on the SHIPPED constants imported
# from the material module (change a constant and this gate re-evaluates), swept
# against the sky ladder this engine measures
# (test_scene_adaptation_luminance_gate). It also asserts the two old readings
# are GONE from the material sources rather than merely outvoted.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import importlib.util
from ork.testing import verdict

MTL_DIR = os.path.join(_ROOT, "obt.project", "scripts", "ork", "hypergraph",
                       "assets", "materials")
SPLAT_PY = os.path.join(MTL_DIR, "star_splat.py")
DOME_PY  = os.path.join(MTL_DIR, "star_dome.py")

# THE MEASURED SKY LADDER, brightest first (radiance units).
SKY_DAY        = 9.37e-2   # sun 60 deg up
SKY_SUNSET     = 5.0e-3    # sun just above the horizon (between the two measured
                           # legs; the point the brightest stars must clear)
SKY_TWILIGHT   = 5.49e-4   # sun 4 deg down
SKY_MOONLIT    = 2.64e-4   # moonless-floor sky plus a unit-illuminance moon 45 up.
                           # PREDICTED, battery-verification pending: W16-S1 cut
                           # _moonRayleighStrength 0.032 -> 0.004 to seat a moonlit
                           # night under the tonemap's twilight anchor (was 1.98e-3).
                           # Diagnostic only — it feeds the printed table, no
                           # assertion reads it; the checks sweep sky continuously.
SKY_MOONLESS   = 1.79e-5   # the night-emission floor alone

# CATALOG luminances the splat material carries per star (its payload is linear
# radiance; V=7.96 is 6.5e-4 by the module's own note, and a magnitude is a
# factor of 10**0.4).
def lum_of_magnitude(v):
  return 6.5e-4 * (10.0 ** (0.4 * (7.96 - v)))

STARS = [
    ("Sirius   V=-1.46", lum_of_magnitude(-1.46)),
    ("Vega     V= 0.03", lum_of_magnitude(0.03)),
    ("Polaris  V= 1.98", lum_of_magnitude(1.98)),
    ("naked-eye V=5.00", lum_of_magnitude(5.00)),
    ("faintest V= 7.96", lum_of_magnitude(7.96)),
]

failures = []


def check(label, ok, detail=""):
  print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
  if not ok:
    failures.append(label)


def load_module(path, name):
  # by PATH: importing the package would pull the whole ptex3d DSL chain in, and
  # this gate only wants the module's CONSTANTS.
  src = open(path).read()
  consts = {}
  for line in src.splitlines():
    if line.startswith("THRESHOLD_"):
      k, _, v = line.partition("=")
      consts[k.strip()] = float(v.split("#")[0].strip())
  return consts, src


def visibility(lum, sky, C, B, K):
  """The shipped law: threshold = C * sky**B, visibility = r**K / (1 + r**K)."""
  thresh = C * (max(sky, 1e-9) ** B)
  r = (lum / max(thresh, 1e-12)) ** K
  return r / (1.0 + r)


def main():
  consts, splat_src = load_module(SPLAT_PY, "star_splat")
  _, dome_src       = load_module(DOME_PY, "star_dome")
  C = consts["THRESHOLD_C"]
  B = consts["THRESHOLD_B"]
  K = consts["THRESHOLD_K"]
  print("shipped constants: C=%g B=%g K=%g" % (C, B, K), flush=True)

  ##########################################################################
  # 1 MONOTONICITY — sweep the sky from full day down to the night floor and
  #   assert no star ever gets DIMMER as the sky gets darker.
  ##########################################################################
  N = 400
  hi, lo = SKY_DAY, SKY_MOONLESS
  skies = [hi * ((lo / hi) ** (i / float(N))) for i in range(N + 1)]  # log sweep, descending
  print("=== monotonicity: %d sky samples, %.4g -> %.4g ===" % (len(skies), hi, lo), flush=True)
  worst_back = 0.0
  worst_who = ""
  for name, lum in STARS:
    vis = [visibility(lum, s, C, B, K) for s in skies]
    back = max((vis[i] - vis[i + 1]) for i in range(N))
    if back > worst_back:
      worst_back, worst_who = back, name
    print("  %-18s day=%.6f  sunset=%.6f  twilight=%.6f  moonlit=%.6f  floor=%.6f"
          % (name,
             visibility(lum, SKY_DAY, C, B, K),
             visibility(lum, SKY_SUNSET, C, B, K),
             visibility(lum, SKY_TWILIGHT, C, B, K),
             visibility(lum, SKY_MOONLIT, C, B, K),
             visibility(lum, SKY_MOONLESS, C, B, K)), flush=True)
  check("visibility_never_goes_backwards_as_the_sun_goes_down", worst_back <= 0.0,
        "worst regression %.3g (%s)" % (worst_back, worst_who or "none"))

  ##########################################################################
  # 2 BRIGHTEST FIRST — at every sky level the ordering by catalog luminance
  #   is preserved, and the brightest star is already out while the sun is
  #   still just above the horizon.
  ##########################################################################
  ordered = all(
      all(visibility(STARS[j][1], s, C, B, K) >= visibility(STARS[j + 1][1], s, C, B, K)
          for j in range(len(STARS) - 1))
      for s in skies)
  check("brighter_stars_are_never_less_visible", ordered)

  v_bright_sunset = visibility(STARS[0][1], SKY_SUNSET, C, B, K)
  check("brightest_star_visible_with_the_sun_just_above_the_horizon",
        v_bright_sunset > 0.25, "%.6f at sky=%.3g" % (v_bright_sunset, SKY_SUNSET))

  ##########################################################################
  # 3 THE DAY IS STILL DAY, and the night is still the whole field.
  ##########################################################################
  v_bright_day = visibility(STARS[0][1], SKY_DAY, C, B, K)
  check("no_stars_by_day", v_bright_day < 0.02, "brightest=%.6f" % v_bright_day)
  v_faint_floor = visibility(STARS[-1][1], SKY_MOONLESS, C, B, K)
  check("faintest_star_reaches_the_moonless_floor", v_faint_floor > 0.3,
        "%.6f" % v_faint_floor)

  ##########################################################################
  # 4 SMOOTH FILL-IN — the field's total visibility rises without a step.
  ##########################################################################
  totals = [sum(visibility(l, s, C, B, K) for _, l in STARS) for s in skies]
  worst_step = max(abs(totals[i + 1] - totals[i]) for i in range(N))
  print("=== fill-in: field total %.4f (day) -> %.4f (floor), worst step %.4g ==="
        % (totals[0], totals[-1], worst_step), flush=True)
  check("field_fills_in_smoothly", worst_step < 0.08, "worst step %.4g" % worst_step)

  ##########################################################################
  # 5 THE OLD READINGS ARE GONE, not outvoted.
  ##########################################################################
  print("=== the crossing readings are removed ===", flush=True)
  for label, src in (("star_splat", splat_src), ("star_dome", dome_src)):
    check(label + "_no_elevation_band", "NIGHT_START_DEG" not in src)
    check(label + "_no_intensity_reading", "sun_intensity" not in src)
    check(label + "_reads_measured_sky", "ctx.sky_luminance" in src)

  ok = (len(failures) == 0)
  detail = ("C=%g B=%g K=%g bright(day)=%.5f bright(sunset)=%.5f faint(floor)=%.5f "
            "worst_regression=%.3g" % (C, B, K, v_bright_day, v_bright_sunset,
                                       v_faint_floor, worst_back))
  if failures:
    detail += " failed=" + ",".join(failures)
  verdict(ok, detail)
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main())
