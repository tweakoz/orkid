#!/usr/bin/env ork.python
################################################################################
# ptex3d TEMPLATE mesh-shader splice gate — the material a REAL SCENE gets.
#
# test_terrain_meshshader_ab.py proves the raw generator's two paths agree in a
# hand-written program. This one proves the same through the PRODUCTION assembly:
# the ptex3d template (fxv2_template.generate/materialize_surface_fxv2) fed by
# TerrainChunkVertexSource, i.e. exactly what ork.hypergraph.ecs.scene.terrain()
# materializes — so FWD_SSBO_CUSTOM_MESH + FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS
# exist on a real material and carry the full forward varying contract.
#
#   PHASE A (no GPU) — BYTE-STABILITY. The mesh splice must be purely ADDITIVE:
#     * every mesh-OFF config still generates its recorded byte-identical text
#       (digests below were taken from the pre-splice generators), and
#     * mesh-ON text differs from mesh-OFF by exactly ONE contiguous insertion.
#     Toggle-off byte-identity is what keeps every existing material's cached
#     shader (and every downstream content-addressed bake) exactly where it was.
#
#   PHASE B (GPU) — PIXEL PARITY through the template. One generated material,
#     one SSBO, four techniques: the SSBO-pull VS pair vs the mesh pair, color
#     and depth-prepass. Same varyings + same fragment => same image. The colour
#     material is UNLIT (out_clr = emissive) so the comparison is a pure function
#     of the varyings the mesh stage array-ifies, with no scene lighting state.
#
# Fully synthetic (procedural heightfield, generated shader, no scene assets, no
# window). Runtime-gated on ctx.supports_mesh_shader: a device without
# VK_EXT_mesh_shader SKIPs clean after phase A, so the canary stays green fleet-wide.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)
import difflib
import hashlib

# repo root = five levels up; prepend THIS checkout's scripts dir so ork.testing +
# the ptex3d template resolve from the same tree as this test.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import numpy

from orkengine import core   # core before lev2
from orkengine import lev2
from ork.testing import headless_app, verdict
from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource
from ork.hypergraph.ptex3d.fxv2_template import generate_surface_fxv2, materialize_surface_fxv2

tokens = core.CrcStringProxy()

################################################################################
# PHASE A — byte stability
################################################################################

# The surface body the digests below were taken with. It reads the varyings the
# mesh stage must reproduce (uv / world normal / object position), so phase B's
# image is a direct readout of the varying contract.
BODY = ("o.albedo   = vec3(uv.x, uv.y, 0.5);\n"
        "o.emissive = (wnrm * 0.5 + vec3(0.5)) * 0.55\n"
        "           + vec3(uv.x, uv.y, fract(opos.y * 0.02)) * 0.45;")

CAP_BODY = "c.base = vec4(uv, 0.0, 1.0);\nc.nrmao = vec4(wnrm, 1.0);"

# Generator configs, keyed by name. `mesh` / `relax` drive the vertex source; the
# rest are template kwargs. The *_off configs are the byte-stability population.
CONFIGS = {
  "terr_mono_off":        dict(),
  "terr_mono_on":         dict(mesh=True),
  "terr_relax_off":       dict(relax=True),
  "terr_relax_on":        dict(relax=True, mesh=True),
  "terr_mono_cap_off":    dict(wants_capture=True, capture_targets=("base", "nrmao"), capture_body=CAP_BODY),
  "terr_mono_cap_on":     dict(mesh=True, wants_capture=True, capture_targets=("base", "nrmao"), capture_body=CAP_BODY),
  "terr_relax_cap_off":   dict(relax=True, wants_capture=True, capture_targets=("base", "nrmao"), capture_body=CAP_BODY),
  "terr_relax_cap_on":    dict(relax=True, mesh=True, wants_capture=True, capture_targets=("base", "nrmao"), capture_body=CAP_BODY),
  "terr_mono_masked_off": dict(masked_dpp_expr="_a", masked_dpp_body="float _a = uv.x;"),
  "terr_mono_masked_on":  dict(mesh=True, masked_dpp_expr="_a", masked_dpp_body="float _a = uv.x;"),
}

# sha1 of the generated text for every mesh-OFF config. A mismatch means the template
# moved existing materials' shader text (every cached shader + every downstream
# content-addressed bake re-keys). Regenerate ONLY with a deliberate template
# contract change: run this test, take the printed digests, and say so in the
# commit — do not "fix" a red gate by pasting the new numbers.
# RE-RECORDED at the single-pass-stereo (_ST) technique lowering: every generated
# family now emits its per-view peer alongside the mono technique, so every whole-file
# digest moved by exactly that addition. The MONO half is unmoved and is proved so
# independently — test_spvr_ptex3d_st_lowering_gate regenerates with the lowering
# disarmed and compares against the pre-lowering generator, digest for digest.
GOLDEN = {
  "terr_mono_off":        "3d0951bec07354291ec2ab491e3d8a7c789664da",
  "terr_relax_off":       "ab6247b66039d7f397ff49013c6106b758fdce82",
  "terr_mono_cap_off":    "db9e93545b8d5c798dd4d7e76f6d9b152218b8b8",
  "terr_relax_cap_off":   "ab3d7e593b933fe1fc0b49b042e40d11571db552",
  "terr_mono_masked_off": "7eae9e9172d87ccb96921a834b8a8e858976bbb0",
  "plain_rigid":          "c8c92cbdb3a03ab9a861b12fc822187938516437",
  "inst_impostor":        "478898c42059cfd8f5b078a49953ed653956344a",
}

# the non-terrain populations: a plain rigid material (no vertex source at all)
# and an instanced+impostor SSBO material (the hypermesh shape).
INST_KWARGS = dict(
  ssbo_layout="float Pos[];",
  ssbo_vs_body=("uint i = uint(gl_VertexID);\n"
                "vec4 position = vec4(Pos[i*3u], Pos[i*3u+1u], Pos[i*3u+2u], 1.0);\n"
                "vec3 normal = vec3(0,1,0);\nvec3 binormal = vec3(1,0,0);\n"
                "vec2 uv0 = vec2(0.0);\nvec4 vtxcolor = vec4(1.0);"),
  ssbo_instanced=True,
  wants_capture=True)

GEN_DIM, GEN_EXTENT, GEN_CHUNK = 1024, 1000.0, 128


def gen_text(name):
  kw = dict(CONFIGS[name])
  vs = TerrainChunkVertexSource(dim=GEN_DIM, extent_m=GEN_EXTENT, chunk=GEN_CHUNK,
                                relax=kw.pop("relax", False), mesh=kw.pop("mesh", False))
  return generate_surface_fxv2(BODY, **vs.as_material_kwargs(), **kw)


def phase_a():
  """-> (fails, digests). Pure text; runs before the GPU exists so a broken
  splice is reported even on a machine that cannot render."""
  fails, digests = [], {}
  texts = {}
  for name in sorted(CONFIGS):
    texts[name] = gen_text(name)
  texts["plain_rigid"]   = generate_surface_fxv2(BODY)
  texts["inst_impostor"] = generate_surface_fxv2(BODY, **INST_KWARGS)
  for name, txt in sorted(texts.items()):
    digests[name] = hashlib.sha1(txt.encode("utf-8")).hexdigest()
  ##############################################
  # 1. recorded byte-identity for every mesh-OFF material
  ##############################################
  for name, want in sorted(GOLDEN.items()):
    got = digests[name]
    if got != want:
      fails.append("byte-stability: %s sha1 %s != recorded %s" % (name, got, want))
  ##############################################
  # 2. mesh-OFF text must carry NO mesh stage (a device without the extension
  #    must never be handed mesh SPIR-V — shader modules are created for every
  #    stage in the DB at load, not lazily at pipeline time)
  ##############################################
  for name in sorted(CONFIGS):
    if name.endswith("_off") and ("mesh_shader" in texts[name] or "FWD_SSBO_CUSTOM_MESH" in texts[name]):
      fails.append("%s (mesh off) leaked a mesh stage into the shader text" % name)
  ##############################################
  # 3. ADDITIVE-ONLY: off -> on is exactly one contiguous insertion
  ##############################################
  for name in sorted(CONFIGS):
    if not name.endswith("_on"):
      continue
    off = texts[name[:-3] + "_off"].splitlines()
    on  = texts[name].splitlines()
    ops = [o for o in difflib.SequenceMatcher(None, off, on, autojunk=False).get_opcodes()
           if o[0] != "equal"]
    if len(ops) != 1 or ops[0][0] != "insert":
      fails.append("%s: mesh splice is not a single insertion (%s)" % (name, [o[0] for o in ops]))
    else:
      _, _, _, j1, j2 = ops[0]
      if ("technique FWD_SSBO_CUSTOM_MESH {" not in on[j1:j2] or
          "technique FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS {" not in on[j1:j2]):
        fails.append("%s: inserted block lacks the mesh technique pair" % name)
  return fails, digests

################################################################################
# PHASE B — pixel parity through the template
################################################################################

DIM    = 512      # heightfield grid (procedural)
EXTENT = 1000.0   # world meters per side
HMAX   = 90.0     # world meters of relief
CHUNK  = 128      # -> 4x4 chunks
W, H   = 256, 256

PARITY_MIN_EXACT = 0.995   # fraction of byte-identical pixels required
PARITY_MAX_MEAN  = 1.0     # mean |delta| in 8-bit levels
MIN_COVERAGE     = 0.05    # a rendered view must fill at least this much

EYE = core.vec3(0.0, 420.0, 900.0)
TGT = core.vec3(0.0, 0.0, 0.0)


def put_uints(fxi, ssbo, offset, values):
  """Write consecutive uint32 slots. One scalar call per slot — the int path writes a
  true 4-byte int32 (bit-identical to the uint the shader reads) while the array path
  force-casts to float32."""
  for i, v in enumerate(values):
    fxi.copyDataIntoShaderStorageBuffer(int(v), ssbo, offset + i * 4)


def synth_heights(dim, hmax):
  """Procedural heightfield in TRUE METERS (ridges + a central dome) — the SSBO's
  heights[] contract. No asset, no bake, no disk."""
  xs = (numpy.arange(dim, dtype=numpy.float32) + 0.5) / float(dim) - 0.5
  X, Z = numpy.meshgrid(xs, xs, indexing="xy")
  h = (0.55
       + 0.25 * numpy.sin(X * 14.0) * numpy.cos(Z * 11.0)
       + 0.20 * numpy.exp(-((X * 2.4) ** 2 + (Z * 2.4) ** 2)))
  return (numpy.clip(h, 0.0, 1.0) * hmax).astype(numpy.float32).reshape(-1)


class TemplateAB:
  """The generated ptex3d material rendered through its four SSBO techniques."""

  def __init__(self, app):
    self.app = app
    self.ctx = app.ctx
    fxi = self.ctx.FXI
    self.vs = TerrainChunkVertexSource(dim=DIM, extent_m=EXTENT, chunk=CHUNK, mesh=True)

    ############################################
    # the material: THE production assembly (same call ecs.scene.terrain makes)
    ############################################
    self.path = materialize_surface_fxv2(BODY, surface_mode="unlit", name_hint="meshsplice_gate",
                                         **self.vs.as_material_kwargs())
    self.mtl = lev2.FreestyleMaterial()
    self.mtl.gpuInit(self.ctx, self.path)
    self.mtl.rasterstate.culltest  = tokens.OFF        # one winding, not under test
    self.mtl.rasterstate.depthtest = tokens.LEQUALS    # heightfield self-occludes

    ############################################
    # the SSBO. The cull compute is NOT run here (this gate is about the template,
    # not the cull): the visible-chunk list + the draw command are written from the
    # host so the pull path draws every chunk. The mesh path still self-culls per
    # meshlet — off-frustum meshlets contribute no pixels either way.
    ############################################
    self.heights = synth_heights(DIM, HMAX)
    self.ssbo = fxi.createShaderStorageBufferWithLength(self.vs.TOTAL)
    fxi.copyDataIntoShaderStorageBuffer(self.heights, self.ssbo, self.vs.HEIGHTS_OFF)
    self.vs.upload_dim(fxi, self.ssbo)
    fxi.copyDataIntoShaderStorageBuffer(
        numpy.array([float(self.heights.min()), float(self.heights.max())], dtype=numpy.float32),
        self.ssbo, self.vs.YB_OFF)
    nchunk = self.vs.nchunk
    # UINT slots go through the SCALAR-int path one at a time: the numpy path of
    # copyDataIntoShaderStorageBuffer force-casts to float32 (pyext_gfx.cpp), which would
    # write 16.0f where the shader reads a uint 16.
    put_uints(fxi, self.ssbo, self.vs.ARGS_OFF, [nchunk * self.vs.vpc, 1, 0, 0])  # VkDrawIndirectCommand
    put_uints(fxi, self.ssbo, self.vs.VIS_OFF, [nchunk, nchunk, DIM, nchunk])     # v_count, frustum, u_dim, total
    put_uints(fxi, self.ssbo, self.vs.VLIST_OFF, range(nchunk))                   # every chunk visible
    self.set_view(EYE, TGT)
    assert self.vs.read_dim(fxi, self.ssbo) == DIM, "u_dim did not land in the VIS header"

    ############################################
    # the four techniques + their pipelines
    ############################################
    self.teks = {}
    self.pipe = {}
    for key, name in (("color_vtx",  "FWD_SSBO_CUSTOM"),
                      ("color_mesh", "FWD_SSBO_CUSTOM_MESH"),
                      ("depth_vtx",  "FWD_SSBO_CUSTOM_DEPTHPREPASS"),
                      ("depth_mesh", "FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS")):
      tek = self.mtl.shader.technique(name)
      assert tek, "generated material has no technique %s" % name
      permu = lev2.FxPipelinePermutation()
      permu.rendermodel = "CUSTOM"
      permu.technique = tek
      pipe = self.mtl.fxcache.findPipeline(permu)
      assert pipe, "no pipeline for %s" % name
      pipe.bindStorage(self.mtl.storage("sif_ptex_vtx"), self.ssbo)
      pipe.bindParam(self.mtl.param("mvp"), self.vp)
      pipe.bindParam(self.mtl.param("m"), core.mtx4())
      self.teks[key] = tek
      self.pipe[key] = pipe

    self.groups = self.vs.mesh_groups()

    ############################################
    # offscreen targets: the forward fragment is dual-MRT (out_clr, out_diffuse),
    # the depth fragment single (out_z -> the R channel).
    ############################################
    self.rtg_color = lev2.RtGroup(self.ctx, W, H)
    self.rtb_color = self.rtg_color.createBuffer(tokens.RGBA8, tokens.color)
    self.rtb_color.clearColor = core.vec4(0, 0, 0, 1)
    self.rtg_color.createBuffer(tokens.RGBA8, tokens.color)
    self.rtg_color.createDepthBuffer(tokens.Z32F, True)
    self.rtg_color.autoclear = True

    self.rtg_depth = lev2.RtGroup(self.ctx, W, H)
    self.rtb_depth = self.rtg_depth.createBuffer(tokens.RGBA8, tokens.color)
    self.rtb_depth.clearColor = core.vec4(0, 0, 0, 1)
    self.rtg_depth.createDepthBuffer(tokens.Z32F, True)
    self.rtg_depth.autoclear = True

  ##############################################

  def set_view(self, eye, tgt):
    """One clip-space matrix for BOTH paths: c_vp in the SSBO (the mesh stage's
    meshlet cull reads it) and the material's mvp uniform (both stages transform
    with it) — so the paths cannot disagree about the camera."""
    proj = core.mtx4.perspective(45.0 * 3.14159265 / 180.0, float(W) / float(H), 1.0, 4000.0)
    view = core.mtx4.lookAt(eye, tgt, core.vec3(0, 1, 0))
    self.vp = proj * view   # orkid operator* is rtol; matches CameraMatrices::multiply_ltor(v,p)
    fxi = self.ctx.FXI
    fxi.copyDataIntoShaderStorageBuffer(numpy.array(self.vp, dtype=numpy.float32).reshape(-1),
                                        self.ssbo, self.vs.CAM_OFF)
    fxi.copyDataIntoShaderStorageBuffer(numpy.array(self.vp.inverse, dtype=numpy.float32).reshape(-1),
                                        self.ssbo, self.vs.CAM_OFF + 64)
    fxi.copyDataIntoShaderStorageBuffer(numpy.array([eye.x, eye.y, eye.z, 1.0], dtype=numpy.float32),
                                        self.ssbo, self.vs.CAM_OFF + 128)
    # misc.x = CullFrustumScale 1.0 (exact frustum), misc.yzw = 0 -> HZB occlusion off
    fxi.copyDataIntoShaderStorageBuffer(numpy.array([1.0, 0.0, 0.0, 0.0], dtype=numpy.float32),
                                        self.ssbo, self.vs.CAM_OFF + 144)

  def capture(self, key):
    """One offscreen frame through technique `key`, read back as RGBA8."""
    ctx     = self.ctx
    is_mesh = key.endswith("_mesh")
    rtg     = self.rtg_depth if key.startswith("depth") else self.rtg_color
    rtb     = self.rtb_depth if key.startswith("depth") else self.rtb_color
    capbuf  = lev2.CaptureBuffer()
    ctx.beginFrame()
    ctx.FBI.rtGroupPush(rtg)
    ctx.FBI.rtGroupClear(rtg)
    RCFD = lev2.RenderContextFrameData(ctx)
    RCID = lev2.RenderContextInstData(RCFD)
    RCID.forceTechnique(self.teks[key])
    RCID.genMatrix(lambda: core.mtx4())
    pipe = self.pipe[key]
    if is_mesh:
      gx, gy, gz = self.groups
      pipe.wrappedDrawCall(RCID, lambda: ctx.GBI.drawMeshTasks(gx, gy, gz))
    else:
      pipe.wrappedDrawCall(RCID, lambda: ctx.GBI.drawIndirect(
          args=self.ssbo, primtype=tokens.TRIANGLES, args_offset=self.vs.ARGS_OFF))
    future = ctx.FBI.captureAsFormat(rtb, capbuf, "RGBA8")
    ctx.FBI.rtGroupPop()
    ctx.endFrame()
    frames = 0
    while not future.is_ready:
      self.app.run_frames(1)
      frames += 1
      assert frames < 600, "template %s capture never became ready" % key
    return numpy.array(capbuf, dtype=numpy.uint8).reshape(capbuf.height, capbuf.width, 4)

################################################################################

def compare(a, b, chans):
  """(exact-match fraction, mean |delta|, coverage of a, coverage of b) over `chans`."""
  ra, rb = a[..., :chans].astype(numpy.int16), b[..., :chans].astype(numpy.int16)
  exact = float((ra == rb).all(axis=2).mean())
  mean  = float(numpy.abs(ra - rb).mean())
  cov_a = float((ra.sum(axis=2) > 0).mean())
  cov_b = float((rb.sum(axis=2) > 0).mean())
  return exact, mean, cov_a, cov_b

################################################################################

def main():
  fails, digests = phase_a()
  print("ptex3d template byte-stability (mesh OFF must be byte-identical):")
  for name in sorted(digests):
    mark = ""
    if name in GOLDEN:
      mark = "  OK" if digests[name] == GOLDEN[name] else "  MISMATCH"
    print("  %-22s %s%s" % (name, digests[name], mark))
  if fails:
    for f in fails:
      print("  FAIL: %s" % f)

  code = 1
  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    if not app.ctx.supports_mesh_shader:
      print("SKIP: device has no VK_EXT_mesh_shader (template mesh techniques not exercised)")
      code = verdict(not fails, "template splice: byte-stability %s, GPU parity SKIPPED "
                                "— VK_EXT_mesh_shader unavailable | %s"
                                % ("PASS" if not fails else "FAIL", "; ".join(fails) or "-"))
      sys.exit(code)

    ab = TemplateAB(app)
    print("")
    print("template material: %s" % ab.path)
    print("  dim=%d chunk=%d -> %d chunks | meshlet=%d -> %d verts / %d prims | mesh grid <%d,%d,%d>"
          % (DIM, CHUNK, ab.vs.nchunk, ab.vs.meshlet, ab.vs.mesh_wg, ab.vs.mesh_maxprim,
             ab.groups[0], ab.groups[1], ab.groups[2]))

    rows = []
    for label, a_key, b_key, chans in (("color", "color_vtx", "color_mesh", 3),
                                       ("depth", "depth_vtx", "depth_mesh", 1)):
      img_v = ab.capture(a_key)
      img_m = ab.capture(b_key)
      exact, mean, cov_v, cov_m = compare(img_v, img_m, chans)
      rows.append((label, exact, mean, cov_v, cov_m))
      if exact < PARITY_MIN_EXACT:
        fails.append("%s: exact-match %.4f < %.4f" % (label, exact, PARITY_MIN_EXACT))
      if mean > PARITY_MAX_MEAN:
        fails.append("%s: mean|delta| %.3f > %.3f" % (label, mean, PARITY_MAX_MEAN))
      if cov_v < MIN_COVERAGE:
        fails.append("%s: pull-VS path drew nothing (coverage %.3f)" % (label, cov_v))
      if cov_m < MIN_COVERAGE:
        fails.append("%s: mesh path drew nothing (coverage %.3f)" % (label, cov_m))

    print("")
    print("  pass  | cover vtx/mesh | exact  mean")
    for (label, exact, mean, cov_v, cov_m) in rows:
      print("  %-5s |  %.3f / %.3f | %.4f %.3f" % (label, cov_v, cov_m, exact, mean))
    print("")

    detail = ("ptex3d template mesh splice | byte-stability %d/%d OK | "
              % (len(GOLDEN) - len([f for f in fails if f.startswith("byte-stability")]), len(GOLDEN))
              + " ".join("%s:exact=%.4f cover=%.3f/%.3f" % (r[0], r[1], r[3], r[4]) for r in rows))
    code = verdict(not fails, detail if not fails else detail + " | " + "; ".join(fails))

  sys.exit(code)


main()
