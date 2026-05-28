################################################################################
# col_planar.py — metaballs + PlaneCollider demo.
#
# Clone of metaballs.py with a single PlaneCollider inserted between the
# force chain and the renderer. Particles fall into the gravity well as
# before, but now bounce/slide off a ground plane at y=0 instead of
# passing through it. Lets you see how PlaneCollider's Restitution and
# Friction shape pile-up vs. bouncy behavior on the metaball iso-surface.
#
# Tuning (additional knobs over metaballs.py):
#   FLOOR.Restitution — 0=stick to floor, 0.5=half-bounce (default),
#                       1=elastic, 2+=energy-gain (jumpy)
#   FLOOR.Friction    — 0=frictionless slide, 1=full tangential stop
#
# Hosted by ork.particle.viewer.py:
#   ork.particle.viewer.py col_planar
################################################################################

from orkengine.core import vec3, vec4, dataflow, CrcStringProxy, lev2_pyexdir
from orkengine.lev2 import particles, Texture, GfxEnv

from ork.dflow.particles import ParticleSystem
from ork.dflow import particles as P
from ork.dflow import Expr as E

lev2_pyexdir.addToSysPath()
from lev2utils import shaders

tokens = CrcStringProxy()


class ColPlanarSystem(ParticleSystem):

  def __init__(self):
    super().__init__()

    ctx = GfxEnv.ref.loadingContext()
    self.material = shaders.createPbrMaterialWithColor(
        ctx=ctx,
        color=vec4(0.25, 0.25, 0.25, 1.0),
        metallic=1.0,
        roughness=0.75)

    self.material.freestyle.rasterstate.depthtest = tokens.LESS

    self.ptc_pool = P.PoolData(size=5000, name="POOL")

    self.emitter = P.RingEmitter(self.ptc_pool, name="EMIT",
                                 LifeSpan=15.0,
                                 EmissionRate=100,
                                 EmissionVelocity=0.25,
                                 EmissionRadius=2.0,
                                 DispersionAngle=0.5,
                                 EmitterSpinRate=10.0,
                                 Direction=vec3(1, 0, 0),
                                 Offset=vec3(0, 4, 0))

    self.gravity = P.Gravity(self.emitter,
                             name="GRAV",
                             Center=vec3(0, 0, 0),
                             G=0.5,
                             Mass=0.1,
                             OthMass=0.1,
                             MinDistance=1.0)

    self.turb = P.Turbulence(self.gravity,
                             name="TURB",
                             Amount=vec3(0.8, 0.4, 0.8)*1.0)

    # Ground plane at y=0, normal pointing up. Half-bounce + a touch of
    # friction so particles roll/settle on the floor rather than skating
    # forever. Restitution=0 would have them pile statically; bump up to
    # ~0.8 for a bouncier rubber-ball look.
    self.floor = P.PlaneCollider(self.turb,
                                 name="FLOOR",
                                 Center=vec3(0, 0.1, 0),
                                 Normal=vec3(1, 1, 0).normalized,
                                 Restitution=0.0,
                                 Friction=0.0)

    self._size_curve = dataflow.floatxf.multicurve().multicurve
    self._size_curve.splitSegment(0)
    self._size_curve.splitSegment(0)
    self._size_curve.splitSegment(0)
    self._size_curve.setPoint(0, 0.00, 0.0)
    self._size_curve.setPoint(1, 0.10, 0.25)
    self._size_curve.setPoint(2, 0.75, 1.0)
    self._size_curve.setPoint(3, 0.95, 1.0)
    self._size_curve.setPoint(4, 1.00, 0.00)

    self.blobs = P.VdbLevelSetRenderer(self.floor, name="BLOBS",
                                       material=self.material,
                                       voxel_size=0.100,
                                       kernel=tokens.WYVILL,
                                       Radius=1.0,
                                       Strength=1.0,
                                       IsoLevel=1.0,
                                       Size=E.curve(E.ptc.unit_age,self._size_curve))
    self.render(self.blobs)
