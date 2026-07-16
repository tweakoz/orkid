#!/usr/bin/env python3
###############################################################################
# JUL09 S1 GATE — editor two-thread rebuild + DISPLAY, OFFSCREEN (no window).
#
# Runs the ACTUAL editor app class offscreen (createEzApp(offscreen=True) — a real
# two-thread ezapp, hidden window), captures the viewport framebuffer AFTER settle
# (round 0) and AFTER EACH rebuild round, and vets every capture NON-BLACK / LIT.
#
# This closes the earlier liveness-only gap: a rebuild that swaps to a live sim but
# renders a BLANK viewport (camera/bake/rebind regression) FAILS here. round 0 proves
# the initial terrain draws; rounds 1..N prove it STILL draws after each rebuild.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, shutil, subprocess

from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.editor.terrainedit import TerrainEditor


def _vet(path):
  vet = shutil.which("ork.vet.image.py")
  if not vet:
    return None, "(ork.vet.image.py not on PATH)"
  vcmd = [vet, path, "--kind", "render", "--max-highband", "1.0",
          "--max-spike", "1.0", "--min-range", "0.02"]
  out = subprocess.run(vcmd, capture_output=True, text=True).stdout or ""
  dyn = next((ln for ln in out.splitlines()
              if ln.split("\t")[0] == "frame.dynamic_range"), "(no line)")
  status = dyn.split("\t")[3] if len(dyn.split("\t")) >= 4 else "?"
  return (status == "PASS"), dyn


def main():
  app = TerrainEditor("voronoi", extent_m=512.0, dsl_kwargs={"amplitude": 60.0},
                      preview_dim=256, chunk=128, selftest=True)
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()

  caps = getattr(app, "_captures", [])
  print(f"\n[offscreen] {len(caps)} framebuffer captures:", flush=True)
  all_lit = bool(caps)
  for c in caps:
    tag = "initial" if c["round"] == 0 else f"post-rebuild #{c['round']}"
    vet_ok, dyn = _vet(c["path"])
    lit = c["lit"] and (vet_ok is not False)
    all_lit = all_lit and lit
    print(f"  round {c['round']} ({tag}): inline_range={c['rng']:.4f} lit={c['lit']}  vet[{dyn}]", flush=True)

  fsm_ok = bool(getattr(app, "_selftest_ok", False))
  ok = fsm_ok and all_lit and len(caps) == 7     # round 0 + 3x (during-hold + post-rebuild)
  print(f"\n=== offscreen editor two-thread DISPLAY gate {'PASSED' if ok else 'FAILED'} ===", flush=True)
  print(f"    fsm_completed: {'ok' if fsm_ok else 'FAIL'}", flush=True)
  print(f"    all_frames_lit: {'ok' if all_lit else 'FAIL'}", flush=True)
  sys.exit(0 if ok else 1)


main()
