#!/usr/bin/env ork.python
################################################################################
# HAZE DECK<->SKY COUPLING GATE (G3) — the cloud deck and the sky are ONE
# atmosphere, machine-checked.
#
# The cloud decks used to fake aerial perspective with two deck-local constants:
# an e-folding distance per layer and a frozen horizon color. That produces a
# picture which reads plausibly at ONE hour and lies at every other one — the
# horizon color does not know the sun set, so the distant deck keeps the tint it
# was authored with. S9 replaced it with the engine's own seam
# (skyAerialPerspective, lib_sky): the SAME march, through the SAME Hillaire
# medium, that the forward fragments run and the sky LUTs are baked from. What
# this gate exists to prove is that the replacement is REAL — that the deck's
# horizon is the atmosphere's, and moves when the atmosphere does.
#
# Three things are measured:
#
#   (i) CONVERGENCE, at TWO sun elevations. At the geophysical density the deck
#       and the sky integrate exactly the same medium, so a deck fragment must
#       lose its own contrast against the sky with distance. Measured as the
#       surviving fraction
#           f = |deck_armed - sky| / |deck_disarmed - sky|
#       over the same pixels — f IS the deck's transmittance (deck = raw*T +
#       airlight), which makes it immune to the sky's own gradient and to the
#       deck's radiometry: the same sky value and the same raw deck sit in both
#       terms. f must be near 1 in the NEAR band (deck ~5 km up-frame) and well
#       below it in the FAR band (deck at its 20+ km rim), at BOTH hours. The
#       reference is MEASURED, not modelled: the haze-disabled run is the
#       un-hazed deck and the deck-less run is the sky along the deck's OWN
#       rays, so no color in this gate is hand-derived.
#
#       The far band is ALSO gated in absolute levels — |far_deck - far_sky| on
#       the worst channel, against a ceiling taken from the measurement — which
#       is the coupling claim in the units a viewer sees. It is a BOUND and not
#       an equality on purpose: a deck at 20 km is fronted by 20 km of airlight
#       while the sky pixel beside it is fronted by the whole column, which is
#       legitimately brighter, so demanding equality would gate a bug that is
#       not one. The residual is printed at both hours either way.
#
#  (ii) MOVEMENT. Between the two hours the far-band deck color must move by at
#       least K (=0.5) times what the sky at those same pixels moved. A
#       re-hardcoded constant cannot pass this: a frozen horizon color moves
#       only with the deck's own body radiance while the sky swings through the
#       whole sunset. This is the leg that would have caught the defect the old
#       model shipped.
#
# (iii) NIGHT SANITY (the jul30 defect class): after sunset the deck must not
#       GLOW against the night sky. Asserted as a bound on how far the deck
#       band's levels may sit above the sky at the same pixels.
#
# SCENE + HARNESS. The deck only renders through the real content path (an ECS
# scene: library cloud_decks() on the shipped look table, procedural sky, frozen
# clock), so this gate AUTHORS a bench scene per state in one headless process
# and then snapshots each through `ork.ecs.player.exe --offscreen --snapshot` —
# the same path ork.scene.materialize.py uses, and the one the shipped scenes
# are verified with. Eight states: {deck haze-off, sky haze-off, sky hazed, deck
# hazed} x {noon, dusk}, plus {sky, deck} hazed after sunset.
#
# The bench scene is: ONE cumulus deck (shipped _LAYER_LOOK), no terrain (a
# ground would be hazed by a different provider and is not what is under test),
# stars/moon off, wind and cloud evolution FROZEN (ORK_CLOUDGAUGE_WINDX/_EVO —
# every state is a separate process, so a drifting deck would break the
# pixel-for-pixel comparison the fractions depend on) and the clock frozen at
# the hour. The player's orbit camera puts the horizon just under mid-frame
# with the deck's whole distance ladder above it.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
# FROZEN DECK: the wind scroll and the B-channel evolution ride the GPU clock,
# which is wall-time driven — with them alive, two states captured in two
# processes would not be the same cloud. Set before the deck library imports:
# both multipliers are read at module import.
os.environ["ORK_CLOUDGAUGE_WINDX"] = "0.0"
os.environ["ORK_CLOUDGAUGE_EVO"] = "0.0"

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

# ---- the bench scene -------------------------------------------------------
SITE_LAT_DEG  = 45.0
SITE_DOY      = 220.0
TOD_NOON      = 12.5      # sun high
TOD_DUSK      = 19.3      # sun low, the sky in full swing
TOD_NIGHT     = 22.5      # sun well under
DECK_COVER    = 0.45      # AT the deck's gray_on (0.45): no overcast graying, so
                          # leg (i) reads aerial perspective and not the
                          # cover-driven chroma pull (its own look tier)
DECK_SPEC     = [("cloud_cumulus_2k", "cumulus", "cumulus2k")]

# GEOPHYSICAL-to-moderate. The artist ground layer is kept light on purpose: at
# cranked density the whole frame saturates to one in-scatter color and every
# fraction here divides noise by noise. G2 owns the artist layer's own limits;
# this gate owns the deck<->sky agreement.
HAZE_KNOBS = {"distance_m": 60000.0, "scale_height_m": 1200.0, "phase_g": 0.6}

CAM_DIST_M    = 600.0     # player orbit camera: the horizon sits just under
CAM_HEIGHT_M  = 60.0      # mid-frame with the deck's ladder above it
SNAPSHOT_FRAME = 25       # frames after first-lit (the sky LUTs settle in ~2)
PLAYER_TIMEOUT = 120.0    # per state

# ---- bars (all measured on this scene; see the verdict detail line) ---------
DECK_DELTA_MIN = 10.0    # 8-bit levels: how far the un-hazed deck must sit off
                         # the sky for a pixel to count as deck at all
MIN_BAND_PX    = 250     # a band the detector cannot fill is a FAIL, not a skip
# leg (i), measured on this scene (noon / dusk): f_near 0.823 / 0.767,
# f_far 0.667 / 0.268, |far_deck-far_sky| 8.9 / 0.9 levels.
NEAR_F_MIN     = 0.70    # (i) up-frame deck keeps most of its own contrast
FAR_F_MAX      = 0.75    # (i) rim deck has visibly melted into the sky
FAR_DIST_MAX   = 14.0    # (i) ... and IS the sky there, to within this many
                         # 8-bit levels on the worst channel — the direct
                         # coupling claim, in the units a viewer sees. Only the
                         # NOON reading constrains anything (a dark dusk frame
                         # passes it for free), which is why the fraction above
                         # is gated beside it at both hours.
F_DROP_MIN     = 0.10    # (i) and the drop near->far is not measurement noise
MOVE_K         = 0.5     # (ii) deck movement, as a fraction of the sky's
MOVE_SKY_MIN   = 8.0     # (ii) ... and the sky itself must actually have moved,
                         # or the ratio would divide noise by noise
NIGHT_GLOW_MAX = 8.0     # (iii) levels the night deck band may sit above the sky

OUT_DIR = os.environ.get("HAZE_G3_OUT", "/tmp/haze_deck_sky_coupling")

# (key, time_of_day, haze armed, deck present)
#
# TWO sky references per hour, and they are NOT interchangeable: the haze runs on
# the SKY pixels too (skyHazeSkyOverlay), so the frame a hazed deck must be read
# against is the hazed sky, while the frame that says where the deck IS and what
# its own un-hazed contrast was must be the UN-hazed one. Collapsing the two --
# which was sound only while the skybox opted out of the artist layer -- makes
# the deck detector fire on every sky pixel in the frame.
STATES = [("n_off",  TOD_NOON,  False, False),
          ("n_raw",  TOD_NOON,  False, True),
          ("n_sky",  TOD_NOON,  True,  False),
          ("n_deck", TOD_NOON,  True,  True),
          ("d_off",  TOD_DUSK,  False, False),
          ("d_raw",  TOD_DUSK,  False, True),
          ("d_sky",  TOD_DUSK,  True,  False),
          ("d_deck", TOD_DUSK,  True,  True),
          ("x_sky",  TOD_NIGHT, True,  False),
          ("x_deck", TOD_NIGHT, True,  True)]

_CFG = {}   # the state the scene class below authors (set per build)


class DeckBenchScene(Scene):
  """ONE cumulus deck under the procedural sky on a frozen clock. No terrain,
  no moon, no stars: every pixel in frame is sky or deck, which is what lets the
  deck-less run stand in as the sky along the deck's own rays."""

  def __init__(self):
    super().__init__()
    haze = dict(HAZE_KNOBS)
    haze["enable"] = bool(_CFG["armed"])
    self.sky(
        latitude_deg = SITE_LAT_DEG,
        day_of_year  = SITE_DOY,
        time_of_day  = float(_CFG["tod"]),
        time_scale   = 0.0,        # FROZEN: the library day runs ~2 deg of sun
                                   # per wall second; two states must be the
                                   # same sky, not the same intent
        moon         = False,      # a risen moon is a second sky
        stars        = False,
        clouds       = bool(_CFG["deck"]),
        cloud_cover  = DECK_COVER if _CFG["deck"] else None,
        cloud_params = {"specs": DECK_SPEC} if _CFG["deck"] else None,
        haze         = haze,
        skybox_path  = "<ork_envmaps2>/desert4k.xir",   # IBL warm-up only
        msaa         = 0,
        ssaa         = 0)


__all__ = ["DeckBenchScene"]


###############################################################################
# capture
###############################################################################

def author_scenes(tmpdir):
  """Serialize one .ecs per state, in ONE headless GPU-bound process (the
  materialize.py author phase: the deck's generated material composes inline)."""
  paths = {}
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  try:
    for key, tod, armed, deck in STATES:
      _CFG.clear()
      _CFG.update(tod=tod, armed=armed, deck=deck)
      scene = DeckBenchScene()
      sd = ecs.SceneData()
      scene.build(sd)
      path = os.path.join(tmpdir, "g3_%s.ecs" % key)
      with open(path, "w") as f:
        f.write(sd.serializeJson())
      paths[key] = path
      print("[haze-g3] authored %-6s tod=%.2f armed=%d deck=%d" % (key, tod, armed, deck),
            flush=True)
  finally:
    ezapp.mainThreadEnd()
    ecs.headless_exit()
  return paths


def snapshot(ecs_path, png_path):
  """One offscreen player run — the shipped deck render path, frame-budgeted."""
  runner = shutil.which("ork.ecs.player.exe")
  if runner is None:
    raise RuntimeError("ork.ecs.player.exe not on PATH")
  cmd = [runner, ecs_path, "--offscreen",
         "--snapshot", png_path,
         "--snapshot-frame", str(SNAPSHOT_FRAME),
         "--camdist", str(CAM_DIST_M),
         "--camheight", str(CAM_HEIGHT_M)]
  r = subprocess.run(cmd, capture_output=True, text=True, timeout=PLAYER_TIMEOUT)
  ok = os.path.isfile(png_path)
  if not ok:
    tail = "\n".join((r.stdout or "").splitlines()[-25:])
    print("[haze-g3] player FAILED rc=%d for %s\n%s" % (r.returncode, ecs_path, tail),
          flush=True)
  return ok


def load(png_path):
  img = numpy.asarray(PILImage.open(png_path).convert("RGB"), dtype=numpy.float64)
  return img


###############################################################################
# observables
###############################################################################

def bands(raw, sky_off):
  """(far_mask, near_mask, detail) — deck pixels split by how far the deck is at
  that pixel. The deck's own dome does the ranging: it sags from overhead to the
  skyline, so image ROW is distance. Deck pixels are found by DIFFERENCE (the
  un-hazed deck against the same frame with no deck at all), never by projecting
  a camera model — a hand-rolled fov/orientation model is exactly the drift that
  reads as a physics failure. The snapshot is written top-row-first, so a LARGER
  row index is LOWER in the picture, i.e. farther along the deck.

  BOTH frames are haze-OFF, and that is load-bearing: differencing across the
  haze state would call every sky pixel a deck pixel now that the overlay moves
  the sky as well."""
  delta = numpy.abs(raw - sky_off).max(axis=2)
  mask = delta > DECK_DELTA_MIN
  rows = numpy.nonzero(mask.any(axis=1))[0]
  if rows.size < 8:
    return None, None, "deck not detected (%d rows)" % rows.size
  r0, r1 = int(rows.min()), int(rows.max())
  cut = max(2, int(round(0.15 * (r1 - r0))))
  far = numpy.zeros_like(mask)
  near = numpy.zeros_like(mask)
  far[r1 - cut:r1 + 1] = mask[r1 - cut:r1 + 1]
  near[r0:r0 + cut + 1] = mask[r0:r0 + cut + 1]
  det = ("deck rows %d..%d, far rows %d..%d (%d px), near rows %d..%d (%d px)"
         % (r0, r1, r1 - cut, r1, int(far.sum()), r0, r0 + cut, int(near.sum())))
  return far, near, det


def fraction(raw, sky_off, deck, sky_on, mask):
  """surviving fraction of the deck's own contrast against the sky, per pixel
  then averaged — |deck-sky_on| / |raw-sky_off| IS the transmittance. Each term
  reads the sky of ITS OWN haze state: the hazed deck stands against a hazed
  sky, and the reference contrast it is measured against is the un-hazed one."""
  r, s, d = raw[mask], sky_off[mask], deck[mask]
  c_raw = numpy.abs(r - s).mean(axis=1)
  c_hzd = numpy.abs(d - sky_on[mask]).mean(axis=1)
  keep = c_raw > DECK_DELTA_MIN * 0.5
  if not keep.any():
    return float("nan"), 0
  return float((c_hzd[keep] / c_raw[keep]).mean()), int(keep.sum())


def band_mean(img, mask):
  return img[mask].mean(axis=0)


def analyze(shots):
  failures = []

  def check(label, ok, detail=""):
    print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
    if not ok:
      failures.append(label)

  far, near, det = bands(shots["n_raw"], shots["n_off"])
  check("deck_bands_located",
        far is not None and int(far.sum()) >= MIN_BAND_PX
        and int(near.sum()) >= MIN_BAND_PX, str(det))
  if failures:
    return False, "setup: " + ",".join(failures) + " | " + str(det)

  rows = {}
  for tag, h in (("noon", "n"), ("dusk", "d")):
    raw, sky, deck = shots[h + "_raw"], shots[h + "_sky"], shots[h + "_deck"]
    sky_off = shots[h + "_off"]
    f_far, n_far = fraction(raw, sky_off, deck, sky, far)
    f_near, n_near = fraction(raw, sky_off, deck, sky, near)
    m_deck, m_sky = band_mean(deck, far), band_mean(sky, far)
    rows[tag] = dict(f_far=f_far, f_near=f_near, deck=m_deck, sky=m_sky,
                     resid=float(numpy.abs(m_deck - m_sky).max()))
    print("[haze-g3] %-4s far f=%.3f (%d px) near f=%.3f (%d px) "
          "far_deck=(%5.1f,%5.1f,%5.1f) far_sky=(%5.1f,%5.1f,%5.1f) |deck-sky|max=%.1f"
          % (tag, f_far, n_far, f_near, n_near,
             m_deck[0], m_deck[1], m_deck[2], m_sky[0], m_sky[1], m_sky[2],
             rows[tag]["resid"]), flush=True)

  for tag in ("noon", "dusk"):
    r = rows[tag]
    check("i_%s_near_deck_keeps_contrast" % tag, r["f_near"] >= NEAR_F_MIN,
          "f_near=%.3f (floor %.2f)" % (r["f_near"], NEAR_F_MIN))
    check("i_%s_far_deck_melted" % tag, r["f_far"] <= FAR_F_MAX,
          "f_far=%.3f (ceiling %.2f)" % (r["f_far"], FAR_F_MAX))
    check("i_%s_far_deck_is_the_sky" % tag, r["resid"] <= FAR_DIST_MAX,
          "|far_deck-far_sky|max=%.1f levels (ceiling %.1f)" % (r["resid"], FAR_DIST_MAX))
    check("i_%s_melt_is_distance_driven" % tag,
          (r["f_near"] - r["f_far"]) >= F_DROP_MIN,
          "near-far=%.3f (floor %.2f)" % (r["f_near"] - r["f_far"], F_DROP_MIN))

  move_deck = float(numpy.abs(rows["noon"]["deck"] - rows["dusk"]["deck"]).mean())
  move_sky = float(numpy.abs(rows["noon"]["sky"] - rows["dusk"]["sky"]).mean())
  check("ii_sky_actually_moved", move_sky >= MOVE_SKY_MIN,
        "sky_move=%.1f levels (floor %.1f)" % (move_sky, MOVE_SKY_MIN))
  check("ii_deck_horizon_tracks_the_sky", move_deck >= MOVE_K * move_sky,
        "deck_move=%.1f vs %.2f x sky_move %.1f = %.1f"
        % (move_deck, MOVE_K, move_sky, MOVE_K * move_sky))

  n_deck, n_sky = band_mean(shots["x_deck"], far), band_mean(shots["x_sky"], far)
  glow = float((n_deck - n_sky).max())
  check("iii_night_deck_does_not_glow", glow <= NIGHT_GLOW_MAX,
        "far_band deck=(%.1f,%.1f,%.1f) sky=(%.1f,%.1f,%.1f) excess=%.1f (ceiling %.1f)"
        % (n_deck[0], n_deck[1], n_deck[2], n_sky[0], n_sky[1], n_sky[2],
           glow, NIGHT_GLOW_MAX))

  # the documented v1 residual, printed as evidence (NOT gated): the far deck is
  # fronted by its own distance of airlight, the sky beside it by the whole
  # column, so the two converge without meeting.
  print("[haze-g3] |far_deck - far_sky| max channel: noon=%.1f dusk=%.1f"
        % (rows["noon"]["resid"], rows["dusk"]["resid"]), flush=True)

  detail = ("f_near/f_far noon=%.3f/%.3f dusk=%.3f/%.3f move deck=%.1f sky=%.1f "
            "night_excess=%.1f"
            % (rows["noon"]["f_near"], rows["noon"]["f_far"],
               rows["dusk"]["f_near"], rows["dusk"]["f_far"],
               move_deck, move_sky, glow))
  if failures:
    detail += " failed=" + ",".join(failures)
  return (len(failures) == 0), detail


def main():
  wd = Watchdog(540.0, label="haze_deck_sky_coupling").arm()
  os.makedirs(OUT_DIR, exist_ok=True)
  tmpdir = tempfile.mkdtemp(prefix="haze_g3_")
  try:
    paths = author_scenes(tmpdir)
    shots = {}
    for key, _tod, _armed, _deck in STATES:
      png = os.path.join(OUT_DIR, "%s.png" % key)
      if os.path.isfile(png):
        os.remove(png)
      if not snapshot(paths[key], png):
        wd.disarm()
        return verdict(False, "player produced no snapshot for state %r" % key)
      img = load(png)
      shots[key] = img
      print("[haze-g3] captured %-6s %dx%d mean=%.2f max=%d"
            % (key, img.shape[1], img.shape[0], img.mean(), int(img.max())), flush=True)
    ok, detail = analyze(shots)
  finally:
    shutil.rmtree(tmpdir, ignore_errors=True)
  wd.disarm()
  return verdict(ok, detail)


if __name__ == "__main__":
  sys.exit(main())
