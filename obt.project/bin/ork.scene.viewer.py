#!/usr/bin/env ork.python

###############################################################################
# ork.scene.viewer.py — thin wrapper: tojson → ecsplay.
#
# Since the M0 ECS switch, the right path for viewing a Scene class is:
#   1. ork.scene.tojson.py -i <scene> -o /tmp/x.ecs   (build + serialize)
#   2. ork.ecsplay.py -s /tmp/x.ecs                   (load + play)
# This script folds both into one command — equivalent to running them
# back-to-back with a temp .ecs file.
#
# Identical CLI surface to the old standalone viewer; --list still works
# without entering the lev2 lifecycle.
#
# Usage:
#   ork.scene.viewer.py hello                          # bare name
#   ork.scene.viewer.py hello -f                       # fullscreen
#   ork.scene.viewer.py hello -e                       # ecsedit instead of ecsplay
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
    description="Tier 3 Scene viewer — tojson(scene) → ecsplay")
  p.add_argument("scene_file", nargs="?",
                 help="Scene bare name (resolved via ORK_SCENES_SEARCH_PATH) "
                      "or explicit path to a .py file")
  p.add_argument("--class", dest="class_name", default=None,
                 help="explicit Scene subclass to load (auto-find used if omitted)")
  p.add_argument("--ssaa", type=int, default=1, help="SSAA multiplier (0=off)")
  p.add_argument("-e", "--edit", action="store_true",
                 help="launch ork.ecsedit.py instead of ork.ecsplay.py")
  p.add_argument("-f", "--fullscreen", action="store_true",
                 help="start ecsplay in fullscreen")
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

  target = "ork.ecsedit.py" if args.edit else "ork.ecsplay.py"
  print(f"ork.scene.viewer: wrote {out_path} ({len(js)} bytes); launching {target}",
        file=sys.stderr)

  # exec the chosen runner — replaces this process.
  runner = shutil.which(target)
  if runner is None:
    print(f"ork.scene.viewer: {target} not found on PATH", file=sys.stderr)
    return 2

  cmd = [runner, "-s", out_path, "--ssaa", str(args.ssaa)]
  if args.fullscreen:
    cmd.append("-f")

  os.execvp(runner, cmd)


if __name__ == "__main__":
  sys.exit(main())
