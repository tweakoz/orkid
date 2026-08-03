#!/usr/bin/env ork.python
###############################################################################
# section_normal_bake.py — O3 SectionNormal capture GATE (offscreen, player path).
#
# Proves the NEW third section-bake target (SectionNormal) is baked correctly: the
# stored materials (AdobeStored / TimberStored, schema on SectionArrayPBR) each
# declare self.capture("SectionNormal", ...) — the FAITHFUL TANGENT-SPACE shading
# normal (n*0.5+0.5) stored via transpose(ctx.tbn)*n (owner adjudication 2026-07-24:
# faithful/parity-safe, not albedo-relief). These materials author NO normal detail,
# so that faithful normal is the geometric normal -> a FLAT ~(128,128,255) layer.
#
# Drives the SHIPPING path (tojson -> ork.ecs.player.exe, HypermeshSystem,
# hm_drawable.cpp's N-target-generic bake), then reads the baked SectionNormal atlas
# LAYER PNGs off the content-addressed cache and runs the LIVE check (normal_faithful):
# baked (not-black), blue-dominant, mean near the flat encoded normal — variance NOT
# required (a faithful geometric normal is legitimately flat).
#
# The oracle carries its OWN negative self-test (run first) using the stricter
# CAPABILITY check (normal_alive): it MUST reject synthetic dead / flat-gray / flat-blue
# inputs, else detection is meaningless (negative-proof, committed). The flat-blue REJECT
# in the self-test and the flat-blue-appropriate PASS in the live check are intentional:
# capability vs material-appropriate expectation.
#
# NB the C++ bake path is target-count generic (targets = the sampler material's
# declared capture_targets); this gate adds NO C++ — it exercises the Python DSL +
# JIT shadlang codegen for the new capture. MIP levels of the normal layer are NOT
# renormalized yet (deferred C++ hook) — this gate reads mip-0 only.
#
#   run: MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1 ork.python section_normal_bake.py
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, re, glob, shutil, subprocess, tempfile
import numpy as np

os.environ.setdefault("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "1")

SCENE      = "ren_section_bake"
TARGET     = "SectionNormal"       # the capture target this gate proves
LAYERS     = 2
CAMDIST    = 4.0
CAMHEIGHT  = 2.0
FRAMES     = 600
SNAP_AFTER = 30

# oracle floors (0..255 scale). Two distinct checks (see the block comment on the oracle):
#   normal_alive  — the CAPABILITY check (self-test): requires STRUCTURE (std above a floor) so
#                   it can prove the oracle rejects dead/flat layers.
#   normal_faithful — the LIVE check for a material with NO authored normal detail: the faithful
#                   shading normal is the geometric normal -> a FLAT tangent normal ~(128,128,255).
#                   Variance is NOT required; assert baked + blue-dominant + mean near flat.
STD_FLOOR   = 2.0     # normal_alive only (capability)
BLUE_FLOOR  = 140.0
Z_FLOOR     = 235.0   # normal_faithful: mean blue near max (a flat +Z tangent normal)
XY_TOL      = 16.0    # normal_faithful: mean X,Y within this of 128 (encoded 0)
MIN_BAKEPX  = 50

_DIR_RE = re.compile(r"section_bake\(C\+\+\):\s+(COLD|WARM)\s+target<(\S+)>.*?([-<]>|<-)\s+(\S+)")


###############################################################################
# NORMAL oracle — TWO checks with different jobs:
#   normal_alive    = detection CAPABILITY. Requires structure (variance) so it can
#                     legitimately reject dead/flat layers. Used ONLY by the self-test
#                     below (which asserts it rejects zero / flat-gray / flat-blue and
#                     accepts a structured synthetic) — proving the oracle CAN detect death.
#   normal_faithful = the LIVE per-layer assertion for the materials under test, which
#                     author NO normal detail (owner adjudication 2026-07-24: SectionNormal
#                     stores the FAITHFUL shading normal, flat for adobe/timber/sectionarray).
#                     A faithful flat normal ~(128,128,255) legitimately has ~zero variance,
#                     so the live check does NOT require structure — it asserts baked +
#                     blue-dominant + mean near the flat encoded normal. (A flat-blue layer is
#                     thus a REJECT for normal_alive but the EXPECTED PASS for normal_faithful:
#                     capability vs material-appropriate expectation.)
###############################################################################
def normal_alive(rgb):
  """rgb: HxWx3 float [0..255]. Returns (ok, metrics). A layer is ALIVE iff it has baked
  texels, carries STRUCTURE (per-channel std above STD_FLOOR — a flat/constant normal has
  std 0), and is BLUE-DOMINANT (mean blue is the largest channel and >= BLUE_FLOOR, i.e. a
  proper +Z-pointing tangent normal). The atlas clears to black; unbaked texels are dropped."""
  m   = rgb.reshape(-1, 3)
  lit = m[m.max(axis=1) > 8.0]                       # drop the (0,0,0,0) atlas clear
  npx = int(len(lit))
  if npx < MIN_BAKEPX:
    return False, dict(reason="dead/empty (too few baked texels)", npx=npx)
  mean = lit.mean(axis=0)
  std  = lit.std(axis=0)
  var_ok   = float(std.mean()) > STD_FLOOR
  blue_dom = (mean[2] > mean[0]) and (mean[2] > mean[1]) and (mean[2] >= BLUE_FLOOR)
  ok = var_ok and blue_dom
  return ok, dict(npx=npx,
                  mean=[round(float(x), 1) for x in mean],
                  std=[round(float(x), 2) for x in std],
                  var_ok=bool(var_ok), blue_dom=bool(blue_dom))


def normal_faithful(rgb):
  """LIVE aliveness for a material with NO authored normal detail: assert the layer is BAKED
  (not black/zero), BLUE-DOMINANT (mean blue is the largest channel and >= BLUE_FLOOR) and its
  mean is near the FLAT encoded tangent normal ~(128,128,255) — X,Y within XY_TOL of 128, blue
  above Z_FLOOR. Variance is NOT required (a faithful geometric normal is legitimately flat).
  This is the material-appropriate expectation; contrast normal_alive (the capability check),
  which DOES require variance and so rejects a structureless flat-blue layer."""
  m   = rgb.reshape(-1, 3)
  lit = m[m.max(axis=1) > 8.0]                       # drop the (0,0,0,0) atlas clear
  npx = int(len(lit))
  if npx < MIN_BAKEPX:
    return False, dict(reason="dead/empty (too few baked texels)", npx=npx)
  mean = lit.mean(axis=0)
  std  = lit.std(axis=0)
  blue_dom  = (mean[2] > mean[0]) and (mean[2] > mean[1]) and (mean[2] >= BLUE_FLOOR)
  near_flat = (abs(mean[0] - 128.0) < XY_TOL) and (abs(mean[1] - 128.0) < XY_TOL) and (mean[2] > Z_FLOOR)
  ok = blue_dom and near_flat
  return ok, dict(npx=npx,
                  mean=[round(float(x), 1) for x in mean],
                  std=[round(float(x), 2) for x in std],
                  blue_dom=bool(blue_dom), near_flat=bool(near_flat))


def _oracle_negative_self_test():
  """Prove the oracle REJECTS dead/flat captures before trusting any PASS (committed
  negative-proof). Dead cases: all-zero (unbaked), flat-gray (0.5 everywhere — the
  broken/no-encode look), flat-blue (a correct flat normal, but NO structure). A live
  synthetic (structured blue) must PASS as the positive control."""
  H = 64
  zeros = np.zeros((H, H, 3), np.float32)
  gray  = np.full((H, H, 3), 128.0, np.float32)
  blue  = np.zeros((H, H, 3), np.float32); blue[..., :] = (128.0, 128.0, 255.0)
  rng   = np.random.default_rng(0)
  # live: small tangent-plane perturbations, blue channel high + varied
  live  = np.empty((H, H, 3), np.float32)
  live[..., 0] = 128.0 + rng.normal(0, 25, (H, H))
  live[..., 1] = 128.0 + rng.normal(0, 25, (H, H))
  live[..., 2] = 235.0 + rng.normal(0, 8,  (H, H))
  live = np.clip(live, 0, 255)
  cases = [("all-zero", zeros, False), ("flat-gray", gray, False),
           ("flat-blue", blue, False), ("live-synth", live, True)]
  allok = True
  for name, img, want in cases:
    ok, met = normal_alive(img)
    good = (ok == want)
    allok = allok and good
    print("  [selftest] %-10s oracle=%s expect=%s %s  %s"
          % (name, "ALIVE" if ok else "DEAD", "ALIVE" if want else "DEAD",
             "OK" if good else "MISMATCH", met), flush=True)
  return allok


###############################################################################
# player-path bake (mirrors section_bake_player.py's driver idiom; no shared-file edits)
###############################################################################
def _run_player(ecs, snap):
  player = shutil.which("ork.ecs.player.exe")
  assert player, "ork.ecs.player.exe not on PATH"
  cmd = [player, ecs, "--offscreen", "-S", snap, "-F", str(SNAP_AFTER), "--frames", str(FRAMES),
         "--camdist", str(CAMDIST), "--camheight", str(CAMHEIGHT)]
  try:
    pr = subprocess.run(cmd, timeout=150, capture_output=True, text=True)
  except subprocess.TimeoutExpired:
    print("[secnrm] player TIMEOUT (>150s)", flush=True)
    return 1, "", {}
  out = (pr.stdout or "") + (pr.stderr or "")
  dirs = {}
  for m in _DIR_RE.finditer(out):
    mode, target, _arrow, path = m.group(1), m.group(2), m.group(3), m.group(4)
    dirs[target] = (mode, path)
  if pr.returncode != 0:
    print(f"[secnrm] player rc={pr.returncode}; stderr tail:\n{(pr.stderr or '')[-1000:]}", flush=True)
  return pr.returncode, out, dirs


def main():
  print("[secnrm] oracle negative self-test:", flush=True)
  selftest_ok = _oracle_negative_self_test()
  if not selftest_ok:
    print("SECTION_NORMAL_RESULT=FAIL reason=oracle_self_test_broken", flush=True)
    sys.exit(1)

  workdir = tempfile.mkdtemp(prefix="secnrm_")
  ecs = os.path.join(workdir, "secnrm.ecs")

  tojson = shutil.which("ork.scene.tojson.py")
  assert tojson, "ork.scene.tojson.py not on PATH"
  tj = subprocess.run([tojson, "-i", SCENE, "-o", ecs], capture_output=True, text=True, timeout=180)
  assert os.path.exists(ecs) and os.path.getsize(ecs) > 0, \
      f"tojson produced no scene json (rc={tj.returncode}):\n{(tj.stderr or '')[-800:]}"
  print(f"[secnrm] tojson -> {ecs} ({os.path.getsize(ecs)} bytes)", flush=True)

  # run A: discover this scene's cache dirs (cold or warm)
  rcA, _outA, dirsA = _run_player(ecs, os.path.join(workdir, "snapA.png"))
  assert dirsA, "run A produced no section_bake(C++) cache-dir log lines — driver not engaged"
  assert TARGET in dirsA, ("driver logged no target<%s> — the sampler material did not declare the "
                           "SectionNormal capture (targets=%r)" % (TARGET, list(dirsA)))
  target_dirs = {t: d for t, (_m, d) in dirsA.items()}
  print(f"[secnrm] cache dirs: {target_dirs}", flush=True)

  # force COLD: delete ONLY this scene's own cache dirs (surgical — never wipe other caches)
  for d in set(target_dirs.values()):
    shutil.rmtree(d, ignore_errors=True)

  # COLD run: fresh GPU bake of all targets incl. SectionNormal
  rcC, _outC, dirsC = _run_player(ecs, os.path.join(workdir, "secnrm_cold.png"))
  cold_modes = {t: m for t, (m, _d) in dirsC.items()}
  cold_ok = (TARGET in cold_modes) and (cold_modes.get(TARGET) == "COLD")

  normal_dir = {t: d for t, (_m, d) in dirsC.items()}.get(TARGET)
  results = []
  outdir = os.environ.get("SECNRM_OUT", "/tmp")
  saved = []
  if normal_dir and os.path.isdir(normal_dir):
    from PIL import Image
    for L in range(LAYERS):
      p = os.path.join(normal_dir, "layer%02d.png" % L)
      if not os.path.isfile(p):
        results.append((L, False, dict(reason="missing PNG %s" % p)))
        continue
      rgb = np.asarray(Image.open(p).convert("RGB")).astype(np.float32)
      ok, met = normal_faithful(rgb)          # LIVE check: material-appropriate (faithful-flat OK)
      results.append((L, ok, met))
      dst = os.path.join(outdir, "section_normal_layer%02d.png" % L)
      try:
        shutil.copyfile(p, dst); saved.append(dst)
      except Exception:
        pass
  layers_ok = bool(results) and all(ok for (_L, ok, _m) in results)

  for (L, ok, met) in results:
    print("[secnrm] layer%02d %-5s %s" % (L, "ALIVE" if ok else "DEAD", met), flush=True)
  if saved:
    print("[secnrm] baked SectionNormal layers saved -> %s" % saved, flush=True)

  passed = (rcC == 0 and cold_ok and layers_ok and selftest_ok)
  print("SECTION_NORMAL_RESULT=%s selftest=%d cold=%d layers_alive=%d(%d/%d) target=%s"
        % ("PASS" if passed else "FAIL", int(selftest_ok), int(cold_ok),
           int(layers_ok), sum(1 for (_L, ok, _m) in results if ok), len(results), TARGET),
        flush=True)
  shutil.rmtree(workdir, ignore_errors=True)
  sys.exit(0 if passed else 1)


if __name__ == "__main__":
  main()
