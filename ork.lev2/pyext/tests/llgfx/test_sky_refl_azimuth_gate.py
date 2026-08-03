#!/usr/bin/env ork.python
################################################################################
# Sky IBL REFLECTION gate: the two things a mirror ball under the procedural sky
# gets wrong when the snapshot and the specular sampler disagree.
#
# a) AZIMUTH CONVENTION. The shared specular samplers (envtools
#    env_equirectangular_spec*) read an equirect at the bearing MIRRORED about
#    the X axis — that is the convention the baked .xir assets are authored in,
#    and sky.fxv2's snapshot pass has to write the same one (it negates rd.x).
#    So the snapshot's brightest column, which is where the sun's horizon glow
#    landed, must sit at (0.5 - u(sun)) rather than at u(sun). Written as the
#    IDENTITY convention instead, a mirror ball reflects the sun on the wrong
#    side of the sky: measured 150 px apart on a 90 px ball at +-15 degrees of
#    sun azimuth, with the reflection landing opposite the analytic sun disc,
#    and the snapshot's glow column 57 texels of 256 off where it belongs.
#
# b) NIGHT SPECULAR FLOOR. AmbientLevel is a DIFFUSE floor; folding it into
#    specular_light (fwdtools.i2) put it on surfaces that have no diffuse at
#    all, so a metallic=1 roughness=0 ball glowed at the ambient level under a
#    black night sky. A pure mirror's radiance comes from the environment and
#    from nothing else, which is asserted twice here: against the sky it
#    reflects, and against a CHANGE of AmbientLevel it must not react to.
#
# Not ork.testing capture_app: this gate needs a multi-capture state machine
# (waits on engine-published refilter cycles) inside one warm process. The
# verdict-before-teardown protocol is honoured.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

# repo root = five levels up; prepend THIS checkout's scripts dir so ork.testing
# resolves from the same tree as this test (worktree-shadowing idiom).
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import math
import time
import numpy
from orkengine.core import vec3, vec4, asyncWorkSummary
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict

WIDTH, HEIGHT = 512, 384
CAM_DIST      = 7.0
BALL_POS      = vec3(0, 2, 0)
EYE_POS       = vec3(0, 2, -CAM_DIST)

SUN_ELEV_DAY   = 12.0
SUN_ELEV_NIGHT = -25.0    # well under the horizon: the sky-view LUT goes near-black
SUN_AZIM_A     = 40.0
SUN_AZIM_B     = 220.0    # 180 degrees away: an unmirrored snapshot fails BOTH or NEITHER

AMBIENT_ON  = vec3(0.25)  # a floor big enough that leaking it is unmissable
AMBIENT_OFF = vec3(0)

SETTLE_FRAMES = 20
WAIT_SECONDS  = 30.0

# the brightest column is a texel index and the glow spans many texels, so the
# bar is "the right texel, give or take the sampling grid", as a fraction of the
# snapshot width (a knob, so never restated as a pixel count).
COLUMN_TOL_FRAC = 0.02
# 8-bit levels. Measured on an RTX 5090: a leaked 0.25 ambient floor puts the
# night mirror 37 levels over its sky and moves it 41 levels when AmbientLevel
# changes; without the leak both land at 1 level (the dielectric the metallic
# clamp leaves behind), so these bars sit an order of magnitude either side.
NIGHT_FLOOR_LEVELS = 4.0
AMBIENT_DELTA_LEVELS = 3.0


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


def mirrored_u_for_azimuth(azimuth_deg):
  """equirect column the snapshot must put a bearing's glow in: the specular
  samplers read column (0.5 - u), so the writer stores the X-mirrored bearing."""
  d = dir_to_sun(azimuth_deg, 0.0)
  u = (math.atan2(d.z, d.x) / math.pi + 1.0) * 0.5
  return (0.5 - u) % 1.0


def pending_texture_uploads():
  """count of async-tracker markers held by in-flight GPU texture uploads (the
  TAG, not asyncWorkPending(): a whole-registry poll also hangs on unrelated
  producers)."""
  for field in asyncWorkSummary().split():
    tag, _, count = field.partition(":")
    if tag == "texture_upload":
      return int(count)
  return 0


class ReflAzimuthApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._phase = 0
    self._sub = 0
    self._phase_frame = 0
    self._phase_time = time.time()
    self._done = False
    self._inflight = None
    self._shots = {}
    self._marks = {}
    self._gen_target = 1
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=EYE_POS, tgt=BALL_POS, up=vec3(0, 1, 0),
        grid_variant=None,
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 1.0,
          "AmbientLevel":     AMBIENT_OFF,
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    # the ball frames are shot against a BLACK background so the ball masks
    # itself; the night frames turn the sky back on to read what it reflects.
    SGC.pbr_common.enable_skybox = False

    self.atmo = lev2.SkyAtmosphereData()
    self._snap_wh = (self.atmo.ibl_snapshot_width, self.atmo.ibl_snapshot_height)
    self.atmo.sky_exposure = 8.0
    # chaining OFF: every cycle in this gate is one the gate asked for.
    self.atmo.ibl_continuous_chain = False
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    self.ball = SGC.createBallNode("mirror", ctx=ctx, position=BALL_POS,
                                   color=vec4(1, 1, 1, 1), metallic=1.0, roughness=0.0,
                                   scale=1.2)
    # DIRECTION SOURCE ONLY: intensity 0 keeps every photon on the ball
    # image-based while the prologue still bakes the LUT with this direction.
    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 0.0
    sun.shadowCaster = False
    self.sun = sun
    self._aimSun(SUN_AZIM_A, SUN_ELEV_DAY)
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)
    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass

  ##############################################################

  def _aimSun(self, azimuth_deg, elevation_deg):
    d = dir_to_sun(azimuth_deg, elevation_deg)
    self.sun.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))

  def _pbr(self):
    return self.SGC.pbr_common

  def _rtg(self, ctx):
    rtg = getattr(getattr(self.SGC, "SGVPW", None), "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      rtg = ctx.FBI.main_RTG
    return rtg

  def _issueCapture(self, ctx, key, rtg=None, fmt="RGBA8"):
    buf = lev2.CaptureBuffer()
    target = self._rtg(ctx) if rtg is None else rtg
    fut = ctx.FBI.captureAsFormat(target.buffer(0), buf, fmt)
    self._inflight = (fut, buf, key, fmt)

  def _collect(self):
    fut, buf, key, fmt = self._inflight
    if not bool(fut.is_ready):
      return None
    w, h = buf.width, buf.height
    if fmt == "RGBA8":
      img = numpy.array(buf, dtype=numpy.uint8).reshape(h, w, 4)[..., :3].copy()
    else:
      img = numpy.array(buf, dtype=numpy.float32).reshape(h, w, 4)[..., :3].copy()
    self._shots[key] = img
    self._inflight = None
    print("[refl] captured %s %dx%d mean=%.4f max=%.4f" %
          (key, w, h, float(img.mean()), float(img.max())), flush=True)
    return key

  def _published(self):
    """the refilter cycle this phase asked for is complete, crossfaded and
    sampleable — three different asynchronous mechanisms, all of them able to
    hand back the PREVIOUS sky otherwise."""
    pbr = self._pbr()
    return (int(pbr.sky_ibl_generation) >= self._gen_target
            and bool(pbr.sky_ibl_ready)
            and float(pbr.sky_ibl_fade_weight) >= 0.999
            and not bool(pbr.sky_ibl_inflight)
            and pending_texture_uploads() == 0)

  ##############################################################

  def _phases(self):
    return [
        # 0-1: the two daytime snapshots the azimuth convention is read off.
        dict(key="day_a", snap=True, newgen=True, lit=True,
             after=lambda: self._aimSun(SUN_AZIM_B, SUN_ELEV_DAY)),
        dict(key="day_b", snap=True, newgen=True, lit=True, after=self._goNight),
        # 2: night, ambient ON — the mirror must still be as dark as its sky
        dict(key="night_amb", snap=False, newgen=False, lit=False, after=self._ambientOff),
        # 3: night, ambient OFF. AmbientLevel moves no sun, so this phase waits
        # on the SAME generation: demanding a new one would hang forever.
        dict(key="night_noamb", snap=False, newgen=False, lit=False, after=None),
    ]

  def _goNight(self):
    self._aimSun(SUN_AZIM_A, SUN_ELEV_NIGHT)
    self._pbr().enable_skybox = True          # the reflected sky must be in frame
    self._pbr().ambientLevel = AMBIENT_ON

  def _ambientOff(self):
    self._pbr().ambientLevel = AMBIENT_OFF

  ##############################################################

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._phase_frame = self._frame
        self._phase_time = time.time()
      return

    phases = self._phases()
    step = self._phase // 3
    if step >= len(phases):
      return
    ph = phases[step]

    if self._sub == 0:
      if not self._published():
        if (time.time() - self._phase_time) > WAIT_SECONDS:
          self._fail("engine never published generation %d for phase '%s'" %
                     (self._gen_target, ph["key"]))
        return
      if (self._frame - self._phase_frame) < SETTLE_FRAMES:
        return
      self._issueCapture(ctx, ph["key"])
      self._sub = 1
      return

    if self._sub == 1:
      if self._collect() is None:
        return
      if ph["lit"] and float(self._shots[ph["key"]].max()) <= 0.0:
        # the ball model streams in asynchronously: an all-black DAY frame is a
        # capture that beat it, not a result. A night frame is allowed to be
        # black — that is the whole point of the floor check.
        del self._shots[ph["key"]]
        self._phase_frame = self._frame
        self._sub = 0
        return
      if ph["snap"]:
        rtg = self._pbr().sky_ibl_snapshot_rtgroup
        if rtg is None:
          self._fail("no equirect snapshot RTG after a published cycle")
          return
        self._issueCapture(ctx, "snap_" + ph["key"], rtg=rtg, fmt="RGBA32F")
        self._sub = 2
        return
      self._advance(ph)
      return

    if self._collect() is None:
      return
    self._advance(ph)

  def _advance(self, ph):
    if ph["after"] is not None:
      ph["after"]()
    self._phase += 3
    self._sub = 0
    if ph["newgen"]:
      self._gen_target += 1
    self._phase_frame = self._frame
    self._phase_time = time.time()
    if (self._phase // 3) >= len(self._phases()):
      self._emitVerdict()

  ##############################################################
  # observables
  ##############################################################

  def _ballDisc(self):
    """(cx, cy, r) of the ball, measured off the black-background day frame."""
    lum = self._shots["day_a"].astype(numpy.float64).mean(axis=2)
    mask = lum > (0.10 * lum.max())
    ys, xs = numpy.nonzero(mask)
    return float(xs.mean()), float(ys.mean()), math.sqrt(float(mask.sum()) / math.pi)

  def _ballVsSky(self, key):
    """mean 8-bit luminance of the ball, and of the sky annulus around it."""
    cx, cy, r = self._ballDisc()
    lum = self._shots[key].astype(numpy.float64).mean(axis=2)
    h, w = lum.shape
    yy, xx = numpy.ogrid[0:h, 0:w]
    d2 = (xx - cx) ** 2 + (yy - cy) ** 2
    ball = d2 <= (0.75 * r) ** 2
    sky = (d2 >= (1.6 * r) ** 2) & (d2 <= (2.6 * r) ** 2)
    return float(lum[ball].mean()), float(lum[sky].mean())

  def _sunColumn(self, snap):
    lum = snap.astype(numpy.float32).sum(axis=2)
    return float(numpy.argmax(lum.sum(axis=0)))

  ##############################################################

  def _fail(self, why):
    print("FAIL " + why, flush=True)
    verdict(False, why)
    self._exit_code = 1
    self._done = True
    self.ezapp.signalExit()

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    ##########################################################
    # a) the snapshot writes the MIRRORED-U convention the samplers read
    ##########################################################
    print("=== snapshot azimuth convention ===", flush=True)
    cols = []
    for key, azim in (("snap_day_a", SUN_AZIM_A), ("snap_day_b", SUN_AZIM_B)):
      snap = self._shots[key]
      h, w, _ = snap.shape
      col = self._sunColumn(snap)
      want = mirrored_u_for_azimuth(azim) * w
      # the column axis wraps, so the error is the shorter way round
      err = abs(col - want)
      err = min(err, w - err)
      cols.append((azim, col, want, err, w))
      check("snapshot_extent_%s" % key, (w, h) == self._snap_wh,
            "%dx%d (knob %dx%d)" % (w, h, self._snap_wh[0], self._snap_wh[1]))
      check("snapshot_nonblack_%s" % key, float(snap.max()) > 1.0e-3,
            "max=%.4f" % float(snap.max()))
      check("snapshot_mirrored_u_az%.0f" % azim, err <= (COLUMN_TOL_FRAC * w),
            "brightest col=%.0f want=%.1f err=%.1f px of %d (identity convention would sit at %.1f)"
            % (col, want, err, w, ((math.atan2(dir_to_sun(azim, 0.0).z, dir_to_sun(azim, 0.0).x)
                                    / math.pi + 1.0) * 0.5) * w))

    ##########################################################
    # b) a pure mirror under a night sky carries the sky and nothing else
    ##########################################################
    print("=== night specular floor ===", flush=True)
    b_amb, s_amb = self._ballVsSky("night_amb")
    b_off, s_off = self._ballVsSky("night_noamb")
    print("  ambient %s: ball=%.3f sky=%.3f | ambient %s: ball=%.3f sky=%.3f" %
          (repr(AMBIENT_ON), b_amb, s_amb, repr(AMBIENT_OFF), b_off, s_off), flush=True)
    check("night_mirror_tracks_its_sky", (b_amb - s_amb) <= NIGHT_FLOOR_LEVELS,
          "ball-sky=%.3f levels (bar %.1f)" % (b_amb - s_amb, NIGHT_FLOOR_LEVELS))
    check("night_mirror_ignores_ambient", abs(b_amb - b_off) <= AMBIENT_DELTA_LEVELS,
          "ball delta=%.3f levels across AmbientLevel %.2f -> %.2f (bar %.1f)"
          % (abs(b_amb - b_off), AMBIENT_ON.x, AMBIENT_OFF.x, AMBIENT_DELTA_LEVELS))

    ok = (len(failures) == 0)
    detail = ("cols=%s night_ball=%.2f/%.2f night_sky=%.2f" %
              (",".join("%.0f@az%.0f" % (c, a) for a, c, _, _, _ in cols), b_amb, b_off, s_amb))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = ReflAzimuthApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
