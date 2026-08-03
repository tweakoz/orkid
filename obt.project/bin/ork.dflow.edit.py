#!/usr/bin/env ork.python

################################################################################
# ork.dflow.edit.py — the standalone dataflow graph editor (JUL13_DFLOW E3).
#
# Loads ANY dflow graph family into one family-neutral editor shell (dock + canvas +
# propsheet + placeholder viewport), the family auto-detected from the source content:
#
#   ork.dflow.edit.py <graph.orj>        # a GraphData reflection-JSON graph (particles)
#   ork.dflow.edit.py <terrain-name>     # a terrain DSL asset (resolved on the search path)
#   ork.dflow.edit.py path/to/terrain.py # a terrain DSL .py file
#
# MULTI-DOCUMENT: pass N sources to open them SIMULTANEOUSLY — one canvas tab per source,
# ONE composed viewport (the first payload-bearing source creates the scene/sim; each
# subsequent one is folded into the SAME world):
#
#   ork.dflow.edit.py xxx erox           # two terrains composed into one viewport
#   ork.dflow.edit.py xxx fireball       # terrain viewport + a RUNNING fire folded over it
#
# --offscreen / --selftest boot the shell headless for the gates. The editor VIEWS +
# EDITS the document (select -> propsheet, move/persist node positions, bypass/display
# flags where the family supports them) AND hosts live viewport payloads: terrain (live
# bake), hypermesh (live mesh), and particles (a RUNNING, transport-gated particle system).
# A non-particles doc-only .orj GraphData keeps the placeholder viewport.
#
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import argparse
import glob
import os
import sys

# Prefer THIS repo's obt.project/scripts (so a worktree/lane run shadows the env's
# default scripts dir): the launcher lives in <repo>/obt.project/bin, its sibling
# scripts dir is <repo>/obt.project/scripts. For the mainline this is a no-op re-order.
_SCRIPTS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                         "..", "scripts"))
if _SCRIPTS not in sys.path[:1]:
  sys.path.insert(0, _SCRIPTS)

# import order (gotcha): core before lev2 so lev2's pybind init sees core's types.
from orkengine import core  # noqa: F401
from orkengine import lev2  # noqa: F401

from ork.editor.dflowedit import DflowEditor


def _print_columns(names, cols=4):
  names = sorted(names)
  if not names:
    print("  (none found)")
    return
  width = max(len(n) for n in names) + 2
  for i in range(0, len(names), cols):
    print("  " + "".join(n.ljust(width) for n in names[i:i + cols]).rstrip())


def _dsl_names(search_dirs):
  names = set()
  for d in search_dirs:
    for p in glob.glob(os.path.join(str(d), "*.py")):
      stem = os.path.splitext(os.path.basename(p))[0]
      if not stem.startswith("_"):
        names.add(stem)
  return names


def list_sources():
  from ork.hypergraph.dflow.terrain import resolve as terrain_resolve
  from ork.hypergraph.dflow.particles import resolve as particles_resolve
  from ork.hypergraph.assets.hypermesh import _resolve as hypermesh_resolve
  print("terrain DSL assets:")
  _print_columns(_dsl_names(terrain_resolve.search_path()))
  print("\nparticles DSL assets:")
  _print_columns(_dsl_names(particles_resolve.search_path()))
  print("\nhypermesh assets (graph + live mesh viewport):")
  _print_columns(hypermesh_resolve.list_asset_names())
  # .orj reflection-JSON graphs still OPEN by path — just not enumerated here (owner pref).


def main(argv):
  parser = argparse.ArgumentParser(
      description="standalone dataflow graph editor (any dflow family)")
  # nargs='*' (not '+') so the no-arg default still lists sources (an explicit --list-equiv);
  # one or more sources open simultaneously (multi-document, one composed viewport).
  parser.add_argument("source", nargs="*",
                      help="one or more sources: a .orj reflection-JSON graph, or a terrain "
                           "DSL name / .py path (multiple = multi-document, one viewport)")
  parser.add_argument("--list", "-l", action="store_true",
                      help="list editable sources and exit (also the no-arg default)")
  # OVERRIDES — mirror the sibling viewers' value forms. -e/--envmap is the tool-family standard
  # (ork.ecsplay / ork.particle.viewer / ork.modelviewer): a shortname under <assetcache>/envmaps2/
  # (resolved to the <ork_envmaps2>/<name>.xir house form) OR an explicit <bracketed>/absolute path.
  # --material mirrors ork.hypermesh.viewer.py's [M] cycle MODE names (that tool has no CLI flag —
  # it cycles on the [M] key; dflowedit surfaces the same modes as a one-shot override). Both apply
  # to every payload binding (envmap -> terrain + hypermesh; material -> hypermesh only).
  parser.add_argument("-e", "--envmap", default="",
                      help="envmap override: a shortname under <assetcache>/envmaps2/ (e.g. "
                           "blender_sunset) or an explicit <bracketed>/absolute .xir path; "
                           "empty = each family's default")
  parser.add_argument("--material", default="",
                      help="hypermesh material override mode (mirrors ork.hypermesh.viewer.py's [M] "
                           "cycle): asset / white / mirror / mirror2 / x3 / groups / faces")
  parser.add_argument("--offscreen", action="store_true",
                      help="boot headless (no window) — for gates / CI")
  parser.add_argument("--selftest", action="store_true",
                      help="boot offscreen: canvas model binds + viewport non-black + ECS "
                           "transport (start/pause/stop) advances/freezes; verdict; exit "
                           "(multi-source also exercises tab-switch propsheet rebind + compose)")
  parser.add_argument("--flagtest", action="store_true",
                      help="boot offscreen: a display-flag switch changes viewport pixels + a "
                           "bypass triggers a rebake (rebuild counter); verdict; exit")
  parser.add_argument("--no-bench", action="store_true",
                      help="ignore any asset TESTBENCH (editor-only stimulus) — open the exact "
                           "pre-bench graph (the static production instantiation)")
  parser.add_argument("--reset-layout", dest="reset_layout", action="store_true",
                      help="ignore any saved dock layout; open with the default arrangement")
  parser.add_argument("--uirecord", dest="uirecord", metavar="PATH", default=None,
                      help="record all UI input to PATH (ork.uitest session JSONL; "
                           "flushed at every gesture end, crash-safe)")
  parser.add_argument("--uiplay", dest="uiplay", metavar="PATH", default=None,
                      help="replay a recorded UI session from PATH into this editor "
                           "(frame-locked; secondary-window-tagged events skipped)")
  parser.add_argument("--uiplay-exit", dest="uiplay_exit", action="store_true",
                      help="exit shortly after --uiplay replay completes (headless runs)")
  args = parser.parse_args(argv)

  if args.list or not args.source:
    list_sources()
    return 0

  try:
    app = DflowEditor(args.source, offscreen=args.offscreen, selftest=args.selftest,
                      flagtest=args.flagtest, envmap=args.envmap, material=args.material,
                      benches=not args.no_bench, reset_layout=args.reset_layout,
                      uirecord=args.uirecord,
                      uiplay=args.uiplay, uiplay_exit=args.uiplay_exit)
  except (FileNotFoundError, ValueError) as ex:     # bogus envmap / material -> loud shell refusal
    print(f"ork.dflow.edit: {ex}", file=sys.stderr)
    return 2
  try:
    app.ezapp.mainThreadLoop()
  finally:
    app.stopUiRecord()
  app.ezapp.shutdown()          # clean GPU + ECS teardown (headless lifecycle)
  return 0


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
