#!/usr/bin/env ork.python

################################################################################
# ork.terrain.view.py — bake a terrain HeightField DSL and open the EXR(s).
#
# Akin to ork.particle.viewer.py, but terrain bakes to image channels rather
# than rendering a live drawable — so this resolves + runs the DSL once through
# the HeightField asset wrapper (cook-cache-backed), then opens each baked EXR
# with `open` (macOS Preview) via obt.command.run.
#
# Usage:
#   ork.terrain.view.py hf1                     (bare name; assets/terrain resolve)
#   ork.terrain.view.py hf1 --dim 2048
#   ork.terrain.view.py hf1 -p octaves=7 -p steps=8
#   ork.terrain.view.py /full/path/to/file.py --class HF1
#   ork.terrain.view.py --list
#
# Search path: ORK_TERRAIN_SEARCH_PATH (colon-separated); default
# <hypergraph>/assets/terrain.
################################################################################

import argparse, ast, sys
from pathlib import Path

from obt import command

from orkengine import core   # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.ecs.scene.assets import HeightField
from ork.hypergraph.dflow.terrain.resolve import list_dsl_files


def _parse_param(spec):
  """KEY=VALUE -> (key, value). ast.literal_eval first (numbers/bools/lists),
  falling back to the raw string. e.g. -p octaves=7 -p steps=8.0"""
  if "=" not in spec:
    raise ValueError(f"--param expects KEY=VALUE, got {spec!r}")
  key, raw = spec.split("=", 1)
  try:
    value = ast.literal_eval(raw)
  except (ValueError, SyntaxError):
    value = raw
  return key.strip(), value


def parse_args():
  p = argparse.ArgumentParser(
    description="terrain HeightField DSL viewer — bake the heightfield channels and open them")
  p.add_argument("dsl_file", nargs="?",
                 help="terrain DSL bare name (resolved via ORK_TERRAIN_SEARCH_PATH) or path to a .py")
  p.add_argument("--class", dest="class_name", default=None,
                 help="explicit HeightField subclass (auto-find if omitted)")
  p.add_argument("--dim", "-d", type=int, default=1024, help="bake grid resolution (W=H)")
  p.add_argument("--param", "-p", action="append", dest="params", default=[],
                 metavar="KEY=VALUE", help="DSL constructor kwarg (repeatable)")
  p.add_argument("--no-open", action="store_true", help="bake only; don't open the EXR")
  p.add_argument("--list", "-l", action="store_true",
                 help="list terrain DSL files in the search path and exit")
  return p.parse_args()


def main():
  args = parse_args()
  if args.list or args.dsl_file is None:
    list_dsl_files()
    return 0
  try:
    dsl_kwargs = dict(_parse_param(s) for s in args.params)
  except ValueError as e:
    print(f"terrain view: {e}", file=sys.stderr)
    return 2

  # headless GPU lifecycle (mirrors the llgfx terrain tests).
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"

  name = Path(args.dsl_file).stem
  print(f"terrain view: baking {name} (dim={args.dim}) {dsl_kwargs or ''}", flush=True)
  try:
    hf = HeightField(dsl_file=args.dsl_file, dsl_class=args.class_name,
                     dimension=args.dim, ctx=ctx, **dsl_kwargs)
    hf.gendata.asset_name = name
    artifacts = hf.build()
  except (FileNotFoundError, ValueError) as e:
    ezapp.mainThreadEnd()
    ecs.headless_exit()
    print(f"terrain view: {e}", file=sys.stderr)
    return 2

  ezapp.mainThreadEnd()
  ecs.headless_exit()

  channels = [(ch, p) for ch, p in artifacts.items() if ch != "stats"]
  for ch, path in channels:
    s = artifacts["stats"].get(ch)
    print(f"terrain view:   {ch} -> {path}  {s}", flush=True)
  if not args.no_open:
    for _, path in channels:
      command.run(["open", path])
  return 0


if __name__ == "__main__":
  sys.exit(main())
