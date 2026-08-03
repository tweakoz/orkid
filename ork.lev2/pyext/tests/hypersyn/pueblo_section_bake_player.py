#!/usr/bin/env ork.python
###############################################################################
# pueblo_section_bake_player.py — REGRESSION GATE for the stored-mode section-bake
# black-face defect (hm.section.v3 bake-vs-sample v-orientation fix).
#
# The pueblo room-blocks have many small gid0 WALL charts packed on a gutter-heavy
# atlas (layer0 paintFrac ~0.7). Before the fix the section-bake atlas was written
# v-FLIPPED relative to the stored sampler's read, so ~18% of gid0 faces (parapet
# caps, setback risers, base-flare, recesses) sampled the BLACK gutter and rendered
# BLACK on otherwise-sunlit surfaces. This gate drives the ISOLATED pueblo through
# the shipping player path in BOTH proc + stored modes and asserts the stored render
# has NO missing-albedo faces vs the proc control (the coverage is complete).
#
# ORACLE (fail-loud):
#   * stored COLD bake logs section_bake(C++): COLD for both targets, render LIT.
#   * MISSING-ALBEDO: pixels lit in proc (>30 L) but BLACK in stored (<8 L), as a
#     fraction of the building silhouette, is < 0.5% (proc-control class). Pre-fix
#     this class was ~10%.
#   * EDGE-BLEED (v4 gutter-dilation gate): the black chart-border LINES are THIN and
#     dark-but-not-fully-black (~L 8..25), so the missing-albedo % is blind to them.
#     This oracle detects INTERNAL geometry edges from the clean proc render (Sobel-ish
#     gradient within the eroded silhouette, so the outer silhouette rim is excluded),
#     dilates them to an edge BAND, and measures the fraction of band pixels that are
#     markedly DARKER in stored than proc (Ls < DARK_RATIO*Lp on lit faces). Pre-fix the
#     per-section gutter bleeds black along every face edge -> high band-dark fraction;
#     post-dilation the gutter carries chart color -> stored tracks proc along edges.
#     NEGATIVE-PROOFED: FAILS on v3 (pre-fix) content, PASSES on v4 (dilated).
#
#   run:  ork.python pueblo_section_bake_player.py
#   negative-proof on saved PNGs (no bake):
#         PUEBLOBAKE_ORACLE_PNGS=proc.png:stored.png ork.python pueblo_section_bake_player.py
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, shutil, subprocess, tempfile
import numpy as np

os.environ.setdefault("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "1")

SCENE   = "ren_pueblo_bake"
BAKERES = "512"
CAM     = ["--camdist", "20", "--camheight", "6"]
FRAMES  = 900
SNAP_AT = 40
THRESH  = 0.5    # percent of silhouette allowed to be missing-albedo

# edge-bleed oracle knobs
EDGE_T          = 12.0   # proc luminance gradient (L/px) that marks a geometry/luminance edge
BAND_R          = 2      # dilate the edge set by this many px -> the edge band
SIL_ERODE       = 3      # erode the silhouette by this many px -> exclude the outer rim (bg transition)
DARK_RATIO      = 0.60   # a band pixel is "bled dark" when stored < DARK_RATIO * proc (on lit faces)
EDGE_BLEED_LIMIT = 6.0   # percent of edge-band pixels allowed to be bled-dark (v3 >> this, v4 << this)


def _render(workdir, mode):
  tojson = shutil.which("ork.scene.tojson.py")
  player = shutil.which("ork.ecs.player.exe")
  assert tojson and player, "tojson/player not on PATH"
  ecs = os.path.join(workdir, f"pb_{mode}.ecs")
  png = os.path.join(workdir, f"pb_{mode}.png")
  env = dict(os.environ, PUEBLO_MODE=mode, PUEBLO_VARIANT="stepped",
             SWEST_BAKE_RES=BAKERES, PB_NOSKY="1")
  tj = subprocess.run([tojson, "-i", SCENE, "-o", ecs], env=env, capture_output=True, text=True, timeout=180)
  assert os.path.exists(ecs) and os.path.getsize(ecs) > 0, \
      f"tojson({mode}) produced no scene:\n{(tj.stderr or '')[-800:]}"
  pr = subprocess.run([player, ecs, "--offscreen", "-S", png, "-F", str(SNAP_AT),
                       "--frames", str(FRAMES)] + CAM, env=env, capture_output=True, text=True, timeout=220)
  out = (pr.stdout or "") + (pr.stderr or "")
  from PIL import Image
  a = np.asarray(Image.open(png).convert("RGB")).astype(np.float64)
  return a, out


def _dilate_bool(m, r):
  out = m.copy()
  for _ in range(int(r)):
    o = out.copy()
    o[:-1, :] |= out[1:, :]; o[1:, :] |= out[:-1, :]
    o[:, :-1] |= out[:, 1:]; o[:, 1:] |= out[:, :-1]
    out = o
  return out


def _erode_bool(m, r):
  return ~_dilate_bool(~m, r)


def _edge_bleed(proc, stored):
  """Fraction of INTERNAL-edge-band pixels that are bled-dark in stored vs proc. Returns
  (band_frac, band_px, ctrl_frac) — ctrl_frac is the same dark fraction on interior NON-edge
  faces (should stay low both pre/post; the DEFECT is edge-CONCENTRATED darkening)."""
  Lp  = proc   @ [0.299, 0.587, 0.114]
  Ls  = stored @ [0.299, 0.587, 0.114]
  sil = Lp > 8
  sil_in = _erode_bool(sil, SIL_ERODE)               # interior only (drop the outer silhouette rim)
  gx = np.zeros_like(Lp); gx[:, 1:] = np.abs(Lp[:, 1:] - Lp[:, :-1])
  gy = np.zeros_like(Lp); gy[1:, :] = np.abs(Lp[1:, :] - Lp[:-1, :])
  edge = (gx + gy) > EDGE_T
  band = _dilate_bool(edge & sil_in, BAND_R) & sil_in
  lit  = Lp > 30
  dark = (Ls < DARK_RATIO * Lp) & lit               # markedly darker in stored on a lit face
  band_px   = int(band.sum())
  band_frac = 100.0 * float((band & dark).sum()) / max(1, band_px)
  ctrl      = sil_in & (~band) & lit
  ctrl_frac = 100.0 * float((ctrl & dark).sum()) / max(1, int(ctrl.sum()))
  return band_frac, band_px, ctrl_frac


def _oracle_only():
  """Negative-proof / offline hook: run the edge-bleed oracle on two saved PNGs (proc:stored)
  WITHOUT baking, so the identical math can be pointed at pre-fix (v3) content."""
  from PIL import Image
  pp, sp = os.environ["PUEBLOBAKE_ORACLE_PNGS"].split(":")
  proc   = np.asarray(Image.open(pp).convert("RGB")).astype(np.float64)
  stored = np.asarray(Image.open(sp).convert("RGB")).astype(np.float64)
  bf, bpx, cf = _edge_bleed(proc, stored)
  passed = bf < EDGE_BLEED_LIMIT
  print(f"[pueblobake] EDGE-BLEED band_frac={bf:.2f}% (limit {EDGE_BLEED_LIMIT}%) "
        f"band_px={bpx} ctrl_frac={cf:.2f}%", flush=True)
  print("PUEBLO_EDGE_BLEED_RESULT=%s band_frac=%.2f%%" % ("PASS" if passed else "FAIL", bf), flush=True)
  sys.exit(0 if passed else 1)


def main():
  if os.environ.get("PUEBLOBAKE_ORACLE_PNGS"):
    _oracle_only()
  workdir = tempfile.mkdtemp(prefix="pueblobake_")
  proc, _o = _render(workdir, "proc")
  stored, sout = _render(workdir, "stored")

  cold = "section_bake(C++): COLD" in sout
  warm = "section_bake(C++): WARM" in sout
  baked_ok = cold or warm

  Lp = proc @ [0.299, 0.587, 0.114]
  Ls = stored @ [0.299, 0.587, 0.114]
  sil = Lp > 8
  sil_px = int(sil.sum())
  stored_lit = float(Ls[sil].mean()) > 8.0 if sil_px else False
  missing = sil & (Ls < 8) & (Lp > 30)
  miss_pct = 100.0 * missing.sum() / max(1, sil_px)

  band_frac, band_px, ctrl_frac = _edge_bleed(proc, stored)

  eyeball = os.environ.get("PUEBLOBAKE_OUT", "/tmp/pueblo_section_bake_stored.png")
  try:
    from PIL import Image
    Image.fromarray(stored.astype(np.uint8)).save(eyeball)
    Image.fromarray(proc.astype(np.uint8)).save(eyeball.replace(".png", "_proc.png"))
  except Exception:
    pass

  print(f"[pueblobake] baked={cold and 'COLD' or (warm and 'WARM' or 'NONE')} "
        f"sil_px={sil_px} stored_lit={stored_lit} missing-albedo={miss_pct:.2f}% (limit {THRESH}%)", flush=True)
  print(f"[pueblobake] EDGE-BLEED band_frac={band_frac:.2f}% (limit {EDGE_BLEED_LIMIT}%) "
        f"band_px={band_px} ctrl_frac={ctrl_frac:.2f}%", flush=True)
  print(f"[pueblobake] eyeball stored snapshot -> {eyeball}", flush=True)

  edge_ok = band_frac < EDGE_BLEED_LIMIT
  passed = baked_ok and stored_lit and sil_px > 5000 and miss_pct < THRESH and edge_ok
  print("PUEBLO_SECTION_BAKE_RESULT=%s baked=%d stored_lit=%d missing=%.2f%% edge_bleed=%.2f%%"
        % ("PASS" if passed else "FAIL", int(baked_ok), int(stored_lit), miss_pct, band_frac), flush=True)
  shutil.rmtree(workdir, ignore_errors=True)
  sys.exit(0 if passed else 1)


if __name__ == "__main__":
  main()
