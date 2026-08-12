#!/usr/bin/env ork.python
###############################################################################
# section_bake_player_scatter.py — GATE: per-section texture-array BAKE composed
# with SCATTER-SSBO INSTANCING, through the shipping player path.
#
# section_bake_player_instanced.py proves the same bake against EXPLICIT instance
# matrices (drawable_data(instance_matrices=...)). The forest trees do NOT use that:
# they pull their placements from a terrain scatter sink
# (drawable_data(instance_source=(asset, sink, type_id)) -> fillInstanceSetFromScatter
# -> FWD_SSBO_CUSTOM_INSTANCED). This gate covers THAT combination — a bake keyed to
# the un-instanced prototype, drawn N times from a scatter-resolved matrix SSBO.
#
# Path: ren_section_bake_scatter.py -> ork.scene.tojson.py -> ork.ecs.player.exe
# --offscreen. The scene's terrain (assets/terrain/secbake_scatter.py) bakes a
# deterministic GRID_N x GRID_N "props" sink; one gid-partitioned SdfBaked mesh draws
# once per point with the stored SectionArrayPBR sampler, its layers baked per gid
# (AdobeStored sides / TimberStored tops).
#
# ORACLES (all measured on the settled offscreen snapshot / the cache PNGs):
#   * COLD: the player logs "section_bake(C++): COLD" for every capture target and
#     writes layer PNGs; WARM: it logs WARM for every target and renders the same.
#   * MULTIPLICITY: >= MULT_MIN warm BLOBS (connected components of the warm mask).
#     The scene's ground is cool and the skybox draw is off, so a warm blob is a prop.
#   * CONTENT: mean(R-B) and its spread over prop pixels — a prop whose section arrays
#     never bound loses its baked albedo entirely (measured: black + glossy).
#   * PER-SECTION DISTINCTNESS: baked albedo layer 0 (adobe) vs layer 1 (timber) have
#     distinct mean colors — each gid baked with ITS material, under instancing.
#   * NEGATIVE PROOFS (both must be rejected by the oracle that owns them):
#       SECBAKE_SCATTER_NOBAKE=1 — same instanced draw, no bake  -> CONTENT must fail.
#       SECBAKE_SCATTER_SOLO=1   — same bake, same technique, ONE scattered point
#                                  -> MULTIPLICITY must fail, CONTENT must still pass.
#
#   run: ork.python section_bake_player_scatter.py
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, glob, shutil, subprocess, tempfile
from collections import deque
import numpy as np

os.environ.setdefault("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "1")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import section_bake_player as base   # _DIR_RE (COLD/WARM log parse) + _section_distinctness

SCENE     = "ren_section_bake_scatter"
TARGETS   = 3            # SectionAlbedo + SectionParams + SectionNormal
LAYERS    = 2            # the SdfBaked gid partition: sides/bottom + top
N_PROPS   = 16           # secbake_scatter.GRID_N ** 2 — the sink's deterministic count
FRAMES    = 900
SNAP_AFTER = 30
# The camera looks DOWN on the prop grid (elevation ~48 deg): at 45 deg the rows occlude
# each other and merge into 3 blobs, which would blunt the multiplicity oracle.
CAMDIST   = 22.0
CAMHEIGHT = 34.0

# oracle thresholds — measured on this scene (grid / solo / nobake):
#   warm blobs   16 / 1 / 0        warm mean(R-B)  55.3 / 52.8 / 20.0
#   blob min px  1502              spread          16.1 / 16.0 / 2.2
MULT_MIN   = 12          # of N_PROPS placed; slack for two props merging at the frame edge
BLOB_MIN   = 400         # px: a prop is ~1500 px here; specular fringe on unbaked props is < 100
WARM_MIN   = 40.0        # mean(R-B) over prop pixels
SPREAD_MIN = 8.0         # std(R-B): the baked grain's variation (flat/unbaked reads ~2)
DIST_MIN   = 12.0        # adobe-vs-timber baked-layer mean-color distance

# The checkout this test lives in — pinned into every child process. Bare tool names on
# PATH can resolve another checkout's scripts (and PYTHONPATH already carries one), so the
# gate would silently measure a tree it is not testing.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)


def _child_env(extra=None):
  env = dict(os.environ)
  scripts = os.path.join(_ROOT, "obt.project", "scripts")
  env["PYTHONPATH"] = scripts + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
  env["ORKID_WORKSPACE_DIR"] = _ROOT
  env.update(extra or {})
  return env


def _player():
  """The player binary of THIS staging. A stale player earlier on PATH (an older staging's
  bin dir ahead of the current one) SIGSEGVs on a current scene — a failure that reads as a
  gate failure but is a PATH accident."""
  stage = os.environ.get("OBT_STAGE", "")
  cand  = os.path.join(stage, "bin", "ork.ecs.player.exe") if stage else ""
  if cand and os.path.exists(cand):
    return cand
  p = shutil.which("ork.ecs.player.exe")
  assert p, "ork.ecs.player.exe not found (no OBT_STAGE/bin copy, none on PATH)"
  return p


def _blobs(mask, min_area):
  """Connected components (4-neighbour) of `mask` with area >= min_area. No scipy: the
  masks here are a few tens of thousands of pixels."""
  h, w = mask.shape
  seen  = np.zeros_like(mask)
  sizes = []
  for y in range(h):
    for x0 in np.nonzero(mask[y] & ~seen[y])[0]:
      if seen[y, x0]:
        continue
      q = deque([(y, x0)]); seen[y, x0] = True; n = 0
      while q:
        cy, cx = q.popleft(); n += 1
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
          ny, nx = cy + dy, cx + dx
          if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not seen[ny, nx]:
            seen[ny, nx] = True
            q.append((ny, nx))
      if n >= min_area:
        sizes.append(n)
  return sizes


def _analyze(path):
  """Characterize a snapshot's PROP pixels. The scene guarantees the only warm (R>B)
  surfaces in frame are props sampling their baked section arrays."""
  d = dict(blobs=0, warmpx=0, warm=0.0, spread=0.0, mx=0, content=False, mult=False)
  if not (os.path.exists(path) and os.path.getsize(path) > 0):
    return d
  from PIL import Image
  rgb  = np.asarray(Image.open(path).convert("RGB")).astype(np.float32)
  rb   = rgb[:, :, 0] - rgb[:, :, 2]
  mask = (rb > 16.0) & (rgb.mean(axis=2) > 12.0)
  d["mx"]     = int(rgb.max())
  d["warmpx"] = int(mask.sum())
  sizes       = _blobs(mask, BLOB_MIN)
  d["blobs"]  = len(sizes)
  px = rgb[mask]
  if len(px) > 100:
    v = px[:, 0] - px[:, 2]
    d["warm"]   = float(v.mean())
    d["spread"] = float(v.std())
  d["content"] = (d["warm"] > WARM_MIN) and (d["spread"] > SPREAD_MIN) and (d["blobs"] > 0)
  d["mult"]    = d["blobs"] >= MULT_MIN
  return d


def _tojson(workdir, name, extra_env=None):
  ecs    = os.path.join(workdir, name)
  tojson = shutil.which("ork.scene.tojson.py")
  assert tojson, "ork.scene.tojson.py not on PATH"
  tj = subprocess.run([tojson, "-i", SCENE, "-o", ecs], capture_output=True, text=True,
                      timeout=300, env=_child_env(extra_env))
  assert os.path.exists(ecs) and os.path.getsize(ecs) > 0, \
      f"tojson({name}) produced no scene json (rc={tj.returncode}):\n{(tj.stderr or '')[-800:]}"
  return ecs


def _run_player(ecs, snap):
  """Offscreen player over the prop grid; returns (rc, {target: (mode, cache_dir)})."""
  cmd = [_player(), ecs, "--offscreen", "-S", snap, "-F", str(SNAP_AFTER), "--frames", str(FRAMES),
         "--camdist", str(CAMDIST), "--camheight", str(CAMHEIGHT)]
  try:
    pr = subprocess.run(cmd, timeout=200, capture_output=True, text=True, env=_child_env())
  except subprocess.TimeoutExpired:
    print("[secbake-scatter] player TIMEOUT (>200s)", flush=True)
    return 1, {}
  out  = (pr.stdout or "") + (pr.stderr or "")
  dirs = {}
  for m in base._DIR_RE.finditer(out):
    dirs[m.group(2)] = (m.group(1), m.group(4))
  if pr.returncode != 0:
    print(f"[secbake-scatter] player rc={pr.returncode}; stderr tail:\n{(pr.stderr or '')[-1200:]}", flush=True)
  return pr.returncode, dirs


def main():
  workdir = tempfile.mkdtemp(prefix="secbake_scatter_")
  ecs     = _tojson(workdir, "secbake_scatter.ecs")
  print(f"[secbake-scatter] tojson -> {ecs} ({os.path.getsize(ecs)} bytes)", flush=True)

  # ---- run A: discover the cache dirs this scene uses, then delete ONLY those (surgical:
  # every other gate's content-addressed cache is untouched) to force a COLD bake ----
  rcA, dirsA = _run_player(ecs, os.path.join(workdir, "snapA.png"))
  assert dirsA, "run A logged no section_bake(C++) cache dirs — the bake never engaged"
  target_dirs = {t: d for t, (_m, d) in dirsA.items()}
  print(f"[secbake-scatter] cache dirs: {target_dirs}", flush=True)
  for d in set(target_dirs.values()):
    shutil.rmtree(d, ignore_errors=True)

  # ---- COLD: GPU bake + placeholder->rebind, drawn through the scatter-instanced technique ----
  cold_snap = os.path.join(workdir, "cold.png")
  rcC, dirsC = _run_player(ecs, cold_snap)
  cold_modes = {t: m for t, (m, _d) in dirsC.items()}
  cold_ok    = (len(cold_modes) >= TARGETS) and all(m == "COLD" for m in cold_modes.values())
  c = _analyze(cold_snap)
  npngs = sum(len(glob.glob(os.path.join(d, "layer*.png"))) for d in set(target_dirs.values()))
  cache_ok = npngs >= LAYERS * TARGETS

  # ---- per-section distinctness, straight off the baked albedo cache ----
  albedo_dir = target_dirs.get("SectionAlbedo")
  dist, m0, m1, lp0, lp1 = (0.0, np.zeros(3), np.zeros(3), 0, 0)
  if albedo_dir and os.path.isdir(albedo_dir):
    dist, m0, m1, lp0, lp1 = base._section_distinctness(albedo_dir)
  distinct_ok = dist > DIST_MIN

  # ---- WARM: the cache-load bind must survive instancing and render the same content ----
  warm_snap = os.path.join(workdir, "warm.png")
  rcW, dirsW = _run_player(ecs, warm_snap)
  warm_modes = {t: m for t, (m, _d) in dirsW.items()}
  warm_ok    = (len(warm_modes) >= TARGETS) and all(m == "WARM" for m in warm_modes.values())
  w = _analyze(warm_snap)

  # ---- NEGATIVE PROOF 1: no bake at all -> the CONTENT oracle must reject ----
  nb_ecs  = _tojson(workdir, "nobake.ecs", {"SECBAKE_SCATTER_NOBAKE": "1"})
  nb_snap = os.path.join(workdir, "nobake.png")
  _rcN, _dN = _run_player(nb_ecs, nb_snap)
  nb = _analyze(nb_snap)
  nobake_rejected = not nb["content"]

  # ---- NEGATIVE PROOF 2: same bake, same instanced technique, ONE scattered point ->
  # the MULTIPLICITY oracle must reject while CONTENT still passes ----
  so_ecs  = _tojson(workdir, "solo.ecs", {"SECBAKE_SCATTER_SOLO": "1"})
  so_snap = os.path.join(workdir, "solo.png")
  _rcS, _dS = _run_player(so_ecs, so_snap)
  so = _analyze(so_snap)
  solo_rejected = (not so["mult"]) and so["content"]

  # eyeball copies for a reviewer
  out = os.environ.get("SECBAKE_SCATTER_OUT", "/tmp/section_bake_player_scatter.png")
  try:
    stem = os.path.splitext(out)[0]
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    shutil.copyfile(cold_snap, out)
    shutil.copyfile(warm_snap, stem + "_warm.png")
    shutil.copyfile(nb_snap,   stem + "_nobake.png")
    shutil.copyfile(so_snap,   stem + "_solo.png")
  except Exception:
    pass

  print(f"[secbake-scatter] COLD modes={cold_modes} blobs={c['blobs']}/{N_PROPS} warmpx={c['warmpx']} "
        f"warm={c['warm']:.1f} spread={c['spread']:.1f} content={c['content']} mult={c['mult']} pngs={npngs}",
        flush=True)
  print(f"[secbake-scatter] DISTINCT albedo layer0(adobe) mean={m0.round(1).tolist()} litpx={lp0} | "
        f"layer1(timber) mean={m1.round(1).tolist()} litpx={lp1} | dist={dist:.1f}", flush=True)
  print(f"[secbake-scatter] WARM modes={warm_modes} blobs={w['blobs']} warm={w['warm']:.1f} "
        f"spread={w['spread']:.1f} content={w['content']} mult={w['mult']}", flush=True)
  print(f"[secbake-scatter] NEG nobake: blobs={nb['blobs']} warm={nb['warm']:.1f} spread={nb['spread']:.1f} "
        f"-> content_rejected={nobake_rejected}", flush=True)
  print(f"[secbake-scatter] NEG solo:   blobs={so['blobs']} warm={so['warm']:.1f} spread={so['spread']:.1f} "
        f"-> mult_rejected={solo_rejected}", flush=True)
  print(f"[secbake-scatter] eyeball snapshots -> {out}", flush=True)

  passed = (rcC == 0 and cold_ok and c["content"] and c["mult"] and cache_ok and distinct_ok and
            rcW == 0 and warm_ok and w["content"] and w["mult"] and
            nobake_rejected and solo_rejected)
  print("SECTION_BAKE_SCATTER_RESULT=%s cold=%d blobs=%d/%d content=%d cache_pngs=%d distinct=%d(%.1f) "
        "warm=%d warm_blobs=%d warm_content=%d neg_nobake=%d neg_solo=%d"
        % ("PASS" if passed else "FAIL", int(cold_ok), c["blobs"], N_PROPS, int(c["content"]), npngs,
           int(distinct_ok), dist, int(warm_ok), w["blobs"], int(w["content"]),
           int(nobake_rejected), int(solo_rejected)), flush=True)
  shutil.rmtree(workdir, ignore_errors=True)
  sys.exit(0 if passed else 1)


if __name__ == "__main__":
  main()
