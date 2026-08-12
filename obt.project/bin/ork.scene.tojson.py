#!/usr/bin/env ork.python

###############################################################################
# ork.scene.tojson.py — serialize a Tier 3 Scene class to JSON.
#
# Headless app-lifecycle: spins up lev2 with subsystems + offscreen
# context, binds the main gfx ctx to this thread, constructs the Scene
# class (eager builds whatever assets the Scene's __init__ declares —
# materials, SDFs, HDRI probes), lowers into an ecs.SceneData via
# Scene.build, serializes to JSON, writes to the output file.
#
# Subsystem mode + bindGfxToCurrentThread() is what lets HdriToXir and
# other GPU-bound asset gens run inline during the Scene's eager
# composition (vs. ecs.headless_init which only pushes loader TLS).
#
# Usage:
#   ork.scene.tojson.py -i saddle           -o saddle.json
#   ork.scene.tojson.py -i scenes/foo.py    -o foo.json
#   ork.scene.tojson.py -i foo.py --class MyScene -o foo.json
#
# The -i argument accepts a bare name (resolved via
# ORK_SCENES_SEARCH_PATH; defaults to <ork.data>/scenes) OR an
# explicit .py path. The Scene class is auto-detected; --class names
# it explicitly for multi-class files.
###############################################################################

import argparse
import os
import sys

from orkengine import core  # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.ecs.scene.resolve import resolve_scene_file, load_scene_class, portable_scene_path


def parse_args():
  p = argparse.ArgumentParser(
    description="Serialize a Scene class (.py) to a SceneData JSON file.")
  p.add_argument("-i", "--input", required=True, metavar="GENSCNFILE",
                 help="Scene .py file or bare name (resolved via ORK_SCENES_SEARCH_PATH)")
  p.add_argument("-o", "--output", required=True, metavar="JSON_OUT",
                 help="Output JSON file path")
  p.add_argument("--class", dest="class_name", default=None,
                 help="Explicit Scene subclass name (auto-detected if omitted)")
  return p.parse_args()


def main():
  args = parse_args()

  # headless_appinit registers BOTH ECS + lev2 reflection AND creates an
  # offscreen ezapp (subsystem mode). Without ECS reflection registered,
  # sd.declareSystem("SceneGraphSystem") asserts in addSystemWithClassName.
  # bindGfxToCurrentThread pins the main gfx ctx on this thread so the
  # Scene's eager self.asset.HdriToXir(...) bake can drive GPU work inline.
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "ezapp.bindGfxToCurrentThread() returned null"

  try:
    scene_path  = resolve_scene_file(args.input)
    scene_class = load_scene_class(scene_path, args.class_name)
  except (FileNotFoundError, ValueError) as e:
    print(f"ork.scene.tojson: {e}", file=sys.stderr)
    ezapp.mainThreadEnd()
    os._exit(2)

  print(f"ork.scene.tojson: {scene_class.__name__} ← {scene_path}", file=sys.stderr)

  scene = scene_class()
  sd    = ecs.SceneData()
  # reflected scene source (token form) — see portable_scene_path()
  sd.scene_script_path = portable_scene_path(scene_path)
  scene.build(sd)

  js = sd.serializeJson()
  with open(args.output, "w") as f:
    f.write(js)

  print(f"ork.scene.tojson: wrote {args.output} ({len(js)} bytes)", file=sys.stderr)

  ezapp.mainThreadEnd()
  os._exit(0)


if __name__ == "__main__":
  main()
