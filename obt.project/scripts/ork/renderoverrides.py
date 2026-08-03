###############################################################################
# ENGINE-LEVEL RENDER OVERRIDES — the ORKID_* env family (ORKID_SPVR,
# ORKID_TERRAIN_MESHSHADER, ORKID_HZB_OCCLUSION, ...) as it reaches the python
# side. One reader per knob, imported by every route that could set the thing,
# so the answer is identical no matter which host boots the frame.
#
# CONTRACT for every knob here: UNSET IS A TRUE NO-OP. These knobs exist to A/B
# a live defect, and a knob that redefines "normal" corrupts the comparison it
# was built for. Unset returns None and the caller writes nothing at all.
###############################################################################

import os

_DPP_ENV = "ORKID_DPP"
_dpp_announced = False


def depth_prepass_override():
  """ORKID_DPP, engine-wide, three states:

      unset  -> None   the scene renders exactly as authored (no write)
      "0"    -> False  depth prepass OFF, overriding whatever the scene declares
      else   -> True   depth prepass ON,  overriding whatever the scene declares

  WHY IT EXISTS: the depth prepass is currently broken under single-pass stereo,
  and turning it off is the standing workaround while that is being fixed — the
  owner has ruled the prepass must eventually work there, so this is a lever for
  the A/B, not a permanent off-switch.

  Announces once per process when armed; silent when unset."""
  raw = os.environ.get(_DPP_ENV)
  if raw is None:
    return None
  enabled = (raw != "0")
  global _dpp_announced
  if not _dpp_announced:
    _dpp_announced = True
    print("[DPP] %s=%s — depth prepass FORCED %s engine-wide, overriding whatever "
          "the scene declares" % (_DPP_ENV, raw, "ON" if enabled else "OFF"))
  return enabled
