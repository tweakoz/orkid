#!/usr/bin/env python3
###############################################################################
# Do a scene's DECLARED scenegraph params survive the runtime start?
#
# WAS A STANDING-RED WITNESS (witness_scenegraph_param_survival.py) until the
# defect below was fixed; renamed to test_* per its own instruction once green,
# and extended with the two converse cases so the fix cannot regress in either
# direction.
#
# THE DEFECT: EcsRuntime.start_simulation() calls ensure_scenegraph_system()
# (ork/hypergraph/ecs/runtime.py) as its first act on the scene data, and that
# method ended with an UNCONDITIONAL declareParams(defaults) — a fixed dict of
# preset / ssaa / SkyboxIntensity / DiffuseIntensity / SpecularIntensity /
# AmbientLight / enable_skybox / use_float_color_buffer / clearcolor. It was
# written as "ensure defaults exist", but declareParams is dict ASSIGNMENT into
# the user params, so on a scene that already declared its own it was not a
# fallback, it was an OVERWRITE. Every value an author tuned was silently
# replaced on any EcsRuntime path. The defaults are now SET-IF-ABSENT, matching
# the C++ default_if_absent in AssetSystem::materializeAndWireScene.
#
# THE HISTORICAL VICTIM: the baked cascade gauge (scn_sun_test, retired jul28)
# restated AmbientLight 0.1 because the sky library's default had moved to 0.0
# for the procedural family. That restatement was correct, recorded and
# gate-asserted at declaration time — and silently undone at runtime by this
# override, which is how the defect was found.
#
# WHY NO DEVICE: the override happens before any controller, simulation or
# scenegraph exists. start_simulation() = destroy_simulation() ->
# ensure_scenegraph_system() -> bind/create/start; only the second step touches
# the params. This test calls exactly that step, so it runs anywhere, and
# reads the values back through the same generateSceneGraphParams() the runtime
# itself uses to build the scenegraph — i.e. what the frame would actually get.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import io
import contextlib

# workspace-anchored (never cwd-derived): <ws>/ork.lev2/pyext/tests/llgfx/<this>
_WS = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   "..", "..", "..", ".."))
for _p in (os.path.join(_WS, "obt.project/scripts"),):
  if _p in sys.path:
    sys.path.remove(_p)
  sys.path.insert(0, _p)

from orkengine.core import vec3   # core before lev2
from orkengine import ecs

from ork.hypergraph.ecs.runtime import EcsRuntime
from ork.testing.verdict import verdict

# Authored values, each DELIBERATELY off the runtime's default for that key, so
# a survivor cannot be a coincidence. The property under test is general — it is
# not about any one of these — so several are asserted at once.
AUTHORED = {
    "AmbientLight":      vec3(0.1),          # runtime default vec3(0.0)
    "SkyboxIntensity":   1.7,                # 1.0
    "DiffuseIntensity":  2.5,                # 1.0
    "SpecularIntensity": 0.5,                # 1.0
    "ssaa":              0,                  # 1
    "clearcolor":        vec3(0.5, 0.0, 0.0),# vec3(0.08,0.08,0.1)
}

# The full defaults contract, restated here on purpose: a scene that authors
# NOTHING must still come out of ensure_scenegraph_system with all nine, or the
# set-if-absent rewrite has traded a clobber for a hole (a scene with no preset
# throws "unknown compositor preset type" downstream).
DEFAULTS = {
    "preset":                 "ForwardPBR",
    "ssaa":                   1,
    "SkyboxIntensity":        1.0,
    "DiffuseIntensity":       1.0,
    "SpecularIntensity":      1.0,
    "AmbientLight":           vec3(0.0),
    "enable_skybox":          True,
    "use_float_color_buffer": True,
    "clearcolor":             vec3(0.08, 0.08, 0.1),
}


def _same(a, b):
  if hasattr(a, "x") and hasattr(b, "x"):
    return (abs(a.x - b.x) < 1e-6 and abs(a.y - b.y) < 1e-6
            and abs(a.z - b.z) < 1e-6)
  if isinstance(a, float) or isinstance(b, float):
    return abs(float(a) - float(b)) < 1e-6
  return a == b


def _fresh(authored=None):
  scene_data = ecs.SceneData()
  sgsys      = scene_data.addSceneGraphSystem()
  sgsys.declareLayer("std_forward")
  sgsys.declareLayer("std_transparent")
  if authored:
    sgsys.declareParams(dict(authored))
  runtime = EcsRuntime()
  runtime.scene_data = scene_data
  return runtime, scene_data, sgsys


def test_declared_params_survive_start():
  runtime, scene_data, _ = _fresh(AUTHORED)

  before = scene_data.generateSceneGraphParams()
  authored_ok = [k for k in AUTHORED if not _same(before[k], AUTHORED[k])]
  if authored_ok:
    print("  harness broken: declaration itself did not take for %s" % authored_ok,
          flush=True)
    return False

  # THE STEP UNDER TEST — start_simulation()'s first act on the scene data.
  runtime.ensure_scenegraph_system()

  after = scene_data.generateSceneGraphParams()
  lost  = []
  for key, want in sorted(AUTHORED.items()):
    got = after[key]
    if not _same(got, want):
      lost.append("%s: authored %s -> %s after the runtime start" % (key, want, got))

  print("[param survival] %s (%d/%d authored values survived)" % (
      "OK" if not lost else "BAD", len(AUTHORED) - len(lost), len(AUTHORED)),
      flush=True)
  for l in lost:
    print("  " + l, flush=True)
  return not lost


def test_absent_defaults_are_filled():
  """The converse: author NOTHING, still get all nine defaults."""
  runtime, scene_data, sgsys = _fresh()

  # hasParam is the absent-check the runtime itself uses; the pre-state must be
  # empty or this proves nothing about who filled the values.
  already = [k for k in DEFAULTS if sgsys.hasParam(k)]
  if already:
    print("  harness broken: virgin scene already carries %s" % already, flush=True)
    return False

  runtime.ensure_scenegraph_system()

  after   = scene_data.generateSceneGraphParams()
  missing = [k for k in sorted(DEFAULTS) if not sgsys.hasParam(k)]
  wrong   = ["%s: want %s got %s" % (k, DEFAULTS[k], after[k])
             for k in sorted(DEFAULTS)
             if sgsys.hasParam(k) and not _same(after[k], DEFAULTS[k])]

  print("[defaults filled] %s (%d/%d defaults present)" % (
      "OK" if not (missing or wrong) else "BAD",
      len(DEFAULTS) - len(missing), len(DEFAULTS)), flush=True)
  for k in missing:
    print("  never declared: %s" % k, flush=True)
  for w in wrong:
    print("  " + w, flush=True)
  return not (missing or wrong)


def test_explicit_override_wins_loudly():
  """An explicit params= from the caller IS a deliberate override and still
  wins over the author — but it must SAY SO, naming both values."""
  runtime, scene_data, _ = _fresh({"AmbientLight": vec3(0.1)})

  buf = io.StringIO()
  with contextlib.redirect_stdout(buf):
    runtime.ensure_scenegraph_system(params={"AmbientLight": vec3(0.9)})
  said = buf.getvalue()

  got  = scene_data.generateSceneGraphParams()["AmbientLight"]
  wins = _same(got, vec3(0.9))
  loud = ("AmbientLight" in said
          and "OVERRIDE" in said
          and "0.1" in said        # the authored value it is displacing
          and "0.9" in said)       # the runtime value replacing it

  print("[loud override] %s (runtime value wins: %s, warned: %s)" % (
      "OK" if (wins and loud) else "BAD", wins, loud), flush=True)
  for line in said.splitlines():
    print("  said: " + line, flush=True)
  return wins and loud


def main():
  tests = [test_declared_params_survive_start,
           test_absent_defaults_are_filled,
           test_explicit_override_wins_loudly]
  failed = [t.__name__ for t in tests if not t()]
  ok = (len(failed) == 0)
  print("=== scenegraph param survival %s (%d/%d) ===" % (
      "PASSED" if ok else "FAILED",
      len(tests) - len(failed), len(tests)), flush=True)
  return verdict(ok, "passed=%d/%d%s" % (
      len(tests) - len(failed), len(tests),
      (" failed=" + ",".join(failed)) if failed else ""))


sys.exit(main())
