###############################################################################
# _swest_fire_cam — a composable PythonSystem scene script that AIMS the
# walker's camera once, from scene data.
#
# WHY IT EXISTS. The walkable camera is the only camera an offscreen still can
# get in this scene (the player's own orbit camera looks at the WORLD ORIGIN,
# which is a kilometre from the fire pit), and the character controller has no
# reflected initial heading — it links at heading 0, i.e. looking down -Z. So a
# still framed on the pit needs the yaw and the pitch to be said somewhere, and
# the controller's own DISCRETE camera-step messages (TurnStep / PitchStep,
# absolute radians — the gamepad face-button path) are exactly that surface.
#
# It layers via Scene.append_system_script, so walk_input_system.py keeps the
# keyboard: this script only sends its two steps once, a few ticks after link,
# and then goes quiet. WASD, the cursor keys and the sky clock are untouched.
#
#   SWEST_FIRE_YAW    camera heading in DEGREES. 0 = -Z (the link default);
#                     90 = +X; 180 = +Z; 270 = -X. DEFAULT 180 — the scene's
#                     default spawn sits 15 m at -Z from the pit, so +Z is the
#                     pit-and-village axis this scene exists to frame.
#   SWEST_FIRE_PITCH  camera pitch in DEGREES, + = looking UP. The controller
#                     clamps to +-1.2 rad (+-68.75 deg).
#   SWEST_FIRE_CAMDELAY  ticks to wait before sending (default 8) — the
#                     components must be linked or the step lands on nobody.
###############################################################################
import math
import os

from orkengine.ecssim import CrcStringProxy

tokens = CrcStringProxy()

YAW_DEG   = float(os.environ.get("SWEST_FIRE_YAW", "180.0"))
PITCH_DEG = float(os.environ.get("SWEST_FIRE_PITCH", "0.0"))
DELAY_TICKS = int(os.environ.get("SWEST_FIRE_CAMDELAY", "8"))


class _FireCam:
  def __init__(self):
    self.ticks = 0
    self.sent = False
    self.charctl = None


def onSystemInit(simulation):
  simulation.vars.fire_cam = _FireCam()


def onSystemLink(simulation):
  S = simulation.vars.fire_cam
  S.charctl = simulation.findSystemByName("CharacterControllerSystem")
  print("[fire_cam] armed yaw=%.2f deg pitch=%.2f deg charctl=%s"
        % (YAW_DEG, PITCH_DEG, S.charctl), flush=True)


def onSystemUpdate(simulation):
  S = simulation.vars.fire_cam
  if S.sent:
    return
  S.ticks += 1
  if S.ticks < DELAY_TICKS:
    return
  S.sent = True
  if S.charctl is None:
    print("[fire_cam] no CharacterControllerSystem — scene is not walkable, "
          "camera pose NOT applied", flush=True)
    return
  # DISCRETE steps, not rates: a rate would have to be integrated for a measured
  # number of ticks and would land somewhere different at a different frame rate.
  if abs(YAW_DEG) > 1e-6:
    S.charctl.notify(tokens.TurnStep, {tokens.radians: float(math.radians(YAW_DEG))})
  if abs(PITCH_DEG) > 1e-6:
    S.charctl.notify(tokens.PitchStep, {tokens.radians: float(math.radians(PITCH_DEG))})
  print("[fire_cam] pose applied at tick %d" % S.ticks, flush=True)
