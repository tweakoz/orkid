#!/usr/bin/env ork.python
################################################################################
# Taskless VK_EXT_mesh_shader gate — minimal end-to-end proof.
#
# One mesh workgroup emits ONE solid-color triangle straight into an offscreen
# RtGroup; the readback asserts the triangle interior carries the shader's color
# and the four corners still carry the clear color. Nothing about this frame can
# be produced without the mesh stage: there is no vertex buffer, no index buffer
# and no vertex/input-assembly state in the pipeline at all.
#
# TASKLESS by construction: no task (amplification) stage exists anywhere in this
# shader or in the engine path it drives — MoltenVK's mesh-shader support reports
# taskShader=false, and emitting one would fail device-feature validation.
#
# Runtime-gated: on a device without VK_EXT_mesh_shader (any stock MoltenVK, any
# driver that lacks the ext) ctx.supports_mesh_shader is False and the test emits
# an explicit SKIP + a PASS verdict, so the canary stays green fleet-wide. That
# gate is a real caps query, NOT a try/except around a compile.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

# repo root = five levels up; prepend THIS checkout's scripts dir so ork.testing
# resolves from the same tree as this test (the sibling worktree-shadowing idiom).
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine import core   # core before lev2
from orkengine import lev2
from ork.testing import headless_app, verdict

tokens = core.CrcStringProxy()

W, H = 64, 64
CLEAR = (0, 0, 0)      # RtBuffer clear color
TRICLR = (255, 0, 0)   # what the fragment shader writes (byte-exact in RGBA8)

################################################################################
# The mesh stage's workgroup size and its max output counts are pipeline layout,
# not shader body statements — they ride on the interface the stage inherits (the
# mesh stage stands in for the vertex stage, so it carries the vertex interface).
################################################################################

SHADER = """
fxconfig fxcfg_default {}
////////////////////////////////////////
vertex_interface iface_mesh {
  inputs {
    layout(local_size_x = 1, local_size_y = 1, local_size_z = 1);
  }
  outputs {
    layout(triangles, max_vertices = 3, max_primitives = 1);
  }
}
fragment_interface iface_frg {
  outputs {
    layout(location = 0) vec4 out_clr;
  }
}
////////////////////////////////////////
mesh_shader ms_tri : extension(GL_EXT_mesh_shader) : iface_mesh {
  SetMeshOutputsEXT(3u, 1u);
  gl_MeshVerticesEXT[0].gl_Position = vec4(-0.8, -0.8, 0.0, 1.0);
  gl_MeshVerticesEXT[1].gl_Position = vec4(0.8, -0.8, 0.0, 1.0);
  gl_MeshVerticesEXT[2].gl_Position = vec4(0.0, 0.8, 0.0, 1.0);
  gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0u, 1u, 2u);
}
fragment_shader ps_tri : iface_frg {
  out_clr = vec4(1.0, 0.0, 0.0, 1.0);
}
////////////////////////////////////////
technique tek_mesh {
  fxconfig = fxcfg_default;
  pass p0 {
    mesh_shader = ms_tri;
    fragment_shader = ps_tri;
    state_block = default;
  }
}
"""


def _render(app):
  """Draw the mesh-emitted triangle into a fresh RtGroup and return it as HxWx4 uint8."""
  import numpy
  ctx = app.ctx

  mtl = lev2.FreestyleMaterial()
  mtl.gpuInitFromShaderText(ctx, "mesh_minimal", SHADER)
  # culling OFF: the mesh stage's winding is authored in NDC, and the viewport is
  # Y-flipped — which face ends up front is not the thing under test here.
  mtl.rasterstate.culltest = tokens.OFF

  permu = lev2.FxPipelinePermutation()
  permu.rendermodel = "CUSTOM"
  permu.technique = mtl.shader.technique("tek_mesh")
  assert permu.technique, "technique tek_mesh not found"
  pipeline = mtl.fxcache.findPipeline(permu)

  rtg = lev2.RtGroup(ctx, W, H)
  rtb = rtg.createBuffer(tokens.RGBA8, tokens.color)
  rtb.clearColor = core.vec4(CLEAR[0], CLEAR[1], CLEAR[2], 1)

  capbuf = lev2.CaptureBuffer()
  ctx.beginFrame()
  ctx.FBI.rtGroupPush(rtg)
  ctx.FBI.rtGroupClear(rtg)

  RCFD = lev2.RenderContextFrameData(ctx)
  RCID = lev2.RenderContextInstData(RCFD)
  RCID.forceTechnique(permu.technique)
  RCID.genMatrix(lambda: core.mtx4())

  # ONE mesh workgroup — the entire draw. No vertex buffer, no index buffer.
  pipeline.wrappedDrawCall(RCID, lambda: ctx.GBI.drawMeshTasks(1, 1, 1))

  future = ctx.FBI.captureAsFormat(rtb, capbuf, "RGBA8")
  ctx.FBI.rtGroupPop()
  ctx.endFrame()

  frames = 0
  while not future.is_ready:
    app.run_frames(1)
    frames += 1
    assert frames < 600, "mesh-shader capture never became ready"

  return numpy.array(capbuf, dtype=numpy.uint8).reshape(capbuf.height, capbuf.width, 4)


def main():
  code = 1
  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    if not app.ctx.supports_mesh_shader:
      print("SKIP: device has no VK_EXT_mesh_shader (taskless mesh path not exercised)")
      code = verdict(True, "mesh shader gate SKIPPED — VK_EXT_mesh_shader unavailable on this device")
    else:
      arr = _render(app)
      rgb = arr[..., :3]

      center = tuple(int(v) for v in rgb[H // 2, W // 2])
      corners = {
          "tl": tuple(int(v) for v in rgb[2, 2]),
          "tr": tuple(int(v) for v in rgb[2, W - 3]),
          "bl": tuple(int(v) for v in rgb[H - 3, 2]),
          "br": tuple(int(v) for v in rgb[H - 3, W - 3]),
      }
      covered = int((rgb == TRICLR).all(axis=2).sum())
      total = W * H

      fails = []
      if center != TRICLR:
        fails.append("center=%s want %s" % (center, TRICLR))
      for name, px in corners.items():
        if px != CLEAR:
          fails.append("corner-%s=%s want %s" % (name, px, CLEAR))
      # a real triangle covers part of the frame, never none and never all of it
      if not (0.05 * total < covered < 0.75 * total):
        fails.append("covered=%d/%d outside triangle-shaped range" % (covered, total))

      detail = "mesh shader gate | center=%s covered=%d/%d corners=%s" % (
          center, covered, total, sorted(corners.items()))
      code = verdict(not fails, detail if not fails else detail + " | " + "; ".join(fails))

  sys.exit(code)


main()
