#!/usr/bin/env ork.python
################################################################################
# ENVMAP BAKE HDR RANGE gate (wave19 slice W19-S7, "hdr-bake-branch").
#
# THE DEFECT this gate stands on. The env-map bake picked its CAPTURE RANGE off
# the source file's EXTENSION: .exr/.hdr captured RGBA16F, everything else
# captured RGBA8 and packed each filtered texel with `int(value * 255)`. That
# 8-bit pack is a permanent 1.0 radiance ceiling written INTO the .xir — the
# shipped blender_sunset.xir carries it (measured W13-S1), and no amount of
# exposure downstream can recover a value the container never stored. The fix
# captures fp16 for EVERY source, in BOTH bake implementations (the python tool
# ork/envmap.py and the C++ EnvMapProcessor).
#
# WHAT IS MEASURED. One warm process, one GPU context, three bakes of synthetic
# sources through the REAL tool entrypoint (ork.envmap.process_envmap — the same
# call ork.hdri.genxir.py makes), each read back out of the written .xir through
# EnvMapProcessor.readXIR:
#
#   EQUIRECT  marker.hdr — a flat-RGBE equirect with a floor of 0.02 and ONE
#             gaussian blob whose peak is 8.0. This is the CONTROL: it was
#             already fp16 before the fix, so its numbers must not move.
#
#   STANDARD  the SAME BYTES under a .png name. The tool's branch is chosen by
#             EXTENSION while Image::initFromDataBlock decodes by MAGIC
#             (image_io.cpp) — so a `#?RADIANCE` stream named .png is the one
#             vector that sends FLOAT source data down the non-equirect
#             (tek_filterSpecularMapStandard) branch. That is what makes this a
#             RANGE measurement instead of a container-type assertion: the leg
#             carries a real 8.0 through the branch that used to clip it.
#             Layout is NOT under test here — W13-S1's marker instrument already
#             proved the standard and equirect paths agree to within 0.02deg.
#
#   LDR       a genuine 8-bit PNG (peak 1.0). The regression leg: the branch
#             still bakes an ordinary LDR source correctly, and its container is
#             fp16 too, so the ceiling is gone from the FORMAT and not merely
#             out-armed by the content.
#
# ORACLES (all four asserted per leg where applicable):
#   1. every .xir specular level decodes as fp16 (bytesPerChannel == 2). The
#      pre-fix LDR path wrote bytesPerChannel == 1 — this alone fails it.
#   2. the HDR-content legs' brightest level-0 texel exceeds 1.0 by a wide
#      margin. Pre-fix STANDARD could not report above 1.0 at all: 255/255.
#   3. the two HDR legs agree on that peak within 10% — one filter, one range,
#      two layouts; a branch that quantized would read exactly 1.0 against the
#      control's ~8.
#   4. the LDR leg stays at/below its own 1.0 source peak (fp16 storage does
#      not invent energy).
#
# MEASURED at the fix (128x64 source, roughness levels [0.0, 0.5], 256 samples):
#   EQUIRECT peak 7.7500  STANDARD peak 7.7500  ratio 1.0000  LDR peak 0.9878
# (the ~3% shortfall from 8.0 is the GGX filter's own footprint at roughness 0
# on a 4.5px-sigma blob plus fp16 spacing, identical in both legs — which is why
# oracle 3 compares the legs to each other rather than to the source.)
#
# TEETH, measured by reverting ork/envmap.py to its pre-fix form and rerunning:
#   STANDARD peak 1.0000 EXACTLY (the 8-bit ceiling, nothing else lands there),
#   ratio 0.1290, and both non-.hdr legs report bytesPerChannel 1 — 4 of the
#   checks below fail while the EQUIRECT control is unmoved at 7.7500.
#
# NOT ork.testing headless_app: process_envmap needs the main-thread gfx context
# BOUND for inline GPU work (bindGfxToCurrentThread) and drives its own frame
# iteration, which is the lifecycle ork.hdri.genxir.py established for bakes.
# The verdict-before-teardown protocol is honoured.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

# repo root = five levels up; prepend THIS checkout's scripts dir so ork.envmap /
# ork.testing resolve from the same tree as this test (worktree-shadowing idiom).
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import math
import struct
import shutil
import tempfile
import zlib
import numpy
from orkengine import core
from orkengine import lev2
from ork.testing import verdict

WIDTH        = 128           # source equirect width (height = W/2)
BLOB_PEAK    = 8.0           # the >1.0 the whole gate is about
BLOB_FLOOR   = 0.02
BLOB_SIGMA   = 0.035         # gaussian sigma in normalized U
MARKER_U     = 0.375
MARKER_V     = 0.375
ROUGHNESS    = [0.0, 0.5]    # level 0 is near-passthrough; level 1 exercises the loop
SAMPLES      = 256

HDR_PEAK_MIN = 4.0           # >1.0 with room to spare (measured 7.83)
LEG_RATIO_TOL = 0.10         # equirect vs standard peak agreement
LDR_PEAK_MAX = 1.05          # fp16 storage must not invent energy

_RESULT = {"failures": [], "notes": []}

def check(name, ok, detail=""):
  print("  CHECK %-28s %s %s" % (name, "PASS" if ok else "FAIL", detail), flush=True)
  if not ok:
    _RESULT["failures"].append(name)

################################################################################
# synthetic sources
################################################################################

def _to_rgbe(r, g, b):
  m = max(r, g, b)
  if m < 1e-32:
    return (0, 0, 0, 0)
  frac, exp = math.frexp(m)
  s = frac * 256.0 / m
  return (int(min(255, r * s)), int(min(255, g * s)), int(min(255, b * s)), int(exp + 128))

def write_marker_hdr(path, w):
  """Flat (non-RLE) Radiance RGBE equirect, rows top-to-bottom, one gaussian blob
  peaking at BLOB_PEAK. Flat on purpose: no encoder cleverness between the
  requested radiance and the bytes on disk (W13-S1's marker instrument)."""
  h  = w // 2
  s2 = 2.0 * BLOB_SIGMA * BLOB_SIGMA
  out = bytearray()
  for y in range(h):
    for x in range(w):
      u  = (x + 0.5) / w
      v  = (y + 0.5) / h
      du = min(abs(u - MARKER_U), 1.0 - abs(u - MARKER_U))   # wrap in U
      dv = v - MARKER_V
      g  = math.exp(-(du * du + dv * dv) / s2)
      r, gg, b = (BLOB_FLOOR + BLOB_PEAK * g,
                  BLOB_FLOOR + BLOB_PEAK * g * 0.7,
                  BLOB_FLOOR * 2.0 + BLOB_PEAK * g * 0.35)
      e = _to_rgbe(r, gg, b)
      # a scanline whose first pixel reads (2,2,..) is parsed as new-style RLE
      if x == 0 and e[0] == 2 and e[1] == 2:
        e = (3, 2, e[2], e[3])
      out += bytes(e)
  with open(path, "wb") as f:
    f.write(b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n")
    f.write(("-Y %d +X %d\n" % (h, w)).encode())
    f.write(bytes(out))

def write_marker_png(path, w):
  """8-bit RGB PNG carrying the same blob shape saturated at 1.0 — a REAL LDR
  source (stdlib zlib; no image library needed for 128x64)."""
  h  = w // 2
  s2 = 2.0 * BLOB_SIGMA * BLOB_SIGMA
  raw = bytearray()
  for y in range(h):
    raw.append(0)   # filter type 0
    for x in range(w):
      u  = (x + 0.5) / w
      v  = (y + 0.5) / h
      du = min(abs(u - MARKER_U), 1.0 - abs(u - MARKER_U))
      dv = v - MARKER_V
      g  = math.exp(-(du * du + dv * dv) / s2)
      raw += bytes((min(255, int(255 * (BLOB_FLOOR + g))),
                    min(255, int(255 * (BLOB_FLOOR + g * 0.7))),
                    min(255, int(255 * (BLOB_FLOOR * 2.0 + g * 0.35)))))
  def chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data +
            struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))
  with open(path, "wb") as f:
    f.write(b"\x89PNG\r\n\x1a\n")
    f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))
    f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 6)))
    f.write(chunk(b"IEND", b""))

################################################################################
# .xir readback
################################################################################

def read_xir_levels(path):
  """[(bytesPerChannel, peak_R, mean_R)] per specular roughness level."""
  d = lev2.EnvMapProcessor.readXIR(path)
  levels = []
  for img in d["specular_images"]:
    bpc = img.bytesPerChannel
    buf = bytes(img.data.bytes)
    if bpc == 2:
      floats = numpy.frombuffer(buf, dtype=numpy.float16).astype(numpy.float32)
    elif bpc == 4:
      floats = numpy.frombuffer(buf, dtype=numpy.float32)
    else:
      floats = numpy.frombuffer(buf, dtype=numpy.uint8).astype(numpy.float32) / 255.0
    nc  = img.numcomponents
    red = floats[0::nc] if nc > 1 else floats
    levels.append((bpc, float(red.max()), float(red.mean())))
  return levels

################################################################################

def main():
  from ork.envmap import process_envmap

  tmpdir = tempfile.mkdtemp(prefix="envmap_hdr_range_")
  hdr_path = os.path.join(tmpdir, "marker.hdr")
  std_path = os.path.join(tmpdir, "marker_std.png")   # HDR bytes, non-hdr extension
  ldr_path = os.path.join(tmpdir, "marker_ldr.png")
  write_marker_hdr(hdr_path, WIDTH)
  shutil.copyfile(hdr_path, std_path)
  write_marker_png(ldr_path, WIDTH)

  ezapp = lev2.lev2appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread returned null — no main gfx context"

  peaks = {}
  try:
    for legname, src in (("equirect", hdr_path), ("standard", std_path), ("ldr", ldr_path)):
      out = os.path.join(tmpdir, legname + ".xir")
      print("LEG %s: %s" % (legname, os.path.basename(src)), flush=True)
      ok = process_envmap(src, out, ctx, ezapp, verbose=False,
                          roughness_values=list(ROUGHNESS), specular_samples=SAMPLES)
      check("%s_bake_ok" % legname, bool(ok))
      if not ok:
        continue
      levels = read_xir_levels(out)
      check("%s_level_count" % legname, len(levels) == len(ROUGHNESS),
            "levels=%d" % len(levels))
      allfp16 = all(l[0] == 2 for l in levels)
      check("%s_levels_are_fp16" % legname, allfp16,
            "bytesPerChannel=%s" % str([l[0] for l in levels]))
      peak = levels[0][1] if levels else 0.0
      peaks[legname] = peak
      _RESULT["notes"].append("%s_peak=%.4f" % (legname, peak))
      print("    level0 peak=%.4f mean=%.4f  level1 peak=%.4f"
            % (peak, levels[0][2], levels[1][1] if len(levels) > 1 else -1.0), flush=True)

    for legname in ("equirect", "standard"):
      check("%s_peak_above_ldr_ceiling" % legname,
            peaks.get(legname, 0.0) > HDR_PEAK_MIN,
            "peak=%.4f min=%.2f (source peak %.1f)" % (peaks.get(legname, 0.0), HDR_PEAK_MIN, BLOB_PEAK))

    if "equirect" in peaks and "standard" in peaks and peaks["equirect"] > 0.0:
      ratio = peaks["standard"] / peaks["equirect"]
      _RESULT["notes"].append("std_over_equirect=%.4f" % ratio)
      check("branches_agree_on_range", abs(ratio - 1.0) <= LEG_RATIO_TOL,
            "standard/equirect=%.4f tol=%.2f" % (ratio, LEG_RATIO_TOL))

    check("ldr_source_stays_ldr", peaks.get("ldr", 99.0) <= LDR_PEAK_MAX,
          "peak=%.4f max=%.2f" % (peaks.get("ldr", 99.0), LDR_PEAK_MAX))

  except Exception:
    import traceback
    traceback.print_exc()
    _RESULT["failures"].append("exception")

  ok = (len(_RESULT["failures"]) == 0)
  detail = "src=%dx%d %s" % (WIDTH, WIDTH // 2, " ".join(_RESULT["notes"]))
  if _RESULT["failures"]:
    detail += " failed=" + ",".join(_RESULT["failures"])
  rc = verdict(ok, detail)   # VERDICT BEFORE TEARDOWN (#57)
  ezapp.mainThreadEnd()
  shutil.rmtree(tmpdir, ignore_errors=True)
  return rc


if __name__ == "__main__":
  sys.exit(main())
