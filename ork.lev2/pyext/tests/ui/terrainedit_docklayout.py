#!/usr/bin/env ork.python
################################################################################
# D1 gate: TerrainEditor DockSpace adoption — REAL editor, offscreen.
#
#  Boots the actual TerrainEditor (voronoi, preview 256) and asserts, in-process:
#    roundtrip : default layout signature + names + save-twice determinism;
#                a real scramble (moveChild) then load_layout -> signature + JSON
#                round-trip; a live Shift+L reset (injected chord) restores default;
#                an injected titlebar drag on a real panel re-docks (moveChild);
#                the composited frame is LIT (viewport renders).
#    probe     : with a SCRAMBLED layout written to the session slot, a fresh boot
#                RESTORES it (session persistence).
#    probe_reset: --reset-layout ignores the saved session -> default arrangement.
#
#  All sub-runs share this script's argv[0], hence the SAME app_state session slot
#  (keyed by app name + entry path). Structural oracles (signature / JSON / names)
#  are the parity metric here — byte parity of the LAYOUT is covered by
#  terrainedit_layout_parity.py (the live 3D viewport content is not deterministic).
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse
import subprocess

TERR_KW = dict(extent_m=512.0, dsl_kwargs={"amplitude": 60.0}, preview_dim=256, chunk=128)

################################################################################

def _boot(**kwargs):
  from orkengine import core, lev2, ecs   # core before lev2
  from ork.editor.terrainedit import TerrainEditor
  app = TerrainEditor("voronoi", **TERR_KW, **kwargs)
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  return app

def run_mode(mode, extra=None):
  from ork.ui.app_state import dock_layout_path
  if mode == "roundtrip":
    app = _boot(layouttest=True)
    r = app._layouttest_results
    print(f"RESULT_OK={bool(r.get('ok'))}", flush=True)
    print(f"DEFAULT_JSON={r.get('json0')}", flush=True)
    print(f"SCRAMBLED_JSON={r.get('scrambled_json')}", flush=True)
    print(f"SCRAMBLED_SIG={r.get('sig_scrambled')}", flush=True)
    print(f"DEFAULT_SIG={r.get('sig0')}", flush=True)
  elif mode == "probe":
    _boot(layout_probe=True)
  elif mode == "probe_reset":
    _boot(layout_probe=True, reset_layout=True)
  elif mode == "writescram":
    p = dock_layout_path("terrainedit")
    with open(p, "w") as f:
      f.write(extra)
    print(f"WROTE_SESSION {p}", flush=True)
  elif mode == "slot":
    print(f"SLOT {dock_layout_path('terrainedit')}", flush=True)
  sys.exit(0)

################################################################################

def _run(args, capture=True):
  sp = os.path.abspath(__file__)
  r = subprocess.run([sp] + args, capture_output=capture, text=True)
  if capture:
    sys.stdout.write(r.stdout); sys.stderr.write(r.stderr)
    return r.returncode, r.stdout
  return r.returncode, ""

def _val(out, key):
  for line in out.splitlines():
    if line.startswith(key + "="):
      return line[len(key) + 1:]
  return None

def orchestrate():
  problems = []

  # locate + clear this script's session slot so 'roundtrip' boots on the DEFAULT
  _, slot_out = _run(["--mode", "slot"])
  slot = None
  for line in slot_out.splitlines():
    if line.startswith("SLOT "):
      slot = line[5:].strip()
  if slot and os.path.exists(slot):
    os.remove(slot)

  # 1) roundtrip in the real editor
  rc, out = _run(["--mode", "roundtrip"])
  if rc != 0:
    problems.append(f"roundtrip run failed (rc={rc})")
  if _val(out, "RESULT_OK") != "True":
    problems.append("roundtrip layouttest verdict not OK "
                    "(default/save/scramble/load/shiftl/drag/liveness)")
  default_json  = _val(out, "DEFAULT_JSON")
  scrambled_json = _val(out, "SCRAMBLED_JSON")
  scrambled_sig  = _val(out, "SCRAMBLED_SIG")
  default_sig    = _val(out, "DEFAULT_SIG")
  if not scrambled_json or scrambled_json == default_json:
    problems.append("no distinct scrambled layout produced")

  # 2) session restore: write the scrambled layout, boot -> must restore it
  if scrambled_json:
    _run(["--mode", "writescram", scrambled_json])
    rc, out = _run(["--mode", "probe"])
    if rc != 0:
      problems.append(f"restore probe failed (rc={rc})")
    if _val(out, "PROBE_SIG") != scrambled_sig:
      problems.append(f"session NOT restored: PROBE_SIG={_val(out,'PROBE_SIG')} "
                      f"!= scrambled {scrambled_sig}")
    if _val(out, "PROBE_VALID") != "True":
      problems.append("restore probe validateTree not clean")

    # 3) --reset-layout ignores the saved session -> default
    rc, out = _run(["--mode", "probe_reset"])
    if rc != 0:
      problems.append(f"reset-layout probe failed (rc={rc})")
    ps = _val(out, "PROBE_SIG")
    if ps != default_sig:
      problems.append(f"--reset-layout did not restore default: {ps} != {default_sig}")
    if ps == scrambled_sig:
      problems.append("--reset-layout wrongly restored the saved (scrambled) session")

  # cleanup the slot we created
  if slot and os.path.exists(slot):
    os.remove(slot)

  if problems:
    print("=== TerrainEditor dock-layout gate FAILED ===", flush=True)
    for p in problems:
      print("  - " + p, flush=True)
    sys.exit(1)
  print("=== TerrainEditor dock-layout gate PASSED ===", flush=True)
  sys.exit(0)

################################################################################

def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--mode",
                  choices=["roundtrip", "probe", "probe_reset", "writescram", "slot"],
                  default=None)
  ap.add_argument("extra", nargs="?", default=None)
  args = ap.parse_args()
  if args.mode:
    run_mode(args.mode, args.extra)
  orchestrate()

if __name__ == "__main__":
  main()
