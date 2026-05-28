################################################################################
# metaballs.py — HyperSyn DSL demo for VdbLevelSetRenderer.
#
# Particles emit from a ring 1m above origin and fall through a gravity
# well at world center, jittered by turbulence. Each particle splats its
# kernel into an OpenVDB FloatGrid, marching cubes pulls a triangle
# iso-surface each frame, RigidPrimitive draws it through a PBR material
# — producing the classic merging-blobs metaball look around the well.
#
# Material is high-gloss metal (mirror mode) so the blob shapes read
# clearly via specular highlights — matches the vdb_sculpt.py demo's
# material choice.
#
# Tuning (on the renderer):
#   voxel_size — grid resolution; smaller = smoother but O((R/v)³) memory
#                per splat sphere. 0.05 is a balance.
#   kernel     — tokens.Wyvill (default smooth), .Cubic / .Quartic
#                (sharper), .Gaussian (very soft).
#   Radius     — per-particle kernel falloff. Bigger = particles merge
#                from further apart.
#   Strength   — kernel amplitude. Higher = thicker blob.
#   IsoLevel   — iso-surface threshold. Lower = bigger surface extent.
#
# Hosted by ork.particles.player.py:
#   ork.particles.player.py metaballs
################################################################################

from orkengine.core import vec3, vec4, dataflow, CrcStringProxy, lev2_pyexdir
from orkengine.lev2 import particles, Texture, GfxEnv

from ork.hypergraph.dflow.particles import ParticleSystem
from ork.hypergraph.dflow import particles as P
from ork.hypergraph.dflow import Expr as E

lev2_pyexdir.addToSysPath()
from lev2utils import shaders

tokens = CrcStringProxy()


class MetaballsSystem(ParticleSystem):

  def __init__(self):
    super().__init__()

    ctx = GfxEnv.ref.loadingContext()
    self.material = shaders.createPbrMaterialWithColor(
        ctx=ctx,
        color=vec4(0.5, 0.5, 1.0, 1.0),
        metallic=0.0,
        roughness=0.0)
    
    self.material.freestyle.rasterstate.depthtest = tokens.LESS

    self.ptc_pool = P.PoolData(size=5000, name="POOL")

    self.emitter = P.RingEmitter(self.ptc_pool, name="EMIT",
                                 LifeSpan=5.0,
                                 EmissionRate=250,
                                 EmissionVelocity=0.25,
                                 EmissionRadius=2.0,
                                 DispersionAngle=0.5,
                                 EmitterSpinRate=10.0,
                                 Direction=vec3(1, 0, 0),
                                 Offset=vec3(0, 4, 0))

    self.gravity = P.Gravity(self.emitter, 
                             name="GRAV",
                             Center=vec3(0, 0, 0),
                             G=1.5,
                             Mass=0.1,
                             OthMass=0.1,
                             MinDistance=2.0)

    self.turb = P.Turbulence(self.gravity, 
                             name="TURB",
                             Amount=vec3(0.8, 0.4, 0.8)*1.0)

    self.vortex = P.Vortex(self.turb, 
                           name="VORT",
                           VortexStrength  = 0.3,
                           OutwardStrength = 0.3,
                           Falloff = 1.0 )


    self._size_curve = dataflow.floatxf.multicurve().multicurve
    self._size_curve.splitSegment(0)   # 1 seg → 2 segs, 3 pts
    self._size_curve.splitSegment(0)   # 2 segs → 3 segs, 4 pts
    self._size_curve.splitSegment(0)   # 2 segs → 3 segs, 4 pts
    self._size_curve.setPoint(0, 0.00, 0.0)
    self._size_curve.setPoint(1, 0.10, 1.5)
    self._size_curve.setPoint(2, 0.75, 1.25)
    self._size_curve.setPoint(3, 0.95, 1.0)
    self._size_curve.setPoint(4, 1.00, 0.00)

    self.blobs = P.VdbLevelSetRenderer(self.vortex, name="BLOBS",
                                       material=self.material,
                                       voxel_size=0.125,
                                       kernel=tokens.WYVILL,
                                       Radius=1.0,
                                       Strength=1.0,
                                       IsoLevel=1.0,
                                       Size=E.curve(E.ptc.unit_age,self._size_curve))
    self.render(self.blobs)
