#!/usr/bin/env ork.python
###############################################################################
# section_bake_player_instanced.py — O3 stage 3 GATE, INSTANCED: the C++ PLAYER-
# PATH per-section texture-array bake driven through HypermeshDrawableData WITH
# instance matrices. This is the FIRST coverage of bindSectionArrays (hm_drawable.cpp)
# under instancing — the flagged gap the non-instanced section_bake_player.py never
# exercised (and the section_array_bake_instanced.py gate uses a SEPARATE make_drawable
# path that never touches HypermeshDrawableData).
#
# Path: ren_section_bake.py (REN_SECTION_BAKE_INSTANCED=1) authors the stored sampler
# instanced + N distinct instance matrices -> tojson -> ork.ecs.player.exe --offscreen.
# HypermeshSystem stages the HypermeshDrawableData; the C++ driver detects stored mode
# AND instanced (inst_count>1), binds the section arrays onto the (instanced) sampler
# material across the WARM / COLD-placeholder / COLD-rebind call sites.
#
# ORACLE (fail-loud):
#   * COLD run: player logs "section_bake(C++): COLD" for BOTH targets; the SETTLED
#     offscreen snapshot is LIT (non-black).
#   * MULTIPLICITY: N_INSTANCES horizontally-separated lit clusters in the snapshot (a
#     collapsed / single-technique / zeroed-matrix draw shows 1 — negative-proofed below).
#   * PER-SECTION DISTINCTNESS: baked ALBEDO layer 0 (adobe, gid REST) vs layer 1 (timber,
#     gid TOP) have distinct mean colors (the bake is non-instanced, so identical to the
#     non-instanced gate's cache — proves the array CONTENT survives the instanced draw).
#   * WARM run: player logs "section_bake(C++): WARM".
#   * NEGATIVE PROOF: a second tojson with REN_SECTION_BAKE_INST_COLLAPSE=1 packs every
#     instance at the origin -> the multiplicity oracle MUST report < N (FAIL) on that
#     snapshot; if it does not, the oracle is untrustworthy and the whole gate FAILS.
#
#   run: MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1 ork.python section_bake_player_instanced.py
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, glob, shutil, subprocess, tempfile
import numpy as np

os.environ.setdefault("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "1")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import section_bake_player as base   # reuse _DIR_RE (COLD/WARM log parse) + _section_distinctness (cache PNGs)

SCENE       = "ren_section_bake"
LAYERS      = 2
N_INSTANCES = 3           # must match ren_section_bake.py N_INSTANCES
CAMDIST     = 8.0         # wider than the single-cube gate: fit the 3-instance world-X row
CAMHEIGHT   = 4.0
FRAMES      = 900
SNAP_AFTER  = 30


def _hclusters(mask, min_gap=4, min_cols=3):
  """Horizontally-separated lit-pixel CLUSTERS (the instance-multiplicity oracle; no scipy).
  Per-column occupancy, then contiguous occupied runs separated by >= min_gap empty columns
  (bridges sub-min_gap anti-alias gaps), keeping runs spanning >= min_cols columns. N separated
  cubes -> N clusters; overlapping/collapsed instances -> 1. Adapted from section_array_bake.py."""
  occ = mask.any(axis=0)
  runs, start, gap = [], None, 0
  for i, c in enumerate(occ):
    if c:
      if start is None:
        start = i
      end = i
      gap = 0
    else:
      if start is not None:
        gap += 1
        if gap >= min_gap:
          runs.append((start, end))
          start = None
  if start is not None:
    runs.append((start, end))
  return [(a, b) for (a, b) in runs if (b - a + 1) >= min_cols]


def _tojson(workdir, name, extra_env):
  """Serialize the scene to a player-ready .ecs with extra_env set for construction."""
  ecs = os.path.join(workdir, name)
  tojson = shutil.which("ork.scene.tojson.py")
  assert tojson, "ork.scene.tojson.py not on PATH"
  env = dict(os.environ); env.update(extra_env)
  tj = subprocess.run([tojson, "-i", SCENE, "-o", ecs], capture_output=True, text=True,
                      timeout=180, env=env)
  assert os.path.exists(ecs) and os.path.getsize(ecs) > 0, \
      f"tojson({name}) produced no scene json (rc={tj.returncode}):\n{(tj.stderr or '')[-800:]}"
  return ecs


def _run_player(ecs, snap):
  """Offscreen player at the wider instanced camera; return (rc, stdout, {target:(mode,dir)})."""
  player = shutil.which("ork.ecs.player.exe")
  assert player, "ork.ecs.player.exe not on PATH"
  cmd = [player, ecs, "--offscreen", "-S", snap, "-F", str(SNAP_AFTER), "--frames", str(FRAMES),
         "--camdist", str(CAMDIST), "--camheight", str(CAMHEIGHT)]
  try:
    pr = subprocess.run(cmd, timeout=200, capture_output=True, text=True)
  except subprocess.TimeoutExpired:
    print("[secbake-inst] player TIMEOUT (>200s)", flush=True)
    return 1, "", {}
  out = (pr.stdout or "") + (pr.stderr or "")
  dirs = {}
  for m in base._DIR_RE.finditer(out):
    dirs[m.group(2)] = (m.group(1), m.group(4))
  if pr.returncode != 0:
    print(f"[secbake-inst] player rc={pr.returncode}; stderr tail:\n{(pr.stderr or '')[-1200:]}", flush=True)
  return pr.returncode, out, dirs


def _analyze(path):
  """Load a snapshot (black-bg, IBL-lit cubes) and characterize the CUBE pixels. Returns a dict:
    clusters : # horizontally-separated lit clusters (the instance-multiplicity oracle)
    litpx    : # lit (mask) pixels
    mx       : brightest channel value
    lit      : cube-aware lit verdict (max floor + a real lit-pixel population) — a whole-frame
               mean is USELESS here (black bg dominates), so we gate on the CUBES.
    warm     : mean(R)-mean(B) over cube pixels — the baked adobe/timber sections are WARM (R>B);
               a gray placeholder (bindSectionArrays no-op) would read ~0. The render-content proof.
    spread   : std of (R-B) over cube pixels — section-to-section color variation on the drawn mesh
               (uniform placeholder gray -> ~0). Second render-content discriminator."""
  d = dict(clusters=0, litpx=0, mx=0, lit=False, warm=0.0, spread=0.0)
  if not (os.path.exists(path) and os.path.getsize(path) > 0):
    return d
  from PIL import Image
  rgb  = np.asarray(Image.open(path).convert("RGB")).astype(np.float32)
  mask = rgb.mean(axis=2) > 8.0
  d["clusters"] = len(_hclusters(mask))
  d["litpx"]    = int(mask.sum())
  d["mx"]       = int(rgb.max())
  d["lit"]      = (d["mx"] > 32) and (d["litpx"] > 1500)   # cubes present + brightly lit
  cube = rgb[mask]
  if len(cube) > 100:
    rb          = cube[:, 0] - cube[:, 2]
    d["warm"]   = float(rb.mean())
    d["spread"] = float(rb.std())
  return d


def main():
  workdir = tempfile.mkdtemp(prefix="secbake_inst_")

  # ---- tojson: the INSTANCED scene ----
  ecs = _tojson(workdir, "secbake_inst.ecs", {"REN_SECTION_BAKE_INSTANCED": "1"})
  print(f"[secbake-inst] tojson -> {ecs} ({os.path.getsize(ecs)} bytes)", flush=True)

  # ---- run A: discover the cache dirs this scene uses ----
  rcA, _outA, dirsA = _run_player(ecs, os.path.join(workdir, "snapA.png"))
  assert dirsA, "run A produced no section_bake(C++) cache-dir log lines — instanced driver not engaged"
  target_dirs = {t: d for t, (_m, d) in dirsA.items()}
  print(f"[secbake-inst] cache dirs: {target_dirs}", flush=True)

  # ---- force COLD: delete ONLY this scene's own cache dirs (surgical) ----
  for d in set(target_dirs.values()):
    shutil.rmtree(d, ignore_errors=True)

  # ---- COLD run: GPU bake + placeholder->rebind UNDER INSTANCING + lit render ----
  cold_snap = os.path.join(workdir, "secbake_inst_cold.png")
  rcC, _outC, dirsC = _run_player(ecs, cold_snap)
  cold_modes = {t: m for t, (m, _d) in dirsC.items()}
  cold_ok = (len(cold_modes) >= LAYERS) and all(m == "COLD" for m in cold_modes.values())
  a = _analyze(cold_snap)
  cold_lit = a["lit"]
  mult_ok  = a["clusters"] >= N_INSTANCES
  # RENDER-CONTENT proof: the drawn INSTANCED cubes actually sample the baked warm section arrays
  # (adobe/timber, R>B) — NOT a gray placeholder that a broken instanced bindSectionArrays would leave.
  # warm (mean R-B) is the real non-placeholder discriminator (gray placeholder = R-B≈0); spread (its std)
  # just proves grain variation. spread was 6.0 when the bake atlas was v-flipped vs the sampler read (the
  # hm.section.v3 fix): faces landing in the black gutter inflated the variance. Correct (gutter-free) content
  # measures spread≈4.7, so the floor is 3.0 — comfortably above a flat placeholder, below correct content.
  content_ok = a["warm"] > 8.0 and a["spread"] > 3.0
  npngs = 0
  for d in set(target_dirs.values()):
    npngs += len(glob.glob(os.path.join(d, "layer*.png")))
  cache_ok = npngs >= LAYERS * 2

  # ---- per-section distinctness (from the ALBEDO cache dir) ----
  albedo_dir = target_dirs.get("SectionAlbedo")
  dist, m0, m1, lp0, lp1 = (0.0, np.zeros(3), np.zeros(3), 0, 0)
  if albedo_dir and os.path.isdir(albedo_dir):
    dist, m0, m1, lp0, lp1 = base._section_distinctness(albedo_dir)
  distinct_ok = dist > 12.0

  # ---- WARM run: cache loaded, bind-under-instancing warm path ----
  warm_snap = os.path.join(workdir, "secbake_inst_warm.png")
  rcW, _outW, dirsW = _run_player(ecs, warm_snap)
  warm_modes = {t: m for t, (m, _d) in dirsW.items()}
  warm_ok = (len(warm_modes) >= LAYERS) and all(m == "WARM" for m in warm_modes.values())
  w = _analyze(warm_snap)
  warm_lit      = w["lit"]
  warm_mult_ok  = w["clusters"] >= N_INSTANCES
  # GATED: the WARM cache-load render must sample the SAME warm baked section content as COLD (adobe/timber,
  # R>B). This was the section-array cache-load graying defect — OIIO's PNG reader premultiplied the cache
  # layers' straight alpha (captured as 0) into RGB on load, blacking the array; fixed by requesting
  # unassociated alpha on read (image_io_oiio.cpp). WARM warmth now equals COLD within noise.
  # spread floor 3.0 (see the COLD content_ok note): correct gutter-free content measures spread≈4.7.
  warm_content_ok = w["warm"] > 8.0 and w["spread"] > 3.0

  # ---- NEGATIVE PROOF: collapse every instance to the origin -> the multiplicity oracle MUST report < N.
  # Force COLD (delete the cache) so the collapsed cube renders properly LIT — the ONLY difference from the
  # positive case is the cluster count, which is exactly what the oracle must discriminate. ----
  for d in set(target_dirs.values()):
    shutil.rmtree(d, ignore_errors=True)
  neg_ecs  = _tojson(workdir, "secbake_collapse.ecs",
                     {"REN_SECTION_BAKE_INSTANCED": "1", "REN_SECTION_BAKE_INST_COLLAPSE": "1"})
  neg_snap = os.path.join(workdir, "secbake_collapse.png")
  rcN, _outN, _dirsN = _run_player(neg_ecs, neg_snap)
  n = _analyze(neg_snap)   # a properly-LIT single cube (COLD) — only the CLUSTER count collapses
  # the oracle correctly REJECTS the collapse iff the multiplicity check fails on a lit render
  negproof_ok = n["lit"] and (n["clusters"] < N_INSTANCES)

  # persist eyeball snapshots for a reviewer
  eyeball = os.environ.get("SECBAKE_INST_OUT", "/tmp/section_bake_player_instanced.png")
  try:
    base_out = os.path.splitext(eyeball)[0]
    os.makedirs(os.path.dirname(eyeball) or ".", exist_ok=True)
    shutil.copyfile(cold_snap, eyeball)
    shutil.copyfile(warm_snap, base_out + "_warm.png")
    shutil.copyfile(neg_snap, base_out + "_collapse.png")
  except Exception:
    pass

  print(f"[secbake-inst] COLD modes={cold_modes} lit={cold_lit} max={a['mx']} clusters={a['clusters']}/{N_INSTANCES} "
        f"litpx={a['litpx']} warm={a['warm']:.1f} spread={a['spread']:.1f} content_ok={content_ok} pngs={npngs}", flush=True)
  print(f"[secbake-inst] DISTINCT albedo layer0(adobe) mean={m0.round(1).tolist()} litpx={lp0} | "
        f"layer1(timber) mean={m1.round(1).tolist()} litpx={lp1} | dist={dist:.1f}", flush=True)
  print(f"[secbake-inst] WARM modes={warm_modes} lit={warm_lit} clusters={w['clusters']} warm={w['warm']:.1f} "
        f"spread={w['spread']:.1f} content_ok={warm_content_ok}", flush=True)
  print(f"[secbake-inst] NEGATIVE-PROOF collapse: lit={n['lit']} clusters={n['clusters']} (want <{N_INSTANCES}) "
        f"warm={n['warm']:.1f} -> oracle_rejects={negproof_ok}", flush=True)
  print(f"[secbake-inst] eyeball snapshot -> {eyeball}", flush=True)

  # GATED on the INSTANCING properties (this gate's scope): COLD renders N distinct lit instances with real
  # baked per-section content; the WARM bind SURVIVES instancing (still N lit clusters) AND samples the same
  # warm baked content as COLD; per-section bake is distinct; the multiplicity oracle discriminates a collapse.
  passed = (rcC == 0 and cold_ok and cold_lit and mult_ok and content_ok and cache_ok and distinct_ok and
            rcW == 0 and warm_ok and warm_lit and warm_mult_ok and warm_content_ok and negproof_ok)
  print("SECTION_BAKE_PLAYER_INSTANCED_RESULT=%s cold=%d cold_lit=%d clusters=%d/%d content=%d cache_pngs=%d "
        "distinct=%d(%.1f) warm=%d warm_lit=%d warm_clusters=%d warm_content=%d negproof=%d"
        % ("PASS" if passed else "FAIL", int(cold_ok), int(cold_lit), a["clusters"], N_INSTANCES,
           int(content_ok), npngs, int(distinct_ok), dist, int(warm_ok), int(warm_lit), w["clusters"],
           int(warm_content_ok), int(negproof_ok)), flush=True)
  shutil.rmtree(workdir, ignore_errors=True)
  sys.exit(0 if passed else 1)


if __name__ == "__main__":
  main()
