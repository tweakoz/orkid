#!/usr/bin/env ork.python
###############################################################################
# GATE — a celestial sky survives the TWO-PROCESS playback recipe.
#
# THE DEFECT THIS DEFENDS (jul30): Scene.sun(celestial=...)/moon()/stars()
# handed the sim its site+clock through the PROCESS ENVIRONMENT
# ($ORK_CELESTIAL_CONFIG). ork.scene.viewer.py hides that, because it authors
# and then execvp's the player inside ONE process — the env is inherited and
# every celestial light finds its config. The documented two-step recipe
#     ork.scene.tojson.py  ->  ork.ecs.player.exe
# is TWO processes, so the player started with an EMPTY table, the first
# celestial component to update raised
#     KeyError: no celestial config for light entity <moon> ... (declared: [])
# and PythonSystem's __pcallargs turned that into an OrkAssert — a hard crash
# of the update thread. The same root killed every attempt to REUSE an authored
# .ecs across processes.
#
# The fix makes the config ride the .ecs: PythonComponentData::_scriptData
# (reflected as "ScriptData", read back in the sim as comp.script_data). So the
# gate has to prove BOTH halves, and prove the loud failure is still loud:
#
#   A. CARRIED   — every celestial component in the authored .ecs carries a
#                  parseable config, and its body/site match what the scene
#                  declared. A config that is present but wrong is a silent
#                  wrong sky, so the values are checked, not just presence.
#   B. PLAYED    — the player consumes that .ecs with $ORK_CELESTIAL_CONFIG
#                  SCRUBBED FROM ITS ENVIRONMENT and runs to a clean exit. The
#                  scrub is the whole point: it is what a second process
#                  actually has, and what the old code could not survive.
#   C. STILL LOUD — the same .ecs with ScriptData stripped back out, played with
#                  the same scrubbed env, must DIE with the named error. A fix
#                  that quietly defaulted a missing config would pass A and B
#                  and hand every mis-wired scene somebody else's sky.
#   D/E. THE DECLARED MOON — the authored lunar placement rides the same channel
#                  and is SOLVED in the player process, so a declared phase must
#                  play clean there and an over-constrained one must be refused
#                  there, by name.
#
# scn_procsky is the gate scene: the lightest shipped scene carrying the whole
# ensemble (sun + moon + star dome over one site and one clock).
#
# Hand-rolled subprocess boot rather than the ork.testing harness: the PROCESS
# BOUNDARY is the thing under test, so both phases have to be real separate
# processes with environments this test controls. verdict() is used for the
# machine-readable line exactly as the harness tests do.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import json, subprocess, sys, tempfile

from ork.testing import verdict

SCENE       = "scn_procsky"
CONFIG_ENV  = "ORK_CELESTIAL_CONFIG"

# What the scene declares (scn_procsky takes Scene.sky()'s defaults).
EXPECT_LATITUDE   = 45.0
EXPECT_DAY        = 220.0
EXPECT_TIME_SCALE = 480.0
# entity name -> the body that entity's config must name. The star dome is not
# a light and never branches on body, so it keeps the config default ("sun").
EXPECT_BODIES = {"sun": "sun", "moon": "moon", "stars": "sun"}

AUTHOR_TIMEOUT = 300
PLAY_TIMEOUT   = 180

# the substring the loud failure must print (leg C)
LOUD_MARKER = "no celestial config"


def _scrubbed_env():
  """A player environment with the legacy side-channel REMOVED — i.e. what any
  process that did not itself author the scene actually has."""
  env = dict(os.environ)
  env.pop(CONFIG_ENV, None)
  return env


def _celestial_components(ecs_json):
  """{entity name: ScriptData string} for every PythonComponentData in the
  scene, keyed by the archetype's entity name (Scene.entity() names its
  archetype Arch_<entity>)."""
  out = {}
  scene_objects = (ecs_json["root"]["object"]["properties"]["SceneObjects"])
  for archname, archnode in scene_objects.items():
    if not archname.startswith("Arch_"):
      continue
    props = archnode.get("object", {}).get("properties", {})
    comps = props.get("Components", {})
    pyc = comps.get("PythonComponentData")
    if pyc is None:
      continue
    out[archname[len("Arch_"):]] = pyc["object"]["properties"].get("ScriptData", "")
  return out


def _edit_script_data(src_path, dst_path, edit):
  """Copy the .ecs with every non-empty ScriptData passed through `edit`
  (a config dict -> config dict, or -> "" to blank it). Returns how many were
  touched."""
  js = json.load(open(src_path))
  scene_objects = (js["root"]["object"]["properties"]["SceneObjects"])
  touched = 0
  for archname, archnode in scene_objects.items():
    props = archnode.get("object", {}).get("properties", {})
    pyc = props.get("Components", {}).get("PythonComponentData")
    if pyc is None:
      continue
    blob = pyc["object"]["properties"].get("ScriptData", "")
    if not blob:
      continue
    edited = edit(json.loads(blob))
    pyc["object"]["properties"]["ScriptData"] = ("" if edited == ""
                                                 else json.dumps(edited))
    touched += 1
  json.dump(js, open(dst_path, "w"))
  return touched


def _strip_script_data(src_path, dst_path):
  """Copy the .ecs with every ScriptData emptied — the pre-fix artifact."""
  return _edit_script_data(src_path, dst_path, lambda cfg: "")


def _with_keys(**overrides):
  def edit(cfg):
    cfg.update(overrides)
    return cfg
  return edit


def main():
  fails = []
  tmpdir = tempfile.mkdtemp(prefix="celestial_persist_")
  ecs_path = os.path.join(tmpdir, "%s.ecs" % SCENE)
  bare_path = os.path.join(tmpdir, "%s_nodata.ecs" % SCENE)

  ###########################################################################
  # AUTHOR — process one. Its environment is irrelevant to the legs below;
  # what matters is only what lands in the file.
  ###########################################################################
  author = subprocess.run(
    ["ork.scene.tojson.py", "-i", SCENE, "-o", ecs_path],
    capture_output=True, text=True, timeout=AUTHOR_TIMEOUT)
  if author.returncode != 0 or not os.path.exists(ecs_path):
    print(author.stdout[-4000:]); print(author.stderr[-4000:])
    return verdict(False, detail="author FAILED rc=%d" % author.returncode)
  print("authored %s (%d bytes)" % (ecs_path, os.path.getsize(ecs_path)))

  ###########################################################################
  # A. CARRIED
  ###########################################################################
  carried = _celestial_components(json.load(open(ecs_path)))
  for name, want_body in EXPECT_BODIES.items():
    blob = carried.get(name, "")
    if not blob:
      fails.append("A: <%s> carries no ScriptData in the .ecs" % name)
      continue
    cfg = json.loads(blob)
    got = (cfg.get("body"), cfg.get("latitude_deg"),
           cfg.get("day_of_year"), cfg.get("time_scale"))
    want = (want_body, EXPECT_LATITUDE, EXPECT_DAY, EXPECT_TIME_SCALE)
    if got != want:
      fails.append("A: <%s> config %r != declared %r" % (name, got, want))
    else:
      print("A carried <%s>: body=%s lat=%.1f day=%.0f scale=%.0f"
            % (name, cfg["body"], cfg["latitude_deg"],
               cfg["day_of_year"], cfg["time_scale"]))

  ###########################################################################
  # B. PLAYED — process two, WITHOUT the env side-channel.
  ###########################################################################
  played = subprocess.run(
    ["ork.ecs.player.exe", ecs_path, "--offscreen"],
    capture_output=True, text=True, timeout=PLAY_TIMEOUT, env=_scrubbed_env())
  play_out = played.stdout + played.stderr
  if played.returncode != 0:
    fails.append("B: player rc=%d on the carried .ecs with $%s scrubbed"
                 % (played.returncode, CONFIG_ENV))
    print(play_out[-4000:])
  elif LOUD_MARKER in play_out:
    fails.append("B: player printed the missing-config error despite ScriptData")
  else:
    print("B played clean: rc=0 with $%s absent" % CONFIG_ENV)

  ###########################################################################
  # C. STILL LOUD — same recipe, config removed. Must die, and must say why.
  ###########################################################################
  stripped = _strip_script_data(ecs_path, bare_path)
  if stripped == 0:
    fails.append("C: nothing to strip — leg A already failed")
  else:
    bare = subprocess.run(
      ["ork.ecs.player.exe", bare_path, "--offscreen"],
      capture_output=True, text=True, timeout=PLAY_TIMEOUT, env=_scrubbed_env())
    bare_out = bare.stdout + bare.stderr
    if bare.returncode == 0:
      fails.append("C: a scene with NO celestial config played to a clean exit "
                   "— the missing-config error was softened into a default")
    elif LOUD_MARKER not in bare_out:
      fails.append("C: died rc=%d but never named the missing config"
                   % bare.returncode)
    else:
      print("C failed loudly: rc=%d, named the missing config (%d stripped)"
            % (bare.returncode, stripped))

  ###########################################################################
  # D. THE DECLARED MOON RIDES TOO — the authored lunar placement (phase /
  # initial elevation / orbit rate, _celestial.py MOON PLACEMENT) is config, so
  # it crosses on the same ScriptData. Two things have to be true in the PLAYER
  # process, where nothing but the .ecs exists: the declaration arrives, and the
  # solve that turns it into a re-epoched moon RUNS THERE — it happens inside
  # CelestialModel's constructor, in the sim subinterpreter.
  ###########################################################################
  placed_path = os.path.join(tmpdir, "%s_placed.ecs" % SCENE)
  placed = _edit_script_data(ecs_path, placed_path,
                             _with_keys(moon_phase="full", moon_orbit_rate=2.0))
  if placed == 0:
    fails.append("D: no ScriptData to place a moon in — leg A already failed")
  else:
    run = subprocess.run(
      ["ork.ecs.player.exe", placed_path, "--offscreen"],
      capture_output=True, text=True, timeout=PLAY_TIMEOUT, env=_scrubbed_env())
    out = run.stdout + run.stderr
    if run.returncode != 0:
      fails.append("D: player rc=%d on a DECLARED moon (phase=full, rate=2)"
                   % run.returncode)
      print(out[-4000:])
    else:
      print("D played clean: %d configs carried a declared moon" % placed)

  ###########################################################################
  # E. AND ITS VALIDATION RIDES WITH IT — an over-constrained pair (a full moon
  # is opposite the sun; 89 degrees up it is not) must be refused IN THE PLAYER,
  # loudly, naming the two declarations. A validation that only ran at authoring
  # time would let this .ecs play a silently-bent sky.
  ###########################################################################
  bad_path = os.path.join(tmpdir, "%s_impossible.ecs" % SCENE)
  _edit_script_data(ecs_path, bad_path,
                    _with_keys(moon_phase="full", moon_initial_elevation=89.0))
  run = subprocess.run(
    ["ork.ecs.player.exe", bad_path, "--offscreen"],
    capture_output=True, text=True, timeout=PLAY_TIMEOUT, env=_scrubbed_env())
  out = run.stdout + run.stderr
  if run.returncode == 0:
    fails.append("E: an impossible phase+elevation pair played to a clean exit "
                 "— the placement was silently bent to fit")
  elif "cannot both hold" not in out:
    fails.append("E: died rc=%d but never named the conflicting pair"
                 % run.returncode)
  else:
    print("E refused loudly: rc=%d, named the phase/elevation conflict"
          % run.returncode)

  ok = not fails
  for f in fails:
    print("FAIL " + f)
  return verdict(ok, detail=("carried+played+loud+placed, $%s scrubbed"
                             % CONFIG_ENV
                             if ok else "; ".join(fails)[:300]))


if __name__ == "__main__":
  sys.exit(main())
