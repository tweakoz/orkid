#!/usr/bin/env ork.python
################################################################################
# AVAILABLE-LIGHT MEASUREMENT gate (procsky wave4 slice W4-S1).
#
# THE CLAIM UNDER TEST: the engine MEASURES how much light the published
# environment carries, and the number it reports orders the sky's own states
# correctly — full day above twilight above a moonlit night above a moonless
# one — with none of them reading zero. The moonlit-under-twilight leg is the
# one the tonemap's floor limb depends on (see its comment below).
#
# WHY IT MATTERS. That measurement is the whole input to scene adaptation. If it
# were zero at night the adaptation would sit at its floor anchor forever; if it
# were flat across the day it would sit at one value forever; either way the
# stage would look like it worked (a fixed exposure IS a picture) while doing
# nothing. Ordering plus non-zero-ness is the cheapest oracle that catches both.
#
# WHERE THE NUMBER COMES FROM: the mean luminance of the diffuse chain's FLOOR
# level (8x4 for the equirect snapshot), computed at publish out of the CPU
# capture buffer the packaging already holds — no pass, no readback, no wait.
# The pre-scale is divided out at the read, so these are radiance units.
#
# WARMUP PINNED, as the S7/S8 night gates are: ibl_crossfade_frames 0 and the
# fade weight asserted at 1.0 before every reading, so no reading straddles two
# legs' maps.
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
import time
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict

tokens = CrcStringProxy()

WIDTH, HEIGHT = 256, 192
SETTLE_FRAMES = 12
WAIT_SECONDS  = 120.0

SNAPSHOT_INTERVAL = 0.05
MOON_AZIM_DEG     = 90.0

# The three sky states, in the order a day runs through them. `sun_i` is the
# sun light's intensity: a sun below the horizon still contributes nothing, but
# zeroing it as well keeps the leg unambiguous.
LEGS = [
    dict(key="day",            sun_elev= 60.0, sun_i=1.0, moon_elev=-20.0, moon_i=0.0),
    dict(key="twilight",       sun_elev= -4.0, sun_i=0.0, moon_elev=-20.0, moon_i=0.0),
    dict(key="night_moonlit",  sun_elev=-25.0, sun_i=0.0, moon_elev= 45.0, moon_i=1.0),
    dict(key="night_moonless", sun_elev=-25.0, sun_i=0.0, moon_elev=-20.0, moon_i=0.0),
]

CAM_EYE = vec3(0, 2, -7.0)
TGT_POS = vec3(0, 2, 0)


def celestial_dir(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the body (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


class LumApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._leg = 0
    self._state = 0
    self._state_frame = 0
    self._state_time = time.time()
    self._gen_mark = 0
    self._done = False
    self._exit_code = None
    self._lum = {}
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=CAM_EYE, tgt=TGT_POS, up=vec3(0, 1, 0),
        grid_variant=None,
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 0.0,
          "AmbientLevel":     vec3(0),
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    SGC.pbr_common.enable_skybox = False

    self.atmo = lev2.SkyAtmosphereData()
    self.atmo.ibl_crossfade_frames = 0
    self.atmo.ibl_snapshot_interval = SNAPSHOT_INTERVAL
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    self.ball = SGC.createBallNode("recv", ctx=ctx, position=TGT_POS,
                                   color=vec4(1, 1, 1, 1), metallic=0.0,
                                   roughness=1.0, scale=1.6)

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 1.0
    sun.data.priority = 1.0
    sun.data.sky_body = 1
    sun.shadowCaster = False
    self.sun = sun
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)

    moon = lev2.DynamicDirectionalLight()
    moon.data.color = vec3(1, 1, 1)
    moon.data.intensity = 0.0
    moon.data.priority = 0.0
    moon.data.sky_body = 2
    moon.shadowCaster = False
    self.moon = moon
    self.moon_node = SGC.layer_fwd.createLightNode("moon", moon)

    self._aim(sun, 0.0, LEGS[0]["sun_elev"])
    self._aim(moon, MOON_AZIM_DEG, LEGS[0]["moon_elev"])
    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _aim(self, light, azim_deg, elev_deg):
    d = celestial_dir(azim_deg, elev_deg)
    light.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))

  def _onUpdate(self, updinfo):
    pass

  def _pbr(self):
    return self.SGC.pbr_common

  def _fail(self, why):
    verdict(False, why)
    self._exit_code = 1
    self._done = True
    self.ezapp.signalExit()

  def _restate(self, s):
    self._state = s
    self._state_frame = self._frame
    self._state_time = time.time()

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    if not self._built:
      if bool(self._pbr().sky_ibl_ready) and (not bool(self._pbr().sky_ibl_inflight)):
        self._built = True
        self._restate(0)
      elif (time.time() - self._state_time) > WAIT_SECONDS:
        self._fail("the first procedural refilter never published")
      return
    if self._leg >= len(LEGS):
      return

    leg = LEGS[self._leg]

    if self._state == 0:
      self.sun.data.intensity = leg["sun_i"]
      self.moon.data.intensity = leg["moon_i"]
      self._aim(self.sun, 0.0, leg["sun_elev"])
      self._aim(self.moon, MOON_AZIM_DEG, leg["moon_elev"])
      print("[adapt-lum] leg %s: sun %.1f deg (i=%.2f), moon %.1f deg (i=%.2f)"
            % (leg["key"], leg["sun_elev"], leg["sun_i"], leg["moon_elev"], leg["moon_i"]),
            flush=True)
      self._gen_mark = int(self._pbr().sky_ibl_generation)
      self._restate(1)
      return

    if self._state == 1:
      # two fresh publishes: a cycle already in flight when the edit landed
      # carried the PREVIOUS leg's sky into its snapshot.
      if (int(self._pbr().sky_ibl_generation) - self._gen_mark) >= 2:
        if not bool(self._pbr().sky_ibl_inflight):
          self._restate(2)
      elif (time.time() - self._state_time) > WAIT_SECONDS:
        self._fail("leg %s: no fresh refilter published" % leg["key"])
      return

    if self._state == 2:
      if (self._frame - self._state_frame) >= SETTLE_FRAMES:
        fw = float(self._pbr().sky_ibl_fade_weight)
        if fw < 0.9999:
          self._fail("leg %s: crossfade still running (fade weight %.6f)" % (leg["key"], fw))
          return
        lum = float(self._pbr().sky_measured_luminance)
        self._lum[leg["key"]] = lum
        print("[adapt-lum] %-15s measured luminance = %.9g" % (leg["key"], lum), flush=True)
        self._leg += 1
        self._restate(0)
        if self._leg >= len(LEGS):
          self._emitVerdict()
      return

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    L = self._lum
    print("=== measured available light (decoded radiance) ===", flush=True)
    for leg in LEGS:
      print("  %-15s %.9g" % (leg["key"], L[leg["key"]]), flush=True)

    for leg in LEGS:
      k = leg["key"]
      check("measured_" + k, L[k] > 0.0, "%.9g" % L[k])

    check("day_above_twilight", L["day"] > L["twilight"],
          "day=%.6g twilight=%.6g" % (L["day"], L["twilight"]))
    check("day_above_moonlit_night", L["day"] > L["night_moonlit"],
          "day=%.6g moonlit=%.6g" % (L["day"], L["night_moonlit"]))
    check("twilight_above_moonless", L["twilight"] > L["night_moonless"],
          "twilight=%.6g moonless=%.6g" % (L["twilight"], L["night_moonless"]))
    check("moonlit_above_moonless", L["night_moonlit"] > L["night_moonless"],
          "moonlit=%.6g moonless=%.6g" % (L["night_moonlit"], L["night_moonless"]))

    # MOONLIT BELOW TWILIGHT — asserted as of W16-S1, REVERSING what this gate
    # used to say here. The old comment argued that a unit-illuminance moon 45
    # degrees up scattering more than the last of the sun (2.0e-3 vs 5.5e-4)
    # was "a fact about those two skies and not a defect". It was a defect,
    # downstream: PostFxNodeACES has its scotopic FLOOR limb only BELOW the
    # twilight anchor, so a moonlit night measuring above that anchor was
    # graded by the twilight limb at every floor value and displayed ~13x
    # DARKER than the MOONLESS night beside it. The ladder's night end was
    # moved under the anchor instead (_moonRayleighStrength 0.032 -> 0.004,
    # derived in sky_atmosphere.h). The anchor is read off a default-built
    # node rather than written as 5e-4 here, so retuning it retunes this leg.
    twilight_anchor = float(lev2.PostFxNodeACES().adapt_twilight_luminance)
    check("moonlit_below_twilight_anchor", L["night_moonlit"] < twilight_anchor,
          "moonlit=%.6g anchor=%.6g" % (L["night_moonlit"], twilight_anchor))

    ok = (len(failures) == 0)
    detail = " ".join("%s=%.6g" % (leg["key"], L[leg["key"]]) for leg in LEGS)
    if failures:
      detail += " failed=" + ",".join(failures)
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = LumApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
