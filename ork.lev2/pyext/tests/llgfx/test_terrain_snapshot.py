#!/usr/bin/env python3
###############################################################################
# JUL09 S1 DISPLAY GATE — terrain is VISIBLE via the ECS-hosted runtime (headless).
#
# The runtime no longer drives a raw Python chunk-display path (the parity copy that
# rotted the u_dim upload -> zero-vertex draw -> invisible terrain). Instead it builds
# a minimal in-code ECS scene from the DOCUMENT (document.elaborate() -> embedded
# HeightFieldGenData); the C++ terrain path bakes + renders NATIVELY.
#
# This gate proves the pixel observable the raw path silently lost:
#   * TerrainRuntime.export_scene_json (voronoi document) -> a player-ready .ecs.
#   * ork.ecs.player.exe --offscreen -S snap.png renders the settled frame.
#   * ork.vet.image.py --kind render asserts the frame is NON-BLACK / LIT
#     (frame.dynamic_range gate) — the machine proof of visibility, no window.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, shutil, tempfile, subprocess

from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.editor.terrain_runtime import TerrainRuntime

DIM = 512
EXTENT_M = 512.0     # ren_terrain's proven-visible scale (camdist 220 / camheight 90)
HEIGHT_M = 60.0
CAMDIST = 220.0
CAMHEIGHT = 90.0


def _vet_line(text, name):
  for ln in text.splitlines():
    parts = ln.split("\t")
    if len(parts) >= 4 and parts[0] == name:
      return ln, parts[3]
  return None, None


def main():
  workdir = tempfile.mkdtemp(prefix="tered_snap_")
  ecs_path = os.path.join(workdir, "voronoi_doc.ecs")
  snap_path = os.path.join(workdir, "snap.png")

  # ---- export the document's in-code ECS scene to JSON (the tojson flow) ----
  ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ez.mainThreadBegin()
  ctx = ez.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"

  # u_dim seam (the shared gpu_chunk helper for surviving raw-path consumers): upload_dim
  # lands the render dim at VIS_OFF+8; read_dim confirms the offset. u_dim=0 is the invisible
  # -terrain bug (cull derives nchunk from it). The C++ TerrainChunkDrawableData path (used by
  # this runtime's display) uploads it natively; this proves the Python helper mirror.
  from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource
  _vs = TerrainChunkVertexSource(dim=DIM, extent_m=EXTENT_M, height_m=HEIGHT_M, chunk=128)
  _ssbo = ctx.FXI.createShaderStorageBufferWithLength(_vs.TOTAL)
  _vs.upload_dim(ctx.FXI, _ssbo)
  udim_read = _vs.read_dim(ctx.FXI, _ssbo)
  udim_ok = (udim_read == DIM)
  print(f"[snap] u_dim seam: upload_dim({DIM}) -> read_dim={udim_read} @VIS_OFF+8 -> {udim_ok}", flush=True)

  rt = TerrainRuntime(preview_dim=DIM, chunk=128)
  rt.load("voronoi", extent_m=EXTENT_M, height_m=HEIGHT_M)
  rt.set_context(ctx)
  js = rt.export_scene_json(ecs_path, dim=DIM, simple_material=True)
  ez.mainThreadEnd()
  print(f"[snap] exported scene json: {len(js)} bytes -> {ecs_path}", flush=True)
  assert os.path.getsize(ecs_path) > 0, "empty scene json"

  # ---- render the scene offscreen via the C++ player -----------------------
  player = shutil.which("ork.ecs.player.exe")
  assert player, "ork.ecs.player.exe not on PATH"
  cmd = [player, ecs_path, "--offscreen", "-S", snap_path, "-F", "30",
         "--camdist", str(CAMDIST), "--camheight", str(CAMHEIGHT), "--frames", "600"]
  print(f"[snap] player: {' '.join(cmd)}", flush=True)
  try:
    pr = subprocess.run(cmd, timeout=110, capture_output=True, text=True)
  except subprocess.TimeoutExpired:
    print("[snap] player TIMEOUT (>110s)", flush=True)
    pr = None
  if pr is not None and pr.returncode != 0:
    print(f"[snap] player rc={pr.returncode}; stderr tail:\n{(pr.stderr or '')[-1500:]}", flush=True)

  snap_ok = os.path.exists(snap_path) and os.path.getsize(snap_path) > 0
  print(f"[snap] snapshot written: {snap_ok} ({snap_path})", flush=True)

  # ---- vet the snapshot: non-black / LIT -----------------------------------
  vet_ok = False
  verdict_line = "(no snapshot)"
  dynrange_line = "(no snapshot)"
  if snap_ok:
    vet = shutil.which("ork.vet.image.py")
    assert vet, "ork.vet.image.py not on PATH"
    # relax speckle/spike ceilings (a real lit terrain has legit high-freq detail);
    # the load-bearing visibility check is frame.dynamic_range (non-black / non-degenerate).
    vcmd = [vet, snap_path, "--kind", "render", "--max-highband", "1.0",
            "--max-spike", "1.0", "--min-range", "0.02"]
    vr = subprocess.run(vcmd, capture_output=True, text=True)
    out = vr.stdout or ""
    print("[snap] vet output:\n" + out, flush=True)
    dynrange_line, dyn_status = _vet_line(out, "frame.dynamic_range")
    verdict_line = next((ln for ln in out.splitlines() if ln.startswith("# verdict:")),
                        "(no verdict)")
    vet_ok = (dyn_status == "PASS")

  print(f"\n[snap] VERDICT: {verdict_line}", flush=True)
  print(f"[snap] non-black/LIT gate (frame.dynamic_range): {dynrange_line}", flush=True)

  ecs.headless_exit()
  ok = snap_ok and vet_ok and udim_ok
  print(f"\n=== terrain snapshot (display gate) {'PASSED' if ok else 'FAILED'} ===", flush=True)
  print(f"    u_dim_seam: {'ok' if udim_ok else 'FAIL'}", flush=True)
  print(f"    snapshot_written: {'ok' if snap_ok else 'FAIL'}", flush=True)
  print(f"    non_black_lit: {'ok' if vet_ok else 'FAIL'}", flush=True)
  sys.exit(0 if ok else 1)


main()
