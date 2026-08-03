#!/usr/bin/env ork.python
################################################################################
# FOREST TONEMAP DECLARATION gate — the scene's dead-of-night anchor has to reach
# the PLAYER, and a misspelling of it has to reach the AUTHOR.
#
# THE DEFECT THIS CLOSES. scn_forest rendered its midnight black while
# two separate attempts to raise the tone stage's floor anchor sat in the tree
# doing nothing. Neither was a typo in the value: one was never passed at all
# (the scene's self.sky(...) carried no tonemap=, so the shipped .ecs carried the
# engine default 0.65 unconditionally), and the other was spelled as a bare
# sky() kwarg, which falls through **dome_params into declareParams — a dict
# assignment the engine reads key by key, where an unread key is DROPPED WITHOUT
# A WORD. Both failures look identical from the outside: a declaration that
# renders as if it had never been made.
#
# So this gate asserts the two halves that make that impossible:
#
#   THE DECLARATION LANDS. The scene is authored through the shipped serializer
#   (ork.scene.tojson.py — the same phase ork.scene.viewer/materialize and the
#   zero-python player run) and the ACES node is found in the resulting .ecs by
#   flattening it. The floor value there must equal what the scene declared, to
#   float32. Twice: at the scene's own ADAPT_FLOOR constant, and at an override
#   through its FOREST_ADAPT_FLOOR knob — one run alone cannot tell a plumbed
#   value from a coincidence.
#
#   THE MISSPELLING FAILS LOUD. An unknown tone-stage knob and an unknown sky()
#   kwarg both RAISE, and the message names the offending key. Author phase, no
#   device: the point is that the author never gets to the renderer.
#
# The shipped floor is read OUT OF THE SCENE FILE rather than restated here, so
# an owner re-cut of the calibration moves this gate with it instead of leaving
# a stale number behind.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

import json
import shutil
import struct
import subprocess
import tempfile

from ork.testing import verdict

# workspace-anchored (never cwd-derived): <ws>/ork.lev2/pyext/tests/llgfx/<this>
_WS = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   "..", "..", "..", ".."))
SCENE     = "scn_forest"
SCENE_PY  = os.path.join(_WS, "ork.data", "scenes", "scn_forest.py")
FLOOR_ENV = "FOREST_ADAPT_FLOOR"
# a value nothing else in the tree carries, so a match cannot be a default
PROBE_FLOOR  = 137.0
TOJSON_TIMEOUT = 300.0

failures = []


def check(label, ok, detail=""):
  print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
  if not ok:
    failures.append(label)


def f32(v):
  """what a float becomes once it has been through the engine's float32
  properties — the .ecs value is compared against THIS, not against the python
  double, so 32.0 and 0.65 are both judged exactly."""
  return struct.unpack("f", struct.pack("f", float(v)))[0]


def shipped_floor():
  """the scene's OWN declared anchor, read out of the scene file by text (an
  import would pull the whole eager asset DSL in; this gate wants one number)."""
  for line in open(SCENE_PY):
    if line.startswith("ADAPT_FLOOR"):
      return float(line.split("_envf(", 1)[1].rsplit(",", 1)[1].split(")")[0])
  raise RuntimeError("forest tonemap gate: no ADAPT_FLOOR in %s — the scene "
                     "stopped declaring the anchor this gate is about" % SCENE_PY)


def author(floor=None):
  """author the SHIPPED scene through the shipped serializer -> parsed .ecs."""
  tool = shutil.which("ork.scene.tojson.py")
  if tool is None:
    raise RuntimeError("forest tonemap gate: ork.scene.tojson.py not on PATH "
                       "(the gate authors through the shipped path, not a hand-rolled build)")
  env = dict(os.environ)
  if floor is None:
    env.pop(FLOOR_ENV, None)
  else:
    env[FLOOR_ENV] = repr(float(floor))
  # frozen clock + the dead of night: the anchor under test acts only there, and
  # a scene authored mid-day would still serialize it, but a reader of this gate
  # should see the configuration the number is FOR.
  env["ORK_FORESTSKY_TOD"] = "0.0"
  env["FOREST_TIME_SCALE"] = "0.0"
  out = os.path.join(tempfile.mkdtemp(prefix="forestfloor_"), "scene.ecs")
  proc = subprocess.run([tool, "-i", SCENE, "-o", out], env=env,
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                        timeout=TOJSON_TIMEOUT)
  if (proc.returncode != 0) or (not os.path.isfile(out)):
    sys.stdout.write(proc.stdout.decode("utf-8", "replace")[-3000:])
    raise RuntimeError("forest tonemap gate: authoring failed rc=%d floor=%s"
                       % (proc.returncode, floor))
  return json.load(open(out))


def aces_props(ecs):
  """the tone stage's authored properties, found by FLATTENING the .ecs — the
  node's path through the scenegraph system's postfx table is not the claim, the
  presence of the value is."""
  hits = []

  def walk(node):
    if isinstance(node, dict):
      if "adaptFloor" in node:
        hits.append(node)
      for v in node.values():
        walk(v)
    elif isinstance(node, list):
      for v in node:
        walk(v)

  walk(ecs)
  if len(hits) != 1:
    raise RuntimeError("forest tonemap gate: %d ACES property blocks in the "
                       ".ecs, expected exactly 1" % len(hits))
  return hits[0]


def raises_naming(fn, key):
  """the surface refuses `key`, and says which key it refused."""
  try:
    fn()
  except ValueError as e:
    return key in str(e), str(e)
  except Exception as e:                       # a TypeError from the signature
    return False, "%s: %s" % (type(e).__name__, e)
  return False, "<no exception>"


def main():
  ##########################################################################
  # THE DECLARATION LANDS — twice, at two different values
  ##########################################################################
  print("=== the declared floor rides the .ecs ===", flush=True)
  floor = shipped_floor()

  shipped = aces_props(author())
  check("shipped_floor_reaches_the_ecs", shipped["adaptFloor"] == f32(floor),
        "scene declares %g, .ecs carries %r" % (floor, shipped["adaptFloor"]))

  probed = aces_props(author(PROBE_FLOOR))
  check("declared_floor_is_carried_verbatim",
        probed["adaptFloor"] == f32(PROBE_FLOOR),
        "%s=%g -> .ecs %r" % (FLOOR_ENV, PROBE_FLOOR, probed["adaptFloor"]))

  # the two runs differ ONLY in the anchor: a stage that serialized a constant
  # would pass the pair above only by accident, and the rest of the curve moving
  # with it would mean the knob is not the knob.
  rest = [k for k in shipped
          if k != "adaptFloor" and shipped[k] != probed.get(k)]
  check("the_floor_is_the_only_thing_that_moved", not rest,
        "other tone-stage properties that changed: %s" % (rest or "none"))
  check("the_engine_default_is_not_what_ships", shipped["adaptFloor"] != f32(0.65),
        "engine default 0.65, scene ships %r" % shipped["adaptFloor"])

  ##########################################################################
  # THE MISSPELLING FAILS LOUD — author phase, no device
  ##########################################################################
  print("=== the surface refuses what it cannot deliver ===", flush=True)
  from ork.hypergraph.ecs.scene import Scene

  class _Bare(Scene):
    """no scenegraph declared: sky_dome() takes its DECLARE path, which is the
    one a scene that says this wrong would be on."""

  bad_knob = "adapt_flooor"                    # the misspelling class
  ok, msg = raises_naming(lambda: _Bare().sky(tonemap={bad_knob: 512.0}), bad_knob)
  check("unknown_tonemap_knob_raises_naming_the_key", ok, msg[:160])

  bad_kwarg = "adapt_floor"                    # a real knob, said in the wrong place
  ok, msg = raises_naming(lambda: _Bare().sky(**{bad_kwarg: 512.0}), bad_kwarg)
  check("unknown_sky_kwarg_raises_naming_the_key", ok, msg[:160])

  # and the accepted vocabulary is IN the message — an author who misspelled a
  # knob is told what the knobs are, not merely that they were wrong.
  ok, msg = raises_naming(lambda: _Bare().sky(SkyboxIntesity=1.0), "SkyboxIntesity")
  check("the_refusal_names_the_accepted_set", ok and "SkyboxIntensity" in msg,
        msg[:160])

  # the legitimate spellings still pass through untouched (a validator that
  # refuses everything is not a validator). celestial=False keeps this in the
  # author phase: the ensemble's star dome builds a mesh + material, which wants
  # a device, and the declaration is what is under test.
  try:
    scene = _Bare()
    scene.sky(tonemap={"adapt_floor": 512.0}, msaa=2, ssaa=0, celestial=False,
              skybox_path="<ork_envmaps2>/desert4k.xir")
    passed, why = True, ""
  except Exception as e:
    passed, why = False, "%s: %s" % (type(e).__name__, e)
  check("the_valid_declaration_is_still_accepted", passed, why[:160])

  ok = (len(failures) == 0)
  detail = ("scene ADAPT_FLOOR=%g -> .ecs %r | probe %g -> .ecs %r | "
            "engine default 0.65 not shipped"
            % (floor, shipped["adaptFloor"], PROBE_FLOOR, probed["adaptFloor"]))
  if failures:
    detail += " failed=" + ",".join(failures)
  verdict(ok, detail)
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main())
