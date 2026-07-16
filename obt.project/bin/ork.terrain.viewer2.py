#!/usr/bin/env ork.python

################################################################################
# ork.terrain.viewer2.py — THIN terrain viewer (JUL09 S1: ECS-hosted).
#
# Superseded the raw Python chunk-display path (TerrainChunkVertexSource +
# ComputeDrawableData consumer) — that parity copy is what rotted the u_dim upload
# (invisible terrain). This is now a thin wrapper around the SHARED TerrainRuntime:
#   1. load the terrain DOCUMENT (DSL name / .py / doc-JSON path),
#   2. export its in-code ECS scene to JSON (document.elaborate() -> embedded
#      HeightFieldGenData; the C++ terrain path bakes + renders it natively),
#   3. exec ork.ecs.player.exe on it — the ONE true C++ loader/wire/render path
#      (same flow as ork.scene.viewer.py).
#
# Usage:
#   ork.terrain.viewer2.py voronoi
#   ork.terrain.viewer2.py voronoi -p frequency=16 --dim 1024 --camdist 400
#   ork.terrain.viewer2.py voronoi --offscreen -S /tmp/terra.png
################################################################################

import argparse
import ast
import os
import shutil
import sys
import tempfile

from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.editor.terrain_runtime import TerrainRuntime
from ork.hypergraph.dflow.terrain.resolve import list_dsl_files


def parse_param(spec):
  if "=" not in spec:
    raise ValueError(f"--param expects KEY=VALUE, got {spec!r}")
  key, raw = spec.split("=", 1)
  try:
    value = ast.literal_eval(raw)
  except (ValueError, SyntaxError):
    value = raw
  return key.strip(), value


def main():
  parser = argparse.ArgumentParser(description="Terrain viewer (ECS-hosted; execs the C++ player)")
  parser.add_argument("dsl_file", nargs="?", help="terrain DSL bare name, a .py path, or a doc-JSON path")
  parser.add_argument("--class", dest="class_name", default=None, help="explicit HeightField subclass")
  parser.add_argument("--dim", "-d", type=int, default=1024, help="render/bake grid resolution; default 1024")
  parser.add_argument("--chunk", "-c", type=int, default=128, help="GPU cull chunk size in cells/side; default 128")
  parser.add_argument("-x", "--extent", type=float, default=None,
                      help="world XZ extent in meters (default: EXTENT_M from the DSL class); "
                           "heights are TRUE METERS — there is no vertical scale to override")
  parser.add_argument("--param", "-p", action="append", dest="params", default=[],
                      metavar="KEY=VALUE", help="DSL constructor kwarg (repeatable)")
  parser.add_argument("--simple", action="store_true", help="force a plain Solid material")
  parser.add_argument("--no-devkeys", action="store_true", help="disable the player's viewer keys (E envmap / G gamma / T exposure / C saturation / R reset)")
  parser.add_argument("--camdist", type=float, default=None, help="player orbit camera distance")
  parser.add_argument("--camheight", type=float, default=None, help="player orbit camera height")
  parser.add_argument("--offscreen", action="store_true", help="headless render (no window)")
  parser.add_argument("-S", "--snapshot", default=None, help="write the settled offscreen frame to PATH (png)")
  parser.add_argument("--keep-json", action="store_true", help="keep the exported .ecs at /tmp/<name>.ecs")
  parser.add_argument("--list", "-l", action="store_true", help="list terrain DSL files and exit")
  args = parser.parse_args()

  if args.list or args.dsl_file is None:
    list_dsl_files()
    sys.exit(0)

  try:
    dsl_kwargs = dict(parse_param(s) for s in args.params)
  except ValueError as e:
    print(f"ork.terrain.viewer2: {e}", file=sys.stderr)
    sys.exit(2)

  # tojson phase — subsystem-mode lev2 + a bound gfx ctx so the material's eager
  # composition can drive GPU work inline (exactly what ork.scene.viewer.py does).
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"

  rt = TerrainRuntime(preview_dim=args.dim, chunk=args.chunk)
  rt.load(args.dsl_file, dsl_class=args.class_name,
          extent_m=args.extent, **dsl_kwargs)
  rt.set_context(ctx)
  js = rt.export_scene_json(dim=args.dim, simple_material=args.simple)
  ezapp.mainThreadEnd()

  if args.keep_json:
    out_path = f"/tmp/{rt.source_label}.ecs"
    with open(out_path, "w") as f:
      f.write(js)
  else:
    tf = tempfile.NamedTemporaryFile(suffix=".ecs", delete=False, mode="w")
    tf.write(js); tf.close()
    out_path = tf.name

  runner = shutil.which("ork.ecs.player.exe")
  if runner is None:
    print("ork.terrain.viewer2: ork.ecs.player.exe not found on PATH", file=sys.stderr)
    sys.exit(2)
  cmd = [runner, out_path]
  if not args.no_devkeys:
    cmd += ["--devkeys"]
  # camera defaults derived from the terrain EXTENT (heights are true meters with no
  # scale constant, and the bake is deferred to the player, so extent is the only
  # scale known here; the player's own 20m/8m orbit sits inside any real terrain).
  camdist   = args.camdist   if args.camdist   is not None else rt.extent_m * 0.35
  camheight = args.camheight if args.camheight is not None else rt.extent_m * 0.12
  cmd += ["--camdist", str(camdist), "--camheight", str(camheight)]
  if args.offscreen:
    cmd += ["--offscreen"]
  if args.snapshot:
    cmd += ["-S", args.snapshot]
  os.execv(runner, cmd)


if __name__ == "__main__":
  main()
