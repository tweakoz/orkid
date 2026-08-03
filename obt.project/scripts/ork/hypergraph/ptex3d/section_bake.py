###############################################################################
# section_bake — assemble + cache the O3 per-section TEXTURE ARRAY.
#
# SectionUnwrap gives every mesh SECTION its own 0-1 UV domain + a dense LAYER
# index (UV0.z). This module builds the shared sampler2DArray the SectionArray
# material samples: one array LAYER per section, content-addressed on disk so a
# COLD run writes the cache and a WARM run loads it without re-generating.
#
# The per-layer CONTENT is produced by `content_fn(layer, res) -> HxWx4 uint8`
# (RGBA8). The default fills each layer with a distinct hue + a UV checker so the
# per-section unwrap is directly visible. The GPU procedural-surface bake driver
# (rendering the real ptex3d surface per section into each layer) is the next O3
# stage; it plugs in here by supplying its own content/loader, reusing this same
# cache-key + array-assembly path.
#
# Cache dir mirrors the terrain capture cache (_terrain.py):
#   <assetcache>/ptex3d_capture/section/<key>__r<res>__L<layers>/layer<NN>.png
###############################################################################
import os
import time
import hashlib

import numpy as np

from orkengine.core import Path as _Path
from orkengine import lev2
from orkengine.core import CrcStringProxy

_tokens = CrcStringProxy()

# Section-bake FORMAT EPOCH — the single python source of truth, kept string-identical to the C++
# content-key anchor: ork.lev2/src/gfx/hypermesh/hmdflow_render.cpp, sectionBakeContentKey() ->
# h->accumulateString("hm.section.v5"). LOUD DUPLICATION: python cannot read the C++ constant at
# import time, so the literal is mirrored here; if the C++ epoch bumps (v5 -> v6), bump this too or
# python-gate caches will not invalidate in lockstep. Folded into section_cache_dir() so every
# python section cache is epoch-salted exactly like the C++ content-addressed cache.
SECTION_BAKE_EPOCH = "hm.section.v5"

# Gutter-dilation ring count for mip-0 (mirror of C++ kSectionDilateTexels in hmdflow_render.cpp):
# bilinear needs >=1 valid texel beyond a chart edge; trilinear/anisotropic footprints + xatlas
# half-texel chart insets want a few more. 4 covers the mip-0 sampling footprint; the deeper chain
# coverage-weights the downsample so it excludes gutter black at every level regardless of chart size.
_SECTION_DILATE_TEXELS = 4


def _section_mips_enabled(knob):
  """A8: mip-chain enable rides the caller knob (default ON); ORKID_SECTION_MIPS env OVERRIDES
  (0=force off, 1=force on) so a minification A/B can flip the identical bake both ways."""
  e = os.environ.get("ORKID_SECTION_MIPS")
  if e is not None:
    return e != "0"
  return bool(knob)


def _mip_count(res):
  """Full mip-chain level count for a square res (down to 1x1): 256 -> 9 levels (0..8)."""
  n, d = 1, int(res)
  while d > 1:
    d >>= 1
    n += 1
  return n


def _shift2d(a, dy, dx):
  """Shift a 2D array so out[y,x] = a[y+dy, x+dx], with ZERO fill at borders (never wraps — np.roll
  would). Used to gather the 8-neighborhood for coverage dilation."""
  h, w = a.shape[:2]
  out = np.zeros_like(a)
  ys_src = slice(max(0, dy), h + min(0, dy))
  ys_dst = slice(max(0, -dy), h + min(0, -dy))
  xs_src = slice(max(0, dx), w + min(0, dx))
  xs_dst = slice(max(0, -dx), w + min(0, -dx))
  out[ys_dst, xs_dst] = a[ys_src, xs_src]
  return out


def _dilate_coverage_rgba8(px, iters):
  """GUTTER-DILATION in place — exact mirror of C++ _dilateCoverageRGBA8 (hmdflow_render.cpp).
  COVERAGE is the alpha channel (255 covered, 0 gutter). Each pass copies the INTEGER mean RGB of a
  texel's covered 8-neighbors into an uncovered texel and marks it covered (alpha=255), advancing the
  front one ring per pass. Reads a per-pass SNAPSHOT so exactly one ring grows per iteration; stops
  early when no texel grew. Forward samples .xyz only, so writing alpha is invisible to the render —
  alpha's sole job is the coverage mask (also at each mip, where the resampled alpha re-encodes it)."""
  if iters <= 0 or px is None:
    return px
  h, w = px.shape[:2]
  offsets = [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)]
  for _ in range(int(iters)):
    snap = px.copy()
    cov = (snap[..., 3] != 0)
    covi = cov.astype(np.uint32)
    accR = np.zeros((h, w), np.uint32)
    accG = np.zeros((h, w), np.uint32)
    accB = np.zeros((h, w), np.uint32)
    cnt = np.zeros((h, w), np.uint32)
    for dy, dx in offsets:
      ncov = _shift2d(covi, dy, dx)
      accR += _shift2d(snap[..., 0].astype(np.uint32), dy, dx) * ncov
      accG += _shift2d(snap[..., 1].astype(np.uint32), dy, dx) * ncov
      accB += _shift2d(snap[..., 2].astype(np.uint32), dy, dx) * ncov
      cnt += ncov
    target = (~cov) & (cnt > 0)
    if not target.any():
      break                                          # fully flooded — front cannot advance
    denom = np.where(cnt > 0, cnt, 1)                 # integer truncation matches C++ r/cnt
    px[..., 0][target] = (accR // denom)[target].astype(np.uint8)
    px[..., 1][target] = (accG // denom)[target].astype(np.uint8)
    px[..., 2][target] = (accB // denom)[target].astype(np.uint8)
    px[..., 3][target] = 255
  return px


def _cov_weighted_downsample(parent):
  """One coverage-weighted 2x2 halving — exact mirror of the C++ mip inner loop in _buildSectionArray.
  Each child texel's alpha is its coverage; out_rgb = Σ(child_rgb·cov)/Σ(cov) (integer), out_alpha=255
  when any child was covered else 0. This EXCLUDES gutter-black from the average so deep mips do not
  darken as charts shrink below the fixed dilation radius. Source indices clamp (min(2i+1, dim-1)) so a
  non-power-of-two level degrades gracefully, matching the C++ clamp."""
  ph, pw = parent.shape[:2]
  mh, mw = max(1, ph >> 1), max(1, pw >> 1)
  p = parent.astype(np.uint32)
  yi = [np.minimum(2 * np.arange(mh) + o, ph - 1) for o in (0, 1)]
  xi = [np.minimum(2 * np.arange(mw) + o, pw - 1) for o in (0, 1)]
  accCov = np.zeros((mh, mw), np.uint32)
  accRGB = np.zeros((mh, mw, 3), np.uint32)
  for ys in yi:
    for xs in xi:
      block = p[np.ix_(ys, xs)]                       # (mh, mw, 4)
      cov = block[..., 3]
      accCov += cov
      accRGB += block[..., :3] * cov[..., None]
  covered = accCov > 0
  denom = np.where(covered, accCov, 1)
  rgb = (accRGB // denom[..., None]).astype(np.uint8)
  out = np.zeros((mh, mw, 4), np.uint8)
  out[..., :3] = np.where(covered[..., None], rgb, 0)
  out[..., 3] = np.where(covered, 255, 0).astype(np.uint8)
  return out


def _renorm_normals_inplace(px):
  """RENORMALIZE an encoded tangent/world-space NORMAL mip in place (SectionNormal target only). A
  box/coverage-weighted downsample AVERAGES the encoded vectors, shortening them -> flattened lighting
  under minification. Decode 0..255 -> [-1,1], normalize, re-encode. Alpha (AO/coverage) untouched.
  Exact mirror of the C++ renormMips branch in _buildSectionArray."""
  f = px[..., :3].astype(np.float32) * (2.0 / 255.0) - 1.0
  ln = np.sqrt((f * f).sum(axis=2))
  m = ln > 1e-6
  f[m] = f[m] / ln[m][..., None]
  enc = np.clip((f * 0.5 + 0.5) * 255.0 + 0.5, 0.0, 255.0).astype(np.uint8)
  px[..., :3] = enc


def section_cache_dir(key, bake_res, num_layers):
  """Content-only cache dir for a section-array bake: `key` is the caller's content digest
  (mesh identity + material digest), `bake_res` the per-layer resolution, `num_layers` the
  section count. Any change to those -> a fresh dir -> a one-run cold re-bake. The format EPOCH is
  folded in FIRST (mirroring C++ sectionBakeContentKey), so an epoch bump invalidates python-gate
  caches in lockstep with C++ — expect the first post-bump run to go COLD."""
  h = hashlib.sha1(("%s\x00%s\x00%d\x00%d"
                    % (SECTION_BAKE_EPOCH, str(key), int(bake_res), int(num_layers))).encode("utf-8")).hexdigest()[:12]
  return _Path.expandPathString(
      "<assetcache>/ptex3d_capture/section/%s__r%d__L%d" % (h, int(bake_res), int(num_layers)))


def _default_content(layer, res):
  """A distinct hue per layer + a UV checker (so the per-section 0-1 unwrap is visible).
  Deterministic (no randomness) so the content hashes stably for the cache."""
  # per-layer hue via phase-shifted sines (matches SectionViz so the two agree visually).
  base = np.array([0.5 + 0.4 * np.sin(layer * 1.7 + 0.0),
                   0.5 + 0.4 * np.sin(layer * 1.7 + 2.1),
                   0.5 + 0.4 * np.sin(layer * 1.7 + 4.2)], dtype=np.float32)
  yy, xx = np.mgrid[0:res, 0:res]
  checker = (((xx // max(1, res // 8)) + (yy // max(1, res // 8))) & 1).astype(np.float32)
  tint = 0.6 + 0.4 * checker                      # checker modulates brightness
  img = np.empty((res, res, 4), dtype=np.uint8)
  for c in range(3):
    img[..., c] = np.clip(base[c] * tint * 255.0, 0, 255).astype(np.uint8)
  img[..., 3] = 255
  return img


def placeholder_section_array(ctx, *, num_layers, bake_res=256, fmt=None, fill=48):
  """A VALID, sampleable placeholder array (reserve + upload a neutral gray to every layer + finalize
  ONCE). A reserved-only array sits in TRANSFER_DST (NOT shader-readable), so a stored-mode material's
  sampler2DArray needs this to sample safely during the (few-frame) GPU cold-bake wait, BEFORE the real
  content is baked. Writes NO cache — the real bake (bake_section_array) makes a fresh array + rebinds."""
  fmt = fmt or _tokens.RGBA8
  arr = lev2.TextureArray(w=int(bake_res), h=int(bake_res), slices=int(num_layers),
                          fmt=fmt, mipmapped=False)
  txi = ctx.TXI
  txi.reserveTextureArray(tarr=arr, w=int(bake_res), h=int(bake_res),
                          num_slices=int(num_layers), num_mips=1, fmt=fmt)
  gray = np.empty((int(bake_res), int(bake_res), 4), dtype=np.uint8)
  gray[..., :3] = int(fill)
  gray[..., 3] = 255
  data = gray.tobytes()
  for i in range(int(num_layers)):
    txi.uploadTextureRegion(tex=arr.tex, mip_level=0, array_layer=i, offset_x=0, offset_y=0, offset_z=0,
                            extent_w=int(bake_res), extent_h=int(bake_res), extent_d=1, data=data)
  txi.finalizeUpload(tex=arr.tex)
  return arr


def bake_section_array(ctx, *, key, num_layers, bake_res=256, content_fn=None, fmt=None, mips=True,
                       target_name=""):
  """Build (or load-from-cache) the per-section TextureArray — PARITY with the C++ baker
  (hmdflow_render.cpp assembleSectionArraysFromJob / _buildSectionArray, epoch hm.section.v5).
  COLD: `content_fn(layer,res)` (default = hue+checker; the stage-2 GPU driver passes a job-backed
  content_fn returning the REAL baked section surface with COVERAGE in alpha) produces each layer's
  RGBA8; the mip-0 is GUTTER-DILATED (coverage flood, _SECTION_DILATE_TEXELS rings) BEFORE it is written
  to the cache PNG and uploaded, so a WARM load is byte-identical + needs no re-dilate. WARM: every layer
  PNG present -> load (already post-dilation) + upload, no generation. Always allocates a FRESH array
  (reserve -> upload every layer[/mip] -> finalize ONCE — the correct non-streaming sequence). A8: `mips`
  (default True) builds a trilinear mip chain per layer via COVERAGE-WEIGHTED downsample
  (mip = Σ(child_rgb·cov)/Σ(cov), successive halving from mip-0) with a per-level zero-coverage flood
  fill — this excludes gutter-black at every level so deep mips do not darken (the C++ v5 behavior; a
  plain box filter left deep-mip black gutters). `target_name=="SectionNormal"` additionally renormalizes
  each mip texel's decoded normal. The cache stores only mip-0 PNGs (backward-readable); mips regenerate
  on every build. Returns (array, cache_dir, warm). Fails LOUD on a bad content shape / IO."""
  if int(num_layers) < 1:
    raise ValueError("bake_section_array: num_layers must be >= 1 (got %r)" % (num_layers,))
  content_fn = content_fn or _default_content
  fmt = fmt or _tokens.RGBA8
  cache_dir = section_cache_dir(key, bake_res, num_layers)
  os.makedirs(cache_dir, exist_ok=True)
  paths = [os.path.join(cache_dir, "layer%02d.png" % i) for i in range(int(num_layers))]
  warm = all(os.path.isfile(p) for p in paths)

  use_mips = _section_mips_enabled(mips)
  num_mips = _mip_count(bake_res) if use_mips else 1
  txi = ctx.TXI
  arr = lev2.TextureArray(w=int(bake_res), h=int(bake_res), slices=int(num_layers),
                          fmt=fmt, mipmapped=bool(use_mips))
  txi.reserveTextureArray(tarr=arr, w=int(bake_res), h=int(bake_res),
                          num_slices=int(num_layers), num_mips=int(num_mips), fmt=fmt)

  renorm = (target_name == "SectionNormal")           # keyed by NAME, mirrors C++ renormMips
  t0 = time.time()
  from PIL import Image as _PILImage
  for i, p in enumerate(paths):
    if warm:
      rgba = np.asarray(_PILImage.open(p).convert("RGBA"), dtype=np.uint8)
      if rgba.shape[0] != bake_res or rgba.shape[1] != bake_res:
        rgba = np.asarray(_PILImage.fromarray(rgba).resize((int(bake_res), int(bake_res))), dtype=np.uint8)
      rgba = np.ascontiguousarray(rgba)                # already POST-dilation on disk — no re-dilate
    else:
      rgba = content_fn(i, int(bake_res))
      if rgba.shape != (int(bake_res), int(bake_res), 4) or rgba.dtype != np.uint8:
        raise ValueError("bake_section_array: content_fn(%d) must return (%d,%d,4) uint8, got %r %s"
                         % (i, bake_res, bake_res, rgba.shape, rgba.dtype))
      rgba = np.ascontiguousarray(rgba).copy()         # writable for in-place mip-0 dilation
      # GUTTER DILATION on mip-0 (coverage flood) — the cache stores the POST-dilation PNG so a WARM load
      # is byte-identical + needs no re-dilate at mip-0 (mips still coverage-weight per level below).
      _dilate_coverage_rgba8(rgba, _SECTION_DILATE_TEXELS)
      _PILImage.fromarray(rgba, "RGBA").save(p)         # write the cache PNG (cold, mip-0 only)
    txi.uploadTextureRegion(tex=arr.tex, mip_level=0, array_layer=i,
                            offset_x=0, offset_y=0, offset_z=0,
                            extent_w=int(bake_res), extent_h=int(bake_res), extent_d=1,
                            data=rgba.tobytes())
    if use_mips:
      # COVERAGE-WEIGHTED mip chain (successive halving from mip-0). Each level: coverage-weighted
      # downsample -> zero-coverage flood fill -> (SectionNormal) renorm. This excludes gutter-black at
      # every level so deep mips do not darken as charts shrink (the C++ v5 fix — a plain box filter left
      # deep-mip black gutters). mip-0 (already gutter-dilated) seeds the chain.
      cur = rgba
      for m in range(1, num_mips):
        mip = _cov_weighted_downsample(cur)
        mh, mw = mip.shape[0], mip.shape[1]
        _dilate_coverage_rgba8(mip, mw + mh)           # flood any fully-gutter 2x2 that left a hole
        if renorm:
          _renorm_normals_inplace(mip)
        mip = np.ascontiguousarray(mip)
        txi.uploadTextureRegion(tex=arr.tex, mip_level=int(m), array_layer=i,
                                offset_x=0, offset_y=0, offset_z=0,
                                extent_w=mw, extent_h=mh, extent_d=1, data=mip.tobytes())
        cur = mip                                       # this level becomes the parent of the next
  txi.finalizeUpload(tex=arr.tex)
  dt = time.time() - t0
  print("section_bake: %s array %d layers @ %dpx mips=%d -> %s (%.1f ms)  dir=%s"
        % ("WARM(load)" if warm else "COLD(bake+cache)", num_layers, bake_res, num_mips,
           "loaded" if warm else "wrote", dt * 1e3, cache_dir), flush=True)
  return arr, cache_dir, warm


def section_array_warm(key, num_layers, bake_res=256):
  """True iff the content-addressed cache already holds every layer PNG (a WARM run needs no GPU bake)."""
  cache_dir = section_cache_dir(key, bake_res, num_layers)
  paths = [os.path.join(cache_dir, "layer%02d.png" % i) for i in range(int(num_layers))]
  return all(os.path.isfile(p) for p in paths), cache_dir


def job_content_fn(job, target=0):
  """A content_fn(layer,res)->HxWx4 uint8 backed by a completed SectionBakeJob's per-layer host capture
  (the GPU-baked REAL section surface). Pass to bake_section_array(content_fn=...) once job.is_ready."""
  def _fn(layer, res):
    cb = job.layerCapture(layer, int(target))
    if cb is None:
      raise RuntimeError("job_content_fn: SectionBakeJob has no capture for layer %d target %d" % (layer, target))
    rgba = np.array(cb, dtype=np.uint8).reshape(cb.height, cb.width, 4)
    if rgba.shape[0] != res or rgba.shape[1] != res:
      from PIL import Image as _PILImage
      rgba = np.asarray(_PILImage.fromarray(rgba, "RGBA").resize((int(res), int(res))), dtype=np.uint8)
    return np.ascontiguousarray(rgba)
  return _fn


__all__ = ["bake_section_array", "placeholder_section_array", "section_cache_dir",
           "section_array_warm", "job_content_fn", "SECTION_BAKE_EPOCH"]
