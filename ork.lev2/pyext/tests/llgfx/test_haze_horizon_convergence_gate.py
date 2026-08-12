#!/usr/bin/env ork.python
################################################################################
# HAZE HORIZON CONVERGENCE GATE (G2) — aerial perspective, machine-checked.
#
# Distance haze is not "the frame got foggier". It is a SPECIFIC law: what a
# surface contributes to a pixel decays with the optical depth between it and
# the eye, and what replaces it is the air's own in-scatter. A screenshot that
# merely looks hazier is equally produced by a global tint, by a lift on the
# black point, or by a distance-independent blend — none of which is aerial
# perspective and all of which read fine to an eye. So the two things that ARE
# aerial perspective get measured here, separately, because in v1 they have
# different reference colors:
#
#   A) GEOPHYSICAL CONVERGENCE (haze_density = 0, atmosphere armed). The sky and
#      the geometry are integrating the SAME medium through the SAME LUTs, so a
#      surface must converge to the sky beside it. Measured as the fraction of
#      its own un-hazed contrast against that sky each sphere keeps:
#           f = |sphere_armed - sky| / |sphere_disarmed - sky|
#      f IS the transmittance (sphere = raw*T + sky*(1-T) => sphere - sky =
#      T*(raw - sky)), which is what makes it immune to the sky's azimuthal
#      gradient: the same sky value sits in both terms. f must fall MONOTONE
#      with distance, start at ~1 (the near sphere proves the effect is
#      distance-driven and not a global grade) and end well below 1.
#
#   B) ARTIST-LAYER MONOTONICITY (cranked ground haze):
#        B1  each sphere converges to ITS OWN asymptote, monotonically. The
#            asymptote is MEASURED, not modelled: a fourth pass moves every
#            sphere to ASYM_DIST_M along its own view ray (same pixels, same
#            angular size), where the optical depth is total and the pixel is
#            pure in-scatter for that direction.
#        B2  object-to-object contrast collapses: the fraction of each
#            adjacent-distance pair's own un-hazed max-channel spread that
#            survives must fall monotonically, and the far pair must end up one
#            color. Haze that fails to erase contrast is a tint, not a medium.
#
#   C) THE SKYLINE ITSELF (cranked ground haze). The sky pixels run the artist
#      layer too (skyHazeSkyOverlay), so the one thing leg A can only ask of the
#      geophysical medium is now askable of the artist one: far geometry must
#      converge to the SKY BESIDE IT, not to some private asymptote of its own.
#      Measured in the same currency as leg A — the fraction of its own un-hazed
#      contrast against the sky each sphere keeps, both terms read from the
#      capture they belong to (the hazy sphere against the HAZY sky) — so the
#      bar is a measured reference and not a hand-picked level. Required: the
#      far sphere keeps almost none of it, and strictly less of it than the near
#      sphere does. That is the illusion a dense layer breaks when the skybox
#      opts out: geometry veils, open sky stays clean, and the skyline turns
#      into a hard edge.
#
# THE ASYMPTOTE/SKY GAP IS PRINTED, NOT GATED: |asymptote - hazy sky| per column
# stays in the log. It is no longer a contract gap (leg C gates the convergence
# that matters) but the two integrals still differ — the sphere pass stops at
# 100 km, the sky march runs the full clamp — and that residue is worth reading.
#
# SCENE. Procedural sky, sun 25 deg up and BEHIND the camera: the anti-solar sky
# ahead is the smoothest one available, so in-scatter varies only weakly across
# the 11 degrees the sphere row spans and leg B2's contrast is not really a
# lighting gradient. No ground: the grid/terrain shaders are NOT in the forward
# lighting funnel (they are not hazed in v1) and an unhazed ground under hazed
# spheres would be a misleading picture, so every pixel here is sky or sphere.
# Five lit PBR spheres at 200 m .. 51.2 km on a x4 ladder, equal angular size,
# eye level, so each stands against the horizon with clean sky directly above
# it. The ladder's near end is where the margins are thinnest -- 200 m and 800 m
# of Rayleigh differ by 1.3% of contrast against a 0.5% monotonicity margin --
# and it is kept anyway: dropping the near rungs would also drop the only
# evidence that the effect is distance-driven rather than a global grade.
#
# THE SPHERES ARE FOUND BY COLOR, NOT BY PROJECTION. Each carries a distinct
# saturated hue and is located in the disarmed capture by hue classification.
# A hand-rolled fov/aspect/orientation model of the offscreen buffer is exactly
# the kind of silent drift that reads as a physics failure (the editor camera's
# effective fov is not the one handed to perspective(), and the capture arrives
# rotated); color identification cannot drift, and the image's up direction is
# derived from the sky's own above/below-horizon asymmetry.
#
# FOUR CAPTURES, ONE WARM PROCESS: disarmed (raw), geophysical (density 0),
# hazy (cranked), asymptote (cranked + spheres pushed to ASYM_DIST_M).
#
# Not ork.testing capture_app: that harness drives ONE mainThreadLoop capture
# and cannot host a scenegraph app (see test_sky_floor_gate.py); this gate needs
# four captures with live parameter and transform changes in one warm process.
# The verdict-before-teardown protocol (#57) is honoured.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

import math
import numpy
from PIL import Image as PILImage
from orkengine.core import vec3, vec4
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict, Watchdog

WIDTH, HEIGHT = 800, 600
EYE_Y         = 2.0
CAM_NEAR, CAM_FAR = 0.5, 300000.0   # the asymptote pass parks spheres at 100 km
FOVY_DEG      = 45.0

SUN_ELEV_DEG  = 25.0
SUN_AZIM_DEG  = 180.0      # BEHIND the camera: front-lit spheres, smooth sky
# the anti-solar sky is the DIMMEST daylight sky there is; both levels are
# raised until an 8-bit capture can resolve the sphere/sky/in-scatter triple
# without clipping any of them (checked by the headroom leg below).
SUN_INTENSITY = 8.0
SKY_EXPOSURE  = 7.0

SETTLE_FRAMES = 90         # after scene build, before the first capture
STEP_SETTLE   = 40         # after each parameter / transform change

# cranked artist layer — the "bladerunner" end of the knob range
HAZE_DENSITY      = 0.30   # 1/km at layer base
HAZE_SCALE_HEIGHT = 0.5    # km
HAZE_PHASE_G      = 0.70
HAZE_SCATTER_TINT = (1.00, 0.86, 0.66)
HAZE_INSCAT_TINT  = (1.10, 0.92, 0.70)
HAZE_MAX_DIST_KM  = 160.0

ASYM_DIST_M = 100000.0     # inside the 160 km march clamp, far past total tau

BALL_ANG_TAN = 0.015       # angular radius, as a tangent: equal size for all
BALL_MODEL_R = 1.09        # pbr_calib.glb ball radius at scale 1

# (label, metres, lateral tangent offset, albedo). Hues are saturated and
# blue-free so the (blue-grey) sky cannot classify as a sphere; the row is kept
# inside +/-0.10 tangent so every sphere's in-scatter direction is close to its
# neighbour's -- leg B2 measures haze, not an azimuthal lighting ramp.
BALLS = [("d200",     200.0, -0.10, (0.90, 0.07, 0.07)),
         ("d800",     800.0, -0.05, (0.95, 0.42, 0.04)),
         ("d3200",   3200.0,  0.00, (0.88, 0.85, 0.06)),
         ("d12800", 12800.0,  0.05, (0.10, 0.80, 0.12)),
         ("d51200", 51200.0,  0.10, (0.85, 0.08, 0.70))]

# ---- bars (all measured on this scene; see the verdict detail line) ----------
HUE_TOL        = 0.40    # normalized-RGB radius around the NEAREST hue template
CHROMA_MIN     = 30.0    # 8-bit max-min; the sky ahead sits far below this
MIN_BALL_PX    = 40      # a sphere the detector cannot see is a FAIL, not a skip
UP_RATIO_MIN   = 1.30    # above-horizon vs below-horizon sky brightness
GEO_NEAR_MIN   = 0.95    # leg A: the 200 m sphere keeps ~all of its contrast
GEO_FAR_MAX    = 0.75    # leg A: the 51.2 km sphere has visibly converged
ASYM_FAR_MAX   = 0.10    # leg B1: the 51.2 km sphere IS its own asymptote
PAIR_FAR_MAX   = 12.0    # leg B2: the far pair, in 8-bit levels, is one colour
SKY_JOIN_MAX   = 0.20    # leg C: fraction of its own un-hazed contrast against
                         # the sky the FAR sphere still keeps once both it and
                         # the sky carry the artist layer. Measured 0.134 (8.5
                         # of 63 levels) — the residue of a 51.2 km sphere march
                         # read against the sky's full 160 km clamp, not a look
                         # error. The same sphere against an overlay-free sky
                         # scores 1.30 (printed beside it): the bar sits an
                         # order of magnitude under the thing it rejects.
MONO_MARGIN    = 0.005   # every leg is a 0..1 fraction; a step must clear this
                         # to count as a drop. Patch means over ~25 px put 8-bit
                         # quantisation (and the <=1 LSB SPIR-V reordering the S2
                         # adjudication documents) an order of magnitude below it.
HAZE_MIN_LEVELS = 16.0   # cranked haze must move the FAR sphere by this much.
                         # NOT a whole-frame fraction: the skybox runs no artist
                         # haze in v1, so the only pixels haze may move are the
                         # geometry ones -- a few hundred out of half a million.

OUT_DIR = os.environ.get("HAZE_G2_OUT", "/tmp/haze_horizon_convergence")


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


class HazeConvergenceApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._phase = 0
    self._phase_frame = 0
    self._done = False
    self._shots = {}
    self._inflight = None
    self._nodes = {}
    # capture order, and the state each capture leaves behind for the next
    self._steps = [("off",  self._armGeophysical),
                   ("geo",  self._armCranked),
                   ("hazy", self._pushToAsymptote),
                   ("asym", None)]
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(0, EYE_Y, 0), tgt=vec3(0, EYE_Y, 100), up=vec3(0, 1, 0),
        explicit_near_far=True, near=CAM_NEAR, far=CAM_FAR,
        grid_variant=None,          # no ground: the grid is not in the funnel
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 1.0,
          "AmbientLevel":     vec3(0.15),
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    SGC = self.SGC
    self.ezapp.topWidget.enableUiDraw()
    SGC.pbr_common.enable_skybox = True

    self.atmo = lev2.SkyAtmosphereData()
    self.atmo.sky_exposure = SKY_EXPOSURE
    self.atmo.ibl_crossfade_frames = 0   # a half-faded IBL is not a reading
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"
    self._disarm()

    for name, dist, off, col in BALLS:
      self._nodes[name] = SGC.createBallNode(
          name, ctx=ctx,
          position=vec3(dist * off, EYE_Y, dist),
          scale=BALL_ANG_TAN * dist / BALL_MODEL_R,
          color=vec4(col[0], col[1], col[2], 1), metallic=0.0, roughness=0.55)

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = SUN_INTENSITY
    sun.shadowCaster = False
    d = dir_to_sun(SUN_AZIM_DEG, SUN_ELEV_DEG)
    # DirectionalLight::direction() is the light's TRAVEL direction, so aiming
    # it from the sun's position at the origin makes dir_to_sun the negation.
    sun.lookAt(vec3(d.x, d.y, d.z) * 1000.0, vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun = sun
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)
    SGC.scenegraph.lightingmanager.gpuInit(ctx)

    # the editor camera's derived far plane would clip the far spheres outright
    # (test_skybox_depth_gate's finding); pin the projection directly.
    SGC.camera.perspective(CAM_NEAR, CAM_FAR, FOVY_DEG)
    SGC.camera.lookAt(vec3(0, EYE_Y, 0), vec3(0, EYE_Y, 100), vec3(0, 1, 0))

  def _onUpdate(self, updinfo):
    pass   # static: the only frame-to-frame differences are the ones we make

  ##############################################################
  # the four states

  def _crank(self):
    a = self.atmo
    a.haze_density = HAZE_DENSITY
    a.haze_scale_height = HAZE_SCALE_HEIGHT
    a.haze_phase_g = HAZE_PHASE_G
    a.haze_scatter_tint = vec3(*HAZE_SCATTER_TINT)
    a.haze_inscatter_tint = vec3(*HAZE_INSCAT_TINT)
    a.haze_max_distance_km = HAZE_MAX_DIST_KM

  def _disarm(self):
    self.atmo.aerial_perspective_enable = False
    self._crank()   # armed=0 => the knobs cannot reach a pixel (G1's subject)

  def _armGeophysical(self):
    self.atmo.aerial_perspective_enable = True
    self.atmo.haze_density = 0.0

  def _armCranked(self):
    self.atmo.aerial_perspective_enable = True
    self._crank()

  def _pushToAsymptote(self):
    """every sphere to ASYM_DIST_M along its OWN view ray: same pixels, same
    angular size, optical depth total => the pixel becomes that direction's
    pure in-scatter, which is the asymptote leg B1 converges against."""
    for name, dist, off, _col in BALLS:
      inv = 1.0 / math.sqrt(1.0 + off * off)
      node = self._nodes[name]
      node.worldTransform.translation = vec3(ASYM_DIST_M * off * inv,
                                             EYE_Y,
                                             ASYM_DIST_M * inv)
      node.worldTransform.scale = BALL_ANG_TAN * ASYM_DIST_M / BALL_MODEL_R

  ##############################################################

  def _rtg(self, ctx):
    rtg = getattr(getattr(self.SGC, "SGVPW", None), "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      rtg = ctx.FBI.main_RTG
    return rtg

  def _issueCapture(self, ctx):
    buf = lev2.CaptureBuffer()
    fut = ctx.FBI.captureAsFormat(self._rtg(ctx).buffer(0), buf, "RGBA8")
    self._inflight = (fut, buf)

  def _collect(self, key):
    fut, buf = self._inflight
    if not bool(fut.is_ready):
      return False
    w, h = buf.width, buf.height
    img = numpy.array(buf, dtype=numpy.uint8).reshape(h, w, 4)[..., :3].copy()
    self._shots[key] = img
    self._inflight = None
    try:
      os.makedirs(OUT_DIR, exist_ok=True)
      PILImage.fromarray(img[::-1, ::-1]).save(os.path.join(OUT_DIR, "%s.png" % key))
    except Exception as e:
      print("[haze-g2] png write failed: %r" % (e,), flush=True)
    print("[haze-g2] captured %s %dx%d mean=%.3f max=%d"
          % (key, w, h, float(img.mean()), int(img.max())), flush=True)
    return True

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._phase_frame = self._frame
      return

    step = self._phase // 2
    if step >= len(self._steps):
      return
    key, action = self._steps[step]

    if (self._phase % 2) == 0:                # settle, then issue
      settle = SETTLE_FRAMES if step == 0 else STEP_SETTLE
      if self._frame >= self._phase_frame + settle:
        self._issueCapture(ctx)
        self._phase += 1
      return

    if self._collect(key):
      if action is not None:
        action()
      self._phase += 1
      self._phase_frame = self._frame
      if (self._phase // 2) >= len(self._steps):
        self._emitVerdict()
    return

  ##############################################################
  # observables
  ##############################################################

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    need = ("off", "geo", "hazy", "asym")
    if any(self._shots.get(k) is None for k in need):
      self._finish(verdict(False, "missing captures"))
      return

    off  = self._shots["off"].astype(numpy.float64)
    geo  = self._shots["geo"].astype(numpy.float64)
    hazy = self._shots["hazy"].astype(numpy.float64)
    asym = self._shots["asym"].astype(numpy.float64)
    h, w, _ = off.shape

    # image UP, from the sky's own asymmetry: above the horizon is the lit air
    # column, below it the floor -- no assumption about buffer orientation.
    q = h // 4
    top, bot = float(off[:q].mean()), float(off[-q:].mean())
    up = -1 if top > bot else +1        # row step that walks toward the sky
    ratio = max(top, bot) / max(min(top, bot), 1.0e-6)
    check("image_orientation_decisive", ratio >= UP_RATIO_MIN,
          "top=%.1f bottom=%.1f ratio=%.2f up_row_step=%+d" % (top, bot, ratio, up))

    # spheres, by hue, out of the DISARMED capture (pure albedo response)
    spots, det = self._locate(off, up)
    check("all_spheres_located", len(spots) == len(BALLS), det)
    if len(spots) != len(BALLS) or failures:
      self._finish(verdict(False, "setup: " + ",".join(failures) + " | " + det))
      return

    def patch(img, cx, cy, r=2):
      return img[cy - r:cy + r + 1, cx - r:cx + r + 1].reshape(-1, 3).mean(axis=0)

    rows = []
    for (name, dist, _o, _c) in BALLS:
      cx, cy, rad, sx, sy = spots[name]
      p_off, p_geo, p_hzy, p_asy = (patch(im, cx, cy) for im in (off, geo, hazy, asym))
      p_sky = patch(off, sx, sy, 3)
      # leg C reads the sky out of the capture it belongs to: the artist layer
      # moves the sky pixels too, so the disarmed sky is not this sky.
      p_sky_h = patch(hazy, sx, sy, 3)
      # leg A: fraction of the un-hazed contrast against the sky that survives
      c_off = float(numpy.abs(p_off - p_sky).mean())
      c_geo = float(numpy.abs(p_geo - p_sky).mean())
      f_geo = c_geo / max(c_off, 1.0e-6)
      # leg B1: fraction of the un-hazed contrast against its OWN asymptote
      a_off = float(numpy.abs(p_off - p_asy).mean())
      a_hzy = float(numpy.abs(p_hzy - p_asy).mean())
      f_asy = a_hzy / max(a_off, 1.0e-6)
      # leg C: same currency as leg A, cranked layer, sky-of-the-hour
      s_hzy = float(numpy.abs(p_hzy - p_sky_h).mean())
      f_sky = s_hzy / max(c_off, 1.0e-6)
      # the same fraction against the sky the skybox WOULD draw with no overlay
      # (the disarmed capture's sky): printed as the scale of what leg C fixed.
      f_nov = float(numpy.abs(p_hzy - p_sky).mean()) / max(c_off, 1.0e-6)
      rows.append(dict(name=name, dist=dist, cx=cx, cy=cy, rad=rad,
                       off=p_off, geo=p_geo, hazy=p_hzy, asym=p_asy, sky=p_sky,
                       sky_h=p_sky_h,
                       c_off=c_off, c_geo=c_geo, f_geo=f_geo,
                       a_off=a_off, a_hzy=a_hzy, f_asy=f_asy,
                       s_hzy=s_hzy, f_sky=f_sky, f_nov=f_nov,
                       seam=float(numpy.abs(p_asy - p_sky_h).max())))

    print("[haze-g2] per-sphere (px = detected sphere centre; sky = patch above it)", flush=True)
    for r in rows:
      print("    %-7s px=(%3d,%3d) r=%2d off=(%5.1f,%5.1f,%5.1f) sky=(%5.1f,%5.1f,%5.1f) "
            "geo=(%5.1f,%5.1f,%5.1f) hazy=(%5.1f,%5.1f,%5.1f) asym=(%5.1f,%5.1f,%5.1f) "
            "skyH=(%5.1f,%5.1f,%5.1f) "
            "f_geo=%.3f f_asym=%.3f f_sky=%.3f |asym-skyH|max=%.1f"
            % (r["name"], r["cx"], r["cy"], r["rad"],
               r["off"][0], r["off"][1], r["off"][2],
               r["sky"][0], r["sky"][1], r["sky"][2],
               r["geo"][0], r["geo"][1], r["geo"][2],
               r["hazy"][0], r["hazy"][1], r["hazy"][2],
               r["asym"][0], r["asym"][1], r["asym"][2],
               r["sky_h"][0], r["sky_h"][1], r["sky_h"][2],
               r["f_geo"], r["f_asy"], r["f_sky"], r["seam"]), flush=True)

    # the cranked pass has to actually be doing something, and nothing this gate
    # reads may be sitting on a clipped 255 (a clipped patch converges for free)
    moved = float(numpy.abs(rows[-1]["hazy"] - rows[-1]["off"]).max())
    check("cranked_haze_reaches_the_geometry", moved >= HAZE_MIN_LEVELS,
          "far_sphere_max_channel_move=%.1f (floor %.1f)" % (moved, HAZE_MIN_LEVELS))
    hottest = max(float(numpy.max(r[k])) for r in rows
                  for k in ("off", "geo", "hazy", "asym", "sky", "sky_h"))
    check("no_patch_is_clipped", hottest <= 250.0, "hottest_patch_channel=%.1f" % hottest)

    # ---- leg A: geophysical convergence to the adjacent sky
    fg = [r["f_geo"] for r in rows]
    check("A_geophysical_monotone", self._monotone(fg),
          "f_geo=" + " ".join("%.3f" % v for v in fg))
    check("A_near_sphere_unaffected", fg[0] >= GEO_NEAR_MIN,
          "f_geo[near]=%.3f (floor %.2f)" % (fg[0], GEO_NEAR_MIN))
    check("A_far_sphere_converged", fg[-1] <= GEO_FAR_MAX,
          "f_geo[far]=%.3f (ceiling %.2f)" % (fg[-1], GEO_FAR_MAX))

    # ---- leg B1: each sphere converges to its OWN asymptote
    fa = [r["f_asy"] for r in rows]
    check("B1_asymptote_monotone", self._monotone(fa),
          "f_asym=" + " ".join("%.3f" % v for v in fa))
    check("B1_far_sphere_is_asymptotic", fa[-1] <= ASYM_FAR_MAX,
          "f_asym[far]=%.3f (ceiling %.2f)" % (fa[-1], ASYM_FAR_MAX))

    # ---- leg B2: object-to-object contrast collapses. Measured as the FRACTION
    # of each pair's own un-hazed spread that survives: a hue wheel has no
    # equal-step property, so the raw spreads of the five albedos are unequal
    # (they grow along this palette) and an absolute spread would be reporting
    # the palette, not the haze. The far pair's ABSOLUTE spread is bounded too --
    # by then the two spheres must be one color.
    craw = [float(numpy.abs(rows[i]["off"] - rows[i + 1]["off"]).max())
            for i in range(len(rows) - 1)]
    chz  = [float(numpy.abs(rows[i]["hazy"] - rows[i + 1]["hazy"]).max())
            for i in range(len(rows) - 1)]
    cs = [h / max(r, 1.0e-6) for h, r in zip(chz, craw)]
    print("[haze-g2] adjacent-pair max-channel spread: unhazed=%s hazed=%s"
          % (" ".join("%.1f" % v for v in craw), " ".join("%.1f" % v for v in chz)),
          flush=True)
    check("B2_pair_contrast_monotone", self._monotone(cs),
          "surviving_fraction=" + " ".join("%.3f" % v for v in cs))
    check("B2_far_pair_is_one_color", chz[-1] <= PAIR_FAR_MAX,
          "far_pair_spread=%.1f levels (ceiling %.1f)" % (chz[-1], PAIR_FAR_MAX))

    # ---- leg C: the skyline. Far geometry joins the sky it stands against.
    fs = [r["f_sky"] for r in rows]
    print("[haze-g2] hazy sphere vs HAZY sky, levels: %s   (same, against an "
          "overlay-free sky, as fractions: %s)"
          % (" ".join("%.1f" % r["s_hzy"] for r in rows),
             " ".join("%.3f" % r["f_nov"] for r in rows)), flush=True)
    check("C_far_sphere_joins_the_sky", fs[-1] <= SKY_JOIN_MAX,
          "f_sky[far]=%.3f (ceiling %.2f)" % (fs[-1], SKY_JOIN_MAX))
    check("C_convergence_is_distance_driven", fs[-1] <= fs[0] - MONO_MARGIN,
          "f_sky[near]=%.3f f_sky[far]=%.3f" % (fs[0], fs[-1]))

    # ---- the residual asymptote/sky gap, printed as evidence (NOT gated)
    print("[haze-g2] |asymptote - hazy sky| per column = %s"
          % " ".join("%.1f" % r["seam"] for r in rows), flush=True)

    ok = (len(failures) == 0)
    detail = ("f_geo=[%s] f_asym=[%s] f_sky=[%s] pair_contrast=[%s] seam=[%s]"
              % (",".join("%.3f" % v for v in fg),
                 ",".join("%.3f" % v for v in fa),
                 ",".join("%.3f" % v for v in fs),
                 ",".join("%.3f" % v for v in cs),
                 ",".join("%.0f" % r["seam"] for r in rows)))
    if failures:
      detail += " failed=" + ",".join(failures)
    self._finish(verdict(ok, detail))

  ##############################################################

  @staticmethod
  def _monotone(vals):
    """strictly falling, by a margin that 8-bit dither cannot manufacture."""
    return all(vals[i + 1] <= vals[i] - MONO_MARGIN for i in range(len(vals) - 1))

  def _locate(self, off, up):
    """find every sphere by hue in the disarmed capture, and pick the sky patch
    directly above each. Returns {name: (cx, cy, radius, sky_x, sky_y)}.

    Every lit pixel goes to its NEAREST hue template rather than to each
    template independently: the palette runs red->orange->yellow, whose
    normalized templates sit closer to each other than to grey, so independent
    thresholds hand the same pixels to two spheres and drag both centroids.
    A median-then-trim pass then drops the strays a rim pixel or a specular
    highlight can still contribute."""
    mx = off.max(axis=2)
    chroma = mx - off.min(axis=2)
    norm = off / numpy.maximum(mx, 1.0)[..., None]
    tmpls = numpy.array([numpy.array(c, dtype=numpy.float64) / max(c)
                         for (_n, _d, _o, c) in BALLS])
    dist = numpy.linalg.norm(norm[:, :, None, :] - tmpls[None, None, :, :], axis=3)
    best = numpy.argmin(dist, axis=2)
    lit = (chroma > CHROMA_MIN) & (dist.min(axis=2) < HUE_TOL)
    spots = {}
    notes = []
    for idx, (name, _d, _o, _col) in enumerate(BALLS):
      m = lit & (best == idx)
      n = int(m.sum())
      if n < MIN_BALL_PX:
        notes.append("%s:%dpx" % (name, n))
        continue
      ys, xs = numpy.nonzero(m)
      my, mx_ = float(numpy.median(ys)), float(numpy.median(xs))
      rr = numpy.hypot(ys - my, xs - mx_)
      keep = rr <= (3.0 * float(numpy.median(rr)) + 3.0)
      ys, xs = ys[keep], xs[keep]
      cx, cy = int(round(xs.mean())), int(round(ys.mean()))
      rad = max(2, int(round(0.5 * max(ys.max() - ys.min(), xs.max() - xs.min()))))
      sy = cy + up * (2 * rad + 8)
      sy = min(max(sy, 4), off.shape[0] - 5)
      spots[name] = (cx, cy, rad, cx, sy)
      notes.append("%s:%dpx@(%d,%d)r%d" % (name, len(ys), cx, cy, rad))
    return spots, " ".join(notes)

  def _finish(self, rc):
    self._exit_code = rc
    self._done = True
    self.ezapp.signalExit()


def main():
  wd = Watchdog(300.0, label="haze_horizon_convergence").arm()
  app = HazeConvergenceApp()
  app.ezapp.mainThreadLoop()
  wd.disarm()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before all four captures completed")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
