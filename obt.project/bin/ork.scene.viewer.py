#!/usr/bin/env ork.python

###############################################################################
# ork.scene.viewer.py — thin wrapper: tojson → ork.ecs.player.exe.
#
# ONE PLAYBACK PATH (owner-ratified, E.2 era): viewing a Scene class is
#   1. author + serialize the scene to an .ecs (the tojson phase, in-process)
#   2. exec ork.ecs.player.exe on it (the C++ host — the SAME loader/wire/render
#      path serialized scenes ship with; no separate Python playback to drift)
# `-e` still opens ork.ecsedit.py (the editor is Python by design).
#
# Usage:
#   ork.scene.viewer.py hello                          # bare name
#   ork.scene.viewer.py hello --camdist 220            # player camera args pass through
#   ork.scene.viewer.py hello -e                       # ecsedit instead of the player
#   ork.scene.viewer.py --class MyScene scene.py       # multi-class file
#   ork.scene.viewer.py --list                         # list scenes
###############################################################################

import argparse, os, shutil, sys, tempfile

from orkengine import core  # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.ecs.scene.resolve import (
  resolve_scene_file, load_scene_class, list_scene_files,
)


def parse_args():
  p = argparse.ArgumentParser(
    description="Tier 3 Scene viewer — tojson(scene) → ork.ecs.player.exe")
  p.add_argument("scene_file", nargs="?",
                 help="Scene bare name (resolved via ORK_SCENES_SEARCH_PATH) "
                      "or explicit path to a .py file")
  p.add_argument("--class", dest="class_name", default=None,
                 help="explicit Scene subclass to load (auto-find used if omitted)")
  p.add_argument("-e", "--edit", action="store_true",
                 help="launch ork.ecsedit.py instead of the C++ player")
  p.add_argument("--camdist", type=float, default=None,
                 help="player camera distance (passed through)")
  p.add_argument("--camheight", type=float, default=None,
                 help="player camera height (passed through)")
  p.add_argument("--roundtrip", type=float, default=None,
                 help="player: fire the live serdes round-trip at T+N seconds")
  p.add_argument("-t", "--ssaa", type=int, default=0,
                 help="SSAA multiplier (passed through to the player / ecsedit)")
  p.add_argument("-f", "--fullscreen", action="store_true",
                 help="fullscreen (passed through to the player / ecsedit)")
  p.add_argument("--list", "-l", action="store_true",
                 help="list all scenes found in ORK_SCENES_SEARCH_PATH and exit")
  p.add_argument("--keep-json", action="store_true",
                 help="leave the intermediate .ecs file on disk (default: tmpfile)")
  return p.parse_args()


def main():
  args = parse_args()

  if args.list or not args.scene_file:
    # list_scene_files prints to stdout itself and returns None.
    # Bare invocation (no scene_file) defaults to listing — most useful
    # default when the user forgot the name.
    list_scene_files()
    return 0

  try:
    scene_path  = resolve_scene_file(args.scene_file)
    scene_class = load_scene_class(scene_path, args.class_name)
  except (FileNotFoundError, ValueError) as e:
    print(f"ork.scene.viewer: {e}", file=sys.stderr)
    return 2

  print(f"ork.scene.viewer: {scene_class.__name__} ← {scene_path}",
        file=sys.stderr)

  # tojson phase — exactly what ork.scene.tojson.py does. Subsystem-mode
  # lev2 init + bindGfxToCurrentThread so the Scene's eager asset
  # composition (HdriToXir bake, etc.) can drive GPU work inline.
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "ezapp.bindGfxToCurrentThread() returned null"

  scene = scene_class()
  sd    = ecs.SceneData()
  scene.build(sd)
  js    = sd.serializeJson()
  ezapp.mainThreadEnd()

  # Park the .ecs in a tmpfile (or named, if --keep-json).
  if args.keep_json:
    out_path = f"/tmp/{scene_class.__name__}.ecs"
    with open(out_path, "w") as f:
      f.write(js)
  else:
    tf = tempfile.NamedTemporaryFile(suffix=".ecs", delete=False, mode="w")
    tf.write(js)
    tf.close()
    out_path = tf.name

  if args.edit:
    # the EDITOR stays Python (ecsedit) — same -s CLI as before.
    runner = shutil.which("ork.ecsedit.py")
    if runner is None:
      print("ork.scene.viewer: ork.ecsedit.py not found on PATH", file=sys.stderr)
      return 2
    cmd = [runner, "-s", out_path, "--ssaa", str(args.ssaa)]
    if args.fullscreen:
      cmd.append("-f")
  else:
    # PLAYBACK is the C++ player — the one true loader/wire/render path.
    runner = shutil.which("ork.ecs.player.exe")
    if runner is None:
      print("ork.scene.viewer: ork.ecs.player.exe not found on PATH", file=sys.stderr)
      return 2
    cmd = [runner, out_path]
    if args.camdist is not None:
      cmd += ["--camdist", str(args.camdist)]
    if args.camheight is not None:
      cmd += ["--camheight", str(args.camheight)]
    if args.roundtrip is not None:
      cmd += ["--roundtrip", str(args.roundtrip)]
    if args.fullscreen:
      cmd += ["--fullscreen"]
    if args.ssaa and args.ssaa > 1:
      cmd += ["--ssaa", str(args.ssaa)]

  print(f"ork.scene.viewer: wrote {out_path} ({len(js)} bytes); launching {os.path.basename(runner)}",
        file=sys.stderr)

  # exec the chosen runner — replaces this process.
  os.execvp(runner, cmd)


if __name__ == "__main__":
  ret = main()
  ecs.headless_exit()
  sys.exit(ret)
