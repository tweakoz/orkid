#!/usr/bin/env ork.python
################################################################################
# SKY-IBL PUBLISH SWAP-GAP gate.
#
# The bug this exists for: the publish slice created the new diffuse texture and
# specular array, issued their uploads as DEFERRED one-shot commands, and in the
# same breath assigned them into the LIVE RadianceMaps. That slice runs after the
# frame's one-shot drain, so the uploads only execute in the next frame's command
# buffer and complete the frame after that — and until then a published texture's
# sampling image is null, which per-draw binding answers by substituting the
# BLACK default texture. Result: ~2 frames of collapsed ambient at every refilter
# cycle, per-draw selective (a draw whose descriptor set was not rewritten inside
# the gap kept the old maps and stayed lit), and the crossfade could not cover it
# because the fade STARTS at upload completion — i.e. after the black window.
#
# THE CONTRACT, gated here without a single capture: between the frame the
# publish is issued and the frame its uploads complete, the maps the renderer
# binds must still be the OLD ones. So:
#
#   a) NO SWAP MID-UPLOAD   on every frame where a cycle is in flight AND
#                           texture uploads are pending, the live specular array
#                           and diffuse map are IDENTICALLY the objects that were
#                           resident when the cycle started. Pre-fix this fails
#                           on the ~2 frames between issue and completion.
#   b) SWAP AT COMPLETION   the frame the generation counter advances is the
#                           frame the live maps change — the swap and the fade
#                           start together, which is what makes the crossfade
#                           able to cover the transition at all.
#   c) CYCLES REALLY RAN    the swaps counted above are not zero, and each cycle
#                           observed a nonzero in-flight upload window (a gate
#                           that never saw the gap proves nothing about it).
#
# Identity is read by HOLDING the bound texture objects and comparing with `is`.
# Not id(): pybind mints the wrapper on each property read, and a wrapper the
# gate does not keep a reference to is freed immediately — CPython then reuses
# that address for the next one, so an id() comparison silently reports "same
# texture" and "different texture" at random.
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
from orkengine.core import vec3, vec4, asyncWorkSummary
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict

WIDTH, HEIGHT = 320, 240
SUN_ELEV_DEG  = 20.0
SUN_AZIMUTHS  = [0.0, 25.0, 50.0, 75.0]  # first entry starts the feed; the rest force a cycle each
WAIT_SECONDS  = 90.0


def pending_texture_uploads():
  """markers held by in-flight GPU texture uploads (the publish's own tag).

  The TAG rather than asyncWorkPending(): a whole-registry poll would also count
  unrelated producers that never settle offscreen."""
  for field in asyncWorkSummary().split():
    tag, _, count = field.partition(":")
    if tag == "texture_upload":
      return int(count)
  return 0


def dir_to_sun(azimuth_deg, elevation_deg):
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


class SwapGapApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._done = False
    self._t0 = time.time()
    self._aim = 0
    self._gen = 0
    self._resident = None      # the (specular, diffuse) objects the running cycle started from
    self._gap_frames = 0       # frames observed with a publish's uploads pending
    self._gap_frames_cycle = 0
    self._violations = []      # frames where the live maps moved inside the gap
    self._swaps = []           # (generation, gap_frames_that_cycle)
    self._late = []            # generations that advanced with no map change
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(0, 2, -7), tgt=vec3(0, 2, 0), up=vec3(0, 1, 0),
        grid_variant=None,
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 1.0,
          "AmbientLevel":     vec3(0),
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    SGC.pbr_common.enable_skybox = False

    self.atmo = lev2.SkyAtmosphereData()
    self.atmo.sky_exposure = 40.0
    # CHAINING OFF: each cycle is started by an explicit sun move, so the gate
    # knows exactly how many publishes it is measuring. With chaining on the feed
    # would start cycles on its own cadence and the counts would drift.
    self.atmo.ibl_continuous_chain = False
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "baked"

    # one receiver so the forward pass has something IBL-lit to bind maps for
    SGC.createBallNode("recv", ctx=ctx, position=vec3(0, 2, 0),
                       color=vec4(1, 1, 1, 1), metallic=0.0, roughness=1.0, scale=1.2)

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 0.0   # direction source only: every photon is image-based
    sun.shadowCaster = False
    self.sun = sun
    self._aimSun(SUN_AZIMUTHS[0])
    SGC.layer_fwd.createLightNode("sun", sun)
    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass

  ##############################################################

  def _aimSun(self, azimuth_deg):
    d = dir_to_sun(azimuth_deg, SUN_ELEV_DEG)
    self.sun.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))

  def _pbr(self):
    return self.SGC.pbr_common

  def _mapObjs(self):
    """the two bound texture objects, RETAINED (see the identity note at top)"""
    maps = self._pbr().active_radiance_maps
    if maps is None:
      return None
    return (maps.specular, maps.diffuse)

  @staticmethod
  def _same(a, b):
    return (a is not None) and (b is not None) and (a[0] is b[0]) and (a[1] is b[1])

  ##############################################################

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    pbr = self._pbr()
    if not self._built:
      # the baked .xir streams in asynchronously; flipping to procedural before it
      # lands would have the FIRST cycle's window straddle the baked upload too,
      # and this gate would be scoring somebody else's texture swap.
      maps = pbr.active_radiance_maps
      if (maps is not None) and (maps.specular is not None) and (maps.diffuse is not None):
        self._built = True
        pbr.sky_source = "procedural"
      elif (time.time() - self._t0) > WAIT_SECONDS:
        self._finish()
      return

    gen = int(pbr.sky_ibl_generation)
    inflight = bool(pbr.sky_ibl_inflight)
    pending = pending_texture_uploads()
    ids = self._mapObjs()

    if gen > self._gen:
      # the completion frame: the swap and the fade start here together
      if self._same(ids, self._resident):
        self._late.append(gen)
      self._swaps.append((gen, self._gap_frames_cycle))
      self._gen = gen
      self._gap_frames_cycle = 0
      self._resident = ids
      # next cycle (or finish)
      self._aim += 1
      if self._aim < len(SUN_AZIMUTHS):
        self._aimSun(SUN_AZIMUTHS[self._aim])
      else:
        self._finish()
      return

    # The FIRST cycle is out of scope by construction: there is no resident
    # procedural set to keep binding, and the accessor is still serving the baked
    # maps (whose own async load can move underneath this sampling). Policing
    # starts with the first cycle that actually REPLACES a published set.
    if self._gen < 1:
      return

    if self._resident is None:
      self._resident = ids
    elif inflight and (pending > 0):
      # THE GAP: a publish's uploads are outstanding. The renderer must still be
      # bound to the maps this cycle started from.
      self._gap_frames += 1
      self._gap_frames_cycle += 1
      if not self._same(ids, self._resident):
        # recorded, not fatal: letting the run finish keeps the cycle counts
        # meaningful, so a failing report names ONE defect instead of cascading
        # into "no cycles published" as well.
        self._violations.append(self._frame)

    if (time.time() - self._t0) > WAIT_SECONDS:
      self._finish()

  ##############################################################

  def _finish(self):
    self._done = True
    checks = []
    # only the REPLACING cycles are in scope (see the gen<1 note above)
    policed = [(gen, g) for (gen, g) in self._swaps if gen >= 2]
    nswaps = len(policed)
    gapped = [g for (_, g) in policed if g > 0]
    checks.append(("cycles_published", str(nswaps), ">=2", nswaps >= 2))
    checks.append(("upload_gap_observed", str(self._gap_frames), ">0", self._gap_frames > 0))
    checks.append(("cycles_with_a_gap", "%d/%d" % (len(gapped), nswaps), "all",
                   (nswaps > 0) and (len(gapped) == nswaps)))
    checks.append(("no_swap_inside_upload_gap",
                   str(self._violations or "none"), "none", not self._violations))
    checks.append(("swap_lands_at_completion",
                   str(self._late or "none"), "none", not self._late))
    nfail = 0
    for name, got, want, ok in checks:
      nfail += 0 if ok else 1
      print("  %s %s %s (want %s)" % ("PASS" if ok else "FAIL", name, got, want), flush=True)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(nfail == 0,
            "swaps=%d gap_frames=%d gaps_per_cycle=%s violations=%d" %
            (nswaps, self._gap_frames, [g for (_, g) in policed], len(self._violations)))
    self._exit_code = 0 if nfail == 0 else 1
    self.ezapp.signalExit()


def main():
  app = SwapGapApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
