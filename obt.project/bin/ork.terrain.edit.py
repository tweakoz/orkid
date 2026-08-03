#!/usr/bin/env ork.python

################################################################################
# ork.terrain.edit.py — terrain graph editor launcher (JUL09 S1).
#
# Opens the TerrainEditor on a terrain: a DSL bare name (resolved via
# ORK_TERRAIN_SEARCH_PATH), a path to a .py DSL file, or an editor doc-JSON path.
# Every edit mutates the structured DOCUMENT then re-elaborates + rebakes (L2).
#
#   ork.terrain.edit.py voronoi
#   ork.terrain.edit.py voronoi -p frequency=16 --dim 1024
#   ork.terrain.edit.py /path/to/mydoc.json
#   ork.terrain.edit.py new          # minimal viable terrain; grow via the add flow
################################################################################

import argparse
import ast
import sys

from ork.editor.terrainedit import TerrainEditor
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
  parser = argparse.ArgumentParser(description="Terrain graph editor")
  parser.add_argument("terrain", nargs="?",
                      help="terrain DSL bare name, a .py path, an editor doc-JSON path, "
                           "or 'new' (minimal viable terrain)")
  parser.add_argument("--class", dest="class_name", default=None,
                      help="explicit HeightField subclass (multi-class DSL files)")
  parser.add_argument("--dim", "-d", type=int, default=1024,
                      help="preview bake resolution (W=H); default 1024")
  parser.add_argument("--full-dim", type=int, default=4096,
                      help="full-res bake resolution (toolbar toggle); default 4096")
  parser.add_argument("--chunk", "-c", type=int, default=128,
                      help="GPU cull chunk size in cells/side; default 128")
  parser.add_argument("-x", "--extent", type=float, default=None,
                      help="world XZ extent in meters (default: EXTENT_M from the DSL class); "
                           "heights are TRUE METERS — there is no vertical scale to override")
  parser.add_argument("--param", "-p", action="append", dest="params", default=[],
                      metavar="KEY=VALUE", help="DSL constructor kwarg (repeatable)")
  parser.add_argument("--list", "-l", action="store_true", help="list terrain DSL files and exit")
  parser.add_argument("--reset-layout", dest="reset_layout", action="store_true",
                      help="ignore any saved dock layout; open with the default arrangement")
  parser.add_argument("--uirecord", dest="uirecord", metavar="PATH", default=None,
                      help="record all UI input to PATH (ork.uitest session JSONL; "
                           "flushed at every gesture end, crash-safe)")
  args = parser.parse_args()

  if args.list or args.terrain is None:
    list_dsl_files()
    sys.exit(0)

  try:
    dsl_kwargs = dict(parse_param(s) for s in args.params)
  except ValueError as e:
    print(f"ork.terrain.edit: {e}", file=sys.stderr)
    sys.exit(2)

  app = TerrainEditor(args.terrain, dsl_class=args.class_name,
                      extent_m=args.extent,
                      preview_dim=args.dim, full_dim=args.full_dim, chunk=args.chunk,
                      dsl_kwargs=dsl_kwargs, reset_layout=args.reset_layout,
                      uirecord=args.uirecord)
  try:
    app.ezapp.mainThreadLoop()
  finally:
    app.stopUiRecord()
  app.ezapp.shutdown()


if __name__ == "__main__":
  main()
