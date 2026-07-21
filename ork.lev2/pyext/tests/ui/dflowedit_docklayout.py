#!/usr/bin/env ork.python
################################################################################
# D2 gate: DflowEditor DockSpace adoption — REAL editor, offscreen.
#
#  Boots the actual DflowEditor (voronoi source) and asserts, in-process:
#    roundtrip : default layout signature + names + save-twice determinism;
#                a real scramble (moveChild) then load_layout -> signature + JSON
#                round-trip; a live Shift+L reset (injected chord) restores default;
#                an injected titlebar drag on a real panel re-docks (moveChild).
#    probe     : with a SCRAMBLED layout written to the session slot, a fresh boot
#                RESTORES it (session persistence).
#    probe_reset: --reset-layout ignores the saved session -> default arrangement.
#
#  All sub-runs share this script's argv[0], hence the SAME app_state session slot
#  (app name "dflowedit"). Structural oracles (signature / JSON / names) are the
#  parity metric here — byte parity of the LAYOUT is covered by
#  dflowedit_layout_parity.py (the live viewport content is not deterministic).
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse
import subprocess

SRC = ["voronoi"]

################################################################################

def _boot(**kwargs):
  from orkengine import core, lev2, ecs   # core before lev2
  from ork.editor.dflowedit import DflowEditor
  app = DflowEditor(SRC, **kwargs)
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
    p = dock_layout_path("dflowedit")
    with open(p, "w") as f:
      f.write(extra)
    print(f"WROTE_SESSION {p}", flush=True)
  elif mode == "slot":
    print(f"SLOT {dock_layout_path('dflowedit')}", flush=True)
  sys.exit(0)

################################################################################

def _run(args):
  sp = os.path.abspath(__file__)
  r = subprocess.run([sp] + args, capture_output=True, text=True)
  sys.stdout.write(r.stdout); sys.stderr.write(r.stderr)
  return r.returncode, r.stdout

def _val(out, key):
  for line in out.splitlines():
    if line.startswith(key + "="):
      return line[len(key) + 1:]
  return None

def orchestrate():
  problems = []

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
                    "(default/save/scramble/load/shiftl/drag)")
  default_json   = _val(out, "DEFAULT_JSON")
  scrambled_json = _val(out, "SCRAMBLED_JSON")
  scrambled_sig  = _val(out, "SCRAMBLED_SIG")
  default_sig    = _val(out, "DEFAULT_SIG")
  if not scrambled_json or scrambled_json == default_json:
    problems.append("no distinct scrambled layout produced")

  # 2) session restore + 3) --reset-layout override
  if scrambled_json:
    _run(["--mode", "writescram", scrambled_json])
    rc, out = _run(["--mode", "probe"])
    if rc != 0:
      problems.append(f"restore probe failed (rc={rc})")
    if _val(out, "PROBE_SIG") != scrambled_sig:
      problems.append(f"session NOT restored: {_val(out,'PROBE_SIG')} != {scrambled_sig}")
    if _val(out, "PROBE_VALID") != "True":
      problems.append("restore probe validateTree not clean")

    rc, out = _run(["--mode", "probe_reset"])
    if rc != 0:
      problems.append(f"reset-layout probe failed (rc={rc})")
    ps = _val(out, "PROBE_SIG")
    if ps != default_sig:
      problems.append(f"--reset-layout did not restore default: {ps} != {default_sig}")
    if ps == scrambled_sig:
      problems.append("--reset-layout wrongly restored the saved session")

  if slot and os.path.exists(slot):
    os.remove(slot)

  if problems:
    print("=== DflowEditor dock-layout gate FAILED ===", flush=True)
    for p in problems:
      print("  - " + p, flush=True)
    sys.exit(1)
  print("=== DflowEditor dock-layout gate PASSED ===", flush=True)
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
