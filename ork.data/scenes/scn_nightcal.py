###############################################################################
# scn_nightcal.py — the NIGHT DISPLAY CALIBRATION rig.
#
# The one thing a night-visibility measurement needs and no shipped gauge scene
# gives: a MOONLESS sky on a FROZEN clock, with a hard skyline and the star field
# in frame, and nothing in the picture whose brightness is a constant.
#
# WHY EACH OF THOSE IS LOAD-BEARING:
#   moon=False    a risen moon is two to three orders of magnitude of extra sky
#                 radiance (the moon-Rayleigh term). The dead-of-night floor is
#                 the sky WITHOUT it, so the moon has to be gone, not merely low.
#   time_scale=0  the library clock runs a day in 180 wall seconds, i.e. ~2 deg
#                 of sun per wall second. An offscreen boot spends tens of those
#                 seconds, so a scene that opens at midnight is measured in
#                 twilight. Zero freezes the ephemeris at time_of_day, which is
#                 what makes two runs comparable at all.
#   clouds off    the deck colors are AUTHORED CONSTANTS (lit/shadow/haze) that
#                 do not darken with the sky, so at night a cloud is the
#                 brightest thing in frame and any "sky mean" reading is really
#                 a cloud reading.
#   no grade      no HSVG gamma, no post lift: the 8-bit frame this scene renders
#                 IS the engine's display-referred night. A grade here would
#                 hide exactly the defect the rig exists to measure.
#
# The terrain is walkable so the player rides the walker camera — level, at eye
# height on the surface — which puts the skyline across the middle of the frame
# with open sky (and stars) above it. That is the geometry the two display
# claims are read off: sky-vs-terrain separation at the skyline, and star pixels
# above it.
#
#   ork.scene.materialize.py scn_nightcal -S /tmp/night.png    # the measurement
#   ork.scene.viewer.py scn_nightcal                           # by eye
#
# TWO KNOBS, and only two — a calibration rig whose own levels drift is not a
# rig:
#   ORK_NIGHTCAL_TOD    the hour on the frozen clock. 0.0 (the default) is the
#                       dead of night the calibration is read off; 19.0 is the
#                       EARLY-NIGHT anchor, the band the owner has already
#                       accepted and which a calibration change must not move.
#   ORK_NIGHTCAL_FLOOR  overrides NIGHT_FLOOR below. Exists so the display gate
#                       can run the SAME hour at this rig's declared value and at
#                       the pre-calibration 0.65 and diff the two frames — an A/B
#                       is the only way to say "the early night did not move"
#                       about a rendered picture — and so the owner can walk the
#                       calibration ladder without editing anything.
###############################################################################

import os

from orkengine.core import vec3

from ork.hypergraph.ecs.scene import Scene

TERRA    = "nightcal_terra"
RELIEF_M = 100.0   # scatterhills default relief

# the frozen site + clock. Latitude/season are the library's procedural-sky day;
# only the hour moves, and only through the knob below.
LATITUDE_DEG = 45.0
DAY_OF_YEAR  = 220.0
TIME_OF_DAY  = float(os.environ.get("ORK_NIGHTCAL_TOD", "0.0"))

# THE CALIBRATION ITSELF — the dead-of-night adaptation anchor this rig is
# calibrated at, and the number the display gate reads its thresholds off. The
# engine ships a far smaller default on purpose (PostFxNodeACES.h): this value
# develops a CLEAR night correctly and clips a CLOUDED one, so until night
# content tracks the sky it is authored per scene rather than globally. A clear-
# sky scene that wants a night on screen states the same thing this rig does:
#     Scene.sky(tonemap={"adapt_floor": 512.0})
# Measured on this rig at the dead of night, in 8-bit steps (sky / |terrain-sky|
# / star pixels): 256 -> 1.90 / 1.17 / 12791, 512 -> 4.51 / 2.57 / 74889,
# 1024 -> 11.37 / 5.70 / 248903. The pre-calibration 0.65 -> 0.21 / 0.00 / 20,
# i.e. black.
NIGHT_FLOOR = 512.0

ADAPT_FLOOR = os.environ.get("ORK_NIGHTCAL_FLOOR", "")


class NightCalScene(Scene):

  def __init__(self):
    super().__init__()

    floor = float(ADAPT_FLOOR) if ADAPT_FLOOR else NIGHT_FLOOR

    self.sky(
        latitude_deg = LATITUDE_DEG,
        day_of_year  = DAY_OF_YEAR,
        time_of_day  = TIME_OF_DAY,
        time_scale   = 0.0,     # FROZEN — see the header
        moon         = False,   # MOONLESS — see the header
        stars        = True,
        clouds       = False,
        tonemap      = {"adapt_floor": floor},
        skybox_path  = "<ork_envmaps2>/desert4k.xir",   # IBL warm-up fallback only
        msaa         = 2,
        ssaa         = 0)

    # the skyline: the lightest walkable terrain DSL in the repo (plain fbm, no
    # erosion loop), baked small — the rig measures BANDS of the frame, not
    # detail, so resolution buys nothing here and costs boot time.
    self.terrain(TERRA,
                 dsl_file         = "scatterhills",
                 spawn            = vec3(0.0, RELIEF_M + 10.0, 0.0),
                 chunk            = 128,
                 render_dimension = 256,
                 bake_dimension   = 256,
                 bake_res         = 1024,
                 walkable         = True)


__all__ = ["NightCalScene"]
