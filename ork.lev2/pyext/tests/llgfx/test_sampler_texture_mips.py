#!/usr/bin/env ork.python
###############################################################################
# sampler-texture MIP gate.
#
# Ptex3d/PbrMaterialGenData sampler_textures bindings (baked terrain atlases, cloud
# decks, leaf/needle cards) upload through TXI::initTextureFromImage with mips ON.
# Two things must BOTH hold or the flag is a lie:
#
#   1. the upload allocates and fills a full mip chain
#      (num_mips == 1 + floor(log2(min(w,h))), the Vulkan uploader's level count)
#   2. the texture's minify filter ends up MIP-ENABLED — the sampling-mode default is
#      plain LINEAR, which lowers to a non-trilinear mip mode with the LOD range
#      clamped below a large atlas's chain tail; a chain the sampler cannot reach
#      samples mip 0 and aliases exactly as it did before the chain existed.
#
# Covered: power-of-two RGB, NON-power-of-two RGBA (straight alpha — OIIO loads PNG
# unassociated), a wide non-square (the chain stops at the SHORT side), and the
# mipmapped=False control, which must stay single-level with an unmipped filter —
# mips are the default for the sampler-texture path, not for every upload.
###############################################################################
import math
import os
import sys
import tempfile

from orkengine import core   # core MUST be imported before lev2
from orkengine import lev2

from ork.testing import headless_app, verdict


def _write_png(path, w, h, alpha):
  img = (lev2.Image.createRGBA8FromColor(w, h, core.vec4(0.25, 0.5, 0.75, 0.5)) if alpha
         else lev2.Image.createRGB8FromColor(w, h, core.vec3(0.25, 0.5, 0.75)))
  img.writeToFile(path)
  return path


def _expected_mips(w, h):
  return 1 + int(math.floor(math.log2(min(w, h))))


CASES = (
  # label               w    h    alpha  mipmapped
  ("pot_rgb",           256, 256, False, True),
  ("npot_rgba_alpha",   300, 180, True,  True),
  ("wide_nonsquare",    512, 128, True,  True),
  ("control_unmipped",  256, 256, True,  False),
)

MIP_FILTERS = ("LINEAR_MIPMAP_LINEAR", "LINEAR_MIPMAP_NEAREST",
               "NEAREST_MIPMAP_LINEAR", "NEAREST_MIPMAP_NEAREST")


def main():
  tmp = tempfile.mkdtemp(prefix="samplermips_")
  results = []
  rc = 1

  with headless_app(width=64, height=64) as app:
    for label, w, h, alpha, mipmapped in CASES:
      path = _write_png(os.path.join(tmp, label + ".png"), w, h, alpha)
      img = lev2.Image.createFromFile(path)
      tex = lev2.Texture(label)
      # async=False (positional — `async` is a Python keyword): the sampler-texture
      # materialize path uploads synchronously, outside any render pass, which is what
      # the Vulkan backend asserts on.
      app.ctx.TXI.updateTexture(tex, img, False, mipmapped)

      want_mips = _expected_mips(w, h) if mipmapped else 1
      got_mips = tex.num_mips
      got_filt = tex.min_filter
      filt_ok = (got_filt in MIP_FILTERS) if mipmapped else (got_filt not in MIP_FILTERS)
      ok = (got_mips == want_mips) and filt_ok
      results.append(ok)
      print("[%-17s] %4dx%-4d mipmapped=%d -> num_mips=%d (want %d) min_filter=%s%s"
            % (label, w, h, mipmapped, got_mips, want_mips, got_filt,
               "" if ok else "   <-- FAIL"), flush=True)

    n_ok = sum(1 for ok in results if ok)
    rc = verdict(n_ok == len(results),
                 "sampler-texture mips %d/%d cases" % (n_ok, len(results)))

  sys.exit(rc)


if __name__ == "__main__":
  main()
