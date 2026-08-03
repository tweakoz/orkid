#!/usr/bin/env ork.python
###############################################################################
# scn_forest_moonprobe.py — W16-S1 gate-runner scratch instrument, NOT repo
# source. Wraps the real scn_forest.ForestProcSkyScene and overrides
# ONLY the moon half of its self.sky(...) call, via env:
#
#   W16_MOON_MODE = moonless | risen | asis   (default asis = scene default)
#
# moonless -> moon=False (no moon ensemble member at all)
# risen    -> moon=True, phase='full', moon_initial_elevation=None (ephemeris
#             solves the reachable elevation for a FULL moon at the forest's
#             own lat/day/tod=0 -- measured +31.85 deg, well above the
#             horizon). W16_MOON_ELEV overrides that ephemeris pick only if
#             explicitly set (must fall in the reachable band or this raises).
# asis     -> whatever the shipped scene declares (moon=True, ephemeris pose
#             -- measured -30.02 deg at TOD=0: the shipped "moon=True" default
#             is actually BELOW the horizon at dead-of-night on this scene's
#             solved date, i.e. moonless in effect).
#
# W16_TWILIGHT_LUM=<float> -- G3's decisive toggle: merges
# tonemap={"adapt_twilight_luminance": <value>} alongside the scene's own
# adapt_floor (FOREST_ADAPT_FLOOR, untouched native knob). Unset = the
# scene's shipped default (5e-4, PostFxNodeACES.h).
#
# Everything else (grade, floor, TOD, time_scale, cloud knobs) passes through
# the real scene's own env knobs untouched.
###############################################################################
import importlib.util
import os

from ork import path as ork_path

# The real scene is this file's SIBLING. Anchored on ork_path.data (ORKID_WORKSPACE_DIR)
# rather than a hardcoded absolute path — the previous literal pinned both the user's
# home and a checkout literally named "orkid2", so this scene only loaded on one machine
# in one clone, and raised everywhere else (worktrees included).
_REAL = str(ork_path.data / "scenes" / "scn_forest.py")
_spec = importlib.util.spec_from_file_location("scn_forest_real", _REAL)
_mod = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_mod)
ForestProcSkyScene = _mod.ForestProcSkyScene

_MODE = os.environ.get("W16_MOON_MODE", "asis")
_ELEV = os.environ.get("W16_MOON_ELEV")  # None = let the ephemeris solve it
_TWILIGHT_LUM = os.environ.get("W16_TWILIGHT_LUM")  # None = scene default


class ForestMoonProbe(ForestProcSkyScene):

  def sky(self, **kwargs):
    if _MODE == "moonless":
      kwargs["moon"] = False
      kwargs.pop("phase", None)
      kwargs.pop("moon_initial_elevation", None)
    elif _MODE == "risen":
      kwargs["moon"] = True
      kwargs["phase"] = "full"
      kwargs["moon_initial_elevation"] = (None if _ELEV is None else float(_ELEV))
    elif _MODE == "asis":
      pass
    else:
      raise ValueError("W16_MOON_MODE must be moonless|risen|asis, got %r" % _MODE)
    if _TWILIGHT_LUM is not None:
      tm = dict(kwargs.get("tonemap") or {})
      tm["adapt_twilight_luminance"] = float(_TWILIGHT_LUM)
      kwargs["tonemap"] = tm
    return super().sky(**kwargs)


__all__ = ["ForestMoonProbe"]
