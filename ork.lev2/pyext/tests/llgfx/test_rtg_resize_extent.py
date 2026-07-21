#!/usr/bin/env ork.python
"""
RTG resize extent oracle — regression guard for the HZB-vs-resize bug class.

RtGroup-owned Texture metadata (_width/_height) is write-once in the RtBuffer ctor. The bug:
RtGroup::Resize updated the RtBuffer dims + miW/miH but NOT the backing Texture objects, and never
touched the depth buffer at all — so after a resize a group's color/depth Textures kept reporting
the first-paint size forever. HZBBuilder::build reads depth->_width/_height as its SOLE extent
source, so the occlusion pyramid + its mip0 texelFetch clamp locked at the original size, and the
per-view instance cull mapped fresh NDC onto a stale pyramid (post-maximize banding / false culls).

This drives the real backend-agnostic class seam (RtGroup::Resize, via the `resize` binding) and
asserts every extent field follows the new size: rtg.width/height, each color texture, and the
depth texture. It does NOT drive the full HZB rebuild (that needs a live forward-rendered scene +
a real window resize — the owner windowed maximize+turn in scn_scatter is the final verify). But
HZBBuilder derives _baseW/_baseH deterministically from depth->_width/_height, so a depth texture
whose extent follows the resize is exactly what closes that loop.

Ported onto ork.testing.headless_app: the harness owns the proven init order + the update-stop /
settle / shutdown teardown sequence, so this oracle is just the inline GPU checks. The observable
line "=== rtg resize extent oracle PASSED ===" is unchanged (gate briefs quote it).
"""
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys; sys.stdout.reconfigure(line_buffering=True)

# repo root = five levels up; prepend THIS checkout's scripts dir so ork.testing resolves
# from the same tree as this test (mirrors the sibling worktree-shadowing idiom).
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine import core   # core before lev2
from orkengine import lev2
from ork.testing import headless_app

tokens = core.CrcStringProxy()

W0, H0 = 512, 384    # first-paint extent
W1, H1 = 1024, 768   # resized extent


def main():
  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    ctx = app.ctx

    rtg   = lev2.RtGroup(ctx, W0, H0)
    color = rtg.createBuffer(tokens.RGBA32F, tokens.color)  # with_texture defaults true
    rtg.createDepthBuffer(tokens.Z32F, True)
    depth = rtg.depth_buffer

    failures = []

    def check(label, got, want):
      ok = (got == want)
      print(f"  {'PASS' if ok else 'FAIL'} {label}: got {got}, want {want}", flush=True)
      if not ok:
        failures.append(label)

    print(f"=== initial extent {W0}x{H0} ===", flush=True)
    check("rtg.width", rtg.width, W0)
    check("rtg.height", rtg.height, H0)
    check("color.texture.width", color.texture.width, W0)
    check("color.texture.height", color.texture.height, H0)
    check("depth.texture.width", depth.texture.width, W0)
    check("depth.texture.height", depth.texture.height, H0)

    print(f"=== resize -> {W1}x{H1} (RtGroup::Resize) ===", flush=True)
    rtg.resize(W1, H1)

    check("rtg.width", rtg.width, W1)
    check("rtg.height", rtg.height, H1)
    check("color.texture.width", color.texture.width, W1)
    check("color.texture.height", color.texture.height, H1)
    check("depth.texture.width", depth.texture.width, W1)   # the HZB extent source
    check("depth.texture.height", depth.texture.height, H1)

    ok = (len(failures) == 0)
    print(f"=== rtg resize extent oracle {'PASSED' if ok else 'FAILED (' + ','.join(failures) + ')'} ===", flush=True)

  sys.exit(0 if ok else 1)


main()
