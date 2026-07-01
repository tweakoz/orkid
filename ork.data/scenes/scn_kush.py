###############################################################################
# scn_kush.py — DroopDemoOct as a HYPERECS scene (ren_hypermesh template, droop mesh).
# A uvsphere whose faces inset to octagon caps then grow drooping, tapering, multi-segment
# strands; a sinewave rides each strand's sphere-U as a traveling gravity wave.
#
# The ONE adaptation vs assets/hypermesh/droop_demo_oct.py: that demo animates the wave with a
# Python onUpdate/param (`self._phase`), which can't run in the zero-Python player. Here the
# phase is S.time * WAVE_SPEED — the C++ host clock drives it in-DSL (model B), so it animates
# under ork.ecs.player.exe / ecsplay / ecsedit with no Python at load.
#
#   ork.scene.viewer.py scn_kush
#   ork.scene.tojson.py -i scn_kush -o /tmp/kush.ecs && ork.ecs.player.exe /tmp/kush.ecs
###############################################################################

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.dflow.hypermesh import (Hypermesh as HypermeshDSL, S,
                                            isolate, group, POLY, sl_sin, vexpr,
                                            GpuMeshRenderSource)
from ork.hypergraph.assets.materials.terrain.solid import Solid

lev2_pyexdir.addToSysPath()

# DroopDemoOct parameters (verbatim from the source demo).
RADIUS = 1.4
LEVELS = 16          # strand segments (rings of the single multi-segment extrude)
SEG    = 0.10        # segment length
DROOP  = 0.95        # downward bias added to the normal each level (gravity strength)
TAPER  = 0.12        # inset per segment -> strands taper toward a tip
INSET  = 0.05        # how far the octagon insets in from each face boundary (0..1)

WAVE_FREQ  = 8.0
WAVE_AMP   = 0.6
WAVE_SPEED = 1.2
UP = vec3(0, 1, 0)


class KushMesh(HypermeshDSL):
  """DroopDemoOct, host-clocked: the traveling wave is driven by S.time (the C++
  host clock) instead of a Python onUpdate/param, so it animates in the zero-Python
  player. Otherwise identical to assets/hypermesh/droop_demo_oct.py."""

  VET = dict(allow_self_intersect=True)  # organic strands legitimately interpenetrate

  def __init__(self):
    super().__init__()
    n = self.uvsphere(radius=RADIUS, segments=24, rings=24)
    roots = (S.N.dot(UP) < 0.95)  # whole sphere except the +Y pole
    n = self.select(n, roots, domain=POLY, op=isolate(group(0)))  # selected faces -> bit 0
    # resample every selected face to a regular octagon cap (bit 1); the collar (bit 0) stays on the surface.
    n = self.inset(n, amount=INSET, sides=24, slot=0, mask_inner=isolate(group(1)))
    # DROOP*S.t ramps the gravity bias along the strand (S.t = ring/N); the wave rides the
    # inherited sphere U, phase-advanced by the host clock (was self._phase from onUpdate).
    droop = DROOP * S.t * (1.0 + WAVE_AMP * sl_sin(S.uv.x * WAVE_FREQ + S.time * WAVE_SPEED))
    n = self.extrude_faces(
        n,
        distance=SEG,                              # per-ring step (accumulates along the bent heading)
        segments=LEVELS,                           # the whole strand in ONE op
        direction=(S.N - vexpr(0, droop, 0)),      # per-ring downward bias; heading preserved
        scale=S.pow(1.0 - TAPER, S.t * LEVELS),    # compounded per-segment inset as an absolute curve
        slot=1)                                    # the inset cap (no frontier-bit chain needed)
    n = self.smooth_normals(n)
    self.output(n)


class KushScene(Scene):

  def __init__(self):
    super().__init__()

    ##########################
    # SceneGraph
    ##########################

    SG = self.scenegraph(
        preset             = "ForwardPBR",
        skybox_path        = "<ork_envmaps2>/pillars4k.xir",
        SkyboxIntensity    = 1.0,
        DiffuseIntensity   = 1.0,
        SpecularIntensity  = 1.0,
        AmbientLight       = vec3(0.0),
        msaa               = 3,
        ssaa               = 1 )

    # the hosting system — components only LINK to systems the scene declares
    self.system_data("HypermeshSystem")

    ##########################
    # Assets — material BY NAME, mesh graph EMBEDDED (model B)
    ##########################

    kush_mat = self.asset.Ptex3d(
        "kush_mat",
        dsl_class     = Solid,
        vertex_source = GpuMeshRenderSource(),
        albedo        = vec3(1),
        metallic      = 1.0,
        roughness     = 0.75)

    kush_mesh = self.asset.Hypermesh(
        "kush_mesh",
        dsl_class = KushMesh)

    ##########################
    # The hypermesh entity
    ##########################

    self.entity(
        "kush0",
        components = [self.declare_component(
            "HypermeshComponent",
            drawabledata = kush_mesh.drawable_data(material=kush_mat),
            layername    = "std_forward",
            nodename     = "kush0")])


__all__ = ["KushScene"]
