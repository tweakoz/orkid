#!/usr/bin/env ork.python
################################################################################
# SOFT-PARTICLE DEPTH FADE gate.
#
# What breaks without the fade: a sprite plume standing on the ground is CUT by
# the ground's depth — every billboard shows the hard straight line where it
# intersects the surface. The fade reads the scene depth (RCFD_DEPTH_MAP, the
# read-only prepass depth), reconstructs both linear depths, and ramps the
# premultiplied output to zero over soft_fade_distance of approach.
#
# TWO halves, both committed here:
#
# (1) CODEGEN (no GPU): a fragment class that never calls ctx.soft_fade() must
#     emit text carrying NO trace of the feature — that is the "existing content
#     is unchanged" contract. A class that DOES call it must declare the depth
#     sampler + the SoftFadeDistance/NearFar uniforms, and must NOT bake the
#     distance in as a literal (A8: it rides the uniform, live).
#
# (2) RENDER: one static sprite column standing on an opaque ground quad (the
#     quad is on fwd_layers, so the depth prepass really fills DEPTH_MAP).
#     FOUR captures in ONE process, camera aimed at the world origin so the
#     plume's CONTACT POINT is the exact image centre:
#         bg      emission off  — the particle-free reference
#         off_a   fade off (soft_fade_distance = 0)
#         off_b   fade off again — the frame-to-frame NOISE FLOOR
#         on      fade on  (soft_fade_distance = FADE_DIST)
#     Oracle (orientation-agnostic — everything is |row - centre_row|):
#         the column must carry real energy CLEAR of the contact row; the fade
#         must remove a real share of the CONTACT band's energy (above both an
#         absolute floor and the noise floor); and it must remove essentially
#         NOTHING from the far band. A global alpha scale (or a fade reading a
#         clear-value depth, which dissolves everything) fails the far test;
#         a fade that never reaches the shader fails the contact test.
#
# Lifecycle: hand-rolled ComponentizedApplication + StandardSceneGraphComponent
# rather than ork.testing.capture_app — capture_app's mainThreadLoop is
# single-shot (one capture per process) and this oracle needs four captures of
# ONE settled particle cloud. Verdict is emitted BEFORE teardown either way.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys

# prepend THIS checkout's scripts dir so ork.testing / ork.app / ork.hypergraph
# resolve from the same tree as the engine under test.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine.core import vec2, vec3, vec4, dataflow, CrcStringProxy, Path as CorePath  # core FIRST
from orkengine import lev2
from orkengine.lev2 import particles

from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.hypergraph.dflow.particles import FreestyleFragment, materialize_fragment
from ork.hypergraph.dflow.particles.fragment import generate_fragment_fxv2
from ork.hypergraph.ptex3d import P as F

tokens = CrcStringProxy()

OUTDIR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.environ.get("TMPDIR", "/tmp"), "ptc_softfade_gate")

WIDTH, HEIGHT = 640, 480
SETTLE_FRAMES = 120   # skybox/texture residency + the pool filling up
GAP_FRAMES    = 20    # between captures (also lets a live knob reach the shader)
FADE_DIST     = 3.0   # eye units — the column is 4 units tall, so this stays contact-local

# oracle thresholds. CONTACT is the band around the image centre row = the world
# origin = where the column meets the ground. Measured on this scene: the removal
# is a ~90-row band that STARTS at the contact row, taking near=0.135 of the
# contact band's plume energy and far=0.0000 (exactly nothing) of the rest.
CONTACT_BAND    = 0.10   # |row-centre| / height within this is "at the contact"
FAR_BAND        = 0.30   # ...beyond this is "clear of the contact"
COL_HALFBAND    = 0.22   # central column window (the plume's screen width)
PTC_FAR_MIN     = 0.25   # >=25% of the plume's energy must live clear of contact
DROP_NEAR_MIN   = 0.05   # >=5% of the CONTACT band's plume energy must be removed
DROP_FAR_MAX    = 0.01   # <=1% of the FAR band's may be — the fade is not a dimmer
DROP_ABS_MIN    = 2000.0 # absolute floor (measured ~18k): a plume too faint to
                         # measure must not pass on ratios alone
DROP_NOISE_MULT = 4.0    # the removal must beat the off_a/off_b drift by 4x


###############################################################################
# the fragment: stock PREMA cookie x ramp, with the fade as an opt-in factor on
# the WHOLE premultiplied output (rgb adds, a occludes — a soft particle has to
# scale both or the additive term survives the fade).
###############################################################################

class PlumeFrag(FreestyleFragment):

  def __init__(self, ctx, soft=False):
    cookie = ctx.cookie()
    ramp   = ctx.ramp()
    lum    = F.length(cookie.xyz) * 0.5773
    rgb    = ramp.xyz * cookie.xyz * ctx.color_factor * ctx.modcolor.xyz
    a      = F.saturate(ramp.w * lum * ctx.alpha_factor * ctx.modcolor.w)
    if soft:
      k = ctx.soft_fade()
      self.output(F.vec4(rgb.x * k, rgb.y * k, rgb.z * k, a * k))
    else:
      self.output(F.vec4(rgb.x, rgb.y, rgb.z, a))


def check_codegen():
  """(ok, detail) — the no-GPU half of the gate."""
  plain = generate_fragment_fxv2(PlumeFrag, soft=False)
  soft  = generate_fragment_fxv2(PlumeFrag, soft=True)
  problems = []
  for tok in ("softfade", "SoftFadeDistance", "DepthMap", "sset_ptcfrag_depth", "NearFar"):
    if tok in plain:
      problems.append("un-opted shader leaked '%s'" % tok)
    if tok not in soft:
      problems.append("opted shader is missing '%s'" % tok)
  # A8: the distance is a uniform read, never a folded constant
  if ("/ SoftFadeDistance" not in soft) or ("/ %f" % FADE_DIST) in soft:
    problems.append("fade distance is not a live uniform read")
  if problems:
    return False, "; ".join(problems)
  return True, "codegen clean (plain=%dB soft=%dB)" % (len(plain), len(soft))


###############################################################################
# the scene
###############################################################################

class GateApp(ComponentizedApplication):

  def __init__(self, outdir):
    super().__init__()
    self._outdir = outdir
    self._frame = 0
    self._built = False
    self._phase = 0
    self._phase_frame = 0
    self._done = False
    self._caps = {}
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(0, 5.0, 9.0), tgt=vec3(0, 0, 0), up=vec3(0, 1, 0),
        near=0.5, far=1000.0,
        grid_variant=None,                      # our own ground quad instead — it
                                                # has to be in the DEPTH PREPASS
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 1.0,
          "AmbientLevel":     vec3(0.05),
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################

  def _buildGround(self, ctx, SGC):
    material = lev2.PBRMaterial()
    color = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    nrmap = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")
    material.assignImages(ctx, color=color, normal=nrmap, mtlruf=color, doConform=True)
    material.metallicFactor = 0.0
    material.roughnessFactor = 0.9
    material.doubleSided = True
    material.gpuInit(ctx)
    self.ground_material = material
    E = 30.0
    quad = lev2.meshutil.SubMesh.createFromDict({
        "vertices": [
          {"p": vec3(-E, 0, -E), "n": vec3(0, 1, 0), "b0": vec3(1, 0, 0), "uv0": vec2(0, 0)},
          {"p": vec3( E, 0, -E), "n": vec3(0, 1, 0), "b0": vec3(1, 0, 0), "uv0": vec2(1, 0)},
          {"p": vec3( E, 0,  E), "n": vec3(0, 1, 0), "b0": vec3(1, 0, 0), "uv0": vec2(1, 1)},
          {"p": vec3(-E, 0,  E), "n": vec3(0, 1, 0), "b0": vec3(1, 0, 0), "uv0": vec2(0, 1)},
        ],
        "faces": [[0, 1, 2], [0, 2, 3]],
    })
    self.ground_prim = lev2.RigidPrimitive(quad, ctx)
    drw = self.ground_prim.createDrawable(material)
    # fwd_layers = std_forward AND depth_prepass: without the prepass leg the
    # ground never lands in DEPTH_MAP and the fade has nothing to fade against.
    self.ground_node = SGC.scenegraph.createDrawableNodeOnLayers(
        SGC.fwd_layers, "softfade_ground", drw)

  ##############################################

  def _buildPlume(self, ctx, SGC):
    # STATIC cloud on purpose: zero emission velocity, no forces, effectively
    # infinite lifespan -> once the pool is full the particle positions stop
    # changing, so the four captures differ only by what the shader did.
    gd = dataflow.GraphData.createShared()
    self.POOL  = gd.create("POOL", particles.Pool)
    self.GLOB  = gd.create("GLOB", particles.Globals)
    self.EMITL = gd.create("EMITL", particles.LineEmitter)
    self.SPRI  = gd.create("SPRI", particles.SpriteRenderer)
    gd.connect(self.EMITL.inputs.pool, self.POOL.outputs.pool)
    gd.connect(self.SPRI.inputs.pool, self.EMITL.outputs.pool)
    self.POOL.pool_size = 500
    self.EMITL.inputs.LifeSpan = 1.0e6
    self.EMITL.inputs.EmissionRate = 0.0      # armed after the bg capture
    self.EMITL.inputs.EmissionVelocity = 0.0
    self.EMITL.inputs.DispersionAngle = 0.0
    self.EMITL.inputs.P1 = vec3(0, 0.0, 0)    # standing ON the ground...
    self.EMITL.inputs.P2 = vec3(0, 4.0, 0)    # ...rising well clear of it
    self.SPRI.inputs.Size = 0.7
    self.SPRI.inputs.GradientIntensity = 1.0

    mtl = particles.FreestyleParticleMaterial.createShared()
    mtl.shader_path = materialize_fragment(PlumeFrag, name_hint="softfade_gate", soft=True)
    mtl.blending = tokens.PREMA
    mtl.depthtest = tokens.LEQUALS
    mtl.texture_asset = "src://effect_textures/knob2"
    mtl.gridDim = 1
    mtl.colorIntensity = 1.0
    mtl.alphaIntensity = 1.0
    mtl.soft_fade_distance = 0.0              # OFF for the first two captures
    mtl.gradient.setColorStops({
      0.00: vec4(0.90, 0.50, 0.18, 0.30),   # deliberately UNSATURATED: a blown-out
      1.00: vec4(0.90, 0.50, 0.18, 0.30),   # column clips the fade's effect away,
                                            # and flat so age cannot modulate it
    })
    self.ptc_material = mtl
    self.SPRI.material = mtl

    ddata = lev2.ParticlesDrawableData()
    ddata.graphdata = gd
    self.ptc_drawable_data = ddata
    self.ptc_node = SGC.layer_fwd.createDrawableNode(
        "softfade_plume", ddata.createSGDrawable(SGC.scenegraph))

  ##############################################

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    SGC.pbr_common.enable_skybox = True
    if not SGC.pbr_common.useDepthPrepass:
      # the gate measures the fade; without the prepass the material REFUSES it
      # by name and there is nothing to measure.
      raise RuntimeError("ptc_softfade_gate needs the depth prepass — pbr_common.useDepthPrepass is false")

    self._buildGround(ctx, SGC)
    self._buildPlume(ctx, SGC)

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 3.0
    sun.shadowCaster = False
    sun.lookAt(vec3(20, 40, 25), vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun = sun
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)
    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass  # the SGC drives the scenegraph; the cloud is static by construction

  ##############################################

  def _rtg(self, ctx):
    rtg = getattr(getattr(self.SGC, "SGVPW", None), "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      rtg = ctx.FBI.main_RTG
    return rtg

  def _capture(self, ctx, name):
    path = os.path.join(self._outdir, "ptc_softfade_%s.png" % name)
    ctx.FBI.captureToFile(self._rtg(ctx).buffer(0), CorePath(path))
    self._caps[name] = path

  ##############################################

  def _verdict(self):
    import numpy
    from PIL import Image
    from ork.testing import verdict

    ok_cg, cg_detail = check_codegen()

    for name in ("bg", "off_a", "off_b", "on"):
      p = self._caps.get(name)
      if not (p and os.path.isfile(p) and os.path.getsize(p) > 0):
        self._verdict_code = verdict(
            False, "soft particle fade | %s capture MISSING | codegen: %s" % (name, cg_detail))
        return

    def load(name):
      return numpy.asarray(Image.open(self._caps[name]).convert("RGB"),
                           dtype=numpy.float32).mean(axis=2)

    bg, off_a, off_b, on = (load(n) for n in ("bg", "off_a", "off_b", "on"))
    if not (bg.shape == off_a.shape == off_b.shape == on.shape) or float(off_a.max()) == 0.0:
      self._verdict_code = verdict(
          False, "soft particle fade | bad captures shape=%s max=%.1f | codegen: %s"
                 % (off_a.shape, float(off_a.max()), cg_detail))
      return

    h, w = off_a.shape
    rc, cc = h * 0.5, w * 0.5
    rows = numpy.abs(numpy.arange(h, dtype=numpy.float32) - rc) / float(h)
    cols = numpy.abs(numpy.arange(w, dtype=numpy.float32) - cc) / float(w)
    plume_cols = cols <= COL_HALFBAND              # the column the sprites occupy
    all_rows   = rows >= 0.0
    far_rows   = rows >= FAR_BAND                  # clear of the ground contact
    near_rows  = rows <= CONTACT_BAND              # AT the ground contact

    def band(img, rmask):
      return float(img[numpy.ix_(rmask, plume_cols)].sum())

    ptc   = numpy.clip(off_a - bg, 0.0, None)      # the plume's own energy
    drop  = numpy.clip(off_a - on, 0.0, None)      # what the fade removed
    noise = numpy.abs(off_a - off_b)               # frame-to-frame drift, fade off

    ptc_all,  ptc_near,  ptc_far  = (band(ptc, m) for m in (all_rows, near_rows, far_rows))
    drop_all, drop_near, drop_far = (band(drop, m) for m in (all_rows, near_rows, far_rows))
    noise_all = band(noise, all_rows)

    ptc_far_frac = ptc_far / max(ptc_all, 1e-6)    # the plume really extends away
    near_frac    = drop_near / max(ptc_near, 1e-6) # ...the fade bites HERE
    far_frac     = drop_far / max(ptc_far, 1e-6)   # ...and not THERE

    checks = [
      ("plume reaches clear of contact", ptc_far_frac >= PTC_FAR_MIN),
      ("fade bites at the contact",      near_frac >= DROP_NEAR_MIN),
      ("fade spares the far plume",      far_frac <= DROP_FAR_MAX),
      ("removal is measurable",          drop_all >= DROP_ABS_MIN),
      ("removal beats noise floor",      drop_all >= DROP_NOISE_MULT * noise_all),
      ("codegen contract",               ok_cg),
    ]
    ok = all(c[1] for c in checks)
    failed = ", ".join(n for n, v in checks if not v) or "-"
    self._verdict_code = verdict(
        ok,
        "soft particle fade | ptc_far=%.3f(min %.2f) near=%.3f(min %.2f) far=%.4f(max %.3f) "
        "drop=%.0f noise=%.0f | failed[%s] | codegen: %s | %s"
        % (ptc_far_frac, PTC_FAR_MIN, near_frac, DROP_NEAR_MIN, far_frac, DROP_FAR_MAX,
           drop_all, noise_all, failed, cg_detail, self._outdir))

  ##############################################

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._phase_frame = self._frame
      return
    if self._done:
      return
    elapsed = self._frame - self._phase_frame

    def advance():
      self._phase += 1
      self._phase_frame = self._frame

    if self._phase == 0:                       # settle the sky/lighting, no particles
      if elapsed >= SETTLE_FRAMES:
        self._capture(ctx, "bg")
        self.EMITL.inputs.EmissionRate = 40000.0   # fill the pool fast, then idle
        advance()
    elif self._phase == 1:                     # let the pool fill and freeze
      if elapsed >= SETTLE_FRAMES:
        self._capture(ctx, "off_a")
        advance()
    elif self._phase == 2:                     # same state again = the noise floor
      if elapsed >= GAP_FRAMES:
        self._capture(ctx, "off_b")
        self.ptc_material.soft_fade_distance = FADE_DIST   # LIVE knob (A8)
        advance()
    elif self._phase == 3:
      if elapsed >= GAP_FRAMES:
        self._capture(ctx, "on")
        advance()
    elif self._phase == 4:
      if elapsed >= GAP_FRAMES:
        self._verdict()                        # evidence FLUSHED before teardown
        self._done = True
        self.ezapp.signalExit()


def main():
  from ork.testing import Watchdog, verdict
  os.makedirs(OUTDIR, exist_ok=True)
  wd = Watchdog(300.0, label="ptc_softfade_gate").arm()
  app = GateApp(os.path.abspath(OUTDIR))
  app.ezapp.mainThreadLoop()
  wd.disarm()
  code = getattr(app, "_verdict_code", None)
  if code is None:
    code = verdict(False, "soft particle fade | loop exited before captures")
  app.ezapp.shutdown()
  sys.exit(code)


if __name__ == "__main__":
  main()
