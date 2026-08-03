#!/usr/bin/env ork.python
###############################################################################
# section_bake_player.py — O3 stage 3 GATE: the C++ PLAYER-PATH per-section
# texture-array bake, driven through the ONE PLAYBACK PATH (tojson -> player).
#
# Unlike section_array_bake.py (stage 2, in-process ComponentizedApplication with a
# Python-driven cold_wait), this exercises the SHIPPING path end to end:
#   ork.scene.tojson.py ren_section_bake -> a player-ready .ecs
#   ork.ecs.player.exe --offscreen -S snap.png : HypermeshSystem stages the
#     HypermeshDrawableData; hm_drawable.cpp's C++ driver detects stored mode, runs the
#     per-gid BAKE MAP (materials={0: AdobeStored, 1: TimberStored}) COLD in-frame (GPU
#     bake -> content-addressed PNG cache -> placeholder->rebind), or WARM-loads the
#     cache — with NO per-frame Python.
#
# ORACLE (fail-loud):
#   * COLD run: player logs "section_bake(C++): COLD" for BOTH capture targets, writes
#     the cache PNGs, and the SETTLED offscreen snapshot is LIT (non-black).
#   * PER-SECTION DISTINCTNESS: the baked ALBEDO atlas layer 0 (adobe, gid REST) and
#     layer 1 (timber, gid TOP) have DISTINCT mean colors — the ground-truth proof that
#     each gid section baked with ITS OWN material (adobe content into adobe layers,
#     timber into timber). The lit render (saved for eyeball) shows both on the cube.
#   * WARM run: player logs "section_bake(C++): WARM" (cache loaded, NO bake).
#   * CACHE INTEROP: dir template <assetcache>/ptex3d_capture/section/<hash>__r<res>__L<layers>/
#     with per-layer layer<NN>.png (the section_bake.py scheme; per-target dir).
#
# Determinism: the player prints the exact cache dirs it uses; the gate deletes ONLY
# those (surgical — never touches another gate's content-addressed cache) to force COLD.
#
#   run: MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1 ork.python section_bake_player.py
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, re, glob, shutil, subprocess, tempfile
import numpy as np

os.environ.setdefault("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "1")

SCENE      = "ren_section_bake"
RES        = 256
LAYERS     = 2
CAMDIST    = 4.0
CAMHEIGHT  = 2.0
FRAMES     = 900
SNAP_AFTER = 30           # -F: frames after first-lit (bake completes well within the async-waited settle)

_DIR_RE = re.compile(r"section_bake\(C\+\+\):\s+(COLD|WARM)\s+target<(\S+)>.*?([-<]>|<-)\s+(\S+)")


def _run_player(ecs, snap):
  """Run the offscreen player; return (returncode, stdout, {target: (mode, cache_dir)})."""
  player = shutil.which("ork.ecs.player.exe")
  assert player, "ork.ecs.player.exe not on PATH"
  cmd = [player, ecs, "--offscreen", "-S", snap, "-F", str(SNAP_AFTER), "--frames", str(FRAMES),
         "--camdist", str(CAMDIST), "--camheight", str(CAMHEIGHT)]
  try:
    pr = subprocess.run(cmd, timeout=150, capture_output=True, text=True)
  except subprocess.TimeoutExpired:
    print("[secbake] player TIMEOUT (>150s)", flush=True)
    return 1, "", {}
  out = (pr.stdout or "") + (pr.stderr or "")
  dirs = {}
  for m in _DIR_RE.finditer(out):
    mode, target, _arrow, path = m.group(1), m.group(2), m.group(3), m.group(4)
    dirs[target] = (mode, path)
  if pr.returncode != 0:
    print(f"[secbake] player rc={pr.returncode}; stderr tail:\n{(pr.stderr or '')[-1200:]}", flush=True)
  return pr.returncode, out, dirs


def _snapshot_lit(path):
  """The settled frame is non-black / lit (render honesty). Returns (ok, mean, mx)."""
  if not (os.path.exists(path) and os.path.getsize(path) > 0):
    return False, 0.0, 0
  from PIL import Image
  im = np.asarray(Image.open(path).convert("RGB")).astype(np.float32)
  return (im.mean() > 8.0 and int(im.max()) > 0), float(im.mean()), int(im.max())


def _lit(a):
  m = a.reshape(-1, 3)
  return m[m.mean(axis=1) > 8.0]


def _section_distinctness(albedo_dir):
  """The per-gid PROOF: baked albedo layer 0 (adobe) vs layer 1 (timber) have distinct mean colors.
  Returns (distance, mean0, mean1, litpx0, litpx1)."""
  from PIL import Image
  p0 = os.path.join(albedo_dir, "layer00.png")
  p1 = os.path.join(albedo_dir, "layer01.png")
  a0 = np.asarray(Image.open(p0).convert("RGB")).astype(np.float32)
  a1 = np.asarray(Image.open(p1).convert("RGB")).astype(np.float32)
  l0, l1 = _lit(a0), _lit(a1)
  m0 = l0.mean(axis=0) if len(l0) else np.zeros(3)
  m1 = l1.mean(axis=0) if len(l1) else np.zeros(3)
  return float(np.linalg.norm(m0 - m1)), m0, m1, len(l0), len(l1)


def main():
  workdir = tempfile.mkdtemp(prefix="secbake_")
  ecs = os.path.join(workdir, "secbake.ecs")

  # ---- tojson: serialize the scene to a player-ready .ecs ----
  tojson = shutil.which("ork.scene.tojson.py")
  assert tojson, "ork.scene.tojson.py not on PATH"
  tj = subprocess.run([tojson, "-i", SCENE, "-o", ecs], capture_output=True, text=True, timeout=180)
  assert os.path.exists(ecs) and os.path.getsize(ecs) > 0, \
      f"tojson produced no scene json (rc={tj.returncode}):\n{(tj.stderr or '')[-800:]}"
  print(f"[secbake] tojson -> {ecs} ({os.path.getsize(ecs)} bytes)", flush=True)

  # ---- run A: discover the exact cache dirs this scene uses (cold OR warm) ----
  rcA, _outA, dirsA = _run_player(ecs, os.path.join(workdir, "snapA.png"))
  assert dirsA, "run A produced no section_bake(C++) cache-dir log lines — driver not engaged"
  target_dirs = {t: d for t, (_m, d) in dirsA.items()}
  print(f"[secbake] cache dirs: {target_dirs}", flush=True)

  # ---- force COLD: delete ONLY this scene's own cache dirs (surgical) ----
  for d in set(target_dirs.values()):
    shutil.rmtree(d, ignore_errors=True)

  # ---- COLD run: GPU bake + cache write + lit render ----
  cold_snap = os.path.join(workdir, "secbake_cold.png")
  rcC, outC, dirsC = _run_player(ecs, cold_snap)
  cold_modes = {t: m for t, (m, _d) in dirsC.items()}
  cold_ok = (len(cold_modes) >= 2) and all(m == "COLD" for m in cold_modes.values())
  cold_lit, cmean, cmax = _snapshot_lit(cold_snap)
  # cache evidence: >= LAYERS*targets PNGs on disk under the r<res>__L<layers> dirs
  npngs = 0
  for d in set(dirsC.values()) if False else set(target_dirs.values()):
    npngs += len(glob.glob(os.path.join(d, "layer*.png")))
  cache_ok = npngs >= LAYERS * 2

  # ---- per-section distinctness: identify the ALBEDO dir (SectionAlbedo target) ----
  albedo_dir = target_dirs.get("SectionAlbedo")
  dist, m0, m1, lp0, lp1 = (0.0, np.zeros(3), np.zeros(3), 0, 0)
  if albedo_dir and os.path.isdir(albedo_dir):
    dist, m0, m1, lp0, lp1 = _section_distinctness(albedo_dir)
  distinct_ok = dist > 12.0     # adobe(warm tan) vs timber(gray-brown) mean-color distance

  # ---- WARM run: cache loaded, no bake ----
  warm_snap = os.path.join(workdir, "secbake_warm.png")
  rcW, outW, dirsW = _run_player(ecs, warm_snap)
  warm_modes = {t: m for t, (m, _d) in dirsW.items()}
  warm_ok = (len(warm_modes) >= 2) and all(m == "WARM" for m in warm_modes.values())
  warm_lit, wmean, _wmax = _snapshot_lit(warm_snap)

  # persist the eyeball snapshot next to the scene tree for a reviewer
  eyeball = os.environ.get("SECBAKE_OUT", "/tmp/section_bake_player.png")
  try:
    shutil.copyfile(cold_snap, eyeball)
  except Exception:
    pass

  print(f"[secbake] COLD modes={cold_modes} lit={cold_lit} mean={cmean:.1f} max={cmax} pngs={npngs}", flush=True)
  print(f"[secbake] DISTINCT albedo layer0(adobe) mean={m0.round(1).tolist()} litpx={lp0} | "
        f"layer1(timber) mean={m1.round(1).tolist()} litpx={lp1} | dist={dist:.1f}", flush=True)
  print(f"[secbake] WARM modes={warm_modes} lit={warm_lit} mean={wmean:.1f}", flush=True)
  print(f"[secbake] eyeball snapshot -> {eyeball}", flush=True)

  passed = (rcC == 0 and cold_ok and cold_lit and cache_ok and distinct_ok and
            rcW == 0 and warm_ok and warm_lit)
  print("SECTION_BAKE_PLAYER_RESULT=%s cold=%d cold_lit=%d cache_pngs=%d distinct=%d(%.1f) warm=%d warm_lit=%d"
        % ("PASS" if passed else "FAIL", int(cold_ok), int(cold_lit), npngs,
           int(distinct_ok), dist, int(warm_ok), int(warm_lit)), flush=True)
  shutil.rmtree(workdir, ignore_errors=True)
  sys.exit(0 if passed else 1)


if __name__ == "__main__":
  main()
