#!/usr/bin/env ork.python

###############################################################################
# ork.scene.materialize.py — OFFSCREEN materialize a Tier 3 Scene.
#
# Authors + serializes the scene (the SAME tojson phase as ork.scene.viewer.py),
# then runs ork.ecs.player.exe HEADLESS (--offscreen, hidden window) for a short
# frame budget so the LOAD-TIME GPU bakes run and write their disk caches with NO
# window — chiefly the terrain proctex texbake (stored-mode captures). Afterward
# ork.scene.viewer.py runs are WARM (cache hit, zero bake).
#
# Same one-true loader/wire/render path as the viewer (the C++ player); only the
# window is suppressed and the run is frame-budgeted + self-exiting.
#
# Usage:
#   ork.scene.materialize.py scn_terrabake               # prime the cache, no window
#   ork.scene.materialize.py scn_terrabake --frames 120  # bigger budget (huge bakes)
#   ork.scene.materialize.py scn_terrabake --movie /tmp/t.mp4   # also record a clip
#   ork.scene.materialize.py --list                      # list scenes
###############################################################################

import argparse, json, os, shutil, subprocess, sys, tempfile

from orkengine import core  # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.ecs.scene.resolve import (
  resolve_scene_file, load_scene_class, list_scene_files,
)


def _find_capture_sinks(obj, out):
  """Recursively collect (capture_dir, [targets], capture_mode) from the serialized
  scene. The terrain drawable records these; we verify the PNGs after the bake."""
  if isinstance(obj, dict):
    cdir = obj.get("capture_dir")
    if cdir:
      out.append((cdir,
                  list(obj.get("capture_targets", []) or []),
                  obj.get("capture_mode", "")))
    for v in obj.values():
      _find_capture_sinks(v, out)
  elif isinstance(obj, list):
    for v in obj:
      _find_capture_sinks(v, out)


def parse_args():
  p = argparse.ArgumentParser(
    description="OFFSCREEN scene materialize — prime load-time GPU bakes/caches headlessly")
  p.add_argument("scene_file", nargs="?", help="scene .py path or short name")
  p.add_argument("--class", dest="class_name", default=None,
                 help="scene class name (disambiguate a multi-class file)")
  p.add_argument("--frames", type=int, default=None,
                 help="offscreen frame safety-cap (default: the player's; exit is load-settle driven)")
  p.add_argument("--keep-json", action="store_true",
                 help="keep the intermediate .ecs at /tmp/<Scene>.ecs")
  p.add_argument("--movie", default=None, metavar="PATH",
                 help="also record an offscreen movie to PATH (mp4)")
  p.add_argument("--moviefps", type=float, default=60.0, help="movie fps (with --movie)")
  p.add_argument("--movieframes", type=int, default=None, help="movie frame count (with --movie)")
  p.add_argument("--list", action="store_true", help="list available scenes")
  return p.parse_args()


def main():
  args = parse_args()

  if args.list or not args.scene_file:
    list_scene_files()
    return 0

  try:
    scene_path  = resolve_scene_file(args.scene_file)
    scene_class = load_scene_class(scene_path, args.class_name)
  except (FileNotFoundError, ValueError) as e:
    print(f"ork.scene.materialize: {e}", file=sys.stderr)
    return 2

  print(f"ork.scene.materialize: {scene_class.__name__} ← {scene_path}", file=sys.stderr)

  ##########################################################################
  # tojson phase — identical to the viewer: subsystem lev2 init + bindGfx so
  # the scene's eager asset composition can run inline. terrain() computes the
  # content-addressed cache dir here (deferred bakes still defer to the player).
  ##########################################################################
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "ezapp.bindGfxToCurrentThread() returned null"

  scene = scene_class()
  sd    = ecs.SceneData()
  scene.build(sd)
  js    = sd.serializeJson()
  ezapp.mainThreadEnd()
  ecs.headless_exit()   # release THIS process's GPU before the player subprocess

  ##########################################################################
  # park the .ecs
  ##########################################################################
  if args.keep_json:
    out_path = f"/tmp/{scene_class.__name__}.ecs"
    with open(out_path, "w") as f:
      f.write(js)
  else:
    tf = tempfile.NamedTemporaryFile(suffix=".ecs", delete=False, mode="w")
    tf.write(js)
    tf.close()
    out_path = tf.name

  ##########################################################################
  # discover the bake sinks (for post-verify) — stored = will bake this run.
  ##########################################################################
  sinks = []
  try:
    _find_capture_sinks(json.loads(js), sinks)
  except Exception as e:
    print(f"ork.scene.materialize: (sink scan skipped: {e})", file=sys.stderr)
  stored = [(d, t) for (d, t, m) in sinks if m == "stored"]
  if stored:
    print("ork.scene.materialize: stored-mode bakes to prime:", file=sys.stderr)
    for d, t in stored:
      print(f"   {d}  targets={t}", file=sys.stderr)
  elif sinks:
    print("ork.scene.materialize: capture sinks present but all WARM (no bake needed)",
          file=sys.stderr)

  ##########################################################################
  # run the player OFFSCREEN (headless, frame-budgeted, self-exiting)
  ##########################################################################
  runner = shutil.which("ork.ecs.player.exe")
  if runner is None:
    print("ork.scene.materialize: ork.ecs.player.exe not found on PATH", file=sys.stderr)
    return 2
  cmd = [runner, out_path, "--offscreen"]
  if args.frames:
    cmd += ["--frames", str(args.frames)]
  if args.movie:
    cmd += ["--movie", args.movie, "--moviefps", str(args.moviefps)]
    if args.movieframes:
      cmd += ["--movieframes", str(args.movieframes)]
  print(f"ork.scene.materialize: launching {os.path.basename(runner)} {' '.join(cmd[1:])}",
        file=sys.stderr)
  rc = subprocess.run(cmd).returncode

  ##########################################################################
  # verify the stored-mode caches got written
  ##########################################################################
  ok = True
  for d, targets in stored:
    for t in targets:
      pp = os.path.join(d, f"{t}.png")
      exists = os.path.isfile(pp)
      ok = ok and exists
      print(f"   [{'OK ' if exists else 'MISS'}] {pp}", file=sys.stderr)
  if args.movie:
    mok = os.path.isfile(args.movie)
    ok = ok and mok
    print(f"   [{'OK ' if mok else 'MISS'}] movie {args.movie}", file=sys.stderr)

  if rc != 0:
    print(f"ork.scene.materialize: player exited rc={rc}", file=sys.stderr)
    return rc
  if stored and not ok:
    print("ork.scene.materialize: some caches MISSING — try a larger --frames budget",
          file=sys.stderr)
    return 1
  print("ork.scene.materialize: DONE — viewer runs are now WARM", file=sys.stderr)
  return 0


if __name__ == "__main__":
  sys.exit(main())
