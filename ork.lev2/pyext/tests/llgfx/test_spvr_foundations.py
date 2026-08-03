#!/usr/bin/env ork.python
"""
Single-pass-stereo FOUNDATIONS oracle — the layered/multiview plumbing, on its own.

Everything single-pass stereo rests on is checked here before any pixel is compared:

  * the DEVICE actually reports core multiview and a view count of at least 2. Without
    that the whole approach is unavailable, and it must say so out loud rather than
    fail later as a validation error nobody attributes to a missing feature bit.
  * a 2-layer RtGroup CONSTRUCTS - color plus depth, both carrying the layer count - and
    RENDERS: it is pushed as the active render target, cleared, popped, and the frame
    completes. Allocation alone says nothing about the backing VkImage's arrayLayers or
    the attachment view's layerCount; a pass that BINDS them is what makes the validator
    speak up about a mismatched layer count.
  * viewMask() is 0b11 when a group is BOTH layered and flagged multiview, and 0 for
    every other shape. This accessor is the single source both vulkan rendering
    structs read; a wrong answer here is invalid-at-draw-time, not a cosmetic bug.
  * legacy RTGs are UNAFFECTED - a plain 1-layer group still reports viewMask 0, which
    is what keeps every existing scene on exactly the path it was on before.
  * a SHAPE MATRIX of layered render targets all push/clear/pop cleanly. Slice 1 extended
    three separate image paths (the RTG texture arm, the textureless arm, and the MSAA
    twin) to carry a layer count, but only the plain 2-layer colour+depth shape was ever
    exercised; the matrix walks the others so an untested arm cannot rot unnoticed.

Deliberately runs with the VALIDATOR ARMED (set from inside this file, so the test
needs nothing from the shell): a layered attachment set that the driver rejects shows
up as a validation error, and a silent validator would hide it.
"""
import os

# self-configuring: the armed validator is part of what this test asserts, so it is set
# HERE rather than expected from the environment. Must precede engine import/init.
os.environ["ORKID_VULKAN_VALIDATE"] = "2"
os.environ["PYTHONUNBUFFERED"] = "1"

import sys
sys.stdout.reconfigure(line_buffering=True)

# repo root = five levels up; prepend THIS checkout's scripts dir so ork.testing resolves
# from the same tree as this test (mirrors the sibling worktree-shadowing idiom).
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine import core   # core before lev2
from orkengine import lev2
from ork.testing import headless_app, verdict

tokens = core.CrcStringProxy()

W, H = 256, 192
NUM_VIEWS = 2


def main():
  rc = 1
  fails = []
  notes = []

  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    ctx = app.ctx

    ############################################################
    # device capability
    ############################################################
    supports = bool(ctx.supports_multiview)
    maxviews = int(ctx.max_multiview_views)
    notes.append("multiview=%d maxviews=%d" % (int(supports), maxviews))
    if not supports:
      fails.append("device reports NO core multiview support")
    if maxviews < NUM_VIEWS:
      fails.append("maxMultiviewViewCount=%d < %d" % (maxviews, NUM_VIEWS))

    ############################################################
    # legacy RTG is untouched: 1 layer, viewMask 0
    ############################################################
    rtg_mono = lev2.RtGroup(ctx, W, H)
    rtg_mono.createBuffer(tokens.RGBA8, tokens.color)
    if int(rtg_mono.numLayers) != 1:
      fails.append("legacy RTG numLayers=%d (expected 1)" % int(rtg_mono.numLayers))
    if int(rtg_mono.viewMask) != 0:
      fails.append("legacy RTG viewMask=0x%x (expected 0)" % int(rtg_mono.viewMask))

    ############################################################
    # layered-but-NOT-multiview: still viewMask 0. Layering alone must not arm a
    # multiview pass, or an array render target picked up for some other purpose
    # would silently start broadcasting draws.
    ############################################################
    rtg_layered = lev2.RtGroup(ctx, W, H)
    rtg_layered.numLayers = NUM_VIEWS
    rtg_layered.createBuffer(tokens.RGBA8, tokens.color)
    if int(rtg_layered.viewMask) != 0:
      fails.append("layered-only RTG viewMask=0x%x (expected 0)" % int(rtg_layered.viewMask))

    ############################################################
    # layered AND multiview: viewMask 0b11
    ############################################################
    rtg_mv = lev2.RtGroup(ctx, W, H)
    rtg_mv.numLayers = NUM_VIEWS
    rtg_mv.multiview = True
    rtb_mv = rtg_mv.createBuffer(tokens.RGBA8, tokens.color)
    rtb_mv.clearColor = core.vec4(0.2, 0.4, 0.6, 1)
    rtg_mv.createDepthBuffer(tokens.Z32F, True)
    expect_mask = (1 << NUM_VIEWS) - 1
    if int(rtg_mv.viewMask) != expect_mask:
      fails.append("multiview RTG viewMask=0x%x (expected 0x%x)"
                   % (int(rtg_mv.viewMask), expect_mask))

    ############################################################
    # the layered group must actually RENDER. This is the leg that exercises the 2D-ARRAY
    # image arm, the 2-layer attachment view, and the layered clear - with the validator
    # armed, so a layer-count mismatch between the color and depth attachments (or between
    # an attachment and the pass's viewMask) surfaces as an error rather than as a quietly
    # wrong image. Reaching the end of the loop at all IS the pass condition.
    ############################################################
    for i in range(4):
      ctx.beginFrame()
      ctx.FBI.rtGroupPush(rtg_mv)
      ctx.FBI.rtGroupClear(rtg_mv)
      ctx.FBI.rtGroupPop()
      ctx.endFrame()
    notes.append("layered-render-frames=4")

    ############################################################
    # SHAPE MATRIX. Each entry is (layers, multiview, depth, msaa) plus the viewMask the
    # accessor must report. Every shape is pushed as a render target and taken through a
    # whole frame: with the validator armed, a layer-count disagreement between colour,
    # depth and the MSAA twin surfaces as an error rather than as a quietly wrong image.
    # msaa=4 and layers=4 are the arms slice 1 added but nothing had ever rendered through.
    ############################################################
    SHAPES = [
      # layers, multiview, depth, msaa, expected viewMask
      (1, False, True,  1, 0x0),   # legacy: unchanged by everything here
      (2, False, True,  1, 0x0),   # layered but NOT multiview: layering alone must not arm a pass
      (2, True,  True,  1, 0x3),   # the shipped stereo shape
      (2, True,  False, 1, 0x3),   # textureless/depthless arm
      (2, True,  True,  4, 0x3),   # layered + MSAA twin
      (4, True,  True,  1, 0xf),   # wider mask than stereo ever needs
    ]
    for (layers, mv, depth, msaa, want_mask) in SHAPES:
      tag = "L%d/mv%d/d%d/msaa%d" % (layers, int(mv), int(depth), msaa)
      rtg = lev2.RtGroup(ctx, W, H, msaa=msaa)
      rtg.numLayers = layers
      rtg.multiview = mv
      rtb = rtg.createBuffer(tokens.RGBA8, tokens.color)
      rtb.clearColor = core.vec4(0.2, 0.4, 0.6, 1)
      if depth:
        rtg.createDepthBuffer(tokens.Z32F, True)
      if int(rtg.viewMask) != want_mask:
        fails.append("%s viewMask=0x%x (expected 0x%x)" % (tag, int(rtg.viewMask), want_mask))
        continue
      ctx.beginFrame()
      ctx.FBI.rtGroupPush(rtg)
      ctx.FBI.rtGroupClear(rtg)
      ctx.FBI.rtGroupPop()
      ctx.endFrame()
    notes.append("shapes=%d" % len(SHAPES))

    ok = (len(fails) == 0)
    detail = "spvr foundations | %s | %s" % (
        " ".join(notes),
        "OK" if ok else ("FAILS: " + "; ".join(fails)))
    print("=== spvr foundations %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    rc = verdict(ok, detail)   # verdict BEFORE teardown

  sys.exit(rc)


main()
